"""Égi uralom (Ötletdoboz Ö2, Dawn of War-szerű pontfoglalás): Hasadékkövek a térképen.

A Hasadékkő lebegő meteoritszilánk. A közelében álló (nem munkás) egységek 20 mp alatt elfoglalják, ha nincs
ott ellenséges egység; amíg a kövön ellenséges őrállás áll, nem foglalható el. Az elfoglalt kő lassan meteoritot ad
(őrállással kétszer annyit), és Égkő-fényt áraszt. Választható győzelem („Uralom”): aki a kövek 2/3-át 5 percig
tartja, nyer.
"""
import math

from . import dmath
from .world import TILE

CAPTURE_TIME = 20.0
CAPTURE_R = 2.5        # csempe
DOMINATION_TIME = 300.0


class RiftStone:
    is_unit = False
    is_building = False
    is_resource = False
    is_point = True
    alive = True

    def __init__(self, idx, tx, ty):
        self.id = -1000 - idx
        self.tx, self.ty = tx, ty
        self.x, self.y = (tx + 0.5) * TILE, (ty + 0.5) * TILE
        self.owner = None        # az elfoglaló játékos
        self.cap_owner = None    # aki épp foglalja
        self.progress = 0.0      # 0..1
        self.guard = None        # őrállás (épület) a kövön
        self.acc = 0.0


def _free(m, tx, ty, r=1):
    return all(m.inb(x, y) and m.passable(x, y) and m.buildable_terrain(x, y) and m.res_grid[y * m.w + x] is None
               for y in range(ty - r, ty + r + 1) for x in range(tx - r, tx + r + 1))


def _snap(m, x, y, taken, starts):
    """A (x, y) csempéhez legközelebbi szabad, építhető hely (3×3 szabad környezettel)."""
    tx, ty = int(round(x)), int(round(y))
    for rad in range(0, 10):
        best = None
        for dy in range(-rad, rad + 1):
            for dx in range(-rad, rad + 1):
                if max(abs(dx), abs(dy)) != rad:
                    continue
                cx, cy = tx + dx, ty + dy
                if (_free(m, cx, cy) and all(abs(cx - a) + abs(cy - b) > 6 for a, b in taken)
                        and all(dmath.hypot(cx - sx, cy - sy) >= 12 for sx, sy in starts)):
                    best = (cx, cy)
                    break
            if best:
                break
        if best:
            return best
    return None


def place_points(game):
    """Igazságos elhelyezés: kezdőhelyenként kettő a középpont felé, a kezdőhelyek közötti felezőkön egy-egy, és egy
    középen. A bázisoktól legalább 12 csempére."""
    m = game.map
    starts = [(int(p.start_xy[0] // TILE), int(p.start_xy[1] // TILE)) for p in game.players]
    if not starts:
        return
    cx, cy = m.w / 2, m.h / 2
    cand = []
    for sx, sy in starts:
        dx, dy = cx - sx, cy - sy
        d = dmath.hypot(dx, dy) or 1
        ux, uy = dx / d, dy / d
        cand.append((sx + ux * 20 - uy * 9, sy + uy * 20 + ux * 9))
        cand.append((sx + ux * 20 + uy * 9, sy + uy * 20 - ux * 9))
    n = len(starts)
    pairs = [(i, (i + 1) % n) for i in range(n)] if n > 2 else []  # két játékosnál a felező = a középpont
    for i, j in pairs:
        mx, my = (starts[i][0] + starts[j][0]) / 2, (starts[i][1] + starts[j][1]) / 2
        cand.append((mx, my))
    cand.append((cx, cy))
    taken = []
    for x, y in cand:
        if any(dmath.hypot(x - sx, y - sy) < 12 for sx, sy in starts):
            continue
        sp = _snap(m, x, y, taken, starts)
        if sp:
            taken.append(sp)
    game.points = [RiftStone(i, tx, ty) for i, (tx, ty) in enumerate(taken)]


def point_at(game, tx, ty):
    for pt in getattr(game, "points", ()):
        if pt.tx == tx and pt.ty == ty:
            return pt
    return None


def guard_alive(pt):
    return pt.guard is not None and pt.guard.alive


def update_points(game, dt):
    pts = getattr(game, "points", None)
    if not pts:
        return
    r = CAPTURE_R * TILE
    for pt in pts:
        present = {}
        for u in game.query(pt.x, pt.y, r + 32):
            if u.is_unit and u.alive and not u.is_worker and u.inside is None and not getattr(u.owner, "neutral", False) \
                    and dmath.hypot(u.x - pt.x, u.y - pt.y) <= r:
                present[u.owner.id] = u.owner
        ps = list(present.values())
        contested = any(a.hostile(b) for a in ps for b in ps if a is not b)
        if ps and not contested:
            p = ps[0]
            if pt.owner is not None and not pt.owner.hostile(p):
                pt.cap_owner, pt.progress = None, 0.0
            elif not (pt.owner is not None and guard_alive(pt)):
                if pt.cap_owner is not p:
                    pt.cap_owner, pt.progress = p, 0.0
                pt.progress += dt / CAPTURE_TIME
                if pt.progress >= 1.0:
                    old = pt.owner
                    pt.owner, pt.cap_owner, pt.progress = p, None, 0.0
                    game.notify(p, "Elfoglaltál egy Hasadékkövet!", (200, 150, 255))
                    if old is not None:
                        game.notify(old, "Elvesztettél egy Hasadékkövet!", (255, 120, 90))
        elif not ps:
            pt.progress = max(0.0, pt.progress - dt / CAPTURE_TIME)
            if pt.progress <= 0:
                pt.cap_owner = None
        # hozam: lassan meteorit (őrállással kétszer annyi)
        if pt.owner is not None and not pt.owner.defeated:
            rate = game.mine_rate("meteor") / 3.0 * (2.0 if guard_alive(pt) and pt.guard.complete else 1.0)
            pt.acc += rate * dt
            if pt.acc >= 1:
                n = int(pt.acc)
                pt.acc -= n
                game.income(pt.owner, "meteor", n)
    if game.options.get("domination"):
        _domination(game, dt)


def _domination(game, dt):
    pts = game.points
    if not pts:
        return
    need = math.ceil(len(pts) * 2 / 3)
    timers = game.__dict__.setdefault("dom_t", {})
    sides = []
    for p in game.players:
        if p.defeated or any(p in s for s in sides):
            continue
        sides.append([q for q in game.players if not q.defeated and (q is p or q.allied(p))])
    for side in sides:
        key = min(q.id for q in game.players if side[0].allied(q))
        held = sum(1 for pt in pts if pt.owner in side)
        if held >= need:
            t0 = timers.get(key, 0.0)
            t = t0 + dt
            timers[key] = t
            for mark in (60, 120, 180, 240):  # visszaszámlálás
                left0, left = DOMINATION_TIME - t0, DOMINATION_TIME - t
                if left0 > mark >= left:
                    for q in game.players:
                        who = "Te" if q in side else side[0].name
                        game.notify(q, f"Uralom: {who} {mark // 60} perc múlva győz, ha megtartja a köveket!",
                                    (255, 210, 120))
            if t >= DOMINATION_TIME:
                for q in game.players:
                    if q not in side and not q.defeated:
                        q.defeated = True
                        for u in game.units:
                            if u.owner is q:
                                u.alive = False
                        for b in list(game.buildings):
                            if b.owner is q and b.alive:
                                game.kill(b, None)
                        game.notify(q, "Uralom: az ellenfél megtartotta a Hasadékköveket.", (255, 120, 90))
        else:
            timers[key] = 0.0
