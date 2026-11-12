"""A szimuláció: játékosok, parancsok, harc, köd, mélység, energiamező."""
import math
import random
from collections import deque

from . import dmath
from . import heroes
from . import points as PTS
from .entities import REACH, Building, Unit, gap, is_sheltered, rect_dist, summon_duration
from .pathfinding import _clear_line, find_path, fits, local_detour, smooth_path
from .world import B_BUILDING, B_RESOURCE, FREE, TILE, generate_map

COLLISION_CELL = 48
CELL = 128  # térbeli hash cellamérete pixelben

_circles = {}


_spans = {}
ONES = b"\x01" * 1024


def circle_spans(r):
    """A kör soronként: (dy, fél-szélesség) – a látómezőt soronkénti szelet-értékadással töltjük (gyors)."""
    if r not in _spans:
        _spans[r] = [(dy, max(dx for dx in range(0, r + 1) if dx * dx + dy * dy <= r * r + r))
                     for dy in range(-r, r + 1) if dy * dy <= r * r + r]
    return _spans[r]


def circle_offsets(r):
    if r not in _circles:
        _circles[r] = [(dx, dy) for dy in range(-r, r + 1) for dx in range(-r, r + 1) if dx * dx + dy * dy <= r * r + r]
    return _circles[r]


class Player:
    def __init__(self, game, pid, name, faction, color, is_ai):
        self.id = pid
        self.name = name
        self.faction = faction
        self.color = tuple(color)
        self.is_ai = is_ai
        self.res = dict(faction.get("start_resources", game.data.game["start_resources"]))
        self.gathered = {}
        self.upg = {}
        self.researched = set()
        self.researching = set()
        self.path = None        # a választott fejlődési irány azonosítója (Felemelkedés után)
        self.ascended = False   # kész a Felemelkedés, választásra vár
        self.rules = {}         # a választott irány szabályai
        self.ally = None        # a Diplomácia csarnokában megkötött szövetséges nép azonosítója
        self.forced_ally = None  # egyéni csata: a gépnek előírt szövetséges nép
        self.supply_used = 0.0
        self.supply_cap = 0
        n = game.map.w * game.map.h
        self.visible = bytearray(n)
        self.explored = bytearray(n)
        self.creep = bytearray(n)
        self.defeated = False
        self.kills = 0
        self.lost = 0
        self.units_made = 0
        # összesítő oldal (a meccs végén): bővített statisztika és 15 mp-es idősor
        self.stats = {"units_killed": 0, "buildings_killed": 0, "units_lost": 0, "buildings_lost": 0,
                      "buildings_built": 0, "killed_value": 0, "lost_value": 0}
        self.spent = {}
        self.timeline = []   # (idő, létszám, sereg-érték, összes gyűjtött, ölések)
        self.start_xy = (0, 0)
        self.links = []  # micélia-hálózat élei
        self.bonus_supply = 0  # kampány: a kezdő sereg ellátmánya
        self.hero_states = {}  # hős-azonosító -> mentett állapot (elesett hős újraélesztése, hadjárat)

    neutral = False
    team = None          # csapat (szövetség); None = mindenki ellen
    difficulty = "normal"

    def hostile(self, q):
        """Ellenség-e q? (önmaga és a csapattársak nem; a semleges szörnyek mindenkinek ellenségei)."""
        return q is not None and q is not self and (self.team is None or q.team is None or q.team != self.team)

    def allied(self, q):
        return q is self or (self.team is not None and q is not None and q.team == self.team)
    def res_name(self, kind, rdefs):
        """A nyersanyag neve ennél a fajnál (pl. Vasbirodalom: étel -> Eszencia)."""
        return self.faction.get("resource_names", {}).get(kind, rdefs[kind]["name"])

    def stat(self, defn, name, base=None):
        v = defn.get(name, 0) if base is None else base
        ups = self.upg.get(name)
        if ups:
            if name in ("shield", "energy") and not defn.get(name) and not defn.get(name + "_slot"):
                return v
            tags = defn.get("tags", ())
            bld = defn.get("kind") == "building"
            for tag, amt in ups.items():
                if tag == "all" or tag in tags or tag == defn.get("id") or (tag == "building" and bld) \
                        or (tag == "tower" and bld and defn.get("attack")):
                    v += amt
        return v

    def can_afford(self, cost):
        return all(self.res.get(k, 0) >= v for k, v in cost.items())

    def missing(self, cost):
        return [k for k, v in cost.items() if self.res.get(k, 0) < v]

    def pay(self, cost):
        for k, v in cost.items():
            self.res[k] -= v
            self.spent[k] = self.spent.get(k, 0) + v

    def refund(self, cost):
        for k, v in cost.items():
            self.res[k] = self.res.get(k, 0) + v
            self.spent[k] = self.spent.get(k, 0) - v


