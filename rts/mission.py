"""Kézzel tervezett küldetéspályák: alaprajz (terep, utak, folyók, erdők, nyersanyagok, kezdőhelyek), elnevezett
pontok, díszletek és ártalmatlan háttérszereplők.

A küldetés JSON „layout” mezője:

    "layout": {
      "size": [64, 56], "seed": 4, "base": "grass",
      "points": {"gyulekezo": [32, 50], "rev": [9, 21], ...},          # csempe-koordináta
      "paint": [{"t": "water", "line": ["a", "b", [40, 26]], "w": 3},  # folyó / szikla / föld sáv vonal mentén
                {"t": "rock", "area": ["fennsik", 5]},                  # vagy kör alakú folt
                {"t": "dirt", "line": [...], "w": 2}],
      "clear": [{"line": ["gazlo_d", "gazlo_e"], "w": 2}],             # járható átjáró (gázló, gáttető, út)
      "forest": [{"area": ["erdo1", 6], "density": 0.8}, {"border": 3}],
      "resources": [{"kind": "stone", "at": "banya"}],
      "starts": ["gyulekezo", "eszak"],                                 # játékosonként a kezdőhely
      "decor": [{"id": "wul_kereghaz", "at": "rev_falu"}],             # díszletépület (nem egység, nem épület)
      "ambient": [{"id": "wul_emu", "n": 3, "at": "rev_falu", "path": ["a", "b"], "shelter": "c"}]
    }

A díszlet csak látvány és terepakadály: nincs kijelölés, életcsík, foglalás, termelés, célpont; az AI nem látja,
nem számít bele az épületszámlálásba, a győzelembe és a felemelkedésbe. A háttérszereplő ugyanígy nem egység:
nem irányítható, nem támadható, a területi sebzés sem éri, nem állja el az utat; csak sétál vagy ácsorog, és harc
közelében a menedékpontjára húzódik.
"""
import math
import random

from . import dmath
from .world import DIRT, FREE, GRASS, ROCK, TILE, WATER, B_TERRAIN, GameMap, Resource

T_CODES = {"grass": GRASS, "dirt": DIRT, "water": WATER, "rock": ROCK}
_DECOR = None


def decor_catalog():
    """Saját díszletek (data/decor.json): nagy sziklák, romok, roncsok, csontvázak – lábnyom, változatok."""
    global _DECOR
    if _DECOR is None:
        import json
        import os
        from .data import ROOT
        try:
            with open(os.path.join(ROOT, "data", "decor.json"), encoding="utf-8") as f:
                _DECOR = json.load(f).get("decor", {})
        except (OSError, ValueError):
            _DECOR = {}
    return _DECOR


def decor_size(data, did):
    """Díszlet lábnyoma csempében: épület-díszletnél az épületé, saját díszletnél a katalógusé; None, ha nincs."""
    b = data.defs.get(did)
    if b is not None and "size" in b:
        return tuple(b["size"])
    d = decor_catalog().get(did)
    if not d:
        return None
    import os
    from .data import ROOT
    if not os.path.isfile(os.path.join(ROOT, "assets", "sprites", "decor", did + ".json")):
        return None  # a képe még nem készült el: a pálya kihagyja
    return tuple(d["size"])


def tile_of(points, p):
    """Pont: név a points-ból, [x, y] csempe, vagy [fx, fy] 0..1 arány (régi küldetések)."""
    if isinstance(p, str):
        return tuple(points[p])
    return tuple(p)


def world_xy(g, p):
    """Küldetés-pont -> világ-pixel (a csempe közepe). Elfogadja a régi 0..1 arányú megadást is."""
    pts = getattr(g, "mission_points", {}) or {}
    if isinstance(p, str):
        x, y = pts[p]
        return (x + 0.5) * TILE, (y + 0.5) * TILE
    x, y = p
    if 0 <= x <= 1 and 0 <= y <= 1 and isinstance(x, float):
        return x * g.map.w * TILE, y * g.map.h * TILE
    return (x + 0.5) * TILE, (y + 0.5) * TILE


