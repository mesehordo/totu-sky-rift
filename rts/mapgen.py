"""Csata- és többjátékos-pályák generálása pályaleírásból (data/maps/*.json), 2–8 játékosra.

A pálya szimmetrikus, így minden kezdőhely egyformán jó:
* ``ring``    – a kezdőhelyek körben, N-szeres forgásszimmetria (2–8 játékos),
* ``corners`` – négy sarok, 90°-os forgatás,
* ``rows``    – két csapat egymással szemben (fent / lent), tükrözés a vízszintes középvonalra.

A domborzati elemeket az alaptartományban helyezzük el, és minden szimmetria-transzformációval
megismételjük. A pályaleírás mezői: name, desc, players, size, layout, theme, features.
"""
import math
import os
import json
import random

from .data import ROOT
from .world import DIRT, FREE, GRASS, ROCK, WATER, B_TERRAIN, GameMap, Resource, tree_variant

MAP_DIR = os.path.join(ROOT, "data", "maps")


def list_maps(data=None):
    """A pályák listája (sorrend: játékosszám, majd név). A modok saját pályái is."""
    out = []
    dirs = [MAP_DIR]
    mods = os.path.join(ROOT, "mods")
    dirs.extend(os.path.join(mods, d, "maps") for d in getattr(data, "mod_ids", ()))
    for d in dirs:
        if not os.path.isdir(d):
            continue
        for fn in sorted(os.listdir(d)):
            if fn.endswith(".json"):
                with open(os.path.join(d, fn), encoding="utf-8") as f:
                    md = json.load(f)
                md.setdefault("id", fn[:-5])
                out.append(md)
    out.sort(key=lambda m: (m.get("order", 99), m["players"], m["id"]))
    return out


def get_map(map_id, data=None):
    for m in list_maps(data):
        if m["id"] == map_id:
            return m
    return None


class _ValueNoise:
    """Simított értékzaj (determinisztikus) – összefüggő erdő- és talajfoltokhoz."""

    def __init__(self, seed):
        self.seed = seed

    def _h(self, ix, iy):
        n = (ix * 374761393 + iy * 668265263 + self.seed * 2147483647) & 0xFFFFFFFF
        n = ((n ^ (n >> 13)) * 1274126177) & 0xFFFFFFFF
        return (n & 0xFFFF) / 65535.0

    def at(self, x, y):
        ix, iy = math.floor(x), math.floor(y)
        fx, fy = x - ix, y - iy
        sx, sy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
        a, b = self._h(ix, iy), self._h(ix + 1, iy)
        c, d = self._h(ix, iy + 1), self._h(ix + 1, iy + 1)
        return (a + (b - a) * sx) * (1 - sy) + (c + (d - c) * sx) * sy


def _fold(layout, players, x, y, cx, cy):
    """Zaj-koordináta a szektoron belül tükrözve, hogy a szektorhatáron ne legyen varrat."""
    if layout == "rows":  # pontszimmetria: az alsó fél a felső tükörképe
        return (x, y) if y < (2 * cy + 1) / 2 else (2 * cx - x, 2 * cy - y)
    k = 4 if layout == "corners" else max(2, players)
    a0 = math.pi if k == 2 else 0.0
    S = 2 * math.pi / k
    r = math.hypot(x - cx, y - cy)
    a = ((math.atan2(y - cy, x - cx) - a0) % (2 * math.pi)) % S  # szektoron belül (minden szektorban ugyanúgy)
    a = min(a, S - a)
    return cx + r * math.cos(a), cy + r * math.sin(a)


def _in_fundamental(layout, players, x, y, cx, cy, W, H, feats):
    """Az (x, y) mező a szimmetria alaptartományában van-e (onnan másolódik a többi szektorba)."""
    if layout == "rows":
        return y < H / 2
    k = 4 if layout == "corners" else max(2, players)
    a0 = math.pi if k == 2 else 0.0
    a = (math.atan2(y - cy, x - cx) - a0) % (2 * math.pi)
    return a < 2 * math.pi / k