class Game:
    def __init__(self, data, setup, seed=None, map_size=None, creeps=False, mapdef=None, options=None, game_map=None):
        """setup: [(faction_id, is_ai, név[, {team, start, color, difficulty}]), ...]
        creeps: semleges szörnytáborok; mapdef: csatapálya-leírás (data/maps); options: kezdő nyersanyag stb."""
        self.data = data
        self.seed = seed if seed is not None else random.randrange(1 << 30)
        random.seed(self.seed)
        self.options = dict(options or {})
        self.upkeep_on = self.options.get("upkeep", True)  # Ö1 Hadtáp: a nagy sereg kevesebb aranyat hoz
        self.mapdef = mapdef
        if game_map is not None:  # kézzel tervezett küldetéspálya (rts/mission.py)
            self.map = game_map
        elif mapdef is not None:
            from .mapgen import generate
            self.map = generate(data, mapdef, self.seed, len(setup))
            self._setup_map_scenery()
        else:
            self.map = generate_map(data, self.seed, map_size)
        self.local_id = 0  # a helyi (képernyő előtt ülő) játékos indexe
        self.director = None  # kampány-küldetésnél a célokat követő rendező
        self.speed = 1.0  # hálózati játék közös sebessége (a gazda állítja; a szimuláció lépésköze nem változik)
        self.time = 0.0
        self.tick = 0
        self._id = 0
        self.units = []
        self.buildings = []
        self.projectiles = []
        self.effects = []
        self.fx = []
        self.corpses = []  # [definíció, tulajdonos, x, y, szög, idő] – halál-animációhoz
        self.found = bytearray(self.map.w * self.map.h)  # Vasbirodalom-alapozás: csempénként tulajdonos id + 1
        self.pond = bytearray(self.map.w * self.map.h)   # Huk'ucha-tavacskák: csempénként tulajdonos id + 1
        self.spike_targets = {}
        self.spikes = bytearray(self.map.w * self.map.h)  # Croftian mérgező tüskék: csempénként tulajdonos id + 1
        self.messages = []
        self.sound_events = []   # (esemény, entitás) – a UI játssza le és üríti; a szimulációra nincs hatása
        self.alerts = []
        self.path_queue = deque()
        self._path_ids = set()
        self.hash = {}
        self.ais = []
        self.creep_version = 0
        self.net_version = 0
        self.lockstep = False
        self.result = None  # None | ("win"|"lose", játékos)
        self.items = []    # földön heverő tárgyak
        self.hatch = []
        self.detected = {}    # croftian fertőzés: [idő, játékos, egység-id, x, y]
        self.camps = []    # semleges szörnytáborok
        self.neutral = None
        self.players = []
        colors = data.game["player_colors"]
        starts = self._assign_starts(setup)
        for i, entry in enumerate(setup):
            fid, is_ai, name = entry[:3]
            opt = entry[3] if len(entry) > 3 else {}
            p = Player(self, i, name, data.factions[fid], colors[opt.get("color", i) % len(colors)], is_ai)
            p.team = opt.get("team")
            p.difficulty = opt.get("difficulty", "normal")
            p.forced_path = opt.get("path") if opt.get("path") in data.factions[fid].get("paths", {}) else None
            p.forced_ally = opt.get("ally") if data.factions.get(opt.get("ally") or "", {}).get("ally") else None
            p.color_index = opt.get("color", i)
            self.players.append(p)
            p.baseless = opt.get("base") is False   # küldetés: bázis nélküli különítmény / szövetséges csoport
            if p.baseless:
                p.start_xy = ((starts[i][0] + 0.5) * TILE, (starts[i][1] + 0.5) * TILE)
            else:
                self._setup_start(p, starts[i])
            res = self.options.get("resources")
            if res:  # kezdő nyersanyag: kevés / közepes / sok
                mult = {"low": 0.6, "medium": 1.0, "high": 2.0}.get(res, 1.0)
                p.res = {k: int(v * mult) for k, v in p.res.items()}
                if res == "high":
                    p.res["meteor"] = p.res.get("meteor", 0) + 150
        if creeps:
            heroes.place_camps(self)
        self.points = []  # Ö2 Égi uralom: Hasadékkövek (minden egyéni csatában; a kampányban nincs)
        if self.options.get("points", False) and getattr(self, "director", None) is None:  # Ö2 elvetve: alapból ki
            PTS.place_points(self)
        self._update_supply()
        self._update_creep(grow=False)
        self._update_power()
        self._update_network()
        for p in self.players:
            self._update_vis(p)
        data.hook("on_game_start", self)

    def _assign_starts(self, setup):
        """Kezdőhelyek: a megadott index, csapatpályán a csapat oldala, egyébként egyenletesen szétosztva."""
        S = list(self.map.starts)
        n = len(setup)
        if len(S) < n:
            raise ValueError(f"A pályán {len(S)} kezdőhely van {n} résztvevőhöz.")
        rnd = random.Random(self.seed ^ 0x5EED)
        free = list(range(len(S)))
        out = [None] * n
        for i, e in enumerate(setup):
            st = (e[3] if len(e) > 3 else {}).get("start")
            if st is not None and st in free:
                out[i] = st
                free.remove(st)
        hint = getattr(self.map, "teams_hint", None)
        teams = sorted({(e[3] if len(e) > 3 else {}).get("team") for e in setup} - {None})
        if not hint and len(S) == n:
            rnd.shuffle(free)
        for i, e in enumerate(setup):
            if out[i] is not None:
                continue
            t = (e[3] if len(e) > 3 else {}).get("team")
            cands = free
            if hint and t in teams and len(teams) <= 2:
                side = teams.index(t)
                cands = [k for k in free if hint[k] == side] or free
            elif len(S) > n:  # kevesebb játékos: egyenletes elosztás a körön
                want = [round(k * len(S) / n) % len(S) for k in range(n)]
                cands = [k for k in want if k in free] or free
            k = cands[0]
            out[i] = k
            free.remove(k)
        return [S[k] for k in out]

    def new_id(self):
        self._id += 1
        return self._id

    def _setup_start(self, p, start):
        f = p.faction
        hq = self.data.defs[f["hq"]]
        w, h = hq["size"]
        if w > self.map.w or h > self.map.h:
            raise ValueError(f"A pálya túl kicsi ehhez a főépülethez: {hq['name']}.")
        tx = max(0, min(self.map.w - w, start[0] - w // 2))
        ty = max(0, min(self.map.h - h, start[1] - h // 2))
        b = self.place_building(p, hq, tx, ty, complete=True)
        if f.get("pond"):  # Huk'ucha: a főépület tavon áll, körülötte kis tóval
            for yy in range(ty - 2, ty + h + 2):
                for xx in range(tx - 2, tx + w + 2):
                    if self.map.buildable_terrain(xx, yy):
                        self.pond[yy * self.map.w + xx] = p.id + 1
        if f.get("foundation"):  # Vasbirodalom: a főépület eleve alapozáson áll (egy csempe peremmel)
            for yy in range(ty - 1, ty + h + 1):
                for xx in range(tx - 1, tx + w + 1):
                    if self.map.buildable_terrain(xx, yy):
                        self.found[yy * self.map.w + xx] = p.id + 1
        p.start_xy = (b.x, b.y)
        cx = self.map.w * TILE / 2
        cy = self.map.h * TILE / 2
        ang = dmath.atan2(cy - b.y, cx - b.x)
        for uid, n in f.get("start_units", {}).items():
            for k in range(n):
                a = ang + (k - n / 2) * 0.35
                x = b.x + dmath.cos(a) * (w * TILE * 0.5 + 40)
                y = b.y + dmath.sin(a) * (h * TILE * 0.5 + 40)
                u = Unit(self, p, self.data.defs[uid], x, y)
                self.units.append(u)
                if u.is_worker:
                    kinds = [f.get("start_gather", "wood"), "stone", "wood", "meteor"]
                    kind = next((k for k in kinds if u.can_gather(k)), None)
                    r = self.nearest_resource(kind, b.x, b.y, 16 * TILE) if kind else None
                    if r is not None:
                        u.set_order({"type": "gather", "res": r})
        if self.data.defs[f["hq"]].get("larva"):
            b.larva = b.defn["larva"]

    # ------------------------------------------------------------------
    # fő ciklus
    def update(self, dt):
        self.time += dt
        self.tick += 1
        Unit.now = self.time
        self._rebuild_hash()
        self._process_paths()
        if self.tick % 15 == 0:
            self._check_ascension()
        if self.tick % 30 == 0:
            self._update_lights_and_guards()
            self._update_rot(1.0)
        if self.tick % 8 == 0:
            self._update_mines()
        if self.tick % 15 == 0:
            self.update_morale(0.5)
            PTS.update_points(self, 0.5)
            self.update_faith(0.5)
            self._update_decay(0.5)
            self._prune_pond_plan()
            self._update_worksites()
            self._update_spikes()
            for p in self.players:
                mo = p.rules.get("moon")
                if mo and (self.time - 0.5) % mo["period"] >= mo["dur"] and self.time % mo["period"] < mo["dur"]:
                    self.notify(p, "Telihold: a falka 15%-kal erősebb (1 percig)!", (220, 220, 255))
        for u in self.units:
            if u.alive:
                u.update(self, dt)
        for b in self.buildings:
            if b.alive:
                b.update(self, dt)
        for pr in self.projectiles:
            pr.update(self, dt)
        for e in self.effects:
            e.update(self, dt)
        self._separate()
        self.units = [u for u in self.units if u.alive]
        self.buildings = [b for b in self.buildings if b.alive]
        self.projectiles = [p for p in self.projectiles if p.alive]
        self.effects = [e for e in self.effects if e.alive]
        for f in self.fx:
            f[4] -= dt
        self.fx = [f for f in self.fx if f[4] > 0]
        if self.tick % 450 == 0:  # idősor az összesítőhöz (15 mp-enként)
            for p in self.players:
                if not hasattr(p, "timeline"):
                    p.timeline = []
                army = [u for u in self.units if u.owner is p and u.alive and not u.is_worker and u.lifetime is None]
                p.timeline.append((self.time, round(p.supply_used, 1), sum(sum(u.defn.get("cost", {}).values())
                                                                          for u in army),
                                   int(sum(p.gathered.values())), p.kills))
        if self.tick % 30 == 0 and self.corpses:
            self.corpses = [c for c in self.corpses if self.time - c[5] < 10]
        self._update_supply()
        if self.tick % 30 == 0:
            rdefs = self.data.game["resources"]
            for r in self.map.resources:
                grow = rdefs[r.kind].get("regrow")
                if grow and r.amount < r.max_amount:
                    r.amount = min(r.max_amount, r.amount + grow)
            self._update_power()
            self._update_creep()
            self._check_victory()
        if self.tick % 30 == 0:
            for b in self.buildings:
                inc = b.defn.get("income")
                if inc and b.complete and b.alive:
                    for k, v in inc.items():
                        b.owner.res[k] = b.owner.res.get(k, 0) + v
                        b.owner.gathered[k] = b.owner.gathered.get(k, 0) + v
        if self.hatch:
            for h in [h for h in self.hatch if h[0] <= self.time]:
                self.hatch.remove(h)
                _, p, uid, x, y = h
                if not p.defeated:
                    u = Unit(self, p, self.data.defs[uid], x, y)
                    self.units.append(u)
                    self.add_fx("death", x, y, 0.5, (150, 255, 120))
        if self.tick % 15 == 0:
            self._update_stealth()
            self._update_spores()
            self._update_network()
            heroes.update_auras(self)
            if self.camps:
                heroes.update_camps(self)
        if self.tick % 6 == 0:
            for p in self.players:
                if not p.defeated:
                    self._update_vis(p)
        for ai in self.ais:
            ai.update(dt)
        if self.director is not None:
            self.director.update(dt)
        elif getattr(self, "actors", None):
            from .mission import update_actors
            update_actors(self, dt)
        if self.data.scripts:
            self.data.hook("on_tick", self, dt)

    # ------------------------------------------------------------------
    # segédek
    def _rebuild_hash(self):
        h, collision = {}, {}
        radius = 0
        for u in self.units:
            if u.alive and u.inside is None:
                h.setdefault((int(u.x) // CELL, int(u.y) // CELL), []).append(u)
                key = (int(u.x) // COLLISION_CELL, int(u.y) // COLLISION_CELL)
                collision.setdefault(key, []).append(u)
                radius = max(radius, u.radius)
        self.hash = h
        self._collision_hash = collision
        self._collision_radius = radius

    def collision_candidates(self, x, y, radius):
        if not hasattr(self, "_collision_hash"):
            return self.query(x, y, radius + TILE)
        # A korábban frissített egységek a lépés eleji helyükhöz vannak indexelve.
        r = radius + self._collision_radius + TILE
        cell = COLLISION_CELL
        out = []
        for cy in range(int(y - r) // cell, int(y + r) // cell + 1):
            for cx in range(int(x - r) // cell, int(x + r) // cell + 1):
                candidates = self._collision_hash.get((cx, cy))
                if candidates:
                    out.extend(candidates)
        return out

    def query(self, x, y, r):
        out = []
        h = self.hash
        for cy in range(int(y - r) // CELL, int(y + r) // CELL + 1):
            for cx in range(int(x - r) // CELL, int(x + r) // CELL + 1):
                lst = h.get((cx, cy))
                if lst:
                    out.extend(lst)
        return out

    def request_path(self, u):
        if u.id not in self._path_ids:
            self._path_ids.add(u.id)
            self.path_queue.append(u)

    def _process_paths(self, budget=6):
        while self.path_queue and budget > 0:
            u = self.path_queue.popleft()
            self._path_ids.discard(u.id)
            if not u.alive or not u.path_pending:
                continue
            if u.path_goal is None:
                u.path_pending = False
                continue
            budget -= 3 if u.repaths else 1
            u.path = self._unit_path(u)
            u.path_pending = False

    def _unit_path(self, u):
        m = self.map
        gx, gy = u.path_goal
        sx, sy = int(u.x // TILE), int(u.y // TILE)
        tx, ty = int(gx // TILE), int(gy // TILE)
        start = (u.x, u.y)
        radius = u.radius
        if not u.repaths and (u.flying or _clear_line(m, *start, gx, gy, radius)):
            return [(gx, gy)]

        occupied = None
        if u.repaths:
            # Újratervezéskor a közeli egységek pillanatnyi helyét is kerülni kell.
            # A távoli, mozgó tömeg miatt nem zárjuk le az egész pályát.
            reach = TILE * 4
            blockers = [(v.x, v.y, dmath.sq(radius + v.radius - 0.01))
                        for v in self.collision_candidates(*start, reach)
                        if v is not u and v.alive and v.flying == u.flying and not v.burrowed
                        and v.inside is None and not u._passes_through(v)
                        and dmath.hypot(v.x - u.x, v.y - u.y) < reach]

            def crowded(x, y):
                return any(dmath.sq(x - bx) + dmath.sq(y - by) < rr for bx, by, rr in blockers)

            if blockers:
                occupied = crowded
                if u.repaths >= 3 and u.wait_t > 4 and self._settle_formation(u, start):
                    return [start]
                if u.flying:
                    distance = dmath.dist(start, (gx, gy))
                    scale = min(1.0, 3 * TILE / max(1, distance))
                    local = (u.x + (gx-u.x)*scale, u.y + (gy-u.y)*scale)
                    detour = local_detour(m, start, local, radius, occupied, flying=True)
                    if detour:
                        return detour + ([(gx, gy)] if scale < 1 else [])
                    if self._settle_formation(u, start):
                        return [start]
                    return [(gx, gy)]
                detour = local_detour(m, start, (gx, gy), radius, occupied)
                if detour:
                    return detour
                order = u.order
                if (u.wait_t > 2 and order and order.get("type") in ("move", "amove")
                        and order.get("move_serial") is not None and dmath.dist(start, (gx, gy)) < 3 * TILE):
                    reserved = [(v.order["x"], v.order["y"], dmath.sq(radius + v.radius + 1))
                                for v in self.units if v is not u and v.alive and not v.flying
                                and v.order and v.order.get("type") in ("move", "amove")
                                and v.order.get("move_serial") is not None]
                    for distance in (TILE / 3, TILE * 2 / 3, TILE, TILE * 1.5):
                        for i in range(12):
                            angle = i * math.tau / 12
                            x, y = gx + dmath.cos(angle) * distance, gy + dmath.sin(angle) * distance
                            if any(dmath.sq(x - bx) + dmath.sq(y - by) < rr for bx, by, rr in reserved):
                                continue
                            if _clear_line(m, gx, gy, x, y, radius) and _clear_line(m, *start, x, y, radius, occupied):
                                order["x"], order["y"] = x, y
                                u.path_goal = (x, y)
                                return [(x, y)]
                    # A formáció helye elérve, a pontos kiosztott pontot most mások zárják el.
                    # Ilyenkor a ténylegesen szabad helyen áll meg, a következő parancs indulhat.
                if self._settle_formation(u, start):
                    return [start]
                tiles = find_path(m, sx, sy, tx, ty, max_nodes=512, radius=radius, occupied=occupied)
                if tiles and tiles[-1] == (tx, ty):
                    points = [((x + .5) * TILE, (y + .5) * TILE) for x, y in tiles]
                    if _clear_line(m, *points[-1], gx, gy, radius, occupied):
                        if points[-1] != (gx, gy):
                            points.append((gx, gy))
                        return smooth_path(m, start, points, radius=radius, occupied=occupied)

        if u.flying or (occupied is None and _clear_line(m, *start, gx, gy, radius)):
            return [(gx, gy)]
        tiles = find_path(m, sx, sy, tx, ty, radius=radius)
        points = [((x + .5) * TILE, (y + .5) * TILE) for x, y in tiles]
        # A célcsempe közepe lehet az a pont, ahonnan a kattintás biztonságosan elérhető.
        if (points and points[-1] != (gx, gy) and m.passable(tx, ty)
                and _clear_line(m, *points[-1], gx, gy, radius)):
            points.append((gx, gy))
        elif not points and _clear_line(m, *start, gx, gy, radius):
            return [(gx, gy)]
        if occupied and points:
            nearby = [(i, p) for i, p in enumerate(points)
                      if TILE < dmath.dist(start, p) < TILE * 4 and not occupied(*p)]
            for i, waypoint in nearby[::-1][:2]:
                detour = local_detour(m, start, waypoint, radius, occupied)
                if detour:
                    return detour + smooth_path(m, waypoint, points[i+1:], radius=radius)
        if points and not _clear_line(m, *start, *points[0], radius):
            center = ((sx + .5) * TILE, (sy + .5) * TILE)
            if _clear_line(m, *start, *center, radius) and _clear_line(m, *center, *points[0], radius):
                points.insert(0, center)
            else:
                escape = local_detour(m, start, points[0], radius, lambda x, y: False)
                if escape:
                    return escape + smooth_path(m, points[0], points[1:], radius=radius)
        return smooth_path(m, start, points, radius=radius)

    def _settle_formation(self, u, point):
        order = u.order
        if not (u.wait_t > 2 and order and order.get("group_move")
                and order.get("type") in ("move", "amove")):
            return False
        goal = u.path_goal
        if dmath.dist(point, goal) >= 2 * TILE:
            center = order.get("destination")
            extent = order.get("formation_extent", 0)
            if not (u.repaths >= 3 and center and dmath.dist(point, center) <= extent):
                return False
        # A csoport közelében állhat meg, de az épület vagy fal túloldalán nem.
        if not u.flying and not _clear_line(self.map, *point, *goal, u.radius):
            return False
        order["x"], order["y"] = point
        u.path_goal = point
        return True

    def _mobility(self, u):
        """Ki tér ki a maradék átfedésnél: mindig a mozgó egység; az álló (harcoló vagy tétlen) egységet nem
        tolják el. Ha mindkettő áll (pl. egymásra képzett egységek), egyformán szétválnak."""
        return 1.0 if u.moving else 0.0

    def _separate(self, passes=1, soft=0.5):
        """Az egységek ne lógjanak egymásra: az átfedés egy részét (soft) oldjuk fel képkockánként, így a tömeg
        lágyan, rángatás nélkül rendeződik. Kivétel: a gyűjtés közben dolgozó munkások (mint a StarCraftban), és a
        különböző rétegek (föld/levegő)."""
        h = self.hash
        mp = self.map
        for _ in range(passes):
            for (cx, cy), lst in h.items():
                near = []
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        l2 = h.get((cx + dx, cy + dy))
                        if l2:
                            near.extend(l2)
                for u in lst:
                    if not u.alive:
                        continue
                    for v in near:
                        if v.id <= u.id or not v.alive or v.flying != u.flying or u.burrowed or v.burrowed:
                            continue
                        if u._passes_through(v):
                            continue
                        ddx, ddy = v.x - u.x, v.y - u.y
                        m = u.radius + v.radius
                        if abs(ddx) >= m or abs(ddy) >= m:
                            continue
                        d = dmath.hypot(ddx, ddy)
                        if d >= m:
                            continue
                        if d < 0.01:  # pontosan egymáson: determinisztikus irány az azonosítóból
                            a = (u.id * 2.399 + v.id) % 6.283
                            ddx, ddy, d = dmath.cos(a), dmath.sin(a), 1.0
                        ov = (m - d) * soft
                        su = u.morph is None and u.speed_base() > 0
                        sv = v.morph is None and v.speed_base() > 0
                        if su and sv:
                            ku, kv = self._mobility(u), self._mobility(v)
                            if ku + kv == 0:
                                ku = kv = 1.0
                            wu, wv = ku / (ku + kv), kv / (ku + kv)
                        elif su or sv:  # mozdíthatatlan egység mellett a másik tér ki
                            wu, wv = float(su), float(sv)
                        else:
                            continue
                        nx, ny = ddx / d, ddy / d
                        ux, uy = u.x - nx * ov * wu, u.y - ny * ov * wu
                        vx, vy = v.x + nx * ov * wv, v.y + ny * ov * wv
                        def allowed(unit, x, y):
                            return (unit.radius <= x <= mp.w*TILE-unit.radius
                                    and unit.radius <= y <= mp.h*TILE-unit.radius
                                    and (unit.flying or fits(mp, x, y, unit.radius)))

                        okv = allowed(v, vx, vy)
                        oku = allowed(u, ux, uy)
                        if su and oku and not okv:  # a fal felőli egység helyett a másik mozdul többet
                            ux, uy = u.x - nx * ov, u.y - ny * ov
                        elif sv and okv and not oku:
                            vx, vy = v.x + nx * ov, v.y + ny * ov
                        if oku and allowed(u, ux, uy):
                            u.x, u.y = ux, uy
                        if okv and allowed(v, vx, vy):
                            v.x, v.y = vx, vy

    def add_fx(self, kind, x, y, dur, color, size=None, el=None):
        """Látvány-effekt (csak megjelenítés). el: a varázslat eleme (a varázslat-látványokhoz, rts/vfx.py)."""
        if len(self.fx) < 300:
            self.fx.append([kind, x, y, dur, dur, color, size, el])

    def notify(self, p, text, color=(230, 230, 200)):
        if p is not None and not p.is_ai:  # címzett szerint: LAN-on mindenki csak a sajátját látja
            self.messages.append((text, color, self.time, p.id))
            if len(self.messages) > 30:
                self.messages = self.messages[-30:]

    def owned_buildings(self, p, complete=None):
        return [b for b in self.buildings if b.owner is p and b.alive and (complete is None or b.complete == complete)]

    def has_building(self, p, bid):
        return any(b.owner is p and b.complete and b.defn["id"] == bid for b in self.buildings)

    def missing_requirements(self, p, defn):
        miss = [self.data.defs[r]["name"] for r in defn.get("requires", []) if not self.has_building(p, r)]
        miss += [self.data.researches.get(r, {}).get("name", r)
                 for r in defn.get("requires_research", []) if r not in p.researched]
        aid = defn.get("ally")
        if aid and getattr(p, "ally", None) != aid:  # szövetséges nép eleme: csak a megkötött szövetséggel
            miss.append("Szövetség: " + self.data.factions.get(aid, {}).get("name", aid))
        if defn.get("requires_ascended") and not p.ascended and self.director is None:
            miss.append("Felemelkedés")
        pid = defn.get("path")
        if pid and p.path != pid and self.director is None:  # a hadjáratban nincs irányzár
            miss.append("Irány: " + p.faction.get("paths", {}).get(pid, {}).get("name", pid))
        return miss

    # ---- Felemelkedés és fejlődési irányok --------------------------------
    def ascension_ready(self, p):
        """A Felemelkedéshez: hány KÜLÖNBÖZŐ fajta kész épület áll – a főépület, a leadóhelyek, a falak és a
        hálózati csomópontok nem számítanak (vissza: (van, kell))."""
        need = p.faction.get("ascension", {}).get("buildings", 3)
        kinds = {b.defn["id"] for b in self.buildings if b.owner is p and b.alive and b.complete
                 and b.defn["id"] != p.faction["hq"] and b.defn.get("shape") not in ("wall", "node", "dropoff")}
        return len(kinds), need

    def cmd_set_speed(self, p, v):
        """Játéksebesség hálózati játékban: csak a gazda (0. játékos) állíthatja, és mindenkinél érvényes."""
        if p.id != 0:
            return False
        self.speed = max(0.5, min(3.0, float(v)))
        self.notify(p, f"Játéksebesség: x{self.speed:g}", (200, 220, 255))
        return True

    def _check_ascension(self):
        """Felemelkedés: amint megvan a 3 kész épület, a nép választhat irányt – ingyen, kutatás nélkül."""
        if self.director is not None:  # a hadjáratban nincs fejlődési irány
            return
        for p in self.players:
            if p.ascended or p.defeated or not p.faction.get("paths") or getattr(p, "neutral", False):
                continue
            have, need = self.ascension_ready(p)
            if have >= need:
                p.ascended = True
                self.notify(p, "Felemelkedés! Válaszd ki a néped fejlődési irányát.", (255, 220, 120))
                if p.is_ai or any(ai.p is p for ai in self.ais):  # gépi (vagy gép irányította, pl. néző) játékos
                    self.ai_choose_path(p)

    # ---- Diplomácia csarnoka: szövetség egy kisebb néppel -------------------
    def ally_ready(self, p):
        return any(b.owner is p and b.alive and b.complete and b.defn.get("diplomacy") for b in self.buildings)

    def cmd_choose_ally(self, p, aid):
        """Szövetség egy kisebb néppel: egyszer, véglegesen; a nép épületei felkerülnek az építési listára."""
        f = self.data.factions.get(aid)
        if not f or not f.get("ally") or getattr(p, "ally", None) is not None:
            return False
        if not self.ally_ready(p):
            self.notify(p, "Ehhez kész Diplomácia csarnoka kell.", (255, 160, 90))
            return False
        cost = p.faction.get("ally_cost", {})
        if not p.can_afford(cost):
            self._notify_missing(p, cost)
            return False
        p.pay(cost)
        p.ally = aid
        self.notify(p, f"Szövetség: {f['name']} – a népük épületei megjelentek az építési listán.", (255, 220, 120))
        return True

    def ai_choose_ally(self, p):
        allies = [f["id"] for f in self.data.allies()]
        if not allies or getattr(p, "ally", None) is not None:
            return None
        forced = getattr(p, "forced_ally", None)
        if forced in allies:
            pick = forced
        else:
            pref = [a for a in p.faction.get("ai", {}).get("ally_pref", []) if a in allies] or allies
            pick = random.choice(pref[:2] if len(pref) > 1 and random.random() < 0.8 else pref)
        if self.cmd_choose_ally(p, pick):
            for ai in self.ais:
                if ai.p is p:
                    ai.on_ally(pick)
            return pick
        return None

    def cmd_choose_path(self, p, pid):
        paths = p.faction.get("paths", {})
        if not p.ascended or p.path is not None or pid not in paths:
            return False
        p.path = pid
        pd = paths[pid]
        p.rules = self.merge_rules(p.rules, pd.get("rules", {}))  # a kutatásokból jött szabályok megmaradnak
        if pd.get("effects"):
            self.apply_research(p, {"effects": pd["effects"]})
        self.notify(p, f"Fejlődési irány: {pd['name']} – megnyíltak az új épületek és egységek.", (255, 220, 120))
        return True

    def power_r(self, b):
        """Energiamező sugara (Rezonancia kutatással nagyobb)."""
        return b.defn.get("power_radius", 0) + b.owner.rules.get("power_add", 0)

    def light_r(self, b):
        """Lámpásfény sugara (Messzefény kutatással nagyobb)."""
        return b.defn.get("light_radius", 0) + b.owner.rules.get("light_add", 0)

    def rule_tags(self, p, key, ent):
        """Az irány-szabály (key) vonatkozik-e erre az entitásra; a szabály vagy None."""
        r = p.rules.get(key)
        if not r:
            return None
        tags = r.get("tags") if isinstance(r, dict) else None
        if tags and not (set(tags) & set(ent.tags)) and ent.defn["id"] not in tags:
            return None
        return r

    def in_own_field(self, ent):
        """Mélység (croftian) vagy hálózat-mező (micélia) – gyorsabb regeneráció."""
        p = ent.owner
        if p.faction.get("mechanic") not in ("creep", "network"):
            return False
        tx, ty = int(ent.x // TILE), int(ent.y // TILE)
        return self.map.inb(tx, ty) and p.creep[ty * self.map.w + tx] == 1

    def pack_mult(self, ent):
        """Wullof falkaszellem (és a Légió-irány): sebzés-szorzó a közeli harcostársak alapján."""
        pb = ent.owner.rules.get("pack_bonus") or ent.owner.faction.get("pack_bonus")
        if not pb or not ent.is_unit or ent.is_worker:
            return 1.0
        if pb.get("tags") and not set(pb["tags"]) & set(ent.tags):
            return 1.0
        return 1.0 + pb["per"] * self._pack_count(ent, pb)

    def pack_speed(self, ent):
        pb = ent.owner.faction.get("pack_bonus")
        if not pb or ent.is_worker or not pb.get("speed_per"):
            return 1.0
        return 1.0 + pb["speed_per"] * self._pack_count(ent, pb)

    def _pack_count(self, ent, pb):
        if getattr(ent, "_pack_t", -1) == self.tick // 15:
            return ent._pack_n
        r = pb.get("radius", 4) * TILE
        tags = set(pb.get("tags") or ())
        n = sum(1 for o in self.query(ent.x, ent.y, r) if o is not ent and o.owner is ent.owner and o.alive
                and o.is_unit and not o.is_worker and "livestock" not in o.tags and (not tags or tags & set(o.tags))
                and abs(o.x - ent.x) <= r and abs(o.y - ent.y) <= r)
        n = min(n, int(pb.get("max", 0.4) / pb["per"]))
        ent._pack_t, ent._pack_n = self.tick // 15, n
        return n

    def pond_speed(self, ent):
        """Tavacskán a békanép gyorsabb, mindenki más lassabb (repülőkre nem hat)."""
        tx, ty = int(ent.x // TILE), int(ent.y // TILE)
        if not self.map.inb(tx, ty) or not self.pond[ty * self.map.w + tx]:
            return 1.0
        ps = self.data.game.get("pond_speed", {"own": 1.3, "other": 0.7})
        return ps["own"] if ent.defn.get("faction") == "hukucha" else ps["other"]

    def on_own_creep(self, ent):
        p = ent.owner
        if p.faction.get("mechanic") != "creep":
            return False
        tx, ty = int(ent.x // TILE), int(ent.y // TILE)
        return self.map.inb(tx, ty) and p.creep[ty * self.map.w + tx] == 1

    # ------------------------------------------------------------------
    # célpontkeresés
    def find_enemy(self, ent, radius):
        best, bestv = None, 1e18
        owner = ent.owner
        visible = self.seen_by if owner.neutral else self.is_visible
        for e in self.query(ent.x, ent.y, radius + 32):
            if not owner.hostile(e.owner) or not ent.can_hit(e) or not visible(owner, e):
                continue
            d = gap(ent, e)
            if d > radius:
                continue
            v = d + (0 if e.attack else 4 * TILE)
            if v < bestv:
                best, bestv = e, v
        if best is not None:
            return best
        for b in self.buildings:
            if not owner.hostile(b.owner) or not ent.can_hit(b) or not visible(owner, b):
                continue
            if abs(b.x - ent.x) > radius + 100 or abs(b.y - ent.y) > radius + 100:
                continue
            d = gap(ent, b)
            if d <= radius:
                v = d - (3 * TILE if b.attack else 0)
                if v < bestv:
                    best, bestv = b, v
        return best

    def enemies_in_radius(self, owner, x, y, r, units_only=False):
        out = [e for e in self.query(x, y, r + 32)
               if e.alive and not is_sheltered(e) and owner.hostile(e.owner)
               and dmath.hypot(e.x - x, e.y - y) - e.radius <= r]
        if not units_only:
            for b in self.buildings:
                if b.alive and owner.hostile(b.owner) and rect_dist(x, y, b.rect) <= r:
                    out.append(b)
        return out

    def count_enemies(self, u, r, ranged_only=False):
        n = 0
        for e in self.query(u.x, u.y, r):
            if not u.owner.hostile(e.owner) or not e.alive or not e.attack:
                continue
            if ranged_only and e.attack["range"] < 1.5:
                continue
            if dmath.hypot(e.x - u.x, e.y - u.y) <= r:
                n += 1
        return n

    def find_cluster(self, u, rng, radius, min_count):
        enemies = [e for e in self.query(u.x, u.y, rng)
                   if e.alive and u.owner.hostile(e.owner) and dmath.hypot(e.x - u.x, e.y - u.y) <= rng]
        best, bestn = None, min_count - 1
        for e in enemies:
            n = sum(1 for o in enemies if dmath.hypot(o.x - e.x, o.y - e.y) <= radius)
            if n > bestn:
                best, bestn = (e.x, e.y), n
        return best

    def find_enemy_near(self, u, x, y, r):
        """Célzott képességhez: a ponthoz legközelebbi ellenséges egység."""
        best, bestd = None, r
        for e in self.query(x, y, r + 32):
            if e.alive and not is_sheltered(e) and u.owner.hostile(e.owner):
                d = dmath.hypot(e.x - x, e.y - y) - e.radius
                if d <= bestd:
                    best, bestd = e, d
        return best

    def most_damaged_ally(self, u, r, tags=("organic",)):
        best, bestv = None, 0.98
        for e in self.query(u.x, u.y, r):
            if e.owner is u.owner and e.alive and any(t in e.tags for t in tags) and dmath.hypot(e.x - u.x, e.y - u.y) <= r:
                v = e.hp / e.max_hp
                if v < bestv:
                    best, bestv = e, v
        return best

    def nearest_resource(self, kind, x, y, maxd=1e9, exclude=None):
        best, bestd = None, maxd
        for r in self.map.resources:
            if r.kind != kind or r is exclude or not r.alive:
                continue
            d = dmath.hypot(r.x - x, r.y - y)
            if d < bestd:
                best, bestd = r, d
        return best

    def landing_position(self, x, y, radius, flying=False, *, origin=None,
                         max_distance=None, ignore=None, reserved=()):
        """Közeli, a teljes egység számára szabad hely. A korlát a végleges pontra vonatkozik."""
        m = self.map
        tx = max(0, min(m.w - 1, int(x // TILE)))
        ty = max(0, min(m.h - 1, int(y // TILE)))
        candidates = [(x, y)]
        candidates.extend(((xx + .5) * TILE, (yy + .5) * TILE)
                          for yy in range(max(0, ty - 4), min(m.h, ty + 5))
                          for xx in range(max(0, tx - 4), min(m.w, tx + 5)))
        candidates.sort(key=lambda pt: dmath.sq(pt[0] - x) + dmath.sq(pt[1] - y))
        occupants = [(u.x, u.y, u.radius) for u in self.units
                     if u is not ignore and u.alive and u.inside is None and u.flying == flying]
        occupants.extend(reserved)
        for px, py in candidates:
            if not (radius <= px <= m.w*TILE-radius and radius <= py <= m.h*TILE-radius):
                continue
            if max_distance is not None and dmath.hypot(px-origin[0], py-origin[1]) > max_distance + 1e-7:
                continue
            if not flying and not fits(m, px, py, radius):
                continue
            if all(dmath.sq(px-ux) + dmath.sq(py-uy) >= dmath.sq(radius+ur) for ux, uy, ur in occupants):
                return px, py
        return None

    def nearest_dropoff(self, p, kind, x, y, exclude=()):
        best, bestd = None, 1e18
        net = p.faction.get("mechanic") == "network"
        for b in self.buildings:
            if b.id not in exclude and b.owner is p and b.complete and b.alive and (
                    kind in b.defn.get("dropoff", ()) or (net and b.connected)):
                d = dmath.hypot(b.x - x, b.y - y)
                if d < bestd:
                    best, bestd = b, d
        return best

    # ------------------------------------------------------------------
    # harc
    def hit(self, src, target, dmg, atk, ranged):
        if is_sheltered(target):
            return
        self.apply_damage(src, target, dmg, atk, ranged)
        if atk and atk.get("splash"):
            self.splash(src, target.x, target.y, dmg, atk, exclude=target)

    def splash(self, src, x, y, dmg, atk, exclude=None):
        for e in self.enemies_in_radius(src.owner, x, y, atk["splash"] * TILE):
            if e is not exclude and src.can_hit(e):
                self.apply_damage(src, e, dmg * 0.5, atk, ranged=True)
        for spike in self.spikes_in_radius(x, y, atk["splash"] * TILE):
            if spike is not exclude:
                self.apply_damage(src, spike, dmg * 0.5, atk, ranged=True)

    def type_mult(self, src, target):
        """Egységtípus-szorzó (Ötletdoboz Ö24, ParaWorld-szerű): a támadó típusa a célpont típusa ellen."""
        if src is None or not getattr(src, "is_unit", False):
            return 1.0
        ut = src.defn.get("utype")
        tt = "epulet" if target.is_building else target.defn.get("utype")
        if not ut or not tt:
            return 1.0
        m = 1.0
        if not src.defn.get("type_nostrength"):
            m = self.data.game.get("unit_types", {}).get("matrix", {}).get(ut, {}).get(tt, 1.0)
        tb = src.defn.get("type_bonus")
        if tb and tt in tb:
            m = max(m, tb[tt])
        if m > 1.0 and target.is_unit:  # gyengeség elvesztése (kutatás vagy aktivált képesség)
            nw = target.owner.rules.get("no_weak")
            if nw and any(ut in v for k, v in nw.items() if k == target.defn["id"] or k in target.tags):
                return 1.0
            for ab_id, until in target.buffs.items():
                if until > self.time and self.data.abilities.get(ab_id, {}).get("no_weak"):
                    return 1.0
        return m

    def apply_element(self, src, target, element, dmg):
        """Elemi hatás a találatkor: tűz -> égés (sebzés idővel, páncélon át), jég -> fagyás (lassulás),
        villám -> sokk (rövid bénulás, célpontonként ritkán), méreg -> mérgezés (gépen nem)."""
        fx = (getattr(self.data, "element_fx", {}) or {}).get(element)
        if not fx or not target.alive or is_sheltered(target):
            return
        now = self.time
        if element == "sunder":                             # páncéltörés: egységen és épületen is
            target.sunder_until = max(getattr(target, "sunder_until", 0.0), now + fx.get("dur", 6))
            target.sunder_armor = fx.get("armor", 2)
            return
        if not target.is_unit:
            return
        wet = getattr(target, "wet_until", 0.0) > now
        if element == "fire":
            if getattr(target, "frozen_until", 0.0) > now:   # tűz felolvasztja a fagyást
                target.frozen_until = 0.0
                target.slow_until = now
            if wet:                                        # vizes egység nem gyullad meg (a tűz felszárítja)
                target.wet_until = now
                return
            dps = max(fx.get("min_dps", 2), min(fx.get("max_dps", 25), dmg * fx.get("ratio", 0.25)))
            cur = getattr(target, "burn", None)
            if cur is None or cur[2] <= now or cur[1] <= dps:
                target.burn = (src, dps, now + fx.get("dur", 3))
        elif element == "ice":
            target.burn = None                             # jég kioltja az égést
            target.slow_mult = min(target.slow_mult if target.slow_until > now else 1.0, fx.get("slow", 0.6))
            target.slow_until = max(target.slow_until, now + fx.get("dur", 2))
            target.frozen_until = max(getattr(target, "frozen_until", 0.0), now + fx.get("dur", 2))
        elif element == "lightning":
            mult = (self.data.element_fx.get("water", {}).get("shock_mult", 2.0)) if wet else 1.0
            if wet or now >= getattr(target, "shock_cd", 0.0):   # vízben szünet nélkül, kétszer hosszabban
                target.stun_until = max(target.stun_until, now + fx.get("dur", 0.4) * mult)
                target.shock_cd = now + fx.get("cooldown", 3)
        elif element == "water":
            target.burn = None                             # víz eloltja a tüzet
            target.wet_until = max(getattr(target, "wet_until", 0.0), now + fx.get("dur", 5))
        elif element == "root":
            if not target.flying:
                target.rooted_until = max(getattr(target, "rooted_until", 0.0), now + fx.get("dur", 2.5))
        elif element == "sleep":
            if not target.hero:                            # a hősök nem alszanak el (csak egy pillanatra)
                target.asleep_until = max(getattr(target, "asleep_until", 0.0), now + fx.get("dur", 6))
            else:
                target.stun_until = max(target.stun_until, now + 1.0)
        elif element == "fear":
            if src is not None:
                target.feared_until = max(getattr(target, "feared_until", 0.0), now + fx.get("dur", 1.6))
                dx, dy = target.x - src.x, target.y - src.y   # a menekülés célja: az okozótól elfelé
                d = dmath.hypot(dx, dy) or 1.0
                flee = fx.get("flee", 3.0) * TILE
                spot = self.map.nearest_free(int((target.x + dx / d * flee) // TILE),
                                             int((target.y + dy / d * flee) // TILE))
                target.fear_from = ((spot[0] + 0.5) * TILE, (spot[1] + 0.5) * TILE) if spot else                     (target.x + dx / d * flee, target.y + dy / d * flee)
                target.path = []
                target.path_goal = None
        elif element == "blind":
            target.blind_until = max(getattr(target, "blind_until", 0.0), now + fx.get("dur", 4))
        elif element == "silence":
            target.silenced_until = max(getattr(target, "silenced_until", 0.0), now + fx.get("dur", 3))
            if target.toggles:                              # a bekapcsolt ének / képesség is elhallgat
                target.toggles.clear()
        elif element == "mark":
            target.marked_until = max(getattr(target, "marked_until", 0.0), now + fx.get("dur", 8))
            target.marked_by = src.owner if src is not None else None
        elif element == "poison":
            if "mech" in target.tags:
                return
            dps = max(fx.get("min_dps", 1.5), min(fx.get("max_dps", 15), dmg * fx.get("ratio", 0.15)))
            if target.poison is None or target.poison[2] <= now or target.poison[1] < dps:
                target.poison = (src, dps, now + fx.get("dur", 4))

    def apply_damage(self, src, target, dmg, atk, ranged, magic=False, element=None):
        if not target.alive or is_sheltered(target):
            return
        el = element or (atk.get("element") if atk else None)
        if target.defn.get("spike_tile"):
            self.damage_spike(src, target, dmg, el, atk)
            return
        if target.is_unit and dmg > 0 and getattr(target, "asleep_until", 0.0) > self.time:
            target.asleep_until = 0.0                      # az első sebzésre felébred
        if ranged and src is not None and getattr(src, "is_unit", False) and \
                getattr(src, "blind_until", 0.0) > self.time and not (atk and atk.get("spell")):
            miss = (self.data.element_fx.get("blind") or {}).get("miss", 0.5)
            if self.rand() < miss:                         # elvakult lövész: mellément
                self.add_fx("ring", target.x + 10, target.y - 4, 0.25, (200, 200, 200))
                return
        if (atk and not atk.get("spell") and dmg > 0 and src is not None and src.is_unit
                and src.defn["id"] == "oon_nian_or" and "oon_kapuorzes" in src.toggles
                and not getattr(src, "gate_guard_slow_used", False)
                and target.is_unit and target.defn.get("utype") == "lovas"):
            src.gate_guard_slow_used = True
            atk = {**atk, "slow": 0.65, "slow_dur": 1.2}
        if el and (target.is_unit or el == "sunder"):
            self.apply_element(src, target, el, dmg)
        if getattr(target, "marked_until", 0.0) > self.time:   # megjelölt: több sebzést kap
            dmg *= (self.data.element_fx.get("mark") or {}).get("dmg_mult", 1.2)
        if target.owner is not None and src is not None and getattr(src, "owner", None) is not target.owner:
            self.sfx("under_attack", target)
        if atk and atk.get("poison") and target.is_unit:  # méreg: (sebzés/mp, időtartam)
            dps, dur = atk["poison"]
            target.poison = (src, dps, self.time + dur)
        elif atk and not atk.get("spell") and target.is_unit and src is not None and src.is_unit:
            pr = self.rule_tags(src.owner, "poison_attacks", src)
            if pr and "organic" in target.tags:
                target.poison = (src, pr["dps"], self.time + pr["dur"])
        if atk and atk.get("spell"):  # hős-képesség: mágikus sebzés + lassítás / bénítás
            magic = True
            sl = atk.get("slow")
            if target.is_unit and sl is not None:
                if sl <= 0:
                    target.stun_until = self.time + atk.get("slow_dur", 1.5)
                    self.add_fx("ring", target.x, target.y, 0.5, (255, 255, 160))
                else:
                    target.slow_until = self.time + atk.get("slow_dur", 1.5)
                    target.slow_mult = sl
        elif atk and atk.get("slow") and target.is_unit:  # lassító fegyver (jégdárda, fagyos balta, mész)
            active_slow = target.slow_mult if target.slow_until > self.time else 1.0
            target.slow_mult = min(active_slow, atk["slow"])
            target.slow_until = self.time + atk.get("slow_dur", 1.5)
        if atk:
            if not atk.get("spell"):  # Ö24 egységtípusok: ki mi ellen erős (az alapsebzésre; a régi bónusz utána jön)
                dmg *= self.type_mult(src, target)
            for tag, v in atk.get("bonus", {}).items():
                if tag in target.tags:
                    dmg += v
        if src is not None and getattr(src, "is_unit", False):
            mo = src.owner.rules.get("moon")
            if mo and self.time % mo["period"] < mo["dur"]:  # Holdtölte
                dmg *= mo["mult"]
            pdm = src.owner.rules.get("pond_dmg")
            if pdm:  # Tavak útja
                tx, ty = int(src.x // TILE), int(src.y // TILE)
                if self.map.inb(tx, ty) and self.pond[ty * self.map.w + tx] == src.owner.id + 1:
                    dmg *= pdm
            lr = src.owner.rules.get("low_hp_rage")
            if lr and src.hp < src.max_hp * lr.get("below", 0.4):  # Medve-ösztön
                dmg *= lr.get("mult", 1.2)
        if target.is_unit and ranged:
            dmg *= target.mod(self, "ranged_taken_mult")
        ward = getattr(target, "ward", None)
        if ward and ward[1] > self.time and ward[0] > 0:  # Ooni Védőfény: ideiglenes pajzs
            a = min(ward[0], dmg)
            target.ward = (ward[0] - a, ward[1])
            dmg -= a
        if target.shield > 0:
            a = min(target.shield, dmg)
            target.shield -= a
            dmg -= a
        if atk and not magic and src is not None:
            if dmg <= 0:
                impact = "shield"
            elif "mechanical" in target.tags:
                impact = "metal"
            elif target.is_building:
                faction = target.owner.faction["id"]
                impact = {"ursan": "wood", "wullof": "wood", "ooni": "wood",
                          "imperium": "metal", "croftian": "organic",
                          "mycel": "organic", "hukucha": "shell"}.get(faction, "stone")
            else:
                impact = "organic"
            self.sfx("impact_" + impact, target)
        if dmg > 0:
            if not magic:
                arm = target.armor
                if getattr(target, "sunder_until", 0.0) > self.time:   # páncéltörés
                    arm = max(0, arm - getattr(target, "sunder_armor", 2))
                dmg = max(0.5, dmg - arm)
            target.hp -= dmg
        target.last_hit = self.time
        target.flash = 0.12
        mo = target.owner.faction.get("morale") if target.is_unit else None
        if mo and dmg > 0 and getattr(target, "morale", None) is not None:  # Harci kedv fogy a sérüléstől
            target.morale = max(0.0, target.morale - dmg / max(1, target.max_hp) * mo.get("hurt", 60)
                                * target.owner.rules.get("morale_hurt_mult", 1.0))
            if target.morale <= 0:
                target.broken = True
        if target.hp <= 0:
            self.kill(target, src)
            return
        if (target.is_unit and target.order is None and target.attack and not target.is_worker
                and src is not None and src.alive and target.can_hit(src)):
            target.order = {"type": "attack", "target": src, "auto": True, "home": (target.x, target.y)}
        if not target.owner.is_ai:
            pid = target.owner.id
            last = next((a for a in reversed(self.alerts) if len(a) < 4 or a[3] == pid), None)
            if last is None or self.time - last[2] > 12:
                self.alerts.append((target.x, target.y, self.time, pid))
                self.notify(target.owner, "Támadás ér!", (255, 90, 70))

    def kill(self, e, src, corpse=True):
        if not e.alive:
            return
        if e.is_unit:
            e.refund_build_orders()
        e.alive = False
        self.sfx("death", e)
        mo = e.owner.faction.get("morale") if e.is_unit else None
        if mo and not e.defn.get("summoned"):  # Harci kedv: a közelben elesett társ vagy hős megrendíti a többieket
            hero = e.hero
            r = (8 if hero else 5) * TILE
            for u in self.query(e.x, e.y, r + 32):
                if u.is_unit and u.alive and u.owner is e.owner and getattr(u, "morale", None) is not None \
                        and dmath.hypot(u.x - e.x, u.y - e.y) <= r:
                    u.morale = max(0.0, u.morale - mo.get("hero_death" if hero else "ally_death", 10))
                    if u.morale <= 0:
                        u.broken = True
        heroes.grant_kill_xp(self, e, src)
        if src is not None and src.is_unit and src.alive and src.owner.hostile(e.owner) and e.is_unit:
            if src.owner.faction.get("spell_resource") == "spores":
                src.spores = min(6, src.spores + 1)
            if src.passive(self, "split_kills"):
                src.meals += 1
                self.try_split(src)
        anchor_id = e.owner.faction.get("soul_anchor")
        if anchor_id and e.is_unit and e.lifetime is None and "lelekhorgony" in e.abilities \
                and anchor_id in self.data.defs and not e.hero:
            a = Unit(self, e.owner, self.data.defs[anchor_id], e.x, e.y)
            a.origin = e.defn["id"]
            a.max_hp = max(50, self.data.defs[e.defn["id"]].get("hp", 100))
            a.hp = a.max_hp * 0.15
            self.units.append(a)
        if e.hero:  # elesett hős: az állapota megmarad az újraélesztéshez
            e.owner.hero_states[e.defn["id"]] = heroes.snapshot(e)
            self.notify(e.owner, f"Elesett: {e.name} – a hősidéző épületben újraéleszthető.", (255, 120, 90))
        if src is not None and src.is_unit and e.is_unit and src.owner.hostile(e.owner) and not e.hero:
            sp = src.owner
            hk = self.rule_tags(sp, "heal_on_kill", src)
            if hk and src.alive:
                src.hp = min(src.max_hp, src.hp + hk["amount"])
                self.add_fx("heal", src.x, src.y, 0.4, (150, 255, 150))
            rk = sp.rules.get("raise_from_kills")
            if rk and "organic" in e.tags and e.lifetime is None and random.random() < rk["chance"]:
                ru = Unit(self, sp, self.data.defs[rk["unit"]], e.x, e.y)
                ru.lifetime = summon_duration(sp, rk.get("lifetime"))
                self.units.append(ru)
                self.add_fx("ring", e.x, e.y, 0.8, tuple(rk.get("color", (160, 255, 200))))
            ck = sp.rules.get("convert_kills")
            if (ck and e.lifetime is None and "large" not in e.tags and not e.is_worker and "livestock" not in e.tags
                    and not e.defn.get("unit_supply") and not getattr(e.owner, "neutral", False)
                    and sp.supply_used + e.defn.get("supply", 1) <= sp.supply_cap
                    and random.random() < ck["chance"]):  # átállítás: jószág / létszámhordozó nem, és a korlát alatt
                cu = Unit(self, sp, e.defn, e.x, e.y)
                cu.hp = cu.max_hp * ck.get("hp", 0.35)
                self.units.append(cu)
                self.add_fx("ring", e.x, e.y, 0.8, (220, 140, 255))
        val = sum(e.defn.get("cost", {}).values())
        if src is not None:
            kh = src.owner.rules.get("kill_heal")
            if kh and getattr(src, "is_unit", False) and src.alive and src.defn["id"] in kh.get("ids", ()):
                src.hp = min(src.max_hp, src.hp + kh.get("hp", 20))  # Oni vérszomj
            src.owner.kills += 1
            st = getattr(src.owner, "stats", None)
            if st is not None and src.owner.hostile(e.owner):
                st["buildings_killed" if e.is_building else "units_killed"] += 1
                st["killed_value"] += val
            bounty = src.owner.faction.get("bounty")
            if bounty and src.owner.hostile(e.owner):  # wullof vadászzsákmány
                gain = sum(e.defn.get("cost", {}).values()) / e.defn.get("count", 1) * bounty * 0.5
                if gain:
                    k = src.owner.faction.get("bounty_res", "wood")
                    src.owner.res[k] = src.owner.res.get(k, 0) + gain
                    bg = src.owner.rules.get("bounty_gold")
                    if bg:  # Zsákmányszerzés: aranyat is hoz (a Hadtáp ezt is csökkenti)
                        self.income(src.owner, "stone", gain * bg)
        inf = src.owner.faction.get("infest") if src is not None else None
        if (inf and e.is_unit and src.owner.hostile(e.owner) and "organic" in e.tags and not e.hero
                and e.lifetime is None and not getattr(e.owner, "neutral", False) and self.on_creep_of(src.owner, e)
                and random.random() < inf.get("chance", 0.3)):
            self.hatch.append([self.time + inf.get("delay", 6), src.owner, inf["unit"], e.x, e.y])
            self.add_fx("ring", e.x, e.y, 0.8, (150, 255, 120))
        if e.is_building and e.garrison:
            self.unload(e)
        if e.is_building and getattr(e, "builder_in", None) is not None:
            self.leave_construction(e, hurt=True)
        burst = e.defn.get("death_burst")
        if burst:
            for t in self.enemies_in_radius(e.owner, e.x, e.y, burst["radius"] * TILE, units_only=True):
                self.apply_damage(e, t, burst["damage"], None, ranged=False, magic=True)
            self.add_fx("death", e.x, e.y, 0.9, (150, 220, 120))
        e.owner.lost += 1
        st = getattr(e.owner, "stats", None)
        if st is not None:
            st["buildings_lost" if e.is_building else "units_lost"] += 1
            st["lost_value"] += val
        big = e.is_building or "large" in e.tags
        self.add_fx("death", e.x, e.y, 0.9 if big else 0.5, e.owner.color)
        if e.is_unit and corpse and not e.defn.get("no_corpse") and len(self.corpses) < 200:
            self.corpses.append([e.defn, e.owner, e.x, e.y, e.angle, self.time])
        if e.is_building:
            for item in e.queue:
                if item["kind"] == "research":
                    e.owner.researching.discard(item["id"])
            r = getattr(e, "res", None)
            if r is not None:
                r.mine = None
                if r.alive:
                    self.map.set_block(r.tx, r.ty, r.w, r.h, B_RESOURCE)
                if getattr(e, "miners", None):
                    self.release_miners(e, hurt=True)
            else:
                self.map.set_block(e.tx, e.ty, e.w, e.h, FREE)
            if getattr(e, "gate_q", None):
                self.release_gate(e)
            pt = getattr(e, "partner", None)
            if pt is not None:
                pt.partner = None
            self.notify(e.owner, f"Elpusztult: {e.name}", (255, 120, 90))
            self._update_power()
            self._update_creep(grow=False)
            self._update_network()

    # ---- rejtőzés és felfedés -------------------------------------------------
    def _update_stealth(self):
        """Beásott (croftian) és erdőben lapuló (wullof) egységek rejtettek; a felfedők és a nagyon közeli
        ellenségek látják őket."""
        m = self.map
        hidden = []
        for u in self.units:
            h = False
            if u.burrowed:
                h = True
            elif u.defn.get("always_hidden"):
                h = True
            elif u.defn.get("mine_stealth") and self.time - u.last_attack > 3 and not u.flying:
                rr = u.defn["mine_stealth"]
                tx, ty = int(u.x // TILE), int(u.y // TILE)
                for dy in range(-rr, rr + 1):
                    for dx in range(-rr, rr + 1):
                        if m.inb(tx + dx, ty + dy):
                            r = m.res_grid[(ty + dy) * m.w + tx + dx]
                            if r is not None and r.kind in ("stone", "meteor"):
                                h = True
            elif ((u.owner.faction.get("forest_stealth") and u.has_ability("erdei_rejtozes"))
                  or self.rule_tags(u.owner, "forest_stealth", u)) and \
                    self.time - u.last_attack > 3 and not u.flying and not u.moving:
                tx, ty = int(u.x // TILE), int(u.y // TILE)
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        if m.inb(tx + dx, ty + dy):
                            r = m.res_grid[(ty + dy) * m.w + tx + dx]
                            if r is not None and r.kind == "wood":
                                h = True
            u.hidden = h
            if h:
                hidden.append(u)
        self.detected = {p.id: set() for p in self.players}
        if not hidden:
            return
        for p in self.players:
            dets = [(u.x, u.y, (u.passive(self, "detect") or 0) * TILE) for u in self.units
                    if p.allied(u.owner) and u.passive(self, "detect")]
            dets += [(b.x, b.y, b.defn["detect"] * TILE) for b in self.buildings
                     if p.allied(b.owner) and b.complete and b.defn.get("detect")]
            for q in self.players:
                da = q.rules.get("detect_all") if p.allied(q) else None
                if da:
                    dets += [(u.x, u.y, da * TILE) for u in self.units if u.owner is q and u.alive]
            for h in hidden:
                if not p.hostile(h.owner):
                    continue
                if any(dmath.hypot(h.x - x, h.y - y) <= r for x, y, r in dets) or any(
                        p.allied(o.owner) and dmath.hypot(o.x - h.x, o.y - h.y) <= 2.2 * TILE
                        for o in self.query(h.x, h.y, 2.4 * TILE)):
                    self.detected[p.id].add(h.id)

    def seen_by(self, p, e):
        """Rejtett ellenséges egység: csak felfedve célozható/látható."""
        return not is_sheltered(e) and (
            not getattr(e, "hidden", False) or e.id in getattr(self, "detected", {}).get(p.id, ()))

    # ---- micélia: spórák és osztódás -------------------------------------------
    def _update_spores(self):
        if self.tick % 300:  # 10 mp-enként +1 spóra a hálózat közelében
            return
        for u in self.units:
            if u.owner.faction.get("spell_resource") != "spores" or u.spores >= 6:
                continue
            if any(b.owner is u.owner and b.connected and dmath.hypot(b.x - u.x, b.y - u.y) < 8 * TILE
                   for b in self.buildings):
                u.spores += 1
        for u in self.units:
            if u.meals and u.passive(self, "split_kills") and u.meals >= u.passive(self, "split_kills"):
                self.try_split(u)

    def hero_summon_count(self, owner, hero_id):
        return sum(u.alive and u.owner is owner and u.summon_hero == hero_id for u in self.units)

    def try_split(self, u):
        """Osztódás: ha elég ellenséget „evett”, kettéválik (ha van rá létszám)."""
        n = u.passive(self, "split_kills")
        if not n or u.meals < n or not u.alive:
            return False
        p = u.owner
        if u.summon_hero is not None:
            if self.hero_summon_count(p, u.summon_hero) >= 4:
                return False
        elif p.supply_used + u.supply() > p.supply_cap + 1e-6:
            return False  # majd ha lesz hely
        u.meals = 0
        a = random.uniform(0, 6.283)
        twin = Unit(self, p, u.defn, u.x + dmath.cos(a) * u.radius * 2, u.y + dmath.sin(a) * u.radius * 2)
        twin.summon_hero = u.summon_hero
        twin.hp = u.hp = u.max_hp * 0.5
        twin.spores = u.spores // 2
        self.units.append(twin)
        p.supply_used += u.supply()
        self.add_fx("death", u.x, u.y, 0.5, (170, 230, 120))
        self.notify(p, f"Osztódott: {u.name}", (170, 230, 120))
        return True

    @staticmethod
    def anchor_order(u, anchor=None):
        order = u.order
        return bool(order and order["type"] == "anchor_rescue"
                    and (anchor is None or order["anchor"] is anchor))

    def anchor_available(self, p, anchor):
        return bool(isinstance(anchor, Unit) and anchor.alive and anchor.owner is p
                    and anchor.defn["id"] == p.faction.get("soul_anchor")
                    and anchor.origin is not None and anchor.inside is None
                    and not any(u.alive and self.anchor_order(u, anchor) for u in self.units))

    def anchor_drop_allowed(self, anchor, x, y, ignore=None):
        return self.unit_placement_allowed(anchor.radius, x, y, ignore, anchor)

    def unit_placement_allowed(self, radius, x, y, ignore=None, replacing=None):
        if not (type(x) in (int, float) and type(y) in (int, float)
                and 0 <= x <= self.map.w * TILE and 0 <= y <= self.map.h * TILE):
            return False
        return fits(self.map, x, y, radius) and all(
            dmath.sq(v.x - x) + dmath.sq(v.y - y) >= dmath.sq(v.radius + radius)
            for v in self.units if v is not replacing and v is not ignore and v.alive
            and v.inside is None and not v.flying and not v.burrowed)

    def cmd_anchor_rescue(self, units, anchor, x, y):
        candidates = [u for u in units if isinstance(u, Unit) and u.alive and u.inside is None
                      and not u.burrowed and u.morph is None and u.speed_base() > 0
                      and u.has_ability("lelekhorgony_mentes") and not self.anchor_order(u)]
        if not candidates:
            return False
        p = candidates[0].owner
        if not self.anchor_available(p, anchor):
            self.notify(p, "Válassz egy szabad saját Lélekhorgonyt.")
            return False
        u = min((u for u in candidates if u.owner is p),
                key=lambda u: (dmath.sq(u.x - anchor.x) + dmath.sq(u.y - anchor.y), u.id))
        if not self.anchor_drop_allowed(anchor, x, y, ignore=u):
            self.notify(p, "A Lélekhorgony nem rakható le ide.")
            return False
        u.set_order({"type": "anchor_rescue", "anchor": anchor, "x": x, "y": y})
        return True

    def release_anchor(self, anchor, carrier):
        spot = self.landing_position(carrier.x, carrier.y, anchor.radius, ignore=anchor)
        if spot is not None:
            anchor.x, anchor.y = spot
            anchor.inside = None

    def rebody(self, anchor):
        """Lélekhorgony: a teljesen újraépített horgonyból újra az eredeti gép lesz."""
        anchor.alive = False
        u = Unit(self, anchor.owner, self.data.defs[anchor.origin], anchor.x, anchor.y)
        self.units.append(u)
        self.add_fx("heal", u.x, u.y, 0.8, (120, 255, 190))
        self.notify(u.owner, f"Újratestesült: {u.name}", (120, 255, 190))

    def on_creep_of(self, p, ent):
        tx, ty = int(ent.x // TILE), int(ent.y // TILE)
        return self.map.inb(tx, ty) and p.creep[ty * self.map.w + tx] == 1

    # ---- építő-stílusok (Ötletdoboz Ö16) ------------------------------------
    def _setup_map_scenery(self):
        """Csatapálya díszletei (tereptárgy) és ártalmatlan háttérszereplői (data/maps: decor, ambient)."""
        from .mission import Actor, Decor
        m = self.map
        self.decor = [Decor(s) for s in getattr(m, "layout_decor", [])]
        self.actors = []
        k = 0
        for sp in getattr(m, "layout_ambient", []):
            cx, cy = sp["xy"]
            for i in range(sp.get("n", 1)):
                a = i * 2.39996
                r = 0 if i == 0 else 16 + 9 * math.sqrt(i)
                x, y = cx + dmath.cos(a) * r, cy + dmath.sin(a) * r
                pth = [(px + dmath.cos(a) * r * 0.6, py + dmath.sin(a) * r * 0.6) for px, py in sp.get("path", [])]
                self.actors.append(Actor(self, sp["id"], x, y, pth, sp.get("shelter"), idle=sp.get("mode") == "idle",
                                         seed=k * 31 + 7))
                k += 1

    def builder_style(self, p):
        """stay | stay_single | enter | consume | leave – ki és hogyan épít a játékos népénél."""
        st = p.faction.get("builder")
        if st:
            return st
        return {"creep": "consume", "standard": "stay"}.get(p.faction.get("mechanic", "standard"), "leave")

    def build_rate(self, b):
        """Az épülő épület haladási sebessége (1 = alap)."""
        if getattr(b, "self_build", False):
            return 1.0
        f = b.owner.faction
        st = self.builder_style(b.owner)
        n = b.builders
        if st == "stay":
            rate = 0 if n == 0 else 1 + 0.5 * (min(n, 4) - 1)
        elif st == "stay_single":
            rate = 1.0 if n else 0.0
        elif st == "enter":
            builder = getattr(b, "builder_in", None)
            rate = 1.0 if builder is not None and builder.alive else 0.0
        else:
            rate = 1.0
        if rate and f.get("pond_build_bonus"):  # Huk’ucha: a körülötte lévő tó gyorsít
            rate *= 1 + f["pond_build_bonus"] * self.pond_ring(b)
        bb = f.get("build_boost")
        if rate and bb:  # Ooni: a közeli Taiko-dobos ritmusa gyorsít
            r = bb.get("radius", 4) * TILE
            if any(u.alive and u.owner is b.owner and u.defn["id"] == bb["unit"] and u.inside is None
                   and rect_dist(u.x, u.y, b.rect) <= r for u in self.units):
                rate *= b.owner.rules.get("build_boost_mult", bb.get("mult", 1.5))
        return rate

    def pond_ring(self, b):
        """A saját tavacska aránya az épület körüli egy csempés gyűrűben (0..1)."""
        m = self.map
        mark = b.owner.id + 1
        tx, ty = int(b.rect[0] // TILE), int(b.rect[1] // TILE)
        w, h = int(b.rect[2] // TILE), int(b.rect[3] // TILE)
        tot = hit = 0
        for y in range(ty - 1, ty + h + 1):
            for x in range(tx - 1, tx + w + 1):
                if tx <= x < tx + w and ty <= y < ty + h:
                    continue
                if not m.inb(x, y):
                    continue
                tot += 1
                hit += self.pond[y * m.w + x] == mark
        return hit / tot if tot else 0.0

    # ---- Ötletdoboz Ö4/Ö5: bányára épülő épületek ----------------------------
    def mine_rate(self, kind):
        """Egy hordó munkás átlagos hozama mp-enként (ehhez mérjük a bányaépületeket)."""
        rd = self.data.game["resources"][kind]
        return rd["carry"] / (rd["gather_time"] + 7.4)

    def can_mine(self, u, b):
        """A tárnába bármely munkás beolvadhat; a Napfúróba az, aki az adott nyersanyagot gyűjti."""
        if b.defn["mine"].get("consume"):
            return u.is_worker
        return u.can_gather(b.res.kind)

    def enter_mine(self, u, b):
        mn = b.defn.get("mine") or {}
        if not (b.alive and b.complete and u.alive) or getattr(b, "res", None) is None:
            u.set_order(None)
            return False
        if mn.get("consume"):  # Fertőzött tárna: a szolga végleg beleolvad
            if b.mine_n >= mn.get("slots", 5):
                self.notify(u.owner, f"{b.name}: tele van.", (255, 160, 90))
                u.set_order(None)
                return False
            b.mine_n += 1
            self.add_fx("ring", b.x, b.y, 0.5, (150, 255, 120))
            self.consume_unit(u)
            return True
        if len(b.miners) >= mn.get("slots", 3):
            self.notify(u.owner, f"{b.name}: már {mn.get('slots', 3)} munkás dolgozik benne.", (255, 160, 90))
            u.set_order(None)
            return False
        u.finish()
        u.queue = []
        u.inside = b
        u.path = []
        b.miners.append(u)
        self.add_fx("ring", b.x, b.y, 0.4, (255, 220, 120))
        return True

    def release_miners(self, b, hurt=False):
        for k, u in enumerate(b.miners):
            a = k * 1.3
            spot = self.map.nearest_free(int((b.x + dmath.cos(a) * b.rect[2]) // TILE),
                                         int((b.y + dmath.sin(a) * b.rect[3]) // TILE))
            if spot:
                u.x, u.y = (spot[0] + 0.5) * TILE, (spot[1] + 0.5) * TILE
            u.inside = None
            if hurt:
                u.hp = max(1, u.hp * 0.5)
            u.set_order(None)
        b.miners = []

    def update_mine(self, b, dt):
        r = b.res
        mn = b.defn["mine"]
        if r is None or not r.alive:
            self.mine_exhausted(b)
            return
        n = len(b.miners) + b.mine_n
        b.mine_acc += (mn.get("passive", 0) + n) * self.mine_rate(r.kind) * dt * self.boost_mult(b, "mine_boost")
        if b.mine_acc >= 1:
            amt = min(int(b.mine_acc), r.amount)
            b.mine_acc -= int(b.mine_acc)
            r.amount -= amt
            self.income(b.owner, r.kind, amt)
            if r.amount <= 0:
                self.map.remove_resource(r)
                self.mine_exhausted(b)

    # ---- Ötletdoboz Ö17: Áhítat (Szürke elfek) és Tóének (Huk’ucha) ------------------------------
    def faith_slots(self, b):
        return b.defn.get("pray_slots", 0) if b.alive and b.complete else 0

    def update_faith(self, dt):
        """A szentélyeknél fohászkodó munkások termelik az Áhítatot / Tóéneket (fejenként 0,3/mp, összesen
        legfeljebb 8, szentélyenként a helyek számáig). Közben az Áradás is itt fut."""
        by_p = {}
        for u in self.units:
            o = u.order
            if (u.alive and u.inside is None and o is not None and o.get("type") == "pray" and o.get("at")
                    and u.stun_until <= self.time and getattr(u, "asleep_until", 0.0) <= self.time
                    and getattr(u, "feared_until", 0.0) <= self.time and not u.burrowed):
                by_p.setdefault(u.owner.id, []).append(u)
            elif o is not None and o.get("type") == "pray":
                o["active"] = False
        for p in self.players:
            fa = p.faction.get("faith")
            if not fa or p.defeated:
                continue
            ws = sorted(by_p.get(p.id, []), key=lambda u: u.id)
            used = {}
            n = 0
            for u in ws:
                b = u.order["b"]
                if n >= fa.get("max", 8) or used.get(b.id, 0) >= self.faith_slots(b):
                    u.order["active"] = False
                    continue
                used[b.id] = used.get(b.id, 0) + 1
                u.order["active"] = True
                n += 1
                if (self.tick // 15 + u.id) % 6 == 0:
                    self.add_fx("heal", u.x, u.y - 10, 0.5, (255, 220, 130) if p.faction["id"] == "elves"
                                else (140, 220, 255))
            p.praying = n
            if n:
                rate = p.rules.get("faith_rate", fa.get("rate", 0.3))
                p.res["faith"] = p.res.get("faith", 0) + n * rate * dt
                p.gathered["faith"] = p.gathered.get("faith", 0) + n * rate * dt
            self._update_flood(p, dt)

    def boost_mult(self, b, effect):
        bo = getattr(b, "boost", None)
        if not bo or effect not in bo:
            return 1.0
        until, mult = bo[effect]
        return mult if until > self.time else 1.0

    def cmd_bpower(self, p, b, ab_id):
        """Épület-képesség (Napáldás, Áradás, Gyöngyérés): Áhítatból / Tóénekből fizetve."""
        ab = self.data.abilities.get(ab_id)
        if ab is None or ab.get("type") != "bpower" or b.owner is not p or not b.alive or not b.complete \
                or ab_id not in b.defn.get("powers", ()):
            return False
        if not hasattr(b, "power_ready"):
            b.power_ready = {}
        if b.power_ready.get(ab_id, 0) > self.time:
            self.notify(p, f"{ab['name']}: még töltődik.", (255, 160, 90))
            return False
        cost = {"faith": ab["faith"]} if ab.get("faith") else {}
        if not p.can_afford(cost):
            self._notify_missing(p, cost)
            return False
        if ab.get("effect") == "mine_boost" and (b.res is None or not b.res.alive):
            self.notify(p, f"{ab['name']}: nincs mit gyorsítani.", (255, 160, 90))
            return False
        p.pay(cost)
        b.power_ready[ab_id] = self.time + ab.get("cooldown", 60)
        until = self.time + ab.get("duration", 30)
        if ab["effect"] == "flood":
            self._start_flood(p, until, ab.get("heal", 3))
        else:
            if not hasattr(b, "boost") or b.boost is None:
                b.boost = {}
            b.boost[ab["effect"]] = (until, ab.get("mult", 1.5))
        self.add_fx("nova", b.x, b.y, 0.7, (255, 220, 130), b.radius * 1.6)
        self.notify(p, f"{ab['name']} – {b.name}", (255, 220, 140))
        return True

    def gpower_cost(self, ab):
        return {k: ab[k] for k in ("faith", "meteor", "stone", "wood") if ab.get(k)}

    def cmd_gpower(self, p, b, ab_id, x=0.0, y=0.0):
        """Ö19 Égi kegy: isteni csapás a főépület gombjáról (célpontos: a térkép látható pontjára)."""
        ab = self.data.abilities.get(ab_id)
        if ab is None or ab.get("type") != "gpower" or b.owner is not p or not b.alive or not b.complete \
                or ab_id not in b.defn.get("powers", ()):
            return False
        if not hasattr(b, "power_ready"):
            b.power_ready = {}
        if b.power_ready.get(ab_id, 0) > self.time:
            self.notify(p, f"{ab['name']}: még töltődik.", (255, 160, 90))
            return False
        m = self.map
        if ab.get("target"):
            tx, ty = int(x // TILE), int(y // TILE)
            if not m.inb(tx, ty) or (not p.is_ai and p.visible[ty * m.w + tx] != 1):
                self.notify(p, f"{ab['name']}: csak látható pontra.", (255, 160, 90))
                return False
        cost = self.gpower_cost(ab)
        if not p.can_afford(cost):
            self._notify_missing(p, cost)
            return False
        if ab.get("hq_hp") and b.hp <= ab["hq_hp"] + 50:
            self.notify(p, f"{ab['name']}: a {b.name} túl sebesült.", (255, 160, 90))
            return False
        p.pay(cost)
        if ab.get("hq_hp"):
            b.hp -= ab["hq_hp"]
        b.power_ready[ab_id] = self.time + ab.get("cooldown", 180)
        eff = ab.get("effect")
        col = tuple(ab.get("color", (255, 220, 140)))
        if eff == "area":
            from .entities import AreaEffect
            self.effects.append(AreaEffect(self, b, ab, x, y))
            if ab.get("reveal"):
                p.reveals = [r for r in getattr(p, "reveals", []) if r[3] > self.time] + \
                    [(x, y, ab["reveal"] * TILE, self.time + 10)]
            self.add_fx("nova", x, y, 0.8, col, ab.get("radius", 2) * TILE)
        elif eff == "summon":
            udef = self.data.defs[ab["unit"]]
            from .entities import Unit
            for k in range(ab.get("count", 1)):
                a = k * 2.1
                u = Unit(self, p, udef, x + dmath.cos(a) * 20, y + dmath.sin(a) * 20)
                duration = ab.get("lifetime", 40) * p.rules.get("summon_life_mult", 1.0)
                u.lifetime = summon_duration(p, duration)
                self.units.append(u)
                self.add_fx("heal", u.x, u.y, 0.6, (200, 220, 255))
        elif eff == "levy":
            buff = self.data.abilities[ab["buff"]]
            for u in self.units:
                if u.alive and u.owner is p and u.is_worker and u.inside is None:
                    u.buffs[ab["buff"]] = self.time + ab.get("duration", 30)
                    if buff.get("drop_work") and u.order and u.order["type"] in ("gather", "construct", "repair"):
                        u.after_buff = (ab["buff"], dict(u.order))
                        u.set_order(None)
                    self.add_fx("ring", u.x, u.y, 0.5, (255, 220, 160))
        elif eff == "wind":
            r = ab.get("radius", 4) * TILE
            for u in self.units:
                if not u.alive or dmath.hypot(u.x - x, u.y - y) > r:
                    continue
                if u.owner is p or p.allied(u.owner):
                    u.buffs[ab["buff"]] = self.time + ab.get("duration", 8)
                elif p.hostile(u.owner):
                    u.slow_until, u.slow_mult = self.time + ab.get("duration", 8), ab.get("slow", 0.5)
            self.add_fx("nova", x, y, 0.9, col, r)
        self.notify(p, f"{ab['name']}!", (255, 220, 140))
        return True

    def _start_flood(self, p, until, heal):
        """Áradás: minden saját tavacska egy csempével kitágul (az Áradás végén visszahúzódik)."""
        fl = getattr(p, "flood", None)
        if fl and fl["until"] > self.time:
            fl["until"] = until
            return
        m = self.map
        mark = p.id + 1
        new = set()
        for y in range(m.h):
            for x in range(m.w):
                if self.pond[y * m.w + x] != mark:
                    continue
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = x + dx, y + dy
                    if m.inb(nx, ny) and not self.pond[ny * m.w + nx] and m.buildable_terrain(nx, ny):
                        new.add(ny * m.w + nx)
        tiles = sorted(new)
        for i in tiles:
            self.pond[i] = mark
        if tiles:
            m.version += 1
        p.flood = {"until": until, "tiles": tiles, "heal": heal}

    def _update_flood(self, p, dt):
        fl = getattr(p, "flood", None)
        if not fl:
            return
        m = self.map
        mark = p.id + 1
        if fl["until"] <= self.time:
            for i in fl["tiles"]:  # visszahúzódik – ahol közben épület áll, ott tó marad
                if self.pond[i] == mark and m.block[i] != B_BUILDING:
                    self.pond[i] = 0
            m.version += 1
            p.flood = None
            return
        for u in self.units:
            if u.alive and u.owner is p and u.hp < u.max_hp:
                tx, ty = int(u.x // TILE), int(u.y // TILE)
                if m.inb(tx, ty) and self.pond[ty * m.w + tx] == mark:
                    u.hp = min(u.max_hp, u.hp + fl["heal"] * dt)

    def mine_exhausted(self, b):
        if not b.alive:
            return
        self.notify(b.owner, f"{b.name}: a bánya kimerült.", (255, 200, 120))
        if b.miners:
            self.release_miners(b)
        self.kill(b, None)

    # ---- Ötletdoboz Ö11: a mélységen álló fák elrothadnak (Croftian fa) ------
    def _update_rot(self, dt):
        for p in self.players:
            cr = p.faction.get("creep_rot")
            if not cr or p.defeated:
                continue
            hqs = [b for b in self.buildings if b.owner is p and b.alive and b.defn["id"] == p.faction["hq"]]
            m = self.map
            acc = getattr(p, "rot_acc", 0.0)
            budget = p.rules.get("rot_cap", cr.get("cap", 6.5)) * dt
            gone = []
            for r in m.resources:
                if budget <= 0:
                    break
                if r.kind != "wood" or not p.creep[r.ty * m.w + r.tx]:
                    continue
                rate = cr.get("rate", 0.1) * p.rules.get("rot_mult", 1.0)
                if any(dmath.hypot(h.x - r.x, h.y - r.y) <= cr.get("hq_radius", 9) * TILE for h in hqs):
                    rate *= cr.get("hq_mult", 2.0)
                take = min(r.amount, rate * dt, budget)
                budget -= take
                r.amount -= take
                r.rot = True
                r.rot_t = self.time      # éppen rothad (a megjelenítéshez)
                r.rot_by = p.id
                acc += take
                if r.amount <= 0:
                    gone.append(r)
            for r in gone:
                m.remove_resource(r)
            if acc >= 1:
                n = int(acc)
                p.res["wood"] = p.res.get("wood", 0) + n
                p.gathered["wood"] = p.gathered.get("wood", 0) + n
                acc -= n
            p.rot_acc = acc

    # ---- Ötletdoboz Ö6: Ooni Torii-kapupár --------------------------------
    def torii_open(self, p, exclude=None):
        """A játékos párja nélküli Torii-kapuja (ehhez kerül a következő)."""
        for o in self.buildings:
            if o.owner is p and o.alive and o is not exclude and o.defn.get("torii") and getattr(o, "partner", None) is None:
                return o
        return None

    def enter_gate(self, u, b):
        pt = getattr(b, "partner", None)
        if pt is None or not pt.alive or not b.complete or not pt.complete:
            u.finish()
            return
        u.finish()
        u.inside = b
        u.path = []
        b.gate_q.append(u)
        if not b.gate_t:
            b.gate_t = max(self.time + 2.0, getattr(b, "gate_cd", 0))

    def update_gate(self, b):
        if not b.gate_t or self.time < b.gate_t:
            return
        pt = b.partner
        if pt is None or not pt.alive or not pt.complete:
            self.release_gate(b)
            return
        for k, u in enumerate(b.gate_q):
            a = k * 0.8
            spot = self.map.nearest_free(int((pt.x + dmath.cos(a) * pt.rect[2] * 0.8) // TILE),
                                         int((pt.y + pt.rect[3] * 0.7 + dmath.sin(a) * 12) // TILE))
            if spot:
                u.x, u.y = (spot[0] + 0.5) * TILE, (spot[1] + 0.5) * TILE
            u.inside = None
            u.path_goal = None
        self.add_fx("ring", pt.x, pt.y, 0.7, (255, 120, 90))
        self.sfx("gate_arrival", pt)
        b.gate_q = []
        b.gate_t = 0
        b.gate_cd = pt.gate_cd = self.time + 8.0

    def release_gate(self, b):
        for u in b.gate_q:
            u.inside = None
            u.x, u.y = b.x, b.y + b.rect[3] * 0.7
        b.gate_q = []
        b.gate_t = 0

    # ---- Ötletdoboz Ö9: Huk’ucha ikra-akna --------------------------------
    def placement_radius(self, ab):
        ids = (ab["via"], ab["into"]) if ab["type"] == "morph" else (ab["unit"],)
        return max(self.data.defs[uid].get("radius", 0.4) for uid in ids) * TILE

    def placement_caster(self, units, ab_id, x, y):
        ab = self.data.abilities.get(ab_id, {})
        if not ab.get("target_placement"):
            return None
        radius = self.placement_radius(ab)
        if not (type(x) in (int, float) and type(y) in (int, float)
                and 0 <= x <= self.map.w * TILE and 0 <= y <= self.map.h * TILE):
            return None
        return min((u for u in units if isinstance(u, Unit) and u.alive and u.inside is None
                    and not u.burrowed and u.morph is None and u.has_ability(ab_id)
                    and u.ab_ok(ab) and u.ab_ready.get(ab_id, 0) <= self.time
                    and self.unit_placement_allowed(radius, x, y, ignore=u)),
                   key=lambda u: (dmath.sq(u.x-x) + dmath.sq(u.y-y), u.id), default=None)

    def cmd_place_ability(self, units, ab_id, x, y):
        u = self.placement_caster(units, ab_id, x, y)
        if u is None:
            if units:
                self.notify(units[0].owner, "A képesség most nem használható ezen a helyen.", (255, 160, 90))
            return False
        u.set_order({"type": "place_ability", "ab": ab_id, "x": x, "y": y})
        return True

    def cmd_lay_mine(self, units, ab_id, position=None):
        ab = self.data.abilities[ab_id]
        for u in units:
            if not (u.is_unit and u.alive and u.has_ability(ab_id)) or u.ab_ready.get(ab_id, 0) > self.time:
                continue
            if not u.ab_ok(ab):
                self.notify(u.owner, "Nincs elég energia.", (255, 160, 90))
                continue
            have = sum(1 for e in self.units if e.alive and e.owner is u.owner and e.defn["id"] == ab["unit"])
            if have >= ab.get("max", 12):
                self.notify(u.owner, f"Egyszerre legfeljebb {ab.get('max', 12)} ikra lehet lerakva.", (255, 160, 90))
                continue
            u.ab_pay(ab)
            u.ab_ready[ab_id] = self.time + ab.get("cooldown", 3)
            x, y = position if position is not None else (u.x, u.y)
            e = Unit(self, u.owner, self.data.defs[ab["unit"]], x, y)
            self.units.append(e)
            self.add_fx("ring", x, y, 0.4, (150, 230, 200))
            return True
        return False

    def _update_mines(self):
        for e in self.units:
            mt = e.defn.get("mine_trigger")
            if not mt or not e.alive:
                continue
            foes = [o for o in self.enemies_in_radius(e.owner, e.x, e.y, mt.get("radius", 1.0) * TILE, units_only=True)
                    if o.is_unit and not o.flying]
            if not foes:
                continue
            tx, ty = int(e.x // TILE), int(e.y // TILE)
            on_pond = self.map.inb(tx, ty) and self.pond[ty * self.map.w + tx] == e.owner.id + 1
            n = mt.get("pond_count", 5) if on_pond else mt.get("count", 3)
            udef = self.data.defs[mt["spawn"]]
            for k in range(n):
                a = k * math.tau / n
                s = Unit(self, e.owner, udef, e.x + dmath.cos(a) * 14, e.y + dmath.sin(a) * 14)
                s.lifetime = summon_duration(e.owner, mt.get("lifetime", 10))
                self.units.append(s)
                s.order = {"type": "attack", "target": foes[0], "auto": True, "home": (s.x, s.y)}
            self.add_fx("death", e.x, e.y, 0.6, (150, 230, 200))
            self.kill(e, None, corpse=False)

    # ---- Égkő-fény: meteorit és Égkő közelében (mágia, pajzs, gyógyulás) ----------
    def update_rift(self):
        """Fél másodpercenként: ki áll meteorit vagy Égkő közelében (bárkié), és ki Lélekreaktor mellett."""
        cfg = self.data.game.get("rift", {})
        r = cfg.get("radius", 5) * TILE
        for u in self.units:
            u.rift = False
            u.near_reactor = False
        srcs = [(x.x, x.y) for x in self.map.resources if x.kind == "meteor" and x.alive]
        srcs += [(b.x, b.y) for b in self.buildings if b.alive and b.complete and b.defn.get("rift_source")]
        srcs += [(pt.x, pt.y) for pt in getattr(self, "points", ())]
        for sx, sy in srcs:
            for u in self.query(sx, sy, r + 32):
                if u.is_unit and u.alive and dmath.hypot(u.x - sx, u.y - sy) <= r:
                    u.rift = True
        for b in self.buildings:  # Reaktorpajzs: a reaktor 6 csempés körzete
            if b.alive and b.complete and b.defn.get("shield_reactor"):
                rr = 6 * TILE
                for u in self.query(b.x, b.y, rr + 32):
                    if u.is_unit and u.owner is b.owner and dmath.hypot(u.x - b.x, u.y - b.y) <= rr:
                        u.near_reactor = True
        mage_range = cfg.get("mage_range", 1)
        for u in self.units:
            if not u.rift or not u.alive:
                continue
            fr = u.owner.faction.get("rift", {})
            add = {}
            if "caster" in u.tags and u.attack and mage_range:  # a varázslók: +1 lőtáv
                add["range_add"] = mage_range
            rg = fr.get("regen")
            if rg and ((rg.get("tags") and set(rg["tags"]) & set(u.tags)) or u.defn["id"] in rg.get("units", ())):
                add["regen_add"] = rg.get("add", 1)
            if add:
                am = u.aura_mods = u.aura_mods or {}
                for k, v in add.items():
                    am[k] = max(am.get(k, 0.0), v)

    def update_morale(self, dt):
        """Harci kedv (Ooni): lassan töltődik, a Taiko-dobos közelében gyorsan; lámpásfényben nem esik a minimum alá;
        megtörés után a küszöbig gyenge marad."""
        for p in self.players:
            mo = p.faction.get("morale")
            if not mo:
                continue
            lights = [b for b in self.buildings if b.owner is p and b.alive and b.complete and b.defn.get("light_radius")]
            drum = mo.get("drum", {})
            drummers = [u for u in self.units if u.alive and u.owner is p and u.defn["id"] == drum.get("unit")]
            for u in self.units:
                if u.owner is not p or not u.alive or u.is_worker or not u.attack or u.defn.get("summoned"):
                    continue
                if getattr(u, "morale", None) is None:
                    u.morale = float(mo.get("max", 100))
                    u.broken = False
                rate = mo.get("regen", 2) if self.time - u.last_hit > 3 else 0
                if drum and any(dmath.hypot(d.x - u.x, d.y - u.y) <= drum.get("radius", 4) * TILE
                                for d in drummers if d is not u):
                    rate += drum.get("regen", 8) * p.rules.get("drum_mult", 1.0)
                u.morale = min(mo.get("max", 100), u.morale + rate * dt)
                if any(dmath.hypot(b.x - u.x, b.y - u.y) <= self.light_r(b) * TILE for b in lights):
                    u.morale = max(u.morale, mo.get("light_min", 30))
                if u.broken and u.morale >= mo.get("break_until", 50):
                    u.broken = False

    def cmd_ward(self, units, ab_id):
        """Ooni Védőfény: a Lámpáspap körül állók ideiglenes pajzsot kapnak."""
        ab = self.data.abilities[ab_id]
        done = False
        for u in units:
            if not (u.is_unit and u.alive and u.has_ability(ab_id)) or u.ab_ready.get(ab_id, 0) > self.time \
                    or not u.ab_ok(ab):
                continue
            u.ab_pay(ab)
            u.ab_ready[ab_id] = self.time + ab.get("cooldown", 18)
            rr = ab.get("radius", 5) * TILE
            for o in self.query(u.x, u.y, rr + 32):
                if o.is_unit and o.alive and o.owner is u.owner and dmath.hypot(o.x - u.x, o.y - u.y) <= rr:
                    o.ward = (ab.get("amount", 50), self.time + ab.get("duration", 10))
            self.add_fx("nova", u.x, u.y, 0.6, tuple(ab.get("color", (255, 230, 150))), rr)
            done = True
        return done

    def free_tap_tree(self, u, exclude=None):
        """A legközelebbi fa (8 csempén belül), amelyet más élő csapoló nem foglal."""
        m = self.map
        tx, ty = int(u.x // TILE), int(u.y // TILE)
        best, bd = None, 1e18
        for y in range(ty - 8, ty + 9):
            for x in range(tx - 8, tx + 9):
                if not m.inb(x, y):
                    continue
                r = m.res_grid[y * m.w + x]
                if r is None or r.kind != "wood" or r is exclude or not r.alive:
                    continue
                tp = getattr(r, "tapper", None)
                if tp is not None and tp is not u and tp.alive and tp.order and tp.order.get("res") is r:
                    continue
                dd = dmath.sq(r.x - u.x) + dmath.sq(r.y - u.y)
                if dd < bd:
                    best, bd = r, dd
        return best

    # ---- Ötletdoboz Ö1: Hadtáp (WC3-szerű fenntartás) -------------------------------
    def upkeep_factor(self, p):
        """Az aranybevétel szorzója a felhasznált létszám szerint (0–80: 100%, 81–140: 70%, 141–200: 40%)."""
        if not getattr(self, "upkeep_on", False):
            return 1.0
        used = p.supply_used
        for top, f in self.data.game.get("upkeep", {}).get("tiers", [[80, 1.0], [140, 0.7], [200, 0.4]]):
            if used <= top:
                return f
        return self.data.game.get("upkeep", {}).get("tiers", [[200, 0.4]])[-1][1]

    def income(self, p, kind, amt):
        """Bevétel könyvelése; az arany a Hadtáp szerint csökken (minden aranybevétel: hordás, bánya, zsákmány)."""
        if kind == "stone":
            amt *= self.upkeep_factor(p)
        p.res[kind] = p.res.get(kind, 0) + amt
        p.gathered[kind] = p.gathered.get(kind, 0) + amt
        return amt

    def rand(self):
        """A szimuláció véletlenszáma (a játék magjából, minden gépen azonos)."""
        return random.random()

    def found_item(self, u, drop, fi):
        """Tanuki-bányász: ritkán egy tárgyat hoz a mélyből, és a leadóhelynél hagyja."""
        pool = [i for i in fi.get("pool", []) if i in self.data.items]
        if not pool:
            return
        it = pool[int(self.rand() * len(pool)) % len(pool)]
        a = self.rand() * math.tau
        heroes.drop_item(self, it, drop.x + dmath.cos(a) * (drop.rect[2] * 0.6 + 12),
                         drop.y + dmath.sin(a) * (drop.rect[3] * 0.6 + 12))
        self.notify(u.owner, f"{u.name}: talált valamit a mélyben – {self.data.items[it]['name']}!", (255, 230, 140))

    def enter_construction(self, u, b):
        """Ursan: a munkás bemászik a félkész épületbe; bent védett, és csak készen jön ki."""
        b.builder_in = u
        u.inside = b
        u.path = []
        self.add_fx("ring", b.x, b.y, 0.4, (220, 200, 140))

    def leave_construction(self, b, hurt=False):
        u = b.builder_in
        b.builder_in = None
        if u is None or not u.alive:
            return
        spot = self.map.nearest_free(int(b.x // TILE), int((b.y + b.rect[3] * 0.6) // TILE))
        if spot:
            u.x, u.y = (spot[0] + 0.5) * TILE, (spot[1] + 0.5) * TILE
        u.inside = None
        if hurt:  # a lerombolt kunyhóból sérülten ugrik ki
            u.hp = max(1, u.hp * 0.5)
        o = u.order
        resume = o.get("resume") if o and o.get("type") == "construct" and not u.queue else None
        u._resume_build_queue(resume)

    def cmd_overdrive(self, units, ab_id):
        """Vasbirodalom – Túlhajtás: az épület, amelyen a Fabrikátor dolgozik, azonnal halad; a Fabrikátor sérül."""
        ab = self.data.abilities[ab_id]
        done = False
        for u in units:
            if not (u.is_unit and u.alive and u.has_ability(ab_id)) or u.ab_ready.get(ab_id, 0) > self.time:
                continue
            o = u.order
            b = o.get("b") if o and o.get("type") in ("construct", "repair") else None
            if b is None or not b.alive or gap(u, b) > REACH + TILE:
                self.notify(u.owner, "Túlhajtás: a Fabrikátor épp nem dolgozik épületen.", (255, 160, 90))
                continue
            share = ab.get("share", 0.5)
            if not b.complete:
                add = (b.build_time - b.progress) * share
                b.progress += add
                b.hp = min(b.max_hp, b.hp + b.max_hp * 0.9 * add / b.build_time)
            else:
                b.hp = min(b.max_hp, b.hp + (b.max_hp - b.hp) * share)
            u.ab_ready[ab_id] = self.time + ab.get("cooldown", 20)
            self.add_fx("ring", b.x, b.y, 0.6, tuple(ab.get("color", (255, 170, 80))))
            u.hp -= u.max_hp * ab.get("self_hp", 0.4)  # saját sérülés: páncél és pajzs nem fogja fel
            u.flash = 0.12
            if u.hp <= 0:
                self.kill(u, None)
            done = True
        return done

    # ---- ursan helyőrség ----------------------------------------------
    def enter_building(self, u, b):
        if (not u.alive or u.inside is not None or b.owner is not u.owner or not b.alive or not b.complete
                or len(b.garrison) >= b.defn.get("garrison", 0)):
            return False
        u.inside = b
        u.finish()
        b.garrison.append(u)
        self.add_fx("ring", b.x, b.y, 0.4, (200, 220, 255))
        return True

    def cmd_garrison(self, units, b=None, queue=False):
        """Beszállás: a megadott (vagy a legközelebbi szabad helyű) saját helyőrség-épületbe."""
        us = [u for u in units if u.is_unit and u.alive and u.inside is None and u.has_ability("beszallas")]
        if not us:
            return False
        p = us[0].owner
        cands = [x for x in ([b] if b is not None else self.buildings)
                 if x.is_building and x.owner is p and x.complete and x.alive and x.defn.get("garrison")]
        issued = False
        for u in us:
            if u.owner is not p:
                continue
            free = [x for x in cands if len(x.garrison) + sum(1 for o in us if o.order and o.order.get("b") is x
                                                               and o.order["type"] == "enter") < x.defn.get("garrison", 0)]
            if not free:
                self.notify(p, "Nincs szabad hely a helyőrségben.", (255, 160, 90))
                break
            t = min(free, key=lambda x: dmath.hypot(x.x - u.x, x.y - u.y))
            u.set_order({"type": "enter", "b": t}, queue)
            issued = True
        return issued

    def unload(self, b):
        if getattr(b, "miners", None):
            self.release_miners(b)
        for k, u in enumerate(b.garrison):
            a = k * 0.9
            spot = self.map.nearest_free(int((b.x + dmath.cos(a) * b.rect[2] * 0.7) // TILE),
                                         int((b.y + b.rect[3] * 0.6 + dmath.sin(a) * 10) // TILE))
            if spot:
                u.x, u.y = (spot[0] + 0.5) * TILE, (spot[1] + 0.5) * TILE
            u.inside = None
        b.garrison = []

    # ---- croftian mutáció ------------------------------------------------
    def cmd_morph(self, units, ab_id):
        ab = self.data.abilities[ab_id]
        into = self.data.defs[ab["into"]]
        morph_supply = into.get("supply", 1) / into.get("count", 1)
        done = 0
        for u in units:
            if not (u.is_unit and u.alive and u.has_ability(ab_id) and u.morph is None):
                continue
            p = u.owner
            if not p.can_afford(ab.get("cost", {})):
                self._notify_missing(p, ab.get("cost", {}))
                break
            reserved = morph_supply if u.summon_hero is None else 0
            if u.summon_hero is None and p.supply_used - u.supply() + reserved > p.supply_cap + 1e-6:
                self.notify(p, "A mutációhoz nincs elég létszám.", (255, 160, 90))
                break
            p.pay(ab.get("cost", {}))
            cocoon = Unit(self, p, self.data.defs[ab["via"]], u.x, u.y)
            cocoon.summon_hero = u.summon_hero
            cocoon.morph = (ab["into"], ab.get("time", 15))
            cocoon.morph_total = ab.get("time", 15)
            p.supply_used += reserved - u.supply()
            u.alive = False
            self.units.append(cocoon)
            self.add_fx("ring", u.x, u.y, 0.5, (150, 255, 120))
            done += 1
        return done > 0

    def finish_morph(self, cocoon, into):
        cocoon.alive = False
        u = Unit(self, cocoon.owner, self.data.defs[into], cocoon.x, cocoon.y)
        u.summon_hero = cocoon.summon_hero
        self.units.append(u)
        self.add_fx("death", u.x, u.y, 0.6, (150, 255, 120))
        self.notify(u.owner, f"Mutáció kész: {u.name}", (170, 255, 140))

    def cmd_nova(self, units, ab_id):
        for u in units:
            if u.is_unit and u.has_ability(ab_id) and u.cast_nova(self, ab_id):
                return True
        if units:
            self.notify(units[0].owner, "Nincs elég energia, vagy töltődik.", (255, 160, 90))
        return False

    def consume_unit(self, u):
        u.alive = False

    # ------------------------------------------------------------------
    # épületek, termelés
    def can_place(self, p, bdef, tx, ty, check_explored=True, check_req=True, ignore_layers=False):
        w, h = bdef["size"]
        m = self.map
        if bdef.get("on_point"):  # Ö2: őrállás – a saját Hasadékkövön
            pt = PTS.point_at(self, tx, ty)
            if pt is None:
                return False, "Hasadékkőre kell építeni"
            if pt.owner is not p:
                return False, "előbb foglald el a Hasadékkövet"
            if PTS.guard_alive(pt):
                return False, "ezen a kövön már áll őrállás"
            if m.block[ty * m.w + tx] != FREE:
                return False, "foglalt hely"
            miss = self.missing_requirements(p, bdef) if check_req else []
            if miss:
                return False, "hiányzik: " + ", ".join(miss)
            return True, ""
        if bdef.get("on_resource"):  # Ötletdoboz Ö4/Ö5: a bányára épül (Napfúró, Fertőzött tárna)
            r = m.res_grid[ty * m.w + tx] if m.inb(tx, ty) else None
            if r is None or r.kind not in bdef["on_resource"] or (r.tx, r.ty) != (tx, ty):
                return False, "aranybányára vagy meteoritra kell építeni"
            if not r.alive or w > r.w or h > r.h:
                return False, "a lelőhely kisebb az épületnél"
            mine = getattr(r, "mine", None)
            if mine is not None and mine.alive:
                return False, "erre a bányára már épült"
            if check_explored and not p.is_ai and not p.explored[ty * m.w + tx]:
                return False, "felderítetlen terület"
            miss = self.missing_requirements(p, bdef) if check_req else []
            if miss:
                return False, "hiányzik: " + ", ".join(miss)
            return True, ""
        for y in range(ty, ty + h):
            for x in range(tx, tx + w):
                if not m.inb(x, y):
                    return False, "a térképen kívül"
                if not m.buildable_terrain(x, y):
                    return False, "nem építhető terep"
                if m.block[y * m.w + x] != FREE:
                    return False, "foglalt hely"
                if check_explored and not p.is_ai and not p.explored[y * m.w + x]:
                    return False, "felderítetlen terület"
        miss = self.missing_requirements(p, bdef) if check_req else []
        if miss:
            return False, "hiányzik: " + ", ".join(miss)
        mech = p.faction.get("mechanic")
        if mech == "network" and not bdef.get("core"):
            cx, cy = (tx + w / 2) * TILE, (ty + h / 2) * TILE
            lr = bdef.get("link_radius", 0)
            if not any(dmath.hypot(o.x - cx, o.y - cy) <= max(lr, o.defn.get("link_radius", 0)) * TILE
                       for o in self.buildings if o.owner is p and o.complete and o.connected):
                return False, "nincs spórafonal-kapcsolat (építs Spórafonalat)"
        if mech == "power" and not bdef.get("no_power"):
            cx, cy = (tx + w / 2) * TILE, (ty + h / 2) * TILE
            if not any(dmath.hypot(o.x - cx, o.y - cy) <= self.power_r(o) * TILE
                       for o in self.buildings if o.owner is p and o.complete and o.defn.get("power_radius")):
                return False, "nincs energiamező (Napobeliszk kell)"
        if mech == "pond" and not bdef.get("no_pond") and not ignore_layers:
            mark = p.id + 1
            for y in range(ty, ty + h):
                for x in range(tx, tx + w):
                    if self.pond[y * m.w + x] != mark and getattr(self, "pond_plan", {}).get(y * m.w + x) != mark:
                        return False, "csak saját tavacskára építhető (áss előbb tavacskát)"
        if bdef.get("torii"):  # Ooni Torii-kapupár: a második fél bárhová, ahol a játékos lát
            open_gate = self.torii_open(p)
            if open_gate is not None:
                if not p.is_ai and not all(p.visible[y * m.w + x] for y in range(ty, ty + h) for x in range(tx, tx + w)):
                    return False, "a kapu másik felét csak oda teheted, ahol épp látsz"
                return True, ""
            pairs = sum(1 for o in self.buildings if o.owner is p and o.alive and o.defn.get("torii")) // 2
            if pairs >= p.rules.get("torii_pairs", 1):
                return False, "több kapupár nem építhető (fejlesztéssel kettő)"
        if mech == "lantern" and not bdef.get("no_light"):
            cx, cy = (tx + w / 2) * TILE, (ty + h / 2) * TILE
            if not any(dmath.hypot(o.x - cx, o.y - cy) <= self.light_r(o) * TILE
                       for o in self.buildings if o.owner is p and o.alive and o.defn.get("light_radius")
                       and (o.complete or o.defn.get("lantern"))):
                return False, "csak lámpásfényben építhető (rakj le papírlámpást)"
        if mech == "creep" and not bdef.get("no_creep"):
            for y in range(ty, ty + h):
                for x in range(tx, tx + w):
                    if not p.creep[y * m.w + x]:
                        return False, "csak mélységre építhető"
        return True, ""

    def place_building(self, p, bdef, tx, ty, complete=False):
        b = Building(self, p, bdef, tx, ty, complete)
        self.buildings.append(b)
        if bdef.get("on_resource"):  # a bánya marad alatta (a blokkolást a nyersanyag adja)
            r = self.map.res_grid[ty * self.map.w + tx]
            b.res = r
            r.mine = b
            return b
        if bdef.get("on_point"):
            pt = PTS.point_at(self, tx, ty)
            if pt is not None:
                pt.guard = b
        if bdef.get("torii"):
            other = self.torii_open(p, exclude=b)
            if other is not None:
                other.partner, b.partner = b, other
        self.map.set_block(tx, ty, b.w, b.h, B_BUILDING)
        for u in self.units:
            if u.alive and u.inside is None and not u.flying and rect_dist(u.x, u.y, b.rect) < u.radius:
                spot = self.landing_position(u.x, u.y, u.radius, ignore=u)
                if spot is not None:
                    u.x, u.y = spot
                    if u.path_goal is not None:
                        u.set_path(self, *u.path_goal)
        if complete:
            self._update_power()
        return b

    # ---- Vasbirodalom: alapozás (Dune 2000-szerű) ----------------------------
    def founded(self, p, tx, ty, w, h):
        """Az épület teljes alapterülete a játékos alapozásán áll-e."""
        m = self.map
        mark = p.id + 1
        return all(m.inb(x, y) and self.found[y * m.w + x] == mark for y in range(ty, ty + h) for x in range(tx, tx + w))

    def can_lay_foundation(self, p, tx, ty, check_explored=True, n=None):
        fd = p.faction.get("foundation")
        if not fd:
            return False, "ennek a népnek nincs alapozása"
        n = n or fd.get("size", 2)
        m = self.map
        new = 0
        for y in range(ty, ty + n):
            for x in range(tx, tx + n):
                if not m.inb(x, y):
                    return False, "a térképen kívül"
                if not m.buildable_terrain(x, y):
                    return False, "nem építhető terep"
                if m.block[y * m.w + x] != FREE:
                    return False, "foglalt hely"
                if check_explored and not p.is_ai and not p.explored[y * m.w + x]:
                    return False, "felderítetlen terület"
                if self.found[y * m.w + x] != p.id + 1:
                    new += 1
        if not new:
            return False, "itt már van alapozás"
        return True, ""

    def cmd_lay_foundation(self, p, tx, ty):
        """Márvány-lapkő (2×2) lerakása: azonnal kész, nem akadályozza a mozgást; ami teljesen rajta áll, teljes
        életerővel épül fel."""
        fd = p.faction.get("foundation")
        ok, why = self.can_lay_foundation(p, tx, ty)
        if not ok:
            if fd:
                self.notify(p, f"Nem lehet alapozni: {why}", (255, 120, 90))
            return False
        cost = fd.get("cost", {})
        if not p.can_afford(cost):
            self._notify_missing(p, cost)
            return False
        p.pay(cost)
        n = fd.get("size", 2)
        m = self.map
        for y in range(ty, ty + n):
            for x in range(tx, tx + n):
                self.found[y * m.w + x] = p.id + 1
        self.map.version += 1
        return True

    def cmd_found(self, p, tx, ty, worker, bid="imp_alapozas", queue=False):
        """Ö3: a Fabrikátor odamegy és lerakja a lapkövet (2×2: 3 mp, Nagy alapozás 4×4: 6 mp). Az ára a parancskor
        fizetendő; ha a hely közben megtelik, visszajár."""
        bdef = self.data.defs.get(bid)
        if (bdef is None or not bdef.get("foundation_tile") or worker is None or not worker.alive
                or worker.inside is not None or worker.owner is not p or not worker.can_build):
            return False
        miss = self.missing_requirements(p, bdef)
        if miss:
            self.notify(p, "Hiányzik: " + ", ".join(miss), (255, 160, 90))
            return False
        n = bdef["size"][0]
        ok, why = self.can_lay_foundation(p, tx, ty, n=n)
        if not ok:
            self.notify(p, f"Nem lehet alapozni: {why}", (255, 120, 90))
            return False
        if not p.can_afford(bdef["cost"]):
            self._notify_missing(p, bdef["cost"])
            return False
        p.pay(bdef["cost"])
        resume = None
        if worker.order and worker.order["type"] == "gather" and not queue:
            resume = {"type": "gather", "res": worker.order["res"]}
        worker.set_order({"type": "found", "bid": bid, "tx": tx, "ty": ty, "n": n, "t": 0.0,
                          "cost": dict(bdef["cost"]), "resume": resume}, queue)
        return True

    def finish_foundation(self, p, tx, ty, n):
        m = self.map
        for y in range(ty, ty + n):
            for x in range(tx, tx + n):
                if m.inb(x, y) and m.buildable_terrain(x, y) and m.block[y * m.w + x] == FREE:
                    self.found[y * m.w + x] = p.id + 1
        m.version += 1
        self.add_fx("heal", (tx + n / 2) * TILE, (ty + n / 2) * TILE, 0.5, (230, 225, 210))

    def _update_decay(self, dt):
        """Ö3: az alapozás nélkül épült Vasbirodalom-épület lassan kopik, fél életerőig."""
        for b in self.buildings:
            if b.alive and b.complete and getattr(b, "unfounded", False):
                dec = b.owner.faction.get("foundation", {}).get("decay")
                if dec and b.hp > b.max_hp * dec.get("min", 0.5):
                    b.hp = max(b.max_hp * dec.get("min", 0.5), b.hp - b.max_hp * dec.get("pct_per_s", 0.002) * dt)

    def can_lay_pond(self, p, tx, ty, check_explored=True):
        pd = p.faction.get("pond")
        if not pd:
            return False, "ennek a népnek nincs tavacskája"
        n = p.rules.get("pond_size", pd.get("size", 2))
        m = self.map
        new = 0
        for y in range(ty, ty + n):
            for x in range(tx, tx + n):
                if not m.inb(x, y):
                    return False, "a térképen kívül"
                if not m.buildable_terrain(x, y):
                    return False, "nem építhető terep"
                if m.block[y * m.w + x] not in (FREE, B_BUILDING):
                    return False, "foglalt hely"
                if check_explored and not p.is_ai and not p.explored[y * m.w + x]:
                    return False, "felderítetlen terület"
                if self.pond[y * m.w + x] != p.id + 1:
                    new += 1
        if not new:
            return False, "itt már van tavacska"
        return True, ""

    def cmd_lay_pond(self, p, tx, ty):
        """Huk'ucha-tavacska (2×2, az Árvíz irányban 3×3): azonnal kész; a szomszédosak egybefüggő tóvá olvadnak."""
        pd = p.faction.get("pond")
        ok, why = self.can_lay_pond(p, tx, ty)
        if not ok:
            if pd:
                self.notify(p, f"Nem lehet tavacskát rakni: {why}", (255, 120, 90))
            return False
        cost = pd.get("cost", {})
        if not p.can_afford(cost):
            self._notify_missing(p, cost)
            return False
        p.pay(cost)
        n = p.rules.get("pond_size", pd.get("size", 2))
        m = self.map
        for y in range(ty, ty + n):
            for x in range(tx, tx + n):
                self.pond[y * m.w + x] = p.id + 1
        self.map.version += 1
        return True

    # ---- Croftian mérgező tüskék ------------------------------------------------------------------------
    SPIKE = {"slow": 0.6, "dps": 4.0, "max": 16}

    def spike_count(self, p):
        sp = getattr(self, "spikes", None)
        return sum(1 for v in sp if v == p.id + 1) if sp else 0

    def can_lay_spikes(self, p, tx, ty, n):
        m = self.map
        sp = self.__dict__.setdefault("spikes", bytearray(m.w * m.h))
        new = 0
        for y in range(ty, ty + n):
            for x in range(tx, tx + n):
                if not m.inb(x, y) or not m.buildable_terrain(x, y):
                    return False, "nem rakható ide"
                if m.block[y * m.w + x] != FREE:
                    return False, "foglalt hely"
                owner = sp[y * m.w + x]
                if owner and owner != p.id + 1:
                    return False, "itt másik játékos tüskéje áll"
                if not owner:
                    new += 1
        if not new:
            return False, "itt már van tüske"
        if self.spike_count(p) + new > p.rules.get("spike_max", self.SPIKE["max"]):
            return False, "elérted a tüskék korlátját"
        return True, ""

    def cmd_lay_spikes(self, p, tx, ty, worker, queue=False):
        """A Csápszolga odamegy, és lerakja a mérgező tüskéket (nem olvad bele)."""
        bdef = self.data.defs.get("cro_merges_tuskek")
        if (bdef is None or p.faction["id"] != bdef["faction"] or worker is None or not worker.alive
                or worker.inside is not None or worker.owner is not p or not worker.can_build):
            return False
        n = p.rules.get("spike_size", 1)
        ok, why = self.can_lay_spikes(p, tx, ty, n)
        if not ok:
            self.notify(p, f"Nem lehet tüskét rakni: {why}", (255, 120, 90))
            return False
        if not p.can_afford(bdef["cost"]):
            self._notify_missing(p, bdef["cost"])
            return False
        p.pay(bdef["cost"])
        resume = None
        if worker.order and worker.order["type"] == "gather" and not queue:
            resume = {"type": "gather", "res": worker.order["res"]}
        worker.set_order({"type": "spike", "tx": tx, "ty": ty, "n": n, "t": 0.0, "cost": dict(bdef["cost"]),
                          "time": bdef.get("time", 2), "resume": resume}, queue)
        return True

    def finish_spikes(self, p, tx, ty, n):
        ok, _ = self.can_lay_spikes(p, tx, ty, n)
        if not ok:
            return False
        m = self.map
        for y in range(ty, ty + n):
            for x in range(tx, tx + n):
                index = y * m.w + x
                if not self.spikes[index]:
                    self.spikes[index] = p.id + 1
                    self.spike_targets[index] = self.make_spike(p, x, y)
        m.version += 1
        return True

    def make_spike(self, owner, tx, ty):
        # A támadható célpont külön van a valódi, útvonalat blokkoló épületektől.
        return Building(self, owner, self.data.defs["cro_merges_tuskek"], tx, ty, complete=True)

    def spikes_in_radius(self, x, y, radius):
        return [spike for spike in self.spike_targets.values()
                if rect_dist(x, y, spike.rect) <= radius]

    def damage_spike(self, src, spike, damage, element, attack):
        if damage <= 0:
            return
        if element not in ("fire", "poison") and not (attack and attack.get("poison")):
            damage *= 0.2
            if src is not None and src.is_unit and src.alive and src.defn.get("faction") != "croftian":
                strength = spike.owner.rules.get("spike_mult", 1.0)
                src.poison = (None, self.SPIKE["dps"] * strength, self.time + 2.0)
        spike.hp = max(0, spike.hp - damage)
        if spike.hp == 0:
            spike.alive = False
            index = spike.ty * self.map.w + spike.tx
            self.spikes[index] = 0
            self.spike_targets.pop(index)
            self.map.version += 1
            self.add_fx("death", spike.x, spike.y, 0.5, spike.owner.color)

    def _update_spikes(self):
        """Fél másodpercenként: a tüskéken álló nem croftian (földi) egységek lassulnak és mérgeződnek."""
        sp = getattr(self, "spikes", None)
        if not sp or not any(sp):
            return
        m = self.map
        for u in self.units:
            if not u.alive or u.flying or u.inside is not None or u.defn.get("faction") == "croftian":
                continue
            tx, ty = int(u.x // TILE), int(u.y // TILE)
            if m.inb(tx, ty) and sp[ty * m.w + tx]:
                ow = self.players[sp[ty * m.w + tx] - 1] if sp[ty * m.w + tx] - 1 < len(self.players) else None
                if ow is None:
                    continue
                k = ow.rules.get("spike_mult", 1.0)  # Bénító méreg
                u.slow_until, u.slow_mult = self.time + 0.7, max(0.2, 1 - (1 - self.SPIKE["slow"]) * k)
                u.poison = (None, self.SPIKE["dps"] * k, self.time + 2.0)

    POND_TIME = 4.0  # mp: ennyi ideig ás a Tóásó egy tavacskát

    def cmd_dig_pond(self, p, tx, ty, worker, queue=False):
        """Huk’ucha: a Tóásó odamegy, és kiássa a tavacskát (az ára a parancskor fizetendő; ha a hely közben
        megtelik, visszajár). A kijelölt helyre már előre lehet építést sorba tenni (tervezett tavacska)."""
        pd = p.faction.get("pond")
        if (not pd or worker is None or not worker.alive or worker.inside is not None
                or worker.owner is not p or not worker.can_build):
            return False
        ok, why = self.can_lay_pond(p, tx, ty)
        if not ok:
            self.notify(p, f"Nem lehet tavacskát ásni: {why}", (255, 120, 90))
            return False
        cost = pd.get("cost", {})
        if not p.can_afford(cost):
            self._notify_missing(p, cost)
            return False
        p.pay(cost)
        n = p.rules.get("pond_size", pd.get("size", 2))
        plan = self.__dict__.setdefault("pond_plan", {})
        m = self.map
        for y in range(ty, ty + n):
            for x in range(tx, tx + n):
                if m.inb(x, y) and not self.pond[y * m.w + x]:
                    plan[y * m.w + x] = p.id + 1
        resume = None
        if worker.order and worker.order["type"] == "gather" and not queue:
            resume = {"type": "gather", "res": worker.order["res"]}
        worker.set_order({"type": "dig", "tx": tx, "ty": ty, "n": n, "t": 0.0, "cost": dict(cost),
                          "resume": resume}, queue)
        return True

    WORKSITE_ABANDON = 90.0   # ennyi másodperc munka nélkül: a félkész hely megszűnik, az ára visszajár

    def worksite(self, p, kind, o):
        """A félkész tavacska / alapozás helye (az első munkamozdulattól). A haladás itt van, nem a munkás
        parancsában: ha a munkás elmegy, a hely megmarad, és bármelyik építő folytathatja."""
        ws = self.__dict__.setdefault("worksites", {})
        key = (kind, p.id, o["tx"], o["ty"])
        s = ws.get(key)
        if o.get("site") and (s is None or o.get("site_ref", s) is not s):
            return None
        if s is None:
            s = ws[key] = {"kind": kind, "owner": p, "tx": o["tx"], "ty": o["ty"], "n": o["n"], "t": 0.0,
                           "cost": dict(o.get("cost") or {}), "bid": o.get("bid"), "idle": 0.0}
        elif o.get("cost"):  # második munkás új paranccsal: a költség csak egyszer kell
            p.refund(o["cost"])
        o["cost"] = {}
        o["site"] = key
        o["site_ref"] = s
        return s

    def worksite_at(self, p, tx, ty):
        for s in getattr(self, "worksites", {}).values():
            if s["owner"] is p and s["tx"] <= tx < s["tx"] + s["n"] and s["ty"] <= ty < s["ty"] + s["n"]:
                return s
        return None

    def end_worksite(self, s, refund=False):
        ws = getattr(self, "worksites", {})
        key = (s["kind"], s["owner"].id, s["tx"], s["ty"])
        if ws.get(key) is not s:
            return
        del ws[key]
        if refund:
            s["owner"].refund(s["cost"])
            if s["kind"] == "dig":
                self.finish_pond(s["owner"], s["tx"], s["ty"], s["n"], False)

    def _update_worksites(self, dt=None):
        last = getattr(self, "_ws_last", self.time)   # játékidő szerint (nem a képkockák száma szerint)
        dt = self.time - last
        self._ws_last = self.time
        for s in list(getattr(self, "worksites", {}).values()):
            s["idle"] += dt
            if s["idle"] >= self.WORKSITE_ABANDON:
                self.end_worksite(s, refund=True)

    def cmd_resume_worksite(self, units, s, queue=False):
        """Jobb klikk egy félkész tavacskára / alapozásra: az építők folytatják (új költség nélkül)."""
        n = 0
        for u in units:
            if not (u.alive and u.is_unit and u.owner is s["owner"] and u.can_build):
                continue
            if s["kind"] == "dig" and not u.owner.faction.get("pond"):
                continue
            o = {"type": s["kind"], "tx": s["tx"], "ty": s["ty"], "n": s["n"], "t": s["t"], "cost": {},
                 "site": (s["kind"], s["owner"].id, s["tx"], s["ty"]), "site_ref": s, "resume": None}
            if s["kind"] == "found":
                o["bid"] = s["bid"]
            u.set_order(o, queue)
            n += 1
        return n

    def _prune_pond_plan(self):
        """A tervezett (még ki nem ásott) tavacskák közül csak azok maradnak, amelyekhez van élő ásás-parancs
        (pl. ha a munkást másra küldték, a terv megszűnik, az ára visszajár)."""
        plan = getattr(self, "pond_plan", None)
        if not plan:
            return
        m = self.map
        live = {}
        for s in getattr(self, "worksites", {}).values():  # a félkész helyek terve megmarad
            if s["kind"] == "dig":
                for y in range(s["ty"], s["ty"] + s["n"]):
                    for x in range(s["tx"], s["tx"] + s["n"]):
                        live[y * m.w + x] = s["owner"].id + 1
        for u in self.units:
            for o in ([u.order] if u.order else []) + list(u.queue):
                if u.alive and o and o.get("type") == "dig":
                    for y in range(o["ty"], o["ty"] + o["n"]):
                        for x in range(o["tx"], o["tx"] + o["n"]):
                            live[y * m.w + x] = u.owner.id + 1
        for i in [i for i in plan if live.get(i) != plan[i]]:
            del plan[i]

    def finish_pond(self, p, tx, ty, n, ok):
        m = self.map
        plan = self.__dict__.setdefault("pond_plan", {})
        for y in range(ty, ty + n):
            for x in range(tx, tx + n):
                i = y * m.w + x
                if not m.inb(x, y):
                    continue
                if plan.get(i) == p.id + 1:
                    del plan[i]
                if ok and m.buildable_terrain(x, y) and (m.block[i] == FREE or self.pond[i] == p.id + 1):
                    self.pond[i] = p.id + 1
        m.version += 1
        if ok:
            self.add_fx("ring", (tx + n / 2) * TILE, (ty + n / 2) * TILE, 0.6, (120, 210, 230))

    def _update_lights_and_guards(self):
        """Ooni: lámpásfényben gyógyulnak az egységeik; a kapuőr nianok páncélt adnak a mellettük álló épületnek."""
        for b in self.buildings:
            if b.guard_armor:
                b.guard_armor = 0
        lights = [b for b in self.buildings if b.alive and b.defn.get("light_radius") and b.complete]
        if not lights:
            return
        for u in self.units:
            if not u.alive or u.hp >= u.max_hp or u.defn.get("faction") != "ooni":
                continue
            for b in lights:
                if b.owner is u.owner and dmath.hypot(u.x - b.x, u.y - b.y) <= self.light_r(b) * TILE:
                    u.hp = min(u.max_hp, u.hp + b.defn.get("light_heal", 2))
                    break
        for u in self.units:
            kg = u.owner.rules.get("kokutya_guard") if u.alive and u.defn["id"] == "oon_kokutya" else None
            if kg:  # Kőkutya-hűség
                for b in self.buildings:
                    if b.owner is u.owner and b.alive and rect_dist(u.x, u.y, b.rect) <= 2.5 * TILE:
                        b.guard_armor = min(6, b.guard_armor + kg)
        for u in self.units:
            if u.alive and "oon_kapuorzes" in u.toggles:
                best = None
                for b in self.buildings:
                    if b.owner is u.owner and b.alive and rect_dist(u.x, u.y, b.rect) <= 2.5 * TILE:
                        best = b
                        break
                if best is not None:
                    best.guard_armor = min(6, best.guard_armor + 3)

    def sfx(self, ev, e):
        se = self.__dict__.setdefault("sound_events", [])
        if len(se) < 64:
            se.append((ev, e))

    def on_building_complete(self, b):
        self.sfx("building_done", b)
        st = getattr(b.owner, "stats", None)
        if st is not None and not b.defn.get("foundation_tile"):
            st["buildings_built"] += 1
        pr = b.defn.get("pond_ring")
        if pr:  # Huk’ucha Gyöngybója: tavacska a kő körül
            m = self.map
            for y in range(b.ty - pr, b.ty + b.h + pr):
                for x in range(b.tx - pr, b.tx + b.w + pr):
                    if m.inb(x, y) and m.buildable_terrain(x, y):
                        self.pond[y * m.w + x] = b.owner.id + 1
            m.version += 1
        fd = b.owner.faction.get("foundation")
        if fd and not b.defn.get("foundation_free") and not self.founded(b.owner, b.tx, b.ty, b.w, b.h):
            b.hp = min(b.hp, b.max_hp * fd.get("unfounded_hp", 0.5))  # alapozás nélkül: fél életerő (javítható)
            b.unfounded = True  # és lassan kopik is, fél életerőig
            self.notify(b.owner, f"{b.name}: alapozás nélkül épült – fél életerővel, és lassan kopik "
                                 "(Fabrikátorral javítható).", (255, 190, 110))
        self.notify(b.owner, f"Elkészült: {b.name}", (150, 230, 150))
        self._update_supply()
        self._update_power()
        self._update_creep(grow=False)
        self._update_network()

    def _update_network(self):
        """Micélia: a Szívgombákból induló fonal-gráf; a nem kapcsolódó épületek leállnak."""
        m = self.map
        for p in self.players:
            if p.faction.get("mechanic") != "network":
                continue
            blds = [b for b in self.buildings if b.owner is p and b.alive]
            done = [b for b in blds if b.complete]
            seen = set()
            stack = [b for b in done if b.defn.get("core")]
            links = []
            for b in stack:
                seen.add(b.id)
            while stack:
                a = stack.pop()
                for o in done:
                    if o.id in seen:
                        continue
                    lr = max(a.defn.get("link_radius", 0), o.defn.get("link_radius", 0)) * TILE
                    if dmath.hypot(a.x - o.x, a.y - o.y) <= lr:
                        seen.add(o.id)
                        stack.append(o)
                        links.append((a, o))
            for b in blds:
                b.connected = (b.id in seen) or not b.complete
                b.powered = b.id in seen or not b.complete
            p.links = links
            field = bytearray(m.w * m.h)
            for b in done:
                if b.id not in seen:
                    continue
                r = b.defn.get("field_radius", 4)
                bx, by = b.x / TILE, b.y / TILE
                for y in range(int(by - r), int(by + r) + 1):
                    for x in range(int(bx - r), int(bx + r) + 1):
                        if m.inb(x, y) and dmath.sq(x + 0.5 - bx) + dmath.sq(y + 0.5 - by) <= r * r:
                            field[y * m.w + x] = 1
            if field != p.creep:
                p.creep = field
                self.net_version += 1

    # ---- Wullof: nomád tábor --------------------------------------------
    def cmd_pack(self, b):
        p = b.owner
        wagon_id = p.faction.get("wagon")
        if not (b.alive and b.complete and b.defn.get("packable") and wagon_id):
            return False
        for i in range(len(b.queue) - 1, -1, -1):
            self.cancel_item(b, i)
        b.alive = False
        self.map.set_block(b.tx, b.ty, b.w, b.h, FREE)
        u = Unit(self, p, self.data.defs[wagon_id], b.x, b.y)
        u.packed = b.defn
        u.max_hp = max(u.max_hp, b.max_hp // 2)
        u.hp = u.max_hp * b.hp / b.max_hp
        self.units.append(u)
        self.add_fx("dust", b.x, b.y, 0.5, (150, 130, 100))
        self.sfx("building_pack", b)
        self.buildings = [x for x in self.buildings if x.alive]
        self._update_supply()
        return u

    def unpack(self, u, tx, ty):
        bdef = u.packed
        b = self.place_building(u.owner, bdef, tx, ty)
        t = u.owner.faction.get("unpack_time", 8)
        b.progress = max(0.0, b.build_time - t)
        b.self_build = True  # a letelepítéshez nem kell munkás
        self.sfx("building_unpack", b)
        b.hp = b.max_hp * min(1.0, u.hp / u.max_hp)
        u.alive = False
        self.add_fx("dust", b.x, b.y, 0.5, (150, 130, 100))
        return b

    def cmd_unpack(self, u, tx, ty):
        if not u.alive or u.inside is not None or not getattr(u, "packed", None):
            return False
        ok, reason = self.can_place(u.owner, u.packed, tx, ty, check_req=False)
        if not ok:
            self.notify(u.owner, f"Nem lehet letelepíteni: {reason}", (255, 120, 90))
            return False
        u.set_order({"type": "unpack", "tx": tx, "ty": ty})
        return True

    # ---- Wullof: bölény – felültetés / leszállás --------------------------
    def cmd_mount(self, units, ab_id):
        ab = self.data.abilities[ab_id]
        done = 0
        for u in [u for u in units if u.alive and u.is_unit and u.has_ability(ab_id)]:
            p = u.owner
            if ab.get("type") == "dismount":
                spots = []
                for dx in (-10, 10):
                    tx = max(0, min(self.map.w - 1, int((u.x + dx) // TILE)))
                    ty = max(0, min(self.map.h - 1, int(u.y // TILE)))
                    candidates = [(x, y) for y in range(max(0, ty-2), min(self.map.h, ty+3))
                                  for x in range(max(0, tx-2), min(self.map.w, tx+3))
                                  if self.map.passable(x, y) and (x, y) not in spots]
                    if not candidates:
                        break
                    spots.append(min(candidates, key=lambda xy: dmath.sq(xy[0]-tx) + dmath.sq(xy[1]-ty)))
                if len(spots) != 2:
                    continue
                frac = u.hp / u.max_hp
                rid = u.rider_id or ab.get("rider", "wul_landzsas")
                for k, uid in enumerate((ab["mount"], rid)):
                    x, y = spots[k]
                    nu = Unit(self, p, self.data.defs[uid], (x + .5) * TILE, (y + .5) * TILE)
                    nu.hp = max(1.0, nu.max_hp * frac)
                    self.units.append(nu)
                u.alive = False
                self.add_fx("dust", u.x, u.y, 0.5, (150, 130, 100))
                done += 1
                continue
            riders = [r for r in self.units if r.alive and r.owner is p and r.defn["id"] in ab["riders"]
                      and r.inside is None and dmath.hypot(r.x - u.x, r.y - u.y) <= ab.get("range", 3) * TILE]
            if not riders:
                far = [r for r in self.units if r.alive and r.owner is p and r.defn["id"] in ab["riders"]
                       and r.inside is None and dmath.hypot(r.x - u.x, r.y - u.y) <= 12 * TILE]
                if far:
                    r = min(far, key=lambda r: dmath.hypot(r.x - u.x, r.y - u.y))
                    self.cmd_move([r], u.x, u.y)
                    self.notify(p, "A harcos a bölényhez indul – ha odaért, ültesd fel.", (200, 200, 200))
                else:
                    self.notify(p, "Nincs felültethető wullof harcos a bölény közelében.", (255, 160, 90))
                continue
            r = min(riders, key=lambda r: dmath.hypot(r.x - u.x, r.y - u.y))
            frac = (u.hp / u.max_hp + r.hp / r.max_hp) / 2
            nu = Unit(self, p, self.data.defs[ab["into"]], u.x, u.y)
            nu.hp = nu.max_hp * frac
            nu.rider_id = r.defn["id"]
            self.units.append(nu)
            u.alive = False
            r.alive = False
            self.add_fx("dust", u.x, u.y, 0.5, (150, 130, 100))
            done += 1
        return done

    # ---- Micélia: összeolvadás ------------------------------------------
    def cmd_merge(self, units, ab_id):
        ab = self.data.abilities[ab_id]
        n = ab.get("count", 3)
        pool = [u for u in units if u.alive and u.is_unit and u.has_ability(ab_id)]
        made = 0
        while len(pool) >= n:
            cx = sum(u.x for u in pool) / len(pool)
            cy = sum(u.y for u in pool) / len(pool)
            pool.sort(key=lambda u: dmath.hypot(u.x - cx, u.y - cy))
            grp, pool = pool[:n], pool[n:]
            gx = sum(u.x for u in grp) / n
            gy = sum(u.y for u in grp) / n
            if any(dmath.hypot(u.x - gx, u.y - gy) > ab.get("range", 4) * TILE for u in grp):
                self.cmd_move(grp, gx, gy)
                self.notify(grp[0].owner, "Az összeolvadáshoz közel kell lenniük – odamennek.", (200, 200, 200))
                continue
            frac = sum(u.hp / u.max_hp for u in grp) / n
            for u in grp:
                u.alive = False
            nu = Unit(self, grp[0].owner, self.data.defs[ab["into"]], gx, gy)
            nu.hp = nu.max_hp * frac
            self.units.append(nu)
            self.add_fx("death", gx, gy, 0.9, (150, 220, 120))
            made += 1
        return made

    # ---- Vasbirodalom: lélekidézés --------------------------------------
    def find_corpse(self, x, y, r):
        best, bestd = None, r
        for c in self.corpses:
            if len(c) > 6 and c[6]:
                continue
            if self.time - c[5] < 1.0:
                continue
            d = dmath.hypot(c[2] - x, c[3] - y)
            if d <= bestd:
                best, bestd = c, d
        return best

    def raise_corpse(self, caster, ab, x, y):
        c = self.find_corpse(x, y, 2.5 * TILE)
        if c is None:
            return False
        c.append(True)
        large = "large" in c[0].get("tags", ()) or c[0].get("supply", 1) >= 4
        uid = ab["large"] if large else ab["small"]
        u = Unit(self, caster.owner, self.data.defs[uid], c[2], c[3])
        self.units.append(u)
        caster.last_cast = self.time
        self.add_fx("heal", c[2], c[3], 0.8, (120, 255, 200))
        self.corpses.remove(c)
        return True

    def _update_supply(self):
        cap_max = self.data.game["max_supply"]
        for p in self.players:
            p.supply_cap = 0
            p.supply_used = 0.0
        for b in self.buildings:
            if b.complete:
                b.owner.supply_cap += b.defn.get("supply", 0)
                sa = b.owner.rules.get("supply_add")
                if sa and b.defn.get("supply"):  # Kiadós tanya
                    b.owner.supply_cap += sa.get(b.defn["id"], 0)
                ns = b.owner.faction.get("network_supply")
                if ns:
                    ns += b.owner.rules.get("network_supply_add", 0)
                if ns and b.connected:  # gombanép: a hálózat minden összekötött épülete létszámot ad
                    b.owner.supply_cap += ns
            for item in b.queue:
                b.owner.supply_used += item.get("supply", 0)
        for u in self.units:
            if not u.alive:
                continue
            target = self.data.defs.get(u.morph[0]) if u.morph and u.summon_hero is None else None
            u.owner.supply_used += (target.get("supply", 1) / target.get("count", 1) if target else u.supply())
            us = u.defn.get("unit_supply")
            if us:  # croftian szemhordozó: ellátmányt hordoz
                u.owner.supply_cap += us
        for p in self.players:
            p.supply_cap = min(cap_max, p.supply_cap + p.bonus_supply)

    def _update_power(self):
        for p in self.players:
            if p.faction.get("mechanic") != "power":
                continue
            pyl = [o for o in self.buildings if o.owner is p and o.complete and o.alive and o.defn.get("power_radius")]
            for b in self.buildings:
                if b.owner is not p:
                    continue
                if b.defn.get("no_power"):
                    b.powered = True
                else:
                    b.powered = any(dmath.hypot(o.x - b.x, o.y - b.y) <= self.power_r(o) * TILE for o in pyl)

    def _update_creep(self, grow=True):
        m = self.map
        changed = False
        for p in self.players:
            if p.faction.get("mechanic") != "creep":
                continue
            c = bytearray(m.w * m.h)
            for b in self.buildings:
                if b.owner is not p or not b.alive:
                    continue
                R = b.defn.get("creep_radius", 0)
                if not R:
                    continue
                if b.complete:
                    if grow:
                        b.creep_r = min(R, b.creep_r + 0.7)
                    r = b.creep_r
                else:
                    r = 1.0
                r += max(b.w, b.h) / 2
                bx, by = b.x / TILE, b.y / TILE
                for y in range(int(by - r) - 1, int(by + r) + 2):
                    for x in range(int(bx - r) - 1, int(bx + r) + 2):
                        if m.buildable_terrain(x, y) and dmath.sq(x + 0.5 - bx) + dmath.sq(y + 0.5 - by) <= r * r:
                            c[y * m.w + x] = 1
            if c != p.creep:
                p.creep = c
                changed = True
        if changed:
            self.creep_version += 1

    def _update_vis(self, p):
        m = self.map
        w, h = m.w, m.h
        fog = self.options.get("fog", "normal") if hasattr(self, "options") else "normal"
        if fog == "none":  # köd nélkül: minden látszik
            p.visible = bytearray(b"" * (w * h))
            p.explored = p.visible
            return
        if fog == "explored" and not getattr(p, "_pre_explored", False):
            p.explored = bytearray(b"" * (w * h))
            p._pre_explored = True
        vis = bytearray(w * h)
        exp = p.explored
        ents = [u for u in self.units if p.allied(u.owner)] + [b for b in self.buildings if p.allied(b.owner)]
        for e in ents:
            tx, ty = int(e.x // TILE), int(e.y // TILE)
            for dy, hw in circle_spans(int(e.sight())):
                y = ty + dy
                if 0 <= y < h:
                    a, b = max(0, tx - hw), min(w - 1, tx + hw)
                    if a <= b:
                        row = y * w
                        vis[row + a:row + b + 1] = ONES[:b - a + 1]
                        exp[row + a:row + b + 1] = ONES[:b - a + 1]
        for rx, ry, rr, until in getattr(p, "reveals", ()):  # Ö19 Napcsóva: ideiglenes felfedés
            if until > self.time:
                tx, ty = int(rx // TILE), int(ry // TILE)
                for dx, dy in circle_offsets(int(rr // TILE)):
                    x, y = tx + dx, ty + dy
                    if 0 <= x < w and 0 <= y < h:
                        vis[y * w + x] = 1
                        exp[y * w + x] = 1
        p.visible = vis

    def is_visible(self, p, ent):
        if is_sheltered(ent):
            return False
        if ent.owner is p or p.allied(ent.owner):
            return True
        mb = getattr(ent, "marked_by", None)            # megjelölt: a jelölő (és szövetségesei) a köd alól is látják
        if mb is not None and getattr(ent, "marked_until", 0.0) > self.time and (mb is p or p.allied(mb)):
            return True
        if getattr(ent, "hidden", False) and not self.seen_by(p, ent):
            return False
        tx, ty = int(ent.x // TILE), int(ent.y // TILE)
        return self.map.inb(tx, ty) and p.visible[ty * self.map.w + tx] == 1

    def finish_item(self, b, item):
        p = b.owner
        if item["kind"] == "unit":
            udef = self.data.defs[item["id"]]
            if self.spawn_unit(p, udef, b, udef.get("count", 1)) is None:
                return False
            p.units_made += 1
            self.notify(p, f"Egység kész: {udef['name']}", (180, 220, 255))
        else:
            r = self.data.researches[item["id"]]
            p.researching.discard(r["id"])
            p.researched.add(r["id"])
            self.apply_research(p, r)
            self.notify(p, f"Fejlesztés kész: {r['name']}", (255, 220, 120))
        return True

    def ai_choose_path(self, p):
        paths = list(p.faction.get("paths", {}))
        if not paths:
            return
        if getattr(p, "forced_path", None) in paths:  # az egyéni csata beállításában megadott irány
            self.cmd_choose_path(p, p.forced_path)
            for ai in self.ais:
                if ai.p is p:
                    ai.on_path(p.forced_path)
            return
        pref = p.faction.get("ai", {}).get("path_pref") or paths
        weights = [3 if k == pref[0] else 2 if len(pref) > 1 and k == pref[1] else 1 for k in paths]
        x = random.random() * sum(weights)
        for k, wgt in zip(paths, weights):
            x -= wgt
            if x <= 0:
                break
        self.cmd_choose_path(p, k)
        for ai in self.ais:
            if ai.p is p:
                ai.on_path(k)

    @staticmethod
    def merge_rules(old, new):
        """Szabályok összefésülése (irány + kutatások): ütközésnél az erősebb (nagyobb esélyű) marad."""
        out = dict(old)
        for k, v in new.items():
            o = out.get(k)
            if isinstance(o, dict) and isinstance(v, dict) and "chance" in o and o.get("chance", 0) > v.get("chance", 0):
                continue
            out[k] = v
        return out

    def apply_research(self, p, r):
        old = {}
        ents = [e for e in self.units + self.buildings if e.owner is p]
        for e in ents:
            old[e.id] = (p.stat(e.defn, "hp"), p.stat(e.defn, "shield"), p.stat(e.defn, "energy"))
        if r.get("rules"):
            p.rules = self.merge_rules(p.rules, r["rules"])
        for eff in r.get("effects", []):
            d = p.upg.setdefault(eff["stat"], {})
            for tag in eff.get("tags", ["all"]):
                d[tag] = d.get(tag, 0) + eff.get("add", 0)
        for e in ents:
            ohp, osh, oen = old[e.id]
            nhp, nsh = p.stat(e.defn, "hp"), p.stat(e.defn, "shield")
            if hasattr(e, "max_energy"):
                e.max_energy += p.stat(e.defn, "energy") - oen
            e.max_hp += nhp - ohp
            e.hp += nhp - ohp
            e.max_shield += nsh - osh
            e.shield += nsh - osh

    def spawn_unit(self, p, udef, b, count=1):
        sg = udef.get("spawn_group")
        if sg:
            count *= sg.get("count", 3)
            udef = self.data.defs[sg["unit"]]
        m = self.map
        rally = b.rally
        tgt = rally[:2] if rally else (b.x, b.y + 999)
        warp = None
        if p.faction.get("warp_in") and rally and not udef.get("worker"):
            obs = [o for o in self.buildings if o.owner is p and o.complete and o.alive and o.defn.get("power_radius")]
            if obs:
                o = min(obs, key=lambda o: dmath.hypot(o.x - tgt[0], o.y - tgt[1]))
                if dmath.hypot(o.x - tgt[0], o.y - tgt[1]) <= self.power_r(o) * TILE:
                    warp = b = o

        radius = udef.get("radius", 0.4) * TILE
        flying = bool(udef.get("flying"))
        left, top = b.tx * TILE, b.ty * TILE
        right, bottom = left + b.w * TILE, top + b.h * TILE
        offset = max(TILE / 2, radius + 1e-6)
        candidates = [(x, y) for x in range(left + TILE // 2, right, TILE)
                      for y in (top - offset, bottom + offset)]
        candidates += [(x, y) for y in range(top + TILE // 2, bottom, TILE)
                       for x in (left - offset, right + offset)]
        candidates += [(x, y) for x in (left - offset, right + offset)
                       for y in (top - offset, bottom + offset)]
        candidates.sort(key=lambda pt: dmath.sq(pt[0] - tgt[0]) + dmath.sq(pt[1] - tgt[1]))
        occupied = [(u.x, u.y, u.radius) for u in self.units
                    if u.alive and u.inside is None and u.flying == flying]
        positions = []
        for x, y in candidates:
            if not (radius <= x <= m.w*TILE-radius and radius <= y <= m.h*TILE-radius):
                continue
            if not flying and not fits(m, x, y, radius):
                continue
            if any(dmath.sq(x-ux) + dmath.sq(y-uy) < dmath.sq(radius+ur) for ux, uy, ur in occupied):
                continue
            positions.append((x, y))
            occupied.append((x, y, radius))
            if len(positions) == count:
                break
        if len(positions) != count or not positions:
            return None

        if warp is not None:
            self.add_fx("heal", warp.x, warp.y, 0.6, (150, 230, 255))
        spawned = []
        for x, y in positions:
            u = Unit(self, p, udef, x, y)
            self.units.append(u)
            spawned.append(u)
            if rally:
                rx, ry, target = rally
                if target is not None and getattr(target, "is_resource", False) and target.alive and u.can_gather(target.kind):
                    u.set_order({"type": "gather", "res": target})
                else:
                    u.set_order({"type": "move", "x": rx, "y": ry})
            self.sfx("trained", u)
        return spawned[0]

    def _check_victory(self):
        for p in self.players:
            if getattr(p, "baseless", False):  # bázis nélküli különítmény: a küldetés-rendező dönt a sorsáról
                continue
            if not p.defeated and not any(b.owner is p for b in self.buildings) and not any(
                    u.owner is p and getattr(u, "packed", None) for u in self.units):
                p.defeated = True
                for u in self.units:
                    if u.owner is p:
                        u.alive = False
                for q in self.players:
                    self.notify(q, f"{p.name} vereséget szenvedett!", (255, 200, 80))
        if getattr(self, "lockstep", False):
            alive = [p for p in self.players if not p.defeated]
            if self.result is None and all(not a.hostile(b) for a in alive for b in alive):
                me = self.players[self.local_id]
                self.result = (("win" if me.allied(alive[0]) else "lose", me)
                               if alive else ("end", None))
            return
        if self.result is None and self.director is None:
            humans = [p for p in self.players if not p.is_ai]
            me = self.players[self.local_id] if self.local_id < len(self.players) else (humans[0] if humans else None)
            if me is not None and not me.is_ai:
                if me.defeated and not any(not q.defeated for q in self.players if q is not me and me.allied(q)):
                    self.result = ("lose", me)
                elif me.defeated and not any(not q.defeated and not q.is_ai for q in self.players if q is not me):
                    self.result = ("lose", me)
                elif all(q.defeated for q in self.players if me.hostile(q)):
                    self.result = ("win", me)
        alive = [p for p in self.players if not p.defeated]
        if self.result is None and all(p.is_ai for p in self.players) and \
                all(not a.hostile(b) for a in alive for b in alive):
            self.result = ("end", alive[0] if alive else None)

    # ------------------------------------------------------------------
    # PARANCSOK – ugyanezt használja a felület és a gépi ellenfél
    def formation(self, units, x, y):
        m = self.map
        goals = []
        ground = sorted((u for u in units if not u.flying),
                        key=lambda u: dmath.hypot(u.x - x, u.y - y))
        if ground:
            moving = set(units)
            occupants = [v for v in self.units if v not in moving and v.alive and not v.flying
                         and v.inside is None and not v.burrowed and not v.path and not v.path_pending]
            sx, sy = int(x // TILE), int(y // TILE)
            start = m.nearest_free(sx, sy) if m.inb(sx, sy) else None
            if start:
                seen = {start}
                pending = deque([start])
                while pending and ground:
                    tx, ty = pending.popleft()
                    center = ((tx + .5) * TILE, (ty + .5) * TILE)
                    points = [(x, y), center] if (tx, ty) == (sx, sy) else [center]
                    for gx, gy in points:
                        for u in ground:
                            if fits(m, gx, gy, u.radius) and all(
                                dmath.sq(gx-px) + dmath.sq(gy-py) >= dmath.sq(u.radius + other.radius + 3)
                                for other, (px, py) in goals
                            ) and all(
                                dmath.sq(gx-v.x) + dmath.sq(gy-v.y) >= dmath.sq(u.radius + v.radius + 1)
                                for v in occupants
                            ):
                                goals.append((u, (gx, gy)))
                                ground.remove(u)
                                break
                        else:
                            continue
                        break
                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        nt = tx + dx, ty + dy
                        if nt not in seen and m.passable(*nt):
                            seen.add(nt)
                            pending.append(nt)
        air = [u for u in units if u.flying]
        reserved = []
        for i, u in enumerate(air):
            a = i * 2.4
            r = 0 if i == 0 else 20 + 6 * math.sqrt(i) * 4
            gx = max(u.radius, min(m.w * TILE - u.radius, x + dmath.cos(a) * r))
            gy = max(u.radius, min(m.h * TILE - u.radius, y + dmath.sin(a) * r))
            spot = self.landing_position(gx, gy, u.radius, flying=True, ignore=u, reserved=reserved)
            if spot is not None:
                goals.append((u, spot))
                reserved.append((*spot, u.radius))
        return goals

    def cmd_move(self, units, x, y, attack=False, queue=False):
        units = [u for u in units if u.alive and u.is_unit and u.speed_base() > 0]
        goals = self.formation(units, x, y)
        if goals:
            self._move_serial = getattr(self, "_move_serial", 0) + 1
        large = sorted((u for u, _ in goals if not u.flying and u.radius > TILE * .72),
                       key=lambda u: (dmath.sq(u.x - x) + dmath.sq(u.y - y), u.id))
        ranks = {u: i for i, u in enumerate(large)}
        extents = {False: 0, True: 0}
        for u, (gx, gy) in goals:
            extents[u.flying] = max(extents[u.flying], dmath.hypot(gx - x, gy - y))
        for u, (gx, gy) in goals:
            typ = "amove" if attack and u.attack else "move"
            u.set_order({"type": typ, "x": gx, "y": gy, "destination": (x, y),
                         "move_serial": self._move_serial, "group_move": len(goals) > 1, "formation_extent": extents[u.flying],
                         "traffic_rank": ranks.get(u, 0)}, queue)
        if len(goals) < len(units):
            for p in dict.fromkeys(u.owner for u in units):
                self.notify(p, "Nem jut minden egységnek szabad hely a cél körül.")
        return bool(goals)

    def cmd_attack(self, units, target, queue=False):
        if target is None or getattr(target, "owner", None) is None or is_sheltered(target):
            return
        for u in units:
            if not u.alive or not u.is_unit or not (target.defn.get("spike_tile") or u.owner.hostile(target.owner)):
                continue
            if u.can_hit(target):
                u.set_order({"type": "attack", "target": target}, queue)
            else:
                u.set_order({"type": "move", "x": target.x, "y": target.y}, queue)

    def cmd_smart(self, units, x, y, target, queue=False):
        """Jobb klikk – kontextusfüggő parancs."""
        units = [u for u in units if u.alive and u.is_unit]
        if not units:
            return
        if target is None and getattr(self, "worksites", None):  # félkész tavacska / alapozás: folytatás
            s = self.worksite_at(units[0].owner, int(x // TILE), int(y // TILE))
            if s is not None and self.cmd_resume_worksite(units, s, queue):
                rest = [u for u in units if not u.can_build]
                if rest:
                    self.cmd_move(rest, x, y, queue=queue)
                return
        if target is not None and getattr(target, "is_resource", False):
            workers = [u for u in units if u.can_gather(target.kind)]
            for u in workers:
                u.set_order({"type": "gather", "res": target}, queue)
            others = [u for u in units if u not in workers]
            if others:
                self.cmd_move(others, x, y, queue=queue)
            return
        if target is not None and target.owner is not None and (
                target.defn.get("spike_tile") or units[0].owner.hostile(target.owner)):
            self.cmd_attack(units, target, queue)
            return
        if (target is not None and target.is_unit and target.owner is units[0].owner and "mech" in target.tags
                and target.hp < target.max_hp and any(u.is_worker for u in units)):
            for u in units:
                if u.is_worker and u is not target:
                    u.set_order({"type": "repair", "b": target}, queue)
            rest = [u for u in units if not u.is_worker]
            if rest:
                self.cmd_move(rest, x, y, queue=queue)
            return
        if (target is not None and target.is_building and target.owner is units[0].owner and target.complete
                and target.defn.get("mine") and getattr(target, "res", None) is not None
                and any(self.can_mine(u, target) for u in units)):
            miners = [u for u in units if self.can_mine(u, target)]
            for u in miners:
                u.set_order({"type": "enter_mine", "b": target}, queue)
            rest = [u for u in units if u not in miners]
            if rest:
                self.cmd_move(rest, x, y, queue=queue)
            return
        if (target is not None and target.is_building and target.owner is units[0].owner and target.complete
                and target.defn.get("torii") and getattr(target, "partner", None) is not None
                and target.partner.complete and target.partner.alive):
            for u in units:
                if not u.flying and u.speed_base() > 0:
                    u.set_order({"type": "gate", "b": target}, queue)
            return
        if (target is not None and target.is_building and target.owner is units[0].owner and target.complete
                and target.defn.get("garrison") and not any(u.is_worker and u.carry_amt for u in units)
                and target.hp >= target.max_hp and any(u.has_ability("beszallas") for u in units)):
            self.cmd_garrison(units, target, queue=queue)
            return
        if (target is not None and target.is_building and target.owner is units[0].owner and target.complete
                and target.defn.get("pray_slots") and target.owner.faction.get("faith")
                and any(u.is_worker for u in units) and not any(u.is_worker and u.carry_amt for u in units)
                and not (target.hp < target.max_hp and target.owner.faction.get("repair",
                                                                                   target.owner.faction.get("mechanic") == "standard"))):
            for u in units:  # Ö17: a munkás fohászkodik / énekel a szentélynél
                if u.is_worker:
                    u.set_order({"type": "pray", "b": target}, queue)
            rest = [u for u in units if not u.is_worker]
            if rest:
                self.cmd_move(rest, x, y, queue=queue)
            return
        if target is not None and target.is_building and target.owner is units[0].owner:
            mech = target.owner.faction.get("mechanic")
            rest = []
            repairs = target.owner.faction.get("repair", mech == "standard")
            helps = self.builder_style(target.owner) in ("stay", "stay_single", "enter")
            for u in units:
                if u.is_worker and helps and not target.complete:
                    u.set_order({"type": "construct", "b": target}, queue)
                elif u.is_worker and repairs and target.complete and target.hp < target.max_hp:
                    u.set_order({"type": "repair", "b": target}, queue)
                elif u.is_worker and u.carry_amt and u.carry_kind in target.defn.get("dropoff", ()):
                    if u.last_res is not None and u.last_res.alive:
                        u.set_order({"type": "gather", "res": u.last_res, "state": "return"}, queue)
                    else:
                        rest.append(u)
                else:
                    rest.append(u)
            if rest:
                self.cmd_move(rest, x, y, queue=queue)
            return
        self.cmd_move(units, x, y, queue=queue)

    def cmd_stop(self, units):
        for u in units:
            if u.is_unit:
                u.set_order(None)

    def cmd_hold(self, units):
        for u in units:
            if u.is_unit:
                u.set_order({"type": "hold"})

    def cmd_build(self, p, bdef, tx, ty, worker, queue=False):
        if bdef.get("kind") != "building":
            return False
        if bdef.get("faction") != p.faction["id"]:
            ally = self.data.factions.get(getattr(p, "ally", None), {})
            if not ally.get("ally") or bdef["id"] not in ally.get("ally_pack", {}).get("buildings", ()):
                return False
        if worker is None or not worker.alive or worker.owner is not p or worker.inside is not None:
            return False
        if not worker.can_build:
            self.notify(p, f"{worker.name} nem épít.", (255, 120, 90))
            return False
        ok, reason = self.can_place(p, bdef, tx, ty)
        if not ok:
            self.notify(p, f"Nem lehet építeni: {reason}", (255, 120, 90))
            return False
        if not p.can_afford(bdef["cost"]):
            self._notify_missing(p, bdef["cost"])
            return False
        p.pay(bdef["cost"])
        resume = None
        if worker.order and worker.order["type"] == "gather":
            resume = {"type": "gather", "res": worker.order["res"]}
        worker.set_order({"type": "build", "bdef": bdef, "tx": tx, "ty": ty, "resume": resume,
                          "reserved_build_cost": dict(bdef["cost"])}, queue)
        return True

    def _notify_missing(self, p, cost):
        names = self.data.game["resources"]
        miss = ", ".join(p.res_name(k, names) for k in p.missing(cost))
        self.notify(p, f"Nincs elég nyersanyag: {miss}", (255, 160, 90))

    def producers(self, p, uid):
        return [b for b in self.buildings
                if b.owner is p and b.complete and b.alive and uid in b.defn.get("produces", ())]

    def hero_count(self, p):
        n = sum(1 for u in self.units if u.alive and u.hero and u.owner is p)
        return n + sum(1 for b in self.buildings if b.owner is p for it in b.queue
                       if self.data.defs.get(it["id"], {}).get("hero"))

    def hero_busy(self, p, uid):
        """Egy hősből egyszerre csak egy lehet (élő vagy képzés alatt álló)."""
        if any(u.alive and u.owner is p and u.defn["id"] == uid for u in self.units):
            return True
        return any(it["id"] == uid for b in self.buildings if b.owner is p for it in b.queue)

    def cmd_train(self, p, uid, building=None):
        udef = self.data.defs[uid]
        if udef.get("hero") and self.hero_busy(p, uid):
            self.notify(p, f"{udef['name']} már a seregedben van.", (255, 160, 90))
            return False
        if udef.get("hero") and self.hero_count(p) >= 3:
            self.notify(p, "Legfeljebb három hősöd lehet egyszerre.", (255, 160, 90))
            return False
        blds = self.producers(p, uid)
        if building is not None:
            blds = [b for b in blds if b is building]
        if not blds:
            self.notify(p, "Nincs megfelelő épület.", (255, 160, 90))
            return False
        miss = self.missing_requirements(p, udef)
        if miss:
            self.notify(p, "Hiányzik: " + ", ".join(miss), (255, 160, 90))
            return False
        if not p.can_afford(udef["cost"]):
            self._notify_missing(p, udef["cost"])
            return False
        left = self.unit_limit_left(p, udef)
        if left is not None and left <= 0:
            lim = udef["per_building"]
            self.notify(p, f"Minden {self.data.defs[lim['building']]['name']} legfeljebb {lim['max']} "
                           f"{udef['name']} egységet tarthat el – építs még egyet.", (255, 160, 90))
            return False
        sup = udef.get("supply", 1)
        if p.supply_used + sup > p.supply_cap + 1e-6:
            how = p.faction.get("supply_desc") or self.data.defs[p.faction["supply_building"]]["name"]
            self.notify(p, f"Elérted a létszámkorlátot – növeld: {how}", (255, 160, 90))
            return False
        cands = []
        for b in blds:
            if not b.powered:
                continue
            if b.defn.get("parallel"):
                if b.larva > 0:
                    cands.append((-b.larva, b))
            elif len(b.queue) < 5:
                cands.append((len(b.queue), b))
        if not cands:
            if any(b.defn.get("parallel") for b in blds):
                self.notify(p, "Nincs szabad lárva.", (255, 160, 90))
            elif not any(b.powered for b in blds):
                self.notify(p, "Az épület nincs energiamezőben!", (255, 160, 90))
            else:
                self.notify(p, "A gyártósor megtelt.", (255, 160, 90))
            return False
        cands.sort(key=lambda c: c[0])
        b = cands[0][1]
        p.pay(udef["cost"])
        if b.defn.get("parallel"):
            b.larva -= 1
        t = udef["time"]
        ttm = p.rules.get("train_time_mult")
        if ttm and (not ttm.get("tags") or set(ttm["tags"]) & set(udef.get("tags", ()))):
            t *= ttm["mult"]
        st = p.hero_states.get(uid)
        if udef.get("hero") and st:  # újraélesztés: szinttel arányos idő
            t = t * (0.6 + 0.12 * st.get("level", 1))
        b.queue.append({"kind": "unit", "id": uid, "time": t, "progress": 0.0,
                        "cost": dict(udef["cost"]), "supply": sup})
        p.supply_used += sup
        return True

    def unit_limit_left(self, p, udef):
        """Épületenkénti darabkorlát (pl. Emukarámonként 5 emu): hány képezhető még; None = nincs korlát."""
        lim = udef.get("per_building")
        if not lim:
            return None
        n_b = sum(1 for b in self.buildings if b.owner is p and b.alive and b.complete
                  and b.defn["id"] == lim["building"])
        have = sum(1 for u in self.units if u.alive and u.owner is p and u.defn["id"] == udef["id"])
        have += sum(1 for b in self.buildings if b.owner is p for it in b.queue if it["id"] == udef["id"])
        return n_b * lim["max"] - have

    def research_blocked(self, p, rid):
        """Kizáró kutatás: ha ugyanabból a csoportból már választott (kész vagy folyamatban), a többi tiltott."""
        grp = self.data.researches[rid].get("exclusive")
        if not grp:
            return None
        for o in sorted(p.researched | p.researching):
            if o != rid and self.data.researches.get(o, {}).get("exclusive") == grp:
                return o
        return None

    def cmd_research(self, p, rid):
        r = self.data.researches[rid]
        if rid in p.researched or rid in p.researching:
            return False
        other = self.research_blocked(p, rid)
        if other:
            self.notify(p, f"Már ezt választottad: {self.data.researches[other]['name']} – a többi nem fejleszthető.",
                        (255, 160, 90))
            return False
        blds = [b for b in self.buildings if b.owner is p and b.complete and b.powered
                and rid in b.defn.get("researches", ())]
        if not blds:
            self.notify(p, "Nincs megfelelő (működő) épület.", (255, 160, 90))
            return False
        miss = self.missing_requirements(p, r)  # pl. a Szoborpark isten-fejlesztéséhez álljon a szobor
        if miss:
            self.notify(p, "Hiányzik: " + ", ".join(miss), (255, 160, 90))
            return False
        if not p.can_afford(r["cost"]):
            self._notify_missing(p, r["cost"])
            return False
        b = min(blds, key=lambda b: len(b.queue))
        if len(b.queue) >= 5:
            self.notify(p, "A gyártósor megtelt.", (255, 160, 90))
            return False
        p.pay(r["cost"])
        p.researching.add(rid)
        b.queue.append({"kind": "research", "id": rid, "time": r["time"], "progress": 0.0,
                        "cost": dict(r["cost"]), "supply": 0})
        return True

    def cancel_item(self, b, idx):
        if not (0 <= idx < len(b.queue)):
            return
        item = b.queue.pop(idx)
        b.owner.supply_used = max(0, b.owner.supply_used - item.get("supply", 0))
        b.owner.refund(item["cost"])
        if item["kind"] == "research":
            b.owner.researching.discard(item["id"])
        elif b.defn.get("parallel"):
            b.larva += 1

    def cancel_unit_type(self, p, uid):
        for b in self.buildings:
            if b.owner is p:
                for i in range(len(b.queue) - 1, -1, -1):
                    if b.queue[i]["id"] == uid:
                        self.cancel_item(b, i)
                        return True
        return False

    def cmd_rally(self, buildings, x, y, target=None):
        for b in buildings:
            if b.is_building:
                b.rally = (x, y, target)

    def cmd_cast(self, units, ab_id, x, y, target=None):
        kind = self.data.abilities.get(ab_id, {}).get("type")
        if kind not in ("area", "bolt", "blink", "raise") or (kind == "bolt" and is_sheltered(target)):
            return False
        cands = [u for u in units if u.alive and u.is_unit and u.has_ability(ab_id)
                 and u.ab_ok(u.ab(self, ab_id))]
        if not cands:
            if units:
                self.notify(units[0].owner, "Nincs elég energia.", (255, 160, 90))
            return False
        ready = [u for u in cands if u.ab_ready.get(ab_id, 0) <= self.time]
        if not ready:
            self.notify(units[0].owner, "A képesség még töltődik.", (255, 160, 90))
            return False
        u = min(ready, key=lambda u: dmath.hypot(u.x - x, u.y - y))
        o = {"type": "cast", "ab": ab_id, "x": x, "y": y}
        if target is not None and (getattr(target, "is_unit", False) and u.owner.hostile(target.owner)
                                   or getattr(target, "defn", {}).get("spike_tile")):
            o["target"] = target
        u.set_order(o)
        return True

    # ---- hősök, tárgyak ------------------------------------------------
    def cmd_learn(self, u, ab_id):
        return heroes.learn(self, u, ab_id)

    def cmd_use_item(self, u, slot):
        return heroes.use_item(self, u, slot)

    def cmd_drop_item(self, u, slot):
        return heroes.drop_from_slot(self, u, slot)

    def cmd_pickup(self, units, gi, queue=False):
        hs = [u for u in units if u.is_unit and u.hero and u.alive]
        if not hs or not gi.alive:
            return False
        h = min(hs, key=lambda u: dmath.hypot(u.x - gi.x, u.y - gi.y))
        h.set_order({"type": "pickup", "item": gi}, queue)
        return True

    def item_at(self, x, y, r=14):
        best, bestd = None, r
        for gi in self.items:
            d = dmath.hypot(gi.x - x, gi.y - y)
            if gi.alive and d <= bestd:
                best, bestd = gi, d
        return best

    def cmd_toggle(self, units, ab_id):
        us = [u for u in units if u.is_unit and u.has_ability(ab_id)]
        on = any(ab_id not in u.toggles for u in us)
        chant = self.data.abilities.get(ab_id, {}).get("type") == "chant"
        for u in us:
            if on:
                if chant:  # egy egység egyszerre csak egy éneket énekelhet
                    for t in list(u.toggles):
                        if self.data.abilities.get(t, {}).get("type") == "chant":
                            u.toggles.discard(t)
                if ab_id not in u.toggles:
                    if ab_id == "oon_kapuorzes":
                        u.gate_guard_slow_used = False
                    self.sfx("ability_" + ab_id, u)
                u.toggles.add(ab_id)
            else:
                u.toggles.discard(ab_id)

    def cmd_summon(self, units, ab_id):
        for u in units:
            if u.alive and u.is_unit and u.inside is None and u.has_ability(ab_id) and u.summon(self, ab_id):
                return True
        if units:
            self.notify(units[0].owner, "Nincs elég energia.", (255, 160, 90))
        return False

    def cmd_buff(self, units, ab_id):
        for u in units:
            if u.is_unit and u.has_ability(ab_id):
                u.activate_buff(self, ab_id)
