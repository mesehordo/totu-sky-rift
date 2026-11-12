"""LAN többjátékos: felfedezés (UDP), lobbi és lockstep-szinkron (TCP, JSON-sorok).

Felépítés: a gazda (host) a 0. játékos és egyben a kiszolgáló. A kliensek a gazdához csatlakoznak.
Játék közben minden gép ugyanazt a determinisztikus szimulációt futtatja; csak a játékosok
parancsai utaznak. A parancsok a kiadásuk utáni 2. körben hajtódnak végre (1 kör = 3 szimulációs
lépés = 100 ms). A gazda minden körhöz összegyűjti mindenki parancsait, és kiküldi a köteget; amíg
egy gép nem kapta meg a következő kör kötegét, vár (a leglassabb gép tempója számít).
"""
import hashlib
import json
import math
from pathlib import Path
import queue
import socket
import threading
import time

PORT = 47810
DISCOVERY_PORT = 47811
TURN_TICKS = 3
DELAY = 2
MAX_FRAME_BYTES = 1024 * 1024
HELLO_TIMEOUT = 10.0
MAGIC = "EGSZAKADAS-LAN-2"


def content_fingerprint(data):
    from .data import ROOT
    from .mapgen import list_maps
    root = Path(ROOT)
    sources = [root / "main.py", *sorted((root / "rts").glob("*.py"))]
    sources += [root / "mods" / mod / "script.py" for mod in data.mod_ids]
    content = {
        "mods": data.mod_ids,
        "data": data.files,
        "maps": list_maps(data),
        "code": [(p.relative_to(root).as_posix(), p.read_text(encoding="utf-8-sig"))
                 for p in sources if p.is_file()],
    }
    return hashlib.sha256(json.dumps(content, sort_keys=True, ensure_ascii=False).encode("utf-8")).hexdigest()



# ---------------------------------------------------------------------------
# parancsok szerializálása (entitás-hivatkozások azonosítóval)
def enc(v):
    if v is None or isinstance(v, (bool, int, float, str)):
        return v
    if isinstance(v, (list, tuple)):
        return [enc(x) for x in v]
    if isinstance(v, dict):
        if "id" in v and "kind" in v and "faction" in v:  # egység-/épületdefiníció
            return {"D": v["id"]}
        return {"M": {k: enc(x) for k, x in v.items()}}
    if getattr(v, "is_resource", False):
        return {"R": [v.tx, v.ty]}
    if getattr(v, "is_item", False):
        return {"I": v.id}
    if hasattr(v, "faction") and hasattr(v, "res"):  # játékos
        return {"P": v.id}
    if hasattr(v, "id") and (getattr(v, "is_unit", False) or getattr(v, "is_building", False)):
        return {"E": v.id}
    raise TypeError(f"nem küldhető paraméter: {v!r}")


def dec(v, g, ents):
    if isinstance(v, float) and not math.isfinite(v):
        raise ValueError("nem véges parancsparaméter")
    if isinstance(v, list):
        return [dec(x, g, ents) for x in v]
    if isinstance(v, dict):
        if "E" in v:
            return ents.get(v["E"])
        if "R" in v:
            tx, ty = v["R"]
            if type(tx) is not int or type(ty) is not int or not g.map.inb(tx, ty):
                raise ValueError("érvénytelen nyersanyagcsempe")
            return g.map.res_grid[ty * g.map.w + tx]
        if "I" in v:
            return next((i for i in g.items if i.id == v["I"]), None)
        if "P" in v:
            index = v["P"]
            if type(index) is not int or not 0 <= index < len(g.players):
                raise ValueError("érvénytelen játékosindex")
            return g.players[index]
        if "D" in v:
            return g.data.defs[v["D"]]
        if "M" in v:
            return {k: dec(x, g, ents) for k, x in v["M"].items()}
    return v