class _Sym:
    """Szimmetria-transzformációk: (x, y) -> [(x', y'), ...] játékosonként."""

    def __init__(self, layout, n, W, H):
        self.layout, self.n, self.W, self.H = layout, n, W, H
        self.cx, self.cy = (W - 1) / 2, (H - 1) / 2

    def apply(self, x, y):
        if self.layout == "rows":
            return [(x, y), (self.W - 1 - x, self.H - 1 - y)]
        k = 4 if self.layout == "corners" else max(2, self.n)
        out = []
        dx, dy = x - self.cx, y - self.cy
        for i in range(k):
            a = 2 * math.pi * i / k
            ca, sa = math.cos(a), math.sin(a)
            out.append((self.cx + dx * ca - dy * sa, self.cy + dx * sa + dy * ca))
        return out


def camp_spots(starts, cx, cy):
    """Szörnytáborok helye (és a gazdag bővítéseké): kezdőhelyenként egy könnyű (1) és egy közepes (2) tábor,
    a szomszédos kezdőhelyek között két erősebb (3/4), középen a főnök (5). A szomszédokat szög szerint
    rendezzük, így a párok iránya is körbeforog: a tábor-elrendezés minden kezdőhelyről ugyanolyan."""
    spots = []
    for sx, sy in starts:
        dx, dy = cx - sx, cy - sy
        d = math.hypot(dx, dy) or 1
        ux, uy = dx / d, dy / d
        spots.append((sx + ux * 15 - uy * 7, sy + uy * 15 + ux * 7, 1))
        spots.append((sx + ux * 17 + uy * 9, sy + uy * 17 - ux * 9, 2))
    n = len(starts)
    if n == 2:
        pairs = [(0, 1)]
    else:
        order = sorted(range(n), key=lambda k: math.atan2(starts[k][1] - cy, starts[k][0] - cx))
        pairs = [(order[k], order[(k + 1) % n]) for k in range(n)]
    for i, j in pairs:
        mx, my = (starts[i][0] + starts[j][0]) / 2, (starts[i][1] + starts[j][1]) / 2
        dx, dy = starts[j][0] - starts[i][0], starts[j][1] - starts[i][1]
        d = math.hypot(dx, dy) or 1
        px, py = -dy / d, dx / d
        spots.append((mx + px * 14, my + py * 14, 3))
        spots.append((mx - px * 14, my - py * 14, 4 if n == 2 else 3))
    spots.append((cx, cy, 5))
    return spots


def _place_rel(starts, cx, cy, spec):
    """Elhelyezés a kezdőhelyhez képest: „base” (d: a közép felé, s: oldalra), „pair” (két szomszéd között,
    d: kifelé a középtől), „centre” (a középtől, kezdőhelyenként elforgatva). Minden kezdőhelyre / párra
    ugyanúgy, így szimmetrikus."""
    at = spec.get("at", "base")
    d, s = float(spec.get("d", 0)), float(spec.get("s", 0))
    out = []
    n = len(starts)
    if at == "centre":
        a0 = math.atan2(starts[0][1] - cy, starts[0][0] - cx) if starts else 0.0
        for k in range(n if (d or s) else 1):
            a = a0 + 2 * math.pi * k / n
            ux, uy = math.cos(a), math.sin(a)
            out.append((cx + ux * d - uy * s, cy + uy * d + ux * s))
        return out
    if at == "pair":
        order = sorted(range(n), key=lambda k: math.atan2(starts[k][1] - cy, starts[k][0] - cx))
        prs = [(order[0], order[1])] if n == 2 else [(order[k], order[(k + 1) % n]) for k in range(n)]
        for i, j in prs:
            mx, my = (starts[i][0] + starts[j][0]) / 2, (starts[i][1] + starts[j][1]) / 2
            ox, oy = mx - cx, my - cy
            dd = math.hypot(ox, oy)
            if dd < 1:  # két játékos: a felező merőlegesen, mindkét irányba
                ox, oy = -(starts[j][1] - starts[i][1]), starts[j][0] - starts[i][0]
                dd = math.hypot(ox, oy) or 1
            ux, uy = ox / dd, oy / dd
            out.append((mx + ux * d - uy * s, my + uy * d + ux * s))
            if n == 2:
                out.append((mx - ux * d + uy * s, my - uy * d - ux * s))
        return out
    for sx, sy in starts:
        ux, uy = cx - sx, cy - sy
        dd = math.hypot(ux, uy) or 1
        ux, uy = ux / dd, uy / dd
        out.append((sx + ux * d - uy * s, sy + uy * d + ux * s))
    return out


