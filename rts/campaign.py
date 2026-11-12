"""Bővíthető kampány-rendszer.

Kampányok: campaigns/<id>/campaign.json (+ mods/<mod>/campaigns/<id>/...).
Minden küldetés külön JSON (célok, triggerek, kiinduló helyzet) – formátum:
docs/STORY_CAMPAIGN.md. A haladás a saves/campaign.json fájlba kerül.
"""
import json
import math
import os

from . import dmath
from .data import MODS_DIR, ROOT
from .entities import Unit
from .world import TILE

CAMPAIGN_DIR = os.path.join(ROOT, "campaigns")
SAVE_PATH = os.path.join(ROOT, "saves", "campaign.json")


class MissionLayoutError(ValueError):
    """A küldetés kötelező eleme nem helyezhető el."""


def _read(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def list_campaigns(data=None):
    """[(campaign dict, mappa)] – az alapjáték és az engedélyezett modok kampányai."""
    roots = [CAMPAIGN_DIR]
    roots.extend(os.path.join(MODS_DIR, name, "campaigns") for name in getattr(data, "mod_ids", ()))
    out = []
    for root in roots:
        if not os.path.isdir(root):
            continue
        for cid in sorted(os.listdir(root)):
            p = os.path.join(root, cid, "campaign.json")
            if os.path.isfile(p):
                try:
                    c = _read(p)
                    if not isinstance(c, dict):
                        raise ValueError("a kampányleírás nem objektum")
                    c.setdefault("id", cid)
                    missions = c.get("missions")
                    if (not isinstance(c["id"], str) or not isinstance(c.get("order", 999), (int, float))
                            or not isinstance(missions, list) or not missions
                            or not all(isinstance(m, str) and m for m in missions)):
                        raise ValueError("hibás kampányazonosító, sorrend vagy küldetéslista")
                    c["missions_data"] = []
                    for mid in missions:
                        mission_path = os.path.join(root, cid, f"{mid}.json")
                        md = _read(mission_path)
                        if not isinstance(md, dict) or md.get("id") != mid:
                            raise ValueError(f"{mission_path}: hiányzó vagy eltérő küldetésazonosító")
                        c["missions_data"].append(md)
                except (OSError, ValueError) as exc:
                    print(f"Kampány betöltési hiba ({p}): {exc}")
                    continue
                out.append((c, os.path.join(root, cid)))
    out.sort(key=lambda cc: (cc[0].get("order", 999), cc[0]["id"]))
    return out


def list_novellas():
    """A novellák önálló pályái (novellak/*.json), népenként csoportosítva, a novellák sorrendjében."""
    base = os.path.join(ROOT, "novellak")
    out = []
    try:
        names = sorted(f for f in os.listdir(base) if f.startswith("nov_") and f.endswith(".json"))
    except OSError:
        return []
    for f in names:
        try:
            out.append(_read(os.path.join(base, f)))
        except Exception as e:  # hibás fájl: kimarad, a többi betölthető
            print("Novella-pálya hiba:", f, e)
    groups = []
    for md in out:
        if not groups or groups[-1][0] != md.get("people", ""):
            groups.append((md.get("people", ""), []))
        groups[-1][1].append(md)
    return groups


def novella(nid):
    for _, ms in list_novellas():
        for md in ms:
            if md["id"] == nid:
                return md
    return None


def campaign_done(campaign, prog):
    done = set(prog.get(campaign["id"], {}).get("completed", []))
    return all(m in done for m in campaign["missions"])


def campaign_available(campaign, prog, all_campaigns):
    """Egy hadjárat akkor nyílik meg, ha az előfeltétel-hadjáratai teljesültek ("requires")."""
    by_id = {c["id"]: c for c, _ in all_campaigns}
    return all(r in by_id and campaign_done(by_id[r], prog) for r in campaign.get("requires", []))


def load_progress():
    try:
        return _read(SAVE_PATH)
    except (OSError, ValueError):
        return {}


def save_progress(prog):
    os.makedirs(os.path.dirname(SAVE_PATH), exist_ok=True)
    with open(SAVE_PATH, "w", encoding="utf-8") as f:
        json.dump(prog, f, ensure_ascii=False, indent=1)


def unlocked(campaign, prog):
    """A már elérhető küldetések azonosítói (az első mindig, a többi az előző teljesítése után)."""
    done = set(prog.get(campaign["id"], {}).get("completed", []))
    out = []
    for i, mid in enumerate(campaign["missions"]):
        if i == 0 or campaign["missions"][i - 1] in done:
            out.append(mid)
    return out


def mark_completed(campaign_id, mission_id):
    prog = load_progress()
    c = prog.setdefault(campaign_id, {"completed": []})
    if mission_id not in c["completed"]:
        c["completed"].append(mission_id)
    save_progress(prog)


def _setup_opts(mission):
    """Csapatok: ha vannak szövetséges (bázis nélküli) csoportok, a játékos és ők egy csapat, az ellenfelek a másik."""
    teams = bool(mission.get("allies"))
    me = {"base": mission.get("player", {}).get("base", True)}
    if teams:
        me["team"] = 1
    return me, teams


def build_game(data, mission, seed=None, flags=None, heroes=None):
    """Küldetésből játék: térkép, játékosok, kiinduló helyzet, gépi ellenfelek, rendező.
    heroes: a profilban tárolt hős-állapotok (szint, képességek, tárgyak) – a hősök ezzel lépnek pályára."""
    from . import heroes as H
    from . import mission as MS
    from .ai import AI
    from .game import Game
    me_opt, teams = _setup_opts(mission)
    setup = [(mission["faction"], False, "Játékos", me_opt)]
    for e in mission.get("enemies", []):
        opt = {"base": e.get("base", True)}
        if teams:
            opt["team"] = 2
        setup.append((e["faction"], True, e.get("name", data.factions[e["faction"]]["name"]), opt))
    for a in mission.get("allies", []):
        setup.append((a["faction"], True, a.get("name", data.factions[a["faction"]]["name"]),
                      {"base": a.get("base", False), "team": 1}))
    mp = mission.get("map", {})
    lay = mission.get("layout")
    gm = MS.build_layout_map(data, lay) if lay else None
    if gm is not None and mission.get("theme"):
        gm.theme = mission["theme"]
    if lay and (mission.get("fixed_starts") or mission.get("novella")):  # kezdőhelyek a leírás sorrendjében
        for k, e in enumerate(setup):
            e[3]["start"] = k
    g = Game(data, setup, seed=mp.get("seed", seed), map_size=(mp.get("width"), mp.get("height")),
             creeps=bool(mission.get("creeps")), options={"upkeep": bool(mission.get("upkeep", False)),
                                                          "points": bool(mission.get("points", False))},
             game_map=gm)
    if lay:
        MS.setup_layout(g, lay)
    me = g.players[0]
    me.hero_states = dict(heroes or {})
    pl = mission.get("player", {})
    for k, v in pl.get("resources", {}).items():
        me.res[k] = v
    spawn(g, me, pl.get("units", {}), me.start_xy)
    for extra in pl.get("units_if", []):  # korábbi döntésektől függő kezdő egységek
        if flag_ok(flags or {}, extra.get("if_flag")):
            spawn(g, me, extra["units"], me.start_xy)
    _place_buildings(g, me, pl.get("buildings", []), mission["id"])
    if pl.get("start_packed"):  # nomád kezdés: a főépület vándorszekérként indul
        for b in [b for b in g.buildings if b.owner is me and b.defn.get("packable")]:
            g.cmd_pack(b)
    n_en = len(mission.get("enemies", []))
    for i, e in enumerate(mission.get("enemies", []), start=1):
        p = g.players[i]
        for k, v in e.get("resources", {}).items():
            p.res[k] = v
        spawn(g, p, e.get("units", {}), p.start_xy)
        _place_buildings(g, p, e.get("buildings", []), mission["id"])
        if e.get("ai", True):
            ai = AI(g, p)
            ai.hold_until = e.get("hold", 0)
            ai.attack_supply = int(ai.attack_supply / max(0.2, e.get("aggression", 1.0)))
            g.ais.append(ai)
    for j, a in enumerate(mission.get("allies", []), start=1 + n_en):
        spawn(g, g.players[j], a.get("units", []), g.players[j].start_xy)
    min_lv = mission.get("hero_level", 1)
    for u in g.units:
        if u.hero:
            if u.owner is me:
                H.set_min_level(g, u, min_lv)
            else:  # ellenséges hős: a küldetés szintjén, kiosztott pontokkal
                H.set_min_level(g, u, mission.get("enemy_hero_level", min_lv))
                H.auto_learn(g, u)
    g._update_supply()
    for p in g.players:  # a küldetés kezdő seregéhez ellátmány-bónusz, hogy a kiképzés ne álljon le
        if p.supply_used > p.supply_cap - 2:
            p.bonus_supply = int(p.supply_used - p.supply_cap) + 4
    g._update_supply()
    for p in g.players:
        g._update_vis(p)
    g.director = Director(g, mission, flags)
    return g


def flag_ok(flags, cond):
    """Feltétel a döntés-jelzőkre: "név", "!név", vagy lista (mind teljesüljön)."""
    if not cond:
        return True
    if isinstance(cond, list):
        return all(flag_ok(flags, c) for c in cond)
    neg = cond.startswith("!")
    return bool(flags.get(cond.lstrip("!"))) != neg


def _place_buildings(g, p, specs, mission_id):
    """Előre lerakott (kész) épületek: [{"id": ..., "offset": [dx, dy]}] a kezdőhelyhez képest, vagy
    {"id": ..., "at": "pont"} a pálya elnevezett pontján (csempében)."""
    from .mission import world_xy
    for sp in specs:
        bdef = g.data.defs[sp["id"]]
        w, h = bdef["size"]
        if "at" in sp:
            x, y = world_xy(g, sp["at"])
            cx, cy = int(x // TILE), int(y // TILE)
        else:
            cx = int(p.start_xy[0] // TILE) + sp.get("offset", [6, 0])[0]
            cy = int(p.start_xy[1] // TILE) + sp.get("offset", [6, 0])[1]
        placed = False
        for r in range(0, 10):
            for dy in range(-r, r + 1):
                for dx in range(-r, r + 1):
                    if max(abs(dx), abs(dy)) != r:
                        continue
                    tx, ty = cx + dx - w // 2, cy + dy - h // 2
                    if all(g.map.passable(x, y) and g.map.buildable_terrain(x, y)
                           for y in range(ty - 1, ty + h + 1) for x in range(tx - 1, tx + w + 1)):
                        b = g.place_building(p, bdef, tx, ty, complete=True)
                        if b is not None:
                            if sp.get("tag"):
                                b.mtag = sp["tag"]
                            placed = True
                            break
                if placed:
                    break
            if placed:
                break
        if not placed:
            detail = f"{mission_id}: {sp['id']} ({sp.get('tag') or p.name}) nem helyezhető el ({cx}, {cy})."
            if sp.get("tag") or sp.get("required"):
                raise MissionLayoutError(detail)
            print(f"Küldetés épület-elhelyezési figyelmeztetés: {detail}")
    g._update_supply()
    g._update_network()


def map_point(g, pt):
    """Küldetés-pont -> pixel: elnevezett pont, csempe-koordináta vagy (régi) 0..1 arány."""
    from .mission import world_xy
    return world_xy(g, pt)


def spawn(g, p, units, xy, order_to=None):
    """Egységek lerakása. units: {"azonosító": darab} (régi forma), vagy lista:
    [{"id": ..., "n": 2, "at": "pont", "name": "Révész", "tag": "rev", "to": "pont", "hold": true}]
    – a "name" névvel ellátott közönséges egységet ad (a név nem ad hősi tulajdonságot), a "tag" a küldetés
    csoportja (kísérés, csatlakozás, kudarc-feltétel)."""
    if isinstance(units, dict):
        return _spawn_near(g, p, units, xy, order_to=order_to)
    made = []
    for sp in units:
        at = map_point(g, sp["at"]) if sp.get("at") is not None else xy
        got = _spawn_near(g, p, {sp["id"]: sp.get("n", 1)}, at, spread=sp.get("spread", 1.0))
        for u in got:
            if sp.get("name"):
                u.custom_name = sp["name"]
            if sp.get("tag"):
                u.mtag = sp["tag"]
            if sp.get("hold"):
                u.set_order({"type": "hold"})
            if sp.get("abilities"):  # névvel ellátott egység történetből kapott képességei
                u.extra_abilities = list(sp["abilities"])
        to = sp.get("to") or order_to
        if to is not None and got:
            tx, ty = map_point(g, to) if not isinstance(to, tuple) else to
            g.cmd_move(got, tx, ty, attack=sp.get("attack", True))
        made += got
    return made


def _spawn_near(g, p, units, xy, order_to=None, spread=1.0):
    made = []
    k = 0
    for uid, n in units.items():
        for _ in range(n):
            a = k * 0.9
            r = (70 + 14 * (k // 7)) * spread if (k or spread >= 1.0) else 0
            u = Unit(g, p, g.data.defs[uid], xy[0] + dmath.cos(a) * r, xy[1] + dmath.sin(a) * r)
            spot = g.map.nearest_free(int(u.x // TILE), int(u.y // TILE))
            if spot:
                u.x, u.y = (spot[0] + 0.5) * TILE, (spot[1] + 0.5) * TILE
            g.units.append(u)
            made.append(u)
            k += 1
    if order_to and made:
        g.cmd_move(made, order_to[0], order_to[1], attack=True)
    return made


def tagged(g, tag, owner=None):
    """A küldetés-csoport élő tagjai (egységek és épületek)."""
    return [u for u in g.units + g.buildings
            if u.alive and getattr(u, "mtag", None) == tag and (owner is None or u.owner is owner)]


class Director:
    """Küldetés-rendező: célok, események, párbeszéd, átvezetők, győzelem/vereség.

    Célok (objectives): destroy_all, destroy, gather, survive, kill, reach, build, train, escort, guard.
      Közös mezők: id, text, after (azonosító vagy lista), activate_near ({"at", "r"}), optional, marker.
      escort: a "tag" csoport legalább "min" élő tagja a játékosé, és a célkörben van ("at", "radius").
      guard: a játékos egy egysége "seconds" ideig a célkörben áll.
      reach: "units" (azonosítók) vagy "tag" szűrővel, "count" darab.
    Események (triggers): feltétel: at (mp), on_objective, on_enter ({"at", "r", "tag"?}), on_choice,
      every (mp, ismétlődő; "until": cél-azonosító, "max": darab), if_flag, if_done / unless_done (célok).
      Műveletek: say (párbeszédsorok), cinematic (átvezető), message, join (tag), attack ({"tag"|"owner", "to"}),
      spawn, activate (célok), complete, add_objective, choice, flag, reward, aggression, fail (szöveg).
    Párbeszédsor: {"who": "Tava", "text": "...", "portrait": "wul_hos_tava", "to": "címzett", "urgent": true}.
    """

    def __init__(self, game, mission, flags=None):
        self.g = game
        self.m = mission
        self.flags = dict(flags or {})   # a korábbi küldetések döntései + az ebben hozottak
        self.new_flags = {}              # csak az ebben a küldetésben hozott döntések
        self.pending = None              # függő döntés: {"title", "text", "options": [...]}
        self.choices = []                # napló: (cím, választott opció)
        self.objectives = []
        for o in mission.get("objectives", []):
            gated = o.get("after") or o.get("activate_near") or o.get("hidden")
            self.objectives.append(dict(o, done=False, failed=False, active=not gated, t0=0.0))
        self.triggers = [dict(t, fired=False, n=0, next_t=t.get("at", t.get("every", 0)))
                         for t in mission.get("triggers", [])]
        self.fail_if_lost = [dict(rule) for rule in mission.get("fail_if_lost", [])]
        self.t = 0.0
        self.done = False
        self.lines = []          # várakozó párbeszédsorok
        self.line = None         # (sor, hátralévő idő)
        self.log = []            # minden elhangzott sor (visszaolvasható)
        self.cine = None         # futó átvezető: {"shots": [...], "i": 0, "t": 0.0}
        self.vars = {}           # küldetés-változók (pl. első mentett csoport)
        self.fail_reason = None

    # ---- segédek ----
    def objective(self, oid):
        for o in self.objectives:
            if o["id"] == oid:
                return o
        return None

    def is_done(self, oid):
        o = self.objective(oid)
        return bool(o and o["done"])

    def _after_ok(self, o):
        aft = o.get("after")
        if not aft:
            return True
        ids = [aft] if isinstance(aft, str) else aft
        return all(self.is_done(i) for i in ids)

    def _near(self, at, r, tag=None, any_owner=False):
        g = self.g
        me = g.players[0]
        x, y = map_point(g, at)
        rr = r * TILE
        for u in g.units:
            if not u.alive or (not any_owner and u.owner is not me):
                continue
            if tag and getattr(u, "mtag", None) != tag:
                continue
            if abs(u.x - x) <= rr and abs(u.y - y) <= rr and dmath.hypot(u.x - x, u.y - y) <= rr:
                return True
        return False

    def activate(self, o):
        if o["active"] or o["done"]:
            return
        o["active"] = True
        o["t0"] = self.g.time
        pre = "Új mellékcél: " if o.get("optional") else "Új cél: "
        self.g.notify(self.g.players[0], pre + o["text"], (255, 230, 120))

    # ---- párbeszéd ----
    def say(self, lines):
        urgent = []
        for ln in lines:
            ln = dict(ln)
            if ln.get("urgent"):
                urgent.append(ln)
            else:
                self.lines.append(ln)
        self.lines[0:0] = urgent

    def _tick_lines(self, dt):
        if self.line is not None:
            ln, left = self.line
            left -= dt
            self.line = (ln, left) if left > 0 else None
        if self.line is None and self.lines:
            ln = self.lines.pop(0)
            dur = max(3.2, min(9.0, len(ln.get("text", "")) / 15.0 + 1.6))
            self.line = (ln, dur)
            self.log.append((round(self.g.time), ln.get("who", ""), ln.get("text", "")))

    def current_line(self):
        if self.cine is not None:
            sh = self.cine["shots"][self.cine["i"]]
            return sh.get("say") and sh["say"][min(self.cine["li"], len(sh["say"]) - 1)]
        return self.line[0] if self.line else None

    # ---- átvezető ----
    def start_cinematic(self, cin):
        shots = [dict(s) for s in cin.get("shots", [])]
        if not shots:
            return
        self.cine = {"shots": shots, "i": 0, "t": 0.0, "li": 0, "lt": 0.0}
        self._cine_log(0)

    def _cine_log(self, i):
        for ln in self.cine["shots"][i].get("say", []):
            self.log.append((round(self.g.time), ln.get("who", ""), ln.get("text", "")))

    def tick_cinematic(self, rdt):
        """Valós időben (a szimuláció áll). A felvételek között a kamera a „camera” pontra áll."""
        c = self.cine
        if c is None:
            return
        sh = c["shots"][c["i"]]
        says = sh.get("say", [])
        c["t"] += rdt
        c["lt"] += rdt
        if says:
            cur = says[min(c["li"], len(says) - 1)]
            dur = max(2.6, min(8.0, len(cur.get("text", "")) / 15.0 + 1.4))
            if c["lt"] >= dur and c["li"] < len(says) - 1:
                c["li"] += 1
                c["lt"] = 0.0
            done = c["li"] >= len(says) - 1 and c["lt"] >= dur
        else:
            done = c["t"] >= sh.get("t", 2.5)
        if done:
            self.next_shot()

    def next_shot(self):
        c = self.cine
        if c is None:
            return
        c["i"] += 1
        c["t"] = c["lt"] = 0.0
        c["li"] = 0
        if c["i"] >= len(c["shots"]):
            self.cine = None
        else:
            self._cine_log(c["i"])

    def skip_cinematic(self):
        """A kihagyás ugyanabba az állapotba visz: az átvezető csak kamera és szöveg, a műveletek az esemény
        indulásakor már megtörténtek; a hátralévő sorok a naplóba kerülnek."""
        c = self.cine
        if c is None:
            return
        for i in range(c["i"] + 1, len(c["shots"])):
            self._cine_log(i)
        self.cine = None

    # ---- frissítés ----
    def update(self, dt):
        from .mission import update_actors
        g = self.g
        me = g.players[0]
        self.t += dt
        self._dt = dt
        update_actors(g, dt)
        self._tick_lines(dt)
        if self.done or self.pending or self.cine:
            return
        if self._drain_objective_events():
            return
        for o in self.objectives:
            if o["done"] or o["failed"]:
                continue
            if not o["active"]:
                an = o.get("activate_near")
                if (o.get("after") or an) and self._after_ok(o) and (not an or self._near(an["at"], an.get("r", 4))):
                    self.activate(o)   # a "hidden" cél csak eseményből ("activate") nyílik meg
                continue
            if self._check(o):
                o["done"] = True
                tag = "Mellékcél teljesítve: " if o.get("optional") else "Teljesítve: "
                g.notify(me, tag + o["text"], (140, 255, 140))
                self._queue_objective_events(o["id"])
                if self._drain_objective_events():
                    return
        for t in self.triggers:
            if t["fired"] and not t.get("every"):
                continue
            if not self._cond(t):
                continue
            self._fire(t)
            if self.pending or self.cine:
                return
        # kudarc: védendő egység, épület vagy csoport elvesztése (a vereség megelőzi az egyidejű győzelmet)
        for f in self.fail_if_lost:
            if not flag_ok(self.flags, f.get("if_flag")) or self.t <= 2:
                continue
            if f.get("unless_done") and self.is_done(f["unless_done"]):
                continue
            if f.get("tag"):
                alive = tagged(g, f["tag"])
                if alive:
                    f["_seen"] = True   # csak a már megjelent csoport veszhet el (pl. később érkező civilek)
                lost = not alive and f.get("_seen", False)
            else:
                lost = not any(e.alive and e.owner is me and e.defn["id"] == f["id"] for e in g.units + g.buildings) and \
                    not any(u.owner is me and getattr(u, "packed", None) and u.packed["id"] == f["id"] for u in g.units)
            if lost:
                self.fail_reason = f.get("text", "Elveszett, amit meg kellett védeni!")
                g.notify(me, self.fail_reason, (255, 100, 80))
                me.defeated = True
                break
        if me.defeated:
            g.result = ("lose", me)
            self.done = True
        elif any(not o.get("optional") for o in self.objectives) and all(
                o["done"] for o in self.objectives if not o.get("optional")):
            g.result = ("win", me)
            self.done = True

    def _queue_objective_events(self, oid):
        queued = self.__dict__.setdefault("_objective_events", [])
        queued.extend(t for t in self.triggers if t.get("on_objective") == oid
                      and not t.get("every") and not t["fired"])

    def _drain_objective_events(self):
        queued = getattr(self, "_objective_events", [])
        while queued and not (self.cine or self.pending):
            trigger = queued.pop(0)
            if not trigger["fired"]:
                self._fire(trigger)
        return bool(self.cine or self.pending)

    def _guards_ok(self, t):
        if t.get("on_choice"):
            flags = getattr(self, "choice_flags", {})
            if not flags or not flag_ok(flags, t["on_choice"]):
                return False
        def ids(v):
            return [v] if isinstance(v, str) else v
        if t.get("if_done") and not all(self.is_done(i) for i in ids(t["if_done"])):
            return False
        if t.get("unless_done") and any(self.is_done(i) for i in ids(t["unless_done"])):
            return False
        return True

    def _cond(self, t):
        g = self.g
        if not self._guards_ok(t):
            return False
        if t.get("every"):
            if t.get("on_objective") and not self.is_done(t["on_objective"]):
                return False
            if t.get("until") and self.is_done(t["until"]):
                return False
            if t.get("if_flag") and not flag_ok(self.flags, t["if_flag"]):  # ismétlődő: megvárja a jelzőt
                return False
            if t.get("delay") is not None and "armed" not in t:  # az első alkalom a feltétel teljesülése után ennyivel
                t["armed"] = True
                t["next_t"] = max(t["next_t"], g.time + t["delay"])
                return False
            if t.get("max") and t["n"] >= t["max"]:
                return False
            if g.time < t["next_t"]:
                return False
            return True
        if "on_objective" in t:
            return False  # a cél teljesülésekor fut le
        if "on_enter" in t:
            e = t["on_enter"]
            return self._near(e["at"], e.get("r", 4), e.get("tag"), e.get("any", False))
        if "at" in t:
            return g.time >= t["at"]
        if "on_var" in t:
            k, v = t["on_var"]
            return self.vars.get(k) == v
        return bool(t.get("on_choice"))

    def _check(self, o):
        g = self.g
        me = g.players[0]
        typ = o["type"]
        enemies = [p for p in g.players if p is not me and me.hostile(p)]
        if typ == "destroy_all":
            return all(p.defeated or not any(b.owner is p for b in g.buildings) for p in enemies)
        if typ == "destroy":
            targets = o["targets"] if isinstance(o.get("targets"), list) else [o.get("target")]
            ids = {target for target in targets if target}
            alive = any(e.alive and me.hostile(e.owner)
                        and (e.defn["id"] in ids or getattr(e, "mtag", None) in ids)
                        for e in g.buildings + g.units)
            if alive:
                o["_seen"] = True  # csak akkor teljesül, ha a cél egyszer már létezett
            return not alive and o.get("_seen", False)
        if typ == "gather":
            base = o.setdefault("_base", me.gathered.get(o["resource"], 0))
            return me.gathered.get(o["resource"], 0) - base >= o["amount"]
        if typ == "survive":
            o["left"] = max(0, o["seconds"] - (g.time - o["t0"]))
            return o["left"] <= 0
        if typ == "kill":
            base = o.setdefault("_base", me.kills)
            o["got"] = me.kills - base
            return o["got"] >= o["amount"]
        if typ in ("reach", "escort"):
            x, y = map_point(g, o["at"])
            r = o.get("radius", 4) * TILE
            ids = o.get("units")
            tag = o.get("tag")
            n = sum(1 for u in g.units if u.alive and u.owner is me and (not ids or u.defn["id"] in ids)
                    and (not tag or getattr(u, "mtag", None) == tag) and dmath.hypot(u.x - x, u.y - y) <= r)
            o["got"] = n
            if typ == "escort":
                o["alive"] = len(tagged(g, tag))
                return n >= o.get("min", 1)
            return n >= o.get("count", 1)
        if typ == "guard":
            x, y = map_point(g, o["at"])
            r = o.get("radius", 2) * TILE
            here = any(u.alive and u.owner is me and dmath.hypot(u.x - x, u.y - y) <= r for u in g.units)
            o["held"] = (o.get("held", 0.0) + self._dt) if here else 0.0
            return o["held"] >= o.get("seconds", 5)
        if typ == "build":
            return sum(1 for b in g.buildings if b.owner is me and b.complete and b.defn["id"] == o["target"]) >= o.get("count", 1)
        if typ == "build_at":  # adott épület(ek) a megjelölt helyen
            ids = o["target"] if isinstance(o["target"], list) else [o["target"]]
            x, y = map_point(g, o["at"])
            r = o.get("radius", 4) * TILE
            n = sum(1 for b in g.buildings if b.owner is me and b.complete and b.alive and b.defn["id"] in ids
                    and dmath.hypot(b.x - x, b.y - y) <= r)
            o["got"] = n
            return n >= o.get("count", 1)
        if typ == "ponds":  # Huk’ucha: kiásott tavacska-csempék (opcionálisan egy körzetben)
            pd = getattr(g, "pond", None)
            if pd is None:
                return False
            if o.get("at") is not None:
                x, y = map_point(g, o["at"])
                cx, cy, rr = int(x // TILE), int(y // TILE), o.get("radius", 6)
                n = sum(1 for yy in range(cy - rr, cy + rr + 1) for xx in range(cx - rr, cx + rr + 1)
                        if g.map.inb(xx, yy) and pd[yy * g.map.w + xx] == me.id + 1)
            else:
                n = sum(1 for v in pd if v == me.id + 1)
            o["got"] = n
            return n >= o["amount"]
        if typ == "train":
            return sum(1 for u in g.units if u.owner is me and u.defn["id"] == o["target"]) >= o.get("count", 1)
        return False

    def _fire(self, t):
        if t.get("on_objective") and not t.get("every") and not self._guards_ok(t):
            t["fired"] = True   # a cél teljesült, de a feltétel nem állt fenn: ez az ág már nem fut le
            return
        t["fired"] = True
        t["n"] += 1
        if t.get("every"):
            t["next_t"] = self.g.time + t["every"]
        if not flag_ok(self.flags, t.get("if_flag")):
            return
        if t.get("choice"):
            if t.get("message"):
                self.g.notify(self.g.players[0], t["message"], tuple(t.get("color", (235, 225, 190))))
            if t.get("say"):
                self.say(t["say"])
            self.pending = dict(t["choice"])
            return
        self._apply(t)

    def choose(self, i):
        """A függő döntés egyik opciójának kiválasztása (a UI hívja)."""
        if not self.pending:
            return
        ch = self.pending
        opt = ch["options"][max(0, min(i, len(ch["options"]) - 1))]
        self.pending = None
        self.choices.append((ch.get("title", ""), opt.get("label", "")))
        choice_flags = self.__dict__.setdefault("choice_flags", {})
        for other in ch["options"]:  # a többi opció jelzőit töröljük (újrajátszáskor is egyértelmű maradjon)
            if other is opt:
                continue
            ofl = other.get("flag")
            for name in ([ofl] if isinstance(ofl, str) else (ofl or [])):
                self.flags[name] = False
                self.new_flags[name] = False
                choice_flags[name] = False
        chosen = opt.get("flag")
        for name in ([chosen] if isinstance(chosen, str) else (chosen or [])):
            choice_flags[name] = True
        self.g.notify(self.g.players[0], f"Döntés: {opt.get('label', '')}", (255, 220, 120))
        self._apply(opt)

    def set_flag(self, name, val=True):
        self.flags[name] = val
        self.new_flags[name] = val

    def _apply(self, t):
        g = self.g
        me = g.players[0]
        fl = t.get("flag")
        for name in ([fl] if isinstance(fl, str) else (fl or [])):
            self.set_flag(name)
        if t.get("set_var"):  # {"név": érték} – csak akkor íródik, ha még nincs értéke (egyszer)
            for k, v in t["set_var"].items():
                if k not in self.vars:
                    self.vars[k] = v
                    if t.get("var_flag"):
                        self.set_flag(f"{k}_{v}")
        for oid in ([t["activate"]] if isinstance(t.get("activate"), str) else t.get("activate", [])):
            o = self.objective(oid)
            if o:
                self.activate(o)
        for oid in ([t["complete"]] if isinstance(t.get("complete"), str) else t.get("complete", [])):
            o = self.objective(oid)
            if o and not o["done"]:
                o["done"] = True
                o["active"] = True
                self._queue_objective_events(oid)
        if t.get("add_objective"):
            o = dict(t["add_objective"], done=False, failed=False, t0=g.time)
            o["active"] = not (o.get("after") or o.get("activate_near") or o.get("hidden"))
            self.objectives.append(o)
            g.notify(me, f"Új cél: {o['text']}", (255, 230, 120))
        if t.get("message"):
            g.notify(me, t["message"], tuple(t.get("color", (235, 225, 190))))
        if t.get("say"):
            self.say(t["say"])
        sid = t.get("say_if_done")  # pl. a megszállt jelzőhely előrejelzése a támadás indulásakor
        if sid and self.is_done(sid["id"]):
            self.say(sid["say"])
        gr = t.get("grant")  # {"tag": ..., "abilities": [...], "text": ...} – a történet során kapott képesség
        if gr:
            for u in tagged(g, gr["tag"]):
                if u.is_unit:
                    u.extra_abilities = list(getattr(u, "extra_abilities", None) or []) +                         [a for a in gr["abilities"] if a not in (getattr(u, "extra_abilities", None) or [])]
            if gr.get("text"):
                g.notify(me, gr["text"], (190, 220, 255))
        if t.get("join"):  # a csoport a játékoshoz csatlakozik (irányítható lesz)
            for u in [u for u in g.units if u.alive and getattr(u, "mtag", None) == t["join"] and u.owner is not me]:
                u.owner = me
                u.set_order(None)
            g._update_supply()
        att = t.get("attack")
        if att:  # ellenséges (vagy bármely) csoport elindítása egy pont felé
            tx, ty = map_point(g, att["to"])
            us = [u for u in g.units if u.alive and u.owner is not me and
                  ((att.get("tag") and getattr(u, "mtag", None) == att["tag"]) or
                   (att.get("owner") is not None and u.owner is g.players[att["owner"]]))]
            if us:
                g.cmd_move(us, tx, ty, attack=True)
        sp = t.get("spawn")
        if sp:
            owner = g.players[sp.get("owner", 1)]
            where = sp.get("near", "enemy_base")
            enemy = g.players[1] if len(g.players) > 1 else me
            ex, ey = enemy.start_xy
            mx, my = me.start_xy
            if where == "enemy_base":
                src = (ex, ey)
            elif where == "player_rear":  # szövetséges erősítés a hátországból
                src = (mx - (ex - mx) * 0.12, my - (ey - my) * 0.12)
            elif isinstance(where, (list, str)) and where not in ("player_base",):
                src = map_point(g, where)
            else:  # player_base: félútról érkeznek, a bázis felé
                src = (mx + (ex - mx) * 0.55, my + (ey - my) * 0.55)
            to = sp.get("to")
            target = map_point(g, to) if to is not None else (None if owner is me else me.start_xy)
            made = spawn(g, owner, sp["units"], src, order_to=target)
            if sp.get("tag"):
                for u in made:
                    u.mtag = sp["tag"]
            if owner is me:
                for u in made:
                    if u.defn.get("hero"):
                        g.notify(me, f"Csatlakozott: {u.name}", (255, 220, 120))
        for k, v in t.get("reward", {}).items():
            me.res[k] = max(0, me.res.get(k, 0) + v)
        if "aggression" in t:
            for ai in g.ais:
                ai.hold_until = 0
                ai.attack_supply = int(ai.attack_supply / max(0.2, t["aggression"]))
        if t.get("fail"):
            self.fail_reason = t["fail"]
            g.notify(me, t["fail"], (255, 100, 80))
            me.defeated = True
        if t.get("reshape"):
            from .mission import reshape
            reshape(g, t["reshape"])
        if t.get("cinematic"):
            self.start_cinematic(t["cinematic"])

    # ---- megjelenítéshez ----
    def victory_text(self):
        """Győzelmi szöveg + a döntésektől függő záró sorok (epilógus)."""
        parts = [self.m.get("victory", "")]
        for v in self.m.get("victory_variants", []):
            if flag_ok(self.flags, v.get("if_flag")):
                parts.append(v["text"])
        return [p for p in parts if p]

    def markers(self):
        """Térképjelzők az aktív, helyhez kötött célokhoz: [(x, y, sugár, felirat)]."""
        out = []
        for o in self.objectives:
            if o["type"] in ("reach", "escort", "guard", "build_at") and o.get("at") is not None and \
                    o["active"] and not o["done"]:
                x, y = map_point(self.g, o["at"])
                out.append((x, y, o.get("radius", 4 if o["type"] != "guard" else 2) * TILE, o.get("marker", "")))
        for t in self.m.get("marks", []):  # állandó jelölések (pl. védendő stég), amíg a feltétel él
            if t.get("until") and self.is_done(t["until"]):
                continue
            if t.get("after") and not self.is_done(t["after"]):
                continue
            x, y = map_point(self.g, t["at"])
            out.append((x, y, t.get("r", 3) * TILE, t.get("label", "")))
        return out

    def status_lines(self):
        out = []
        for o in self.objectives:
            if not o["active"] and not o["done"]:
                continue
            txt = ("(Mellékcél) " if o.get("optional") else "") + o["text"]
            if o["type"] == "survive" and not o["done"]:
                left = int(o.get("left", o["seconds"]))
                txt += f"  ({left // 60}:{left % 60:02d})"
            if o["type"] == "reach" and not o["done"] and o.get("count", 1) > 1:
                txt += f"  ({o.get('got', 0)}/{o['count']})"
            if o["type"] == "escort" and not o["done"]:
                txt += f"  (célban: {o.get('got', 0)}, élnek: {o.get('alive', 0)})"
            if o["type"] in ("build_at", "ponds") and not o["done"]:
                txt += f"  ({o.get('got', 0)}/{o.get('count', o.get('amount', 1))})"
            if o["type"] == "guard" and not o["done"] and o.get("held"):
                txt += f"  ({int(o['held'])}/{o.get('seconds', 5)} mp)"
            if o["type"] == "kill" and not o["done"]:
                txt += f"  ({o.get('got', 0)}/{o['amount']})"
            if o["type"] == "gather" and not o["done"]:
                got = self.g.players[0].gathered.get(o["resource"], 0) - o.get("_base", 0)
                txt += f"  ({int(got)}/{o['amount']})"
            out.append((o["done"], txt))
        return out