def _line_cells(pts, w):
    """A törött vonal mentén w szélességű sáv csempéi."""
    out = set()
    r = w / 2.0
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        n = max(1, int(dmath.hypot(x1 - x0, y1 - y0) * 2))
        for i in range(n + 1):
            cx, cy = x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n
            for y in range(int(cy - r - 1), int(cy + r + 2)):
                for x in range(int(cx - r - 1), int(cx + r + 2)):
                    if dmath.hypot(x + 0.5 - cx - 0.5, y + 0.5 - cy - 0.5) <= r:
                        out.add((x, y))
    return sorted(out)   # rendezett: a véletlenszám-húzás sorrendje ne a set belső sorrendjén múljon (C#-port)


def _area_cells(c, rad, rng=None, rough=0.0):
    out = set()
    for y in range(int(c[1] - rad - 2), int(c[1] + rad + 3)):
        for x in range(int(c[0] - rad - 2), int(c[0] + rad + 3)):
            d = dmath.hypot(x - c[0], y - c[1])
            lim = rad + (rng.uniform(-rough, rough) if rng and rough else 0)
            if d <= lim:
                out.add((x, y))
    return sorted(out)   # rendezett: a véletlenszám-húzás sorrendje ne a set belső sorrendjén múljon (C#-port)


def build_layout_map(data, layout):
    W, H = layout.get("size", [64, 64])
    rng = random.Random(layout.get("seed", 1))
    m = GameMap(W, H)
    base = T_CODES.get(layout.get("base", "grass"), GRASS)
    for i in range(W * H):
        m.terrain[i] = base
        if base in (WATER, ROCK):
            m.block[i] = B_TERRAIN
    pts = {k: tuple(v) for k, v in layout.get("points", {}).items()}
    rdefs = data.game["resources"]

    def P(p):
        return tile_of(pts, p)

    def cells(spec):
        if "line" in spec:
            return _line_cells([P(p) for p in spec["line"]], spec.get("w", 2))
        a = spec["area"]
        return _area_cells(P(a[0]), a[1], rng, spec.get("rough", 0.8))

    def set_t(x, y, t):
        if m.inb(x, y):
            i = y * W + x
            m.terrain[i] = t
            m.block[i] = B_TERRAIN if t in (WATER, ROCK) else FREE

    for sp in layout.get("paint", []):
        t = T_CODES[sp["t"]]
        for x, y in cells(sp):
            set_t(x, y, t)
    keep = set()   # amit fa és nyersanyag sem foglalhat (utak, gázlók, kezdőhelyek, pontok)
    for sp in layout.get("clear", []):
        t = T_CODES.get(sp.get("t", "dirt"), DIRT)
        for x, y in cells(sp):
            set_t(x, y, t)
            keep.add((x, y))
    for name in layout.get("starts", []):
        sx, sy = P(name)
        for x, y in _area_cells((sx, sy), 5):
            if m.inb(x, y) and m.terrain[y * W + x] in (WATER, ROCK):
                set_t(x, y, GRASS)
            keep.add((x, y))
    for (px, py) in pts.values():
        for x, y in _area_cells((px, py), 1.6):
            keep.add((x, y))
    m.starts = [P(s) for s in layout.get("starts", [])] or [(W // 2, H - 8), (W // 2, 7)]
    for sp in layout.get("resources", []):
        x, y = P(sp["at"])
        size = sp.get("size", 2)
        if size <= 0 or any(not m.inb(xx, yy) or m.res_grid[yy * W + xx] is not None
                            for yy in range(y, y + size) for xx in range(x, x + size)):
            raise ValueError(f"Érvénytelen nyersanyaglelőhely: {sp['kind']} ({x}, {y})")
        for yy in range(y, y + size):
            for xx in range(x, x + size):
                set_t(xx, yy, GRASS)
        m.add_resource(Resource(sp["kind"], x, y, size, sp.get("amount", rdefs[sp["kind"]]["node_amount"])))

    def tree(x, y):
        if (x, y) in keep or not m.inb(x, y):
            return
        i = y * W + x
        if m.terrain[i] in (GRASS, DIRT) and m.block[i] == FREE:
            m.add_resource(Resource("wood", x, y, 1, rdefs["wood"]["node_amount"], rng.randrange(12)))
    for sp in layout.get("forest", []):
        if "border" in sp:
            b = sp["border"]
            for y in range(H):
                for x in range(W):
                    d = min(x, y, W - 1 - x, H - 1 - y)
                    if d < b and rng.random() < 0.9 - d * 0.15:
                        tree(x, y)
            continue
        dens = sp.get("density", 0.75)
        for x, y in cells(sp):
            if rng.random() < dens:
                tree(x, y)
    m.layout_decor = []
    for sp in layout.get("decor", []):  # a díszlet talpa terepakadály (csak járhatóság)
        sz = decor_size(data, sp["id"])
        if sz is None:
            continue
        w, h = sz
        cx, cy = P(sp["at"])
        tx, ty = cx - w // 2, cy - h // 2
        for yy in range(ty, ty + h):
            for xx in range(tx, tx + w):
                if m.inb(xx, yy):
                    r = m.res_grid[yy * W + xx]
                    if r is not None:
                        m.remove_resource(r)
                    m.block[yy * W + xx] = B_TERRAIN
        m.layout_decor.append({"id": sp["id"], "tx": tx, "ty": ty, "w": w, "h": h,
                               "frame": sp.get("frame", 0), "ruin": bool(sp.get("ruin"))})
    m.layout_points = pts
    return m


class Decor:
    """Díszletépület: épületgrafika tereptárgyként (csak rajzolódik és utat zár)."""
    is_unit = is_building = is_resource = False

    def __init__(self, spec):
        self.defn_id = spec["id"]
        self.tx, self.ty, self.w, self.h = spec["tx"], spec["ty"], spec["w"], spec["h"]
        self.frame = spec.get("frame", 0)
        self.ruin = spec.get("ruin", False)
        self.rect = (self.tx * TILE, self.ty * TILE, self.w * TILE, self.h * TILE)
        self.x = (self.tx + self.w / 2) * TILE
        self.y = (self.ty + self.h / 2) * TILE


class Actor:
    """Ártalmatlan háttérszereplő: egy meglévő egység képe, saját kis viselkedéssel (járőr vagy ácsorgás)."""
    is_unit = is_building = is_resource = False

    def __init__(self, g, defn_id, x, y, path=None, shelter=None, idle=False, seed=0):
        self.defn_id = defn_id
        self.x, self.y = x, y
        self.home = (x, y)
        self.path = path or []
        self.leg = 0
        self.shelter = shelter
        self.idle = idle or not self.path
        self.angle = (seed * 1.7) % 6.283
        self.anim = (seed * 0.37) % 3
        self.moving = False
        self.wait = 0.0
        self.rng = random.Random(seed)
        self.speed = g.data.defs.get(defn_id, {}).get("speed", 2.5) * TILE * 0.55

    def update(self, g, dt, danger):
        self.anim += dt
        goal = None
        if danger and self.shelter:
            goal = self.shelter
        elif self.wait > 0:
            self.wait -= dt
        elif not self.idle and self.path:
            goal = self.path[self.leg % len(self.path)]
        elif self.idle and self.rng.random() < dt * 0.15:  # ácsorgás: néha megfordul, egymás felé néz
            self.angle = self.rng.uniform(0, 6.283)
        self.moving = False
        if goal is None:
            return
        dx, dy = goal[0] - self.x, goal[1] - self.y
        d = dmath.hypot(dx, dy)
        if d < 6:
            if not (danger and self.shelter):
                self.leg += 1
                self.wait = self.rng.uniform(1.0, 4.0)
            return
        step = min(d, self.speed * dt)
        nx, ny = self.x + dx / d * step, self.y + dy / d * step
        if g.map.passable(int(nx // TILE), int(ny // TILE)):
            self.x, self.y = nx, ny
        else:  # akadály: oldalt kerül
            self.angle += 0.6
            self.x += dmath.cos(self.angle) * step * 0.5
            self.y += dmath.sin(self.angle) * step * 0.5
        self.angle = dmath.atan2(dy, dx)
        self.moving = True


def setup_layout(g, layout):
    """A játék létrejötte után: pontok, díszletek, háttérszereplők a játékba."""
    m = g.map
    g.mission_points = dict(getattr(m, "layout_points", {}))
    g.decor = [Decor(s) for s in getattr(m, "layout_decor", [])]
    g.actors = []
    k = 0
    for sp in layout.get("ambient", []):
        cx, cy = world_xy(g, sp["at"])
        path = [world_xy(g, p) for p in sp.get("path", [])]
        shelter = world_xy(g, sp["shelter"]) if sp.get("shelter") else None
        for i in range(sp.get("n", 1)):
            a = i * 2.39996
            r = (0 if i == 0 else 18 + 10 * math.sqrt(i)) * sp.get("spread", 1.0)
            x, y = cx + dmath.cos(a) * r, cy + dmath.sin(a) * r
            pth = [(px + dmath.cos(a) * r * 0.6, py + dmath.sin(a) * r * 0.6) for px, py in path]
            g.actors.append(Actor(g, sp["id"], x, y, pth, shelter, idle=sp.get("mode") == "idle", seed=k * 31 + 7))
            k += 1


def update_actors(g, dt):
    acts = getattr(g, "actors", None)
    if not acts:
        return
    me = g.players[0]
    if getattr(g, "director", None) is None:  # csatapálya: bármely sereg (nem a semleges szörnyek) elől elhúzódnak
        hostile = [u for u in g.units if u.alive and u.attack and not getattr(u.owner, "neutral", False)]
    else:
        hostile = [u for u in g.units if u.alive and u.owner is not me and me.hostile(u.owner)]
    for a in acts:
        danger = any(abs(u.x - a.x) < 6 * TILE and abs(u.y - a.y) < 6 * TILE for u in hostile)
        a.update(g, dt, danger)


def reshape(g, spec):
    """Futás közbeni terepváltozás: {"paint": [...], "clear": [...]} – ugyanaz a nyelv, mint az alaprajzé.
    Az érintett csempékről a fák eltűnnek; a rajta álló egységek a legközelebbi szabad helyre kerülnek."""
    m = g.map
    W = m.w
    pts = getattr(g, "mission_points", {})

    def P(p):
        return tile_of(pts, p)

    def cells(sp):
        if "line" in sp:
            return _line_cells([P(p) for p in sp["line"]], sp.get("w", 2))
        a = sp["area"]
        return _area_cells(P(a[0]), a[1])
    changed = set()
    for key, default in (("paint", None), ("clear", "dirt")):
        for sp in spec.get(key, []):
            t = T_CODES[sp.get("t", default)]
            for x, y in cells(sp):
                if not m.inb(x, y):
                    continue
                i = y * W + x
                r = m.res_grid[i]
                if r is not None and getattr(r, "mine", None) is not None and r.mine.alive:
                    continue
                if r is None and m.block[i] not in (FREE, B_TERRAIN):
                    continue
                if r is not None:
                    m.remove_resource(r)
                m.terrain[i] = t
                m.block[i] = B_TERRAIN if t in (WATER, ROCK) else FREE
                changed.add((x, y))
    if not changed:
        return
    m.version += 1
    g.terrain_version = getattr(g, "terrain_version", 0) + 1
    for u in g.units:  # a víz / omlás alá került egységek kimenekülnek
        if u.alive and not u.flying and (int(u.x // TILE), int(u.y // TILE)) in changed and \
                not m.passable(int(u.x // TILE), int(u.y // TILE)):
            spot = m.nearest_free(int(u.x // TILE), int(u.y // TILE))
            if spot:
                u.x, u.y = (spot[0] + 0.5) * TILE, (spot[1] + 0.5) * TILE
                u.path = []
                u.path_goal = None