ALLOWED = {"cmd_move", "cmd_attack", "cmd_smart", "cmd_stop", "cmd_hold", "cmd_build", "cmd_train", "cmd_research",
           "cancel_item", "cancel_unit_type", "cmd_rally", "cmd_cast", "cmd_toggle", "cmd_summon", "cmd_buff",
           "cmd_merge", "cmd_pack", "cmd_unpack", "cmd_learn", "cmd_use_item", "cmd_drop_item", "cmd_pickup",
           "cmd_garrison", "unload", "cmd_morph", "cmd_nova", "cmd_mount", "cmd_choose_path", "cmd_set_speed",
           "cmd_choose_ally", "cmd_overdrive", "cmd_anchor_rescue", "cmd_place_ability",
           "cmd_lay_mine", "cmd_ward", "cmd_bpower", "cmd_found", "cmd_gpower", "cmd_dig_pond", "cmd_lay_spikes"}


def execute(g, pid, cmds):
    """Egy játékos parancsainak végrehajtása (csak a saját egységeire hathat)."""
    accepted = set()
    if not cmds:
        return accepted
    ents = {u.id: u for u in g.units}
    ents.update({b.id: b for b in g.buildings})
    ents.update({s.id: s for s in g.spike_targets.values()})
    me = g.players[pid]
    for index, c in enumerate(cmds):
        if not isinstance(c, list) or len(c) < 2 or not isinstance(c[0], str) or not isinstance(c[1], list):
            continue
        name, args = c[0], c[1]
        kw = c[2] if len(c) > 2 and isinstance(c[2], dict) else {}
        kw = {k: v for k, v in kw.items() if k in ("queue", "attack", "target")}
        if name not in ALLOWED:
            continue
        try:
            a = [dec(x, g, ents) for x in args]
        except (KeyError, IndexError, TypeError, ValueError, RecursionError):
            continue
        if any(x is None for x in a[:1]) and name not in ("cmd_build",):
            continue
        # biztonság: a parancs csak a küldő saját dolgaira vonatkozhat
        first = a[0] if a else None
        if isinstance(first, list):
            a[0] = [e for e in first if e is not None and getattr(e, "owner", None) is me]
            if not a[0]:
                continue
        elif hasattr(first, "owner") and getattr(first, "owner", None) is not me :
            continue
        elif hasattr(first, "res") and not hasattr(first, "owner") and first is not me:
            continue
        try:
            if getattr(g, name)(*a, **{k: dec(v, g, ents) for k, v in kw.items()}):
                accepted.add(index)
        except Exception as e:  # egy hibás parancs ne állítsa meg a játékot
            print("[net] parancshiba:", name, e)
    return accepted


def state_hash(g):
    import zlib
    players = [(p.id, p.defeated, p.path, sorted(p.res.items()), sorted(p.researched), sorted(p.researching),
                p.supply_used, p.supply_cap) for p in g.players]
    buildings = [(b.id, b.owner.id, b.defn["id"], b.tx, b.ty, b.hp, b.progress, b.alive, b.queue,
                  enc(b.rally)) for b in g.buildings]
    units = [(u.id, u.owner.id, u.defn["id"], round(u.x, 3), round(u.y, 3), round(u.hp, 3),
              round(u.energy, 3), u.alive, u.cooldown, u.morph, enc(u.inside), enc(u.order), enc(u.queue))
             for u in g.units]
    nodes = [(r.kind, r.tx, r.ty, r.amount) for r in g.map.resources]
    spikes = sorted((i, s.id, s.owner.id, s.hp) for i, s in g.spike_targets.items())
    state = json.dumps([g.tick, players, buildings, units, nodes, spikes], sort_keys=True).encode()
    return zlib.crc32(state, zlib.crc32(bytes(g.map.block)))