def generate(data, md, seed, n_players=None):
    rng = random.Random(seed)
    tsalt = rng.randrange(1 << 20)  # a fafajták keverése pályánként más
    W = H = int(md.get("size", 96))
    n = int(n_players or md["players"])
    layout = md.get("layout", "ring")
    feats = md.get("features", {})
    rdefs = data.game["resources"]
    m = GameMap(W, H)
    m.theme = md.get("theme", "forest")
    sym = _Sym(layout, md["players"], W, H)
    cx, cy = sym.cx, sym.cy

    # ---- kezdőhelyek
    if layout == "rows":
        k = max(1, md["players"] // 2)
        top = [(int(W * (i + 1) / (k + 1)), 12) for i in range(k)]
        starts = top + [(W - 1 - x, H - 1 - y) for x, y in top]
        m.teams_hint = [0] * k + [1] * k
    elif layout == "corners":
        starts = [(int(x), int(y)) for x, y in sym.apply(12, 12)]
    else:
        r0 = min(W, H) * float(feats.get("start_radius", 0.38))
        a0 = float(feats.get("start_angle", 225)) * math.pi / 180
        starts = [(int(round(x)), int(round(y))) for x, y in sym.apply(cx + math.cos(a0) * r0, cy + math.sin(a0) * r0)]
    m.starts = starts

    def near_start(x, y, d):
        return any(math.hypot(x - sx, y - sy) < d for sx, sy in starts)

    def paint(x, y, t):
        for px, py in sym.apply(x, y):
            ix, iy = math.floor(px + 0.5), math.floor(py + 0.5)
            if m.inb(ix, iy):
                m.terrain[iy * W + ix] = t

    def blob(bx, by, r, t, keep=0):
        for y in range(int(by - r - 1), int(by + r + 2)):
            for x in range(int(bx - r - 1), int(bx + r + 2)):
                if math.hypot(x - bx, y - by) <= r + rng.uniform(-0.7, 0.7) and not (keep and near_start(x, y, keep)):
                    paint(x, y, t)

    fords_at = []   # a gázlók közepe és szélessége: a vonalak után minden szimmetrikus másolatban kitisztítjuk

    def line(x0, y0, x1, y1, t, width=1.0, keep=0, gaps=()):
        steps = max(1, int(math.hypot(x1 - x0, y1 - y0) * 2))
        for g in gaps:
            fords_at.append((x0 + (x1 - x0) * g, y0 + (y1 - y0) * g, width))
        for i in range(steps + 1):
            f = i / steps
            if any(abs(f - g) < 0.06 for g in gaps):
                continue
            x, y = x0 + (x1 - x0) * f, y0 + (y1 - y0) * f
            for yy in range(int(y - width), int(y + width) + 1):
                for xx in range(int(x - width), int(x + width) + 1):
                    if math.hypot(xx - x, yy - y) <= width and not (keep and near_start(xx, yy, keep)):
                        paint(xx, yy, t)

    def in_domain():
        """Véletlen pont az alaptartományban (egy szektor / félsík)."""
        if layout == "rows":
            return rng.uniform(2, W - 3), rng.uniform(2, H / 2)
        k = 4 if layout == "corners" else max(2, md["players"])
        a = rng.uniform(0, 2 * math.pi / k) + (math.pi if k == 2 else 0)
        r = rng.uniform(4, min(W, H) * 0.5)
        return cx + math.cos(a) * r, cy + math.sin(a) * r

    # ---- domborzat
    dnoise = _ValueNoise(rng.randrange(1 << 30))
    dthr = 0.78 - int(feats.get("dirt", 14)) * 0.006
    for y in range(H):  # nagy, organikus földfoltok (festett hatás), nem apró pöttyök
        for x in range(W):
            fx, fy = _fold(layout, md["players"], x, y, cx, cy)
            v = dnoise.at(fx / 9.0, fy / 9.0) * 0.75 + dnoise.at(fx / 3.5 + 17, fy / 3.5) * 0.25
            if v > dthr:
                m.terrain[y * W + x] = DIRT
    lakes = feats.get("lakes", [2, 2.0, 4.5])
    for _ in range(int(lakes[0])):
        x, y = in_domain()
        if not near_start(x, y, 16):
            blob(x, y, rng.uniform(lakes[1], lakes[2]), WATER, keep=13)
    ridges = feats.get("ridges", [4, 5, 11])
    for _ in range(int(ridges[0])):
        x, y = in_domain()
        ang = rng.uniform(0, 2 * math.pi)
        for _ in range(int(rng.randrange(int(ridges[1]), int(ridges[2]) + 1) * W / 96)):
            if not near_start(x, y, 13):
                paint(x, y, ROCK)
                if rng.random() < 0.5:
                    paint(x + 1, y, ROCK)
            ang += rng.uniform(-0.5, 0.5)
            x += math.cos(ang)
            y += math.sin(ang)
    for river in ([feats["river"]] if isinstance(feats.get("river"), dict) else feats.get("river") or []):
        tt = ROCK if river.get("terrain") == "rock" else WATER
        fords = [(i + 0.5) / river.get("fords", 2) for i in range(river.get("fords", 2))]
        wd = river.get("width", 1.6)
        if layout == "rows":  # vízszintes folyó a két csapat között
            line(0, cy, W / 2, cy + rng.uniform(-2, 2), tt, wd, gaps=[f * 2 for f in fords if f < 0.5])
        else:  # sugárirányú folyók a szektorok határán
            k = max(2, md["players"]) if layout == "ring" else 4
            a = 2 * math.pi / k * 0.5 + float(feats.get("start_angle", 225)) * math.pi / 180 + river.get("turn", 0)
            r0 = river.get("inner", 8)
            line(cx + math.cos(a) * r0, cy + math.sin(a) * r0, cx + math.cos(a) * W * 0.75, cy + math.sin(a) * H * 0.75,
                 tt, wd, gaps=fords)
    for fx, fy, wd in fords_at:  # a forgatott másolatokban a kerekítés néha bezárná a gázlót: mind egyforma nyitott
        for px, py in sym.apply(fx, fy):
            rr = wd + 1.6
            for yy in range(int(py - rr - 1), int(py + rr + 2)):
                for xx in range(int(px - rr - 1), int(px + rr + 2)):
                    if m.inb(xx, yy) and math.hypot(xx - px, yy - py) <= rr and m.terrain[yy * W + xx] in (WATER, ROCK):
                        m.terrain[yy * W + xx] = DIRT
    ring = feats.get("ring_wall")
    if ring:
        rr = min(W, H) * ring.get("radius", 0.2)
        gates = int(ring.get("gates", max(2, md["players"])))
        steps = int(2 * math.pi * rr * 2)
        for i in range(steps):
            a = 2 * math.pi * i / steps
            if any(abs(((a - (2 * math.pi * g / gates + 0.4)) + math.pi) % (2 * math.pi) - math.pi) < 0.17
                   for g in range(gates)):
                continue
            x, y = cx + math.cos(a) * rr, cy + math.sin(a) * rr
            ix, iy = math.floor(x + 0.5), math.floor(y + 0.5)
            for yy in (iy - 1, iy, iy + 1):
                for xx in (ix - 1, ix, ix + 1):
                    if m.inb(xx, yy):
                        m.terrain[yy * W + xx] = ROCK
    crater = feats.get("crater")
    if crater:  # középső meteorkráter: poros teknő
        cr = min(W, H) * crater.get("radius", 0.1)
        for y in range(int(cy - cr - 1), int(cy + cr + 2)):
            for x in range(int(cx - cr - 1), int(cx + cr + 2)):
                if m.inb(x, y) and math.hypot(x - cx, y - cy) <= cr + rng.uniform(-0.8, 0.8)                         and m.terrain[y * W + x] != ROCK:
                    m.terrain[y * W + x] = DIRT
    # a kezdőhelyek körül tiszta talaj
    for sx, sy in starts:
        for y in range(sy - 7, sy + 8):
            for x in range(sx - 7, sx + 8):
                if m.inb(x, y) and m.terrain[y * W + x] in (WATER, ROCK):
                    m.terrain[y * W + x] = GRASS
    for i in range(W * H):
        m.block[i] = B_TERRAIN if m.terrain[i] in (WATER, ROCK) else FREE

    # ---- nyersanyagok
    def put(kind, x, y, size=2):
        # a hívók a 2×2-es lelőhely közepe − 1 pontot adják: a lelőhely a középhez legközelebbi helyre kerül,
        # így a forgatott (szimmetrikus) másolatok csempére pontosan egyformák
        x, y = math.floor(x + size / 2), math.floor(y + size / 2)
        for yy in range(y - 1, y + size + 1):
            for xx in range(x - 1, x + size + 1):
                if m.inb(xx, yy) and m.terrain[yy * W + xx] in (WATER, ROCK):
                    m.terrain[yy * W + xx] = GRASS
                    m.block[yy * W + xx] = FREE
        m.add_resource(Resource(kind, x, y, size, rdefs[kind]["node_amount"], rng.randrange(4)))

    for sx, sy in starts:  # bázisonként ugyanaz az elrendezés, a középpont felé tájolva
        ux, uy = cx - sx, cy - sy
        d = math.hypot(ux, uy) or 1
        ux, uy = ux / d, uy / d
        vx, vy = -uy, ux
        put("stone", sx + ux * 6 + vx * 7 - 1, sy + uy * 6 + vy * 7 - 1)
        put("stone", sx + ux * 6 - vx * 7 - 1, sy + uy * 6 - vy * 7 - 1)
        put("meteor", sx - ux * 7 - 1, sy - uy * 7 - 1)
        if feats.get("expansions", True):  # terjeszkedési hely félúton
            ex, ey = sx + ux * d * 0.45 + vx * 6, sy + uy * d * 0.45 + vy * 6
            put("stone", ex - 1, ey - 1)
            put("meteor", ex + vx * 4 - 1, ey + vy * 4 - 1)
    rich = feats.get("rich")
    if rich:  # gazdag, de kitett bővítés a szomszédok közötti tábornál (a tábor őrzi)
        cs = camp_spots(starts, cx, cy)
        sel = [s for s in cs if s[2] in (3, 4)]
        if len(starts) > 2:  # páronként a középtől távolabbi hely (a peremen, a szomszéd felé nyitott)
            sel = [max(sel[k:k + 2], key=lambda s: math.hypot(s[0] - cx, s[1] - cy)) for k in range(0, len(sel), 2)]
        for x, y, _ in sel:
            ox, oy = (x - cx), (y - cy)
            dd = math.hypot(ox, oy) or 1
            vx, vy = -oy / dd, ox / dd
            put("meteor", x + vx * 4 - 1, y + vy * 4 - 1)
            put("meteor", x - vx * 4 - 1, y - vy * 4 - 1)
            if rich != "meteor":
                put("stone", x + ox / dd * 4 - 1, y + oy / dd * 4 - 1)
    if crater:
        cnt = int(crater.get("meteors", 4))
        cr = min(W, H) * crater.get("radius", 0.1) * 0.55
        for i in range(cnt):
            a = 2 * math.pi * i / cnt + 0.3
            put("meteor", cx + math.cos(a) * cr - 1, cy + math.sin(a) * cr - 1)
    else:
        put("meteor", cx - 1, cy - 1)

    # ---- erdők
    def tree_at(x, y):
        for px, py in sym.apply(x, y):
            ix, iy = math.floor(px + 0.5), math.floor(py + 0.5)
            if m.inb(ix, iy) and not near_start(ix, iy, 7) and m.terrain[iy * W + ix] in (GRASS, DIRT):
                m.add_resource(Resource("wood", ix, iy, 1, rdefs["wood"]["node_amount"], tree_variant(ix, iy, tsalt)))

    # Loria/WC2-szerű összefüggő erdőtömbök: alacsony frekvenciájú zaj az alaptartományban,
    # a pálya szélén vastag, zajos belső szélű erdőfal; a bázisok körül tisztás
    cover = float(feats.get("forest_cover", min(0.42, 0.16 + int(feats.get("forests", 14)) * 0.008)))
    noise = _ValueNoise(rng.randrange(1 << 30))
    thr = 0.95 - cover * 1.4
    wood_amt = rdefs["wood"]["node_amount"]
    for y in range(H):  # minden mezőt közvetlenül értékelünk (a tükrözött szektor-koordinátával): nincs lyuk
        for x in range(W):
            if near_start(x, y, 10):
                continue
            fx, fy = _fold(layout, md["players"], x, y, cx, cy)
            v = noise.at(fx / 11.0, fy / 11.0) * 0.7 + noise.at(fx / 4.5 + 40, fy / 4.5 + 40) * 0.3
            edge = min(x, y, W - 1 - x, H - 1 - y)
            if edge < 3 + noise.at(x / 6.0 + 90, y / 6.0) * 4 and feats.get("forest_border", True):
                v = 1.0
            if v > thr and m.terrain[y * W + x] in (GRASS, DIRT) and m.block[y * W + x] == FREE:
                m.add_resource(Resource("wood", x, y, 1, wood_amt, tree_variant(x, y, tsalt)))
    for sx, sy in starts:  # minden bázis mellett erdő
        ux, uy = cx - sx, cy - sy
        d = math.hypot(ux, uy) or 1
        ux, uy = ux / d, uy / d
        bx, by = sx - uy * 9 - ux * 3, sy + ux * 9 - uy * 3
        for yy in range(math.floor(by - 3.5), math.floor(by + 4.5)):
            for xx in range(math.floor(bx - 3.5), math.floor(bx + 4.5)):
                if m.inb(xx, yy) and math.hypot(xx - bx, yy - by) <= 3.2 and m.block[yy * W + xx] == FREE \
                        and not near_start(xx, yy, 7) and m.terrain[yy * W + xx] in (GRASS, DIRT):
                    m.add_resource(Resource("wood", xx, yy, 1, rdefs["wood"]["node_amount"], tree_variant(xx, yy, tsalt)))

    # ---- összeköttetés: minden bázisból vezessen út a középre
    centre = m.nearest_free(int(cx), int(cy), maxr=20) or (int(cx), int(cy))
    # ha bármelyik bázis el van zárva, mindegyik ugyanazt az utat kapja (a pálya szimmetrikus marad)
    def linked():
        return all(m.reachable(s, centre) for s in starts) and all(m.reachable(starts[0], s) for s in starts[1:])
    for off in (0, 3, -3, 6):
        if linked():
            break
        for s in starts:
            _carve(m, W, s, (cx, cy), off)  # a pontos középpont felé: a forgatott utak egyformák
        for yy in range(math.floor(cy - 5), math.floor(cy + 6)):  # tisztás a középen (a középső lelőhely körül)
            for xx in range(math.floor(cx - 5), math.floor(cx + 6)):
                if m.inb(xx, yy) and math.hypot(xx - cx, yy - cy) <= 4.6:
                    idx = yy * W + xx
                    r = m.res_grid[idx]
                    if r is not None and r.kind == "wood":
                        m.remove_resource(r)
                    if m.terrain[idx] in (WATER, ROCK):
                        m.terrain[idx] = DIRT
                        m.block[idx] = FREE
        centre = m.nearest_free(int(cx), int(cy), maxr=20) or centre
    _decorate(data, m, md, starts, cx, cy)
    if n < len(starts):  # kevesebb játékos: a kezdőhelyek közül választunk (a többi üres terjeszkedés)
        m.all_starts = list(starts)
    return m


def _decorate(data, m, md, starts, cx, cy):
    """Díszletépületek (tereptárgy) és ártalmatlan háttérszereplők a pályaleírás szerint, minden
    kezdőhelynél / párnál ugyanúgy. A díszlet nem zárhat el utat: ha elvágná a bázist a középtől,
    kimarad; nyersanyag és kezdőhely mellé sem kerül."""
    from .world import TILE
    W = m.w
    m.layout_decor = []
    m.layout_ambient = []
    centre = m.nearest_free(int(cx), int(cy), maxr=20) or (int(cx), int(cy))
    for sp in md.get("decor", []):
        from .mission import decor_size
        sz = decor_size(data, sp["id"])
        if sz is None:
            continue
        w, h = sz
        placed = []
        for x, y in _place_rel(starts, cx, cy, sp):
            tx, ty = int(round(x)) - w // 2, int(round(y)) - h // 2
            cells = [(xx, yy) for yy in range(ty, ty + h) for xx in range(tx, tx + w)]
            ring = [(xx, yy) for yy in range(ty - 1, ty + h + 1) for xx in range(tx - 1, tx + w + 1)]
            if not all(m.inb(xx, yy) and m.terrain[yy * W + xx] not in (WATER, ROCK) for xx, yy in ring):
                continue
            if any(m.res_grid[yy * W + xx] is not None and m.res_grid[yy * W + xx].kind != "wood"
                   for xx, yy in ring):
                continue
            if any(math.hypot(x - sx, y - sy) < 9 for sx, sy in starts):
                continue
            old = []
            for xx, yy in cells:
                old.append(m.block[yy * W + xx])
                m.block[yy * W + xx] = B_TERRAIN
            placed.append((cells, old, tx, ty))
        n_want = len(_place_rel(starts, cx, cy, sp))
        if len(placed) < n_want or not all(m.reachable(s, centre) for s in starts):
            for cells, old, _, _ in placed:  # nem fér el mindenhol, vagy utat zárna: sehova (szimmetria)
                for (xx, yy), b in zip(cells, old):
                    m.block[yy * W + xx] = b
            continue
        for cells, old, tx, ty in placed:
            for xx, yy in cells:
                r = m.res_grid[yy * W + xx]
                if r is not None:
                    m.remove_resource(r)
                m.block[yy * W + xx] = B_TERRAIN
            m.layout_decor.append({"id": sp["id"], "tx": tx, "ty": ty, "w": w, "h": h,
                                   "frame": sp.get("frame", 0), "ruin": bool(sp.get("ruin"))})

    def px(p):
        return ((p[0] + 0.5) * TILE, (p[1] + 0.5) * TILE)
    for sp in md.get("ambient", []):
        homes = _place_rel(starts, cx, cy, sp)
        paths = [_place_rel(starts, cx, cy, p) for p in sp.get("path", [])]
        shel = _place_rel(starts, cx, cy, sp["shelter"]) if sp.get("shelter") else None
        for i, (x, y) in enumerate(homes):
            t = m.nearest_free(int(x), int(y), maxr=6)
            if t is None:
                continue
            pth = []
            for pp in paths:
                if i < len(pp):
                    q = m.nearest_free(int(pp[i][0]), int(pp[i][1]), maxr=6)
                    if q is not None:
                        pth.append(px(q))
            sh = None
            if shel and i < len(shel):
                q = m.nearest_free(int(shel[i][0]), int(shel[i][1]), maxr=6)
                sh = px(q) if q is not None else None
            m.layout_ambient.append({"id": sp["id"], "n": sp.get("n", 1), "xy": px(t), "mode": sp.get("mode"),
                                     "path": pth, "shelter": sh})


def _carve(m, W, s, centre, off):
    """Út vájása a bázistól a középig (fát kivág, vizet/sziklát feltölt); off: íves eltolás."""
    steps = int(math.hypot(centre[0] - s[0], centre[1] - s[1])) + 1
    ux, uy = (centre[0] - s[0]) / steps, (centre[1] - s[1]) / steps
    for i in range(steps + 1):
        k = math.sin(math.pi * i / steps) * off  # eltolt (íves) út, ha az egyenes el van zárva
        x = s[0] + ux * i - uy * k
        y = s[1] + uy * i + ux * k
        ix, iy = math.floor(x + 0.5), math.floor(y + 0.5)
        for yy in range(iy - 1, iy + 2):
            for xx in range(ix - 1, ix + 2):
                if not m.inb(xx, yy):
                    continue
                idx = yy * W + xx
                r = m.res_grid[idx]
                if r is not None and r.kind == "wood":
                    m.remove_resource(r)
                if m.terrain[idx] in (WATER, ROCK):
                    m.terrain[idx] = DIRT
                    m.block[idx] = FREE