# ---------------------------------------------------------------------------
class Conn:
    """JSON-soros TCP-kapcsolat, háttérszálas olvasással."""

    def __init__(self, sock):
        self.sock = sock
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.inbox = queue.Queue()
        self.connected_at = time.monotonic()
        self.alive = True
        self.lock = threading.Lock()
        self.close_lock = threading.Lock()
        threading.Thread(target=self._reader, daemon=True).start()

    def _reader(self):
        buf = b""
        try:
            while self.alive:
                chunk = self.sock.recv(65536)
                if not chunk:
                    break
                buf += chunk
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    if len(line) > MAX_FRAME_BYTES:
                        raise ValueError("túl hosszú hálózati üzenet")
                    if line:
                        msg = json.loads(line.decode("utf-8"))
                        if not isinstance(msg, dict) or msg.get("t") == "_closed":
                            raise ValueError("hibás hálózati üzenet")
                        self.inbox.put(msg)
                if len(buf) > MAX_FRAME_BYTES:
                    raise ValueError("túl hosszú hálózati üzenet")
        except (OSError, ValueError, RecursionError):
            pass
        self.close()

    def send(self, msg):
        if not self.alive:
            return
        data = (json.dumps(msg, ensure_ascii=False) + "\n").encode("utf-8")
        try:
            with self.lock:
                self.sock.sendall(data)
        except OSError:
            self.close()

    def recv_all(self):
        out = []
        while True:
            try:
                out.append(self.inbox.get_nowait())
            except queue.Empty:
                return out

    def close(self):
        with self.close_lock:
            if not self.alive:
                return
            self.alive = False
            self.inbox.put({"t": "_closed"})
        try:
            self.sock.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        try:
            self.sock.close()
        except OSError:
            pass


def discover(timeout=0.8):
    """LAN-on futó játékok keresése: [(cím, info)]."""
    found = {}
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    s.settimeout(0.2)
    try:
        s.sendto(MAGIC.encode(), ("255.255.255.255", DISCOVERY_PORT))
        end = time.time() + timeout
        while time.time() < end:
            try:
                data, addr = s.recvfrom(4096)
                info = json.loads(data.decode("utf-8"))
                if isinstance(info, dict) and info.get("magic") == MAGIC:
                    found[addr[0]] = info
            except (socket.timeout, ValueError, RecursionError):
                continue
            except OSError:
                break
    finally:
        s.close()
    return list(found.items())


# ---------------------------------------------------------------------------
class Lobby:
    """A beállítóképernyő hálózati oldala (gazda vagy kliens)."""

    def __init__(self, host, name, address=None, port=PORT, *, fingerprint):
        self.fingerprint = fingerprint
        self.is_host = host
        self.name = name
        self.messages = []
        self.ready = False
        self.readies = {}
        self.conns = {}          # gazda: helyindex -> Conn
        self.my_slot = 0
        self.started = None      # a játék indító üzenete
        self.closed = False
        self.error = None
        self.setup_cfg = None
        self.setup_revision = 0
        self._setup_snapshot = None
        if host:
            self.server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.server.bind(("", port))
            self.server.listen(8)
            self.server.settimeout(0.0)
            self.udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.udp.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            try:
                self.udp.bind(("", DISCOVERY_PORT))
            except OSError:
                self.udp = None
            if self.udp:
                self.udp.settimeout(0.0)
            self.pending = []    # még helyet nem kapott kapcsolatok
        else:
            s = socket.create_connection((address, port), timeout=4)
            s.settimeout(None)
            self.conn = Conn(s)
            self.conn.send({"t": "hello", "name": name, "magic": MAGIC, "fingerprint": fingerprint})

    # ---- közös -----------------------------------------------------------
    def status_text(self):
        if self.error:
            return self.error
        if self.is_host:
            return f"Gazda – port {PORT}. Csatlakozott játékosok: {len(self.conns)}."
        return "Kapcsolódva. Jelezd, ha kész vagy (KÉSZ gomb)."

    def chat(self, msg):
        if self.is_host:
            self.messages.append((self.name, msg))
            self._broadcast({"t": "chat", "who": self.name, "msg": msg})
        else:
            self.conn.send({"t": "chat", "msg": msg})

    def close(self):
        self.closed = True
        if self.is_host:
            self._broadcast({"t": "bye"})
            for c in self.conns.values():
                c.close()
            for c in getattr(self, "pending", []):
                c.close()
            try:
                self.server.close()
                if self.udp:
                    self.udp.close()
            except OSError:
                pass
        else:
            self.conn.close()

    def slot_ready(self, i):
        return self.readies.get(i, False)

    def all_ready(self):
        if hasattr(self, "screen"):
            self._prune_slots(self.screen)
        return all(self.readies.get(i, False) for i in self.conns)

    def _prune_slots(self, sc):
        n = sc.md()["players"]
        for i, c in list(self.conns.items()):
            if i < n and sc.slots[i]["kind"] == "remote":
                continue
            c.send({"t": "bye"})
            c.close()
            del self.conns[i]
            self.readies.pop(i, None)
            sc.slots[i].update(kind="closed", name="")

    # ---- gazda -------------------------------------------------------------
    def _broadcast(self, msg):
        for c in self.conns.values():
            c.send(msg)

    def push_setup(self, cfg):
        self.setup_cfg = cfg
        if self.is_host and hasattr(self, "screen"):
            self._prune_slots(self.screen)
            setup = {"slots": self.screen.slots, "map": self.screen.map_id, "options": self.screen.options}
            snapshot = json.dumps(setup, sort_keys=True)
            if snapshot != self._setup_snapshot:
                self._setup_snapshot = snapshot
                self.setup_revision += 1
                self.readies = dict.fromkeys(self.conns, False)
            self._broadcast({"t": "setup", "revision": self.setup_revision, **setup})

    def poll(self, screen):
        self.screen = screen
        if self.closed:
            return
        if self.is_host:
            self._host_poll(screen)
        else:
            self._client_poll(screen)

    def _host_poll(self, sc):
        if self.udp:  # felfedezés: válasz a keresőknek
            try:
                while True:
                    data, addr = self.udp.recvfrom(1024)
                    if data.decode(errors="ignore") == MAGIC:
                        md = sc.md()
                        info = {"magic": MAGIC, "name": self.name, "map": md["name"], "players": md["players"],
                                "taken": 1 + len(self.conns), "port": PORT}
                        self.udp.sendto(json.dumps(info, ensure_ascii=False).encode("utf-8"), addr)
            except (BlockingIOError, OSError):
                pass
        try:
            while True:
                s, addr = self.server.accept()
                s.setblocking(True)
                self.pending.append(Conn(s))
        except (BlockingIOError, OSError):
            pass
        now = time.monotonic()
        for c in list(self.pending):
            if not c.alive or now - c.connected_at >= HELLO_TIMEOUT:
                self.pending.remove(c)
                c.close()
                continue
            for m in c.recv_all():
                if m.get("t") == "hello":
                    if m.get("magic") != MAGIC or m.get("fingerprint") != self.fingerprint:
                        self.pending.remove(c)
                        c.send({"t": "incompatible"})
                        c.close()
                        break
                    n = sc.md()["players"]
                    free = [i for i in range(1, n) if sc.slots[i]["kind"] == "open"]
                    self.pending.remove(c)
                    if not free:
                        c.send({"t": "full"})
                        c.close()
                        break
                    i = free[0]
                    sc.slots[i].update(kind="remote", name=str(m.get("name", "?"))[:20])
                    self.conns[i] = c
                    self.readies[i] = False
                    c.send({"t": "welcome", "slot": i})
                    self.messages.append(("*", f"{sc.slots[i]['name']} csatlakozott."))
                    self.push_setup(None)
                    break
        for i, c in list(self.conns.items()):
            for m in c.recv_all():
                t = m.get("t")
                if t == "slot":  # a kliens a saját helyét állítja (faj, szín, csapat, kezdőhely)
                    kv = m.get("kv")
                    if not isinstance(kv, dict):
                        continue
                    colors = sc.app.data.game["player_colors"]
                    starts = sc.md()["players"]
                    valid = {
                        "faction": lambda v: isinstance(v, str) and (v == "random" or v in sc.facs),
                        "color": lambda v: type(v) is int and 0 <= v < len(colors),
                        "team": lambda v: v is None or type(v) is int and 1 <= v <= 4,
                        "start": lambda v: v is None or type(v) is int and 0 <= v < starts,
                    }
                    accepted = {k: v for k, v in kv.items() if k in valid and valid[k](v)}
                    if any(sc.slots[i].get(k) != v for k, v in accepted.items()):
                        sc.slots[i].update(accepted)
                        self.push_setup(None)
                elif t == "ready":
                    if m.get("revision") == self.setup_revision:
                        self.readies[i] = m.get("v") is True
                elif t == "chat":
                    who = sc.slots[i].get("name", "?")
                    self.messages.append((who, m.get("msg", "")))
                    self._broadcast({"t": "chat", "who": who, "msg": m.get("msg", "")})
                elif t == "_closed":
                    self.messages.append(("*", f"{sc.slots[i].get('name')} kilépett."))
                    sc.slots[i].update(kind="open", name="")
                    del self.conns[i]
                    self.readies.pop(i, None)
                    self.push_setup(None)

    def start_game(self, cfg):
        """A gazda indít: a helyindexek -> játékosindexek, mindenkinek elküldve."""
        self._prune_slots(self.screen)
        n = self.screen.md()["players"]
        order = [i for i in range(n) if self.screen.slots[i]["kind"] not in ("closed", "open")]
        cfg = dict(cfg)
        for k, i in enumerate(order):
            if i in self.conns:
                self.conns[i].send({"t": "start", "cfg": dict(cfg, local=k)})
        cfg["local"] = 0
        peers = {order.index(i): c for i, c in self.conns.items()}
        return HostSession(peers, len(order)), cfg

    # ---- kliens -------------------------------------------------------------
    def request_slot(self, kv):
        self.conn.send({"t": "slot", "kv": kv})

    def set_ready(self, v):
        self.ready = v
        self.conn.send({"t": "ready", "v": v, "revision": self.setup_revision})

    def _client_poll(self, sc):
        for m in self.conn.recv_all():
            t = m.get("t")
            if t == "welcome":
                self.my_slot = m["slot"]
            elif t == "setup":
                revision = m["revision"]
                if revision != self.setup_revision:
                    self.ready = False
                    self.setup_revision = revision
                sc.slots = m["slots"]
                sc.options = m["options"]
                if sc.map_id != m["map"]:
                    sc.map_id = m["map"]
            elif t == "chat":
                self.messages.append((m.get("who", "?"), m.get("msg", "")))
            elif t == "start":
                self.started = m["cfg"]
                sc.result = ("start_lan", (ClientSession(self.conn), m["cfg"]))
            elif t == "incompatible":
                self.error = "Eltérő játékverzió, modok vagy pályaadatok."
                self.closed = True
                break
            elif t in ("bye", "_closed", "full"):
                self.error = "A gazda bezárta a játékot." if t != "full" else "A játék megtelt."
                self.closed = True


# ---------------------------------------------------------------------------
class _Session:
    """Lockstep-kör kezelés. A helyi parancsok a DELAY-edik körben hajtódnak végre."""

    def __init__(self, n_players):
        self.n = n_players
        self.turn = 0            # a következő végrehajtandó kör
        self.tick_in_turn = 0
        self.bundles = {k: [] for k in range(DELAY)}  # az első körök üresek
        self.local = []
        self.local_callbacks = {}
        self.turn_callbacks = {}
        self.waiting = False
        self.desync = False
        self.dropped = set()
        self.local_id = 0

    def queue(self, name, args, kw=None, on_result=None):
        e = [name, [enc(a) for a in args]]
        if kw:
            e.append({k: enc(v) for k, v in kw.items()})
        if on_result is not None:
            self.local_callbacks[len(self.local)] = on_result
        self.local.append(e)

    def can_tick(self):
        return self.tick_in_turn > 0 or self.turn in self.bundles

    def before_tick(self, g):
        """A kör első lépése előtt: a köteg végrehajtása és a saját parancsok elküldése."""
        if self.tick_in_turn == 0:
            bundle = self.bundles.pop(self.turn)
            accepted = set()
            for entry in bundle:
                if entry.get("drop") is not None:
                    self._drop(g, entry["drop"])
                else:
                    result = execute(g, entry["p"], entry["c"])
                    if entry["p"] == self.local_id:
                        accepted = result
            for index, callback in self.turn_callbacks.pop(self.turn, {}).items():
                callback(index in accepted)
            send_turn = self.turn + DELAY
            if self.local_callbacks:
                self.turn_callbacks[send_turn] = self.local_callbacks
                self.local_callbacks = {}
            self._send_local(send_turn, g)
        self.tick_in_turn += 1
        if self.tick_in_turn >= TURN_TICKS:
            self.tick_in_turn = 0
            self.turn += 1

    def _drop(self, g, pid):
        if pid in self.dropped:
            return
        self.dropped.add(pid)
        from .ai import AI
        p = g.players[pid]
        p.is_ai = True
        ai = AI(g, p)
        g.ais.append(ai)
        if p.path is not None:
            ai.on_path(p.path)
        if p.ascended and p.path is None:  # felemelkedett, de még nem választott: a gép választ helyette
            g.ai_choose_path(p)
        g.notify(g.players[self.local_id], f"{p.name} kilépett – a gép veszi át.", (255, 200, 90))


class HostSession(_Session):
    def __init__(self, peers, n_players):
        super().__init__(n_players)
        self.peers = peers      # játékosindex -> Conn
        self.collected = {}     # kör -> {játékos: parancsok}
        self.hashes = {}
        self.peer_hashes = {}

    def _send_local(self, turn, g):
        self.collected.setdefault(turn, {})[0] = self.local
        self.local = []
        if turn % 30 == 0:
            h = self.hashes[turn] = state_hash(g)
            if any(value != h for value in self.peer_hashes.pop(turn, {}).values()):
                self.desync = True
            self.hashes.pop(turn - 60, None)

    def pump(self, g):
        for pid, c in list(self.peers.items()):
            messages = c.recv_all()
            if not c.alive and not any(m.get("t") == "_closed" for m in messages):
                messages.append({"t": "_closed"})
            for m in messages:
                if m.get("t") == "cmd":
                    turn, cmds = m.get("turn"), m.get("c")
                    if (type(turn) is int and max(DELAY, self.turn) <= turn <= self.turn + DELAY + 2
                            and turn not in self.bundles and isinstance(cmds, list)):
                        self.collected.setdefault(turn, {}).setdefault(pid, cmds)
                elif m.get("t") == "hash":
                    turn, value = m.get("turn"), m.get("h")
                    if type(turn) is not int or type(value) is not int or turn % 30:
                        continue
                    h0 = self.hashes.get(turn)
                    if h0 is not None:
                        if h0 != value:
                            self.desync = True
                    elif self.turn <= turn <= self.turn + DELAY + 2:
                        self.peer_hashes.setdefault(turn, {})[pid] = value
                elif m.get("t") == "_closed":
                    del self.peers[pid]
                    self.collected.setdefault(self.turn + DELAY, {})
                    self._pending_drop = getattr(self, "_pending_drop", []) + [pid]
        # kötegek kiadása, ha mindenki beküldte
        for turn in sorted(self.collected):
            got = self.collected[turn]
            need = [0] + list(self.peers)
            if all(p in got for p in need):
                bundle = [{"p": p, "c": got[p]} for p in sorted(got)]
                for pid in getattr(self, "_pending_drop", []):
                    bundle.append({"drop": pid})
                self._pending_drop = []
                for c in self.peers.values():
                    c.send({"t": "turn", "k": turn, "b": bundle})
                self.bundles[turn] = bundle
                del self.collected[turn]
            else:
                break
        self.waiting = self.turn not in self.bundles and self.tick_in_turn == 0

    def close(self):
        for c in self.peers.values():
            c.send({"t": "bye"})
            c.close()


class ClientSession(_Session):
    def __init__(self, conn):
        super().__init__(0)
        self.conn = conn

    def _send_local(self, turn, g):
        self.conn.send({"t": "cmd", "turn": turn, "c": self.local})
        self.local = []
        if turn % 30 == 0:
            self.conn.send({"t": "hash", "turn": turn, "h": state_hash(g)})

    def pump(self, g):
        for m in self.conn.recv_all():
            if m.get("t") == "turn":
                self.bundles[m["k"]] = m["b"]
            elif m.get("t") in ("bye", "_closed"):
                self.lost = True
        self.waiting = self.turn not in self.bundles and self.tick_in_turn == 0

    def close(self):
        self.conn.close()
