"""F3 A/B mérés – Python referencia-mag (a docs/alap/SZABALYOK.md 3. fejezete és tools/ab/SPEC.md szerint).

    python tools/ab/python/abcore.py tools/ab/scenarios/center_500.json [--ticks N] [--out eredmeny.json]

A C# és a C++ változat ennek sorról sorra megfelelő átirata; azonos bemenetre bitre azonos állapothash a cél.
Ezért: csak +, -, *, /, sqrt, floor (nincs hypot/sin/cos), a döntetlenek rögzített sorrendben dőlnek el.
"""
import base64
import json
import math
import sys
import time

TILE = 32.0
SQ2 = math.sqrt(2.0)
DIRS = ((1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0), (1, 1, SQ2), (1, -1, SQ2), (-1, 1, SQ2), (-1, -1, SQ2))
CELL = 128.0
PATH_BUDGET = 6
MAX_NODES = 9000
OCC_NODES = 512
FOW_EVERY = 6
HASH_EVERY = 300
MASK = 0xFFFFFFFFFFFFFFFF


def dist(ax, ay, bx, by):
    dx = ax - bx
    dy = ay - by
    return math.sqrt(dx * dx + dy * dy)


class Unit:
    __slots__ = ("id", "team", "t", "x", "y", "hp", "cd", "path", "goal", "pending", "queued", "repaths", "stuck_t",
                 "lpx", "lpy", "wait_t", "scan_t", "sub", "started", "done", "repath_t", "alive", "moving", "r")

    def __init__(self, uid, team, t, x, y, hp, r):
        self.id, self.team, self.t, self.x, self.y, self.hp, self.r = uid, team, t, x, y, hp, r
        self.cd = 0.0
        self.path = []
        self.goal = None
        self.pending = False
        self.queued = False
        self.repaths = 0
        self.stuck_t = 0.0
        self.lpx, self.lpy = x, y
        self.wait_t = 0.0
        self.scan_t = (uid % 8) * 0.05
        self.sub = None
        self.started = False
        self.done = False
        self.repath_t = 0.0
        self.alive = True
        self.moving = False


class Proj:
    __slots__ = ("x", "y", "tgt", "tx", "ty", "dmg", "team", "ut", "speed", "splash", "alive")


class Sim:
    def __init__(self, sc):
        self.w, self.h = sc["w"], sc["h"]
        self.block = base64.b64decode(sc["block"])
        self.dt = 1.0 / sc["hz"]
        self.types = sc["types"]
        self.matrix = sc["matrix"]
        self.destx, self.desty = float(sc["dest"][0]), float(sc["dest"][1])
        self.units = []
        for i, u in enumerate(sc["units"]):
            ty = self.types[u["type"]]
            self.units.append(Unit(i + 1, u["team"], u["type"], float(u["x"]), float(u["y"]), ty["hp"], ty["radius"]))
        self.max_r = max(t["radius"] for t in self.types)
        self.projs = []
        self.queue = []
        self.qhead = 0
        self.tick = 0
        self.cw = int(math.floor(self.w * TILE / CELL)) + 1
        self.ch = int(math.floor(self.h * TILE / CELL)) + 1
        self.cells = [[] for _ in range(self.cw * self.ch)]
        self.clear = {}
        self.vis = [bytearray(self.w * self.h) for _ in range(8)]
        n = self.w * self.h
        self.gcost = [0.0] * n
        self.came = [0] * n
        self.stamp = [0] * n
        self.gen = 0
        self.stats = {"path_requests": 0, "astar_calls": 0, "astar_expanded": 0, "kills": 0, "arrived": 0}
        self.update_fow()

    # ---- térkép ---------------------------------------------------------------------------------------------
    def passable(self, tx, ty):
        return 0 <= tx < self.w and 0 <= ty < self.h and self.block[ty * self.w + tx] == 0

    def fits(self, x, y, r):
        if x - r < 0 or y - r < 0 or x + r > self.w * TILE or y + r > self.h * TILE:
            return False
        ty0, ty1 = int(math.floor((y - r) / TILE)), int(math.floor((y + r) / TILE))
        tx0, tx1 = int(math.floor((x - r) / TILE)), int(math.floor((x + r) / TILE))
        for ty in range(ty0, ty1 + 1):
            for tx in range(tx0, tx1 + 1):
                if self.passable(tx, ty):
                    continue
                dx = max(tx * TILE - x, 0.0, x - (tx + 1) * TILE)
                dy = max(ty * TILE - y, 0.0, y - (ty + 1) * TILE)
                if dx * dx + dy * dy < r * r:
                    return False
        return True

    def tile_ok(self, tx, ty, r):
        grid = self.clear.get(r)
        if grid is None:
            grid = self.clear[r] = bytearray([2]) * (self.w * self.h)
        i = ty * self.w + tx
        v = grid[i]
        if v == 2:
            v = grid[i] = 1 if self.fits((tx + 0.5) * TILE, (ty + 0.5) * TILE, r) else 0
        return v == 1

    def clear_line(self, x0, y0, x1, y1, r, occ):
        dx, dy = x1 - x0, y1 - y0
        d = math.sqrt(dx * dx + dy * dy)
        if d < 1.0:
            return True
        steps = int(math.floor(d / 4.0)) + 1
        for i in range(steps + 1):
            t = i / steps
            px, py = x0 + dx * t, y0 + dy * t
            if not self.fits(px, py, r) or (occ is not None and self.occupied(occ, px, py)):
                return False
        return True

    @staticmethod
    def occupied(occ, x, y):
        for bx, by, rr in occ:
            dx, dy = x - bx, y - by
            if dx * dx + dy * dy < rr:
                return True
        return False

    def nearest_free(self, gx, gy, sx, sy):
        for ring in range(1, 13):
            best, bd = None, 0
            for ty in range(gy - ring, gy + ring + 1):
                for tx in range(gx - ring, gx + ring + 1):
                    if max(abs(tx - gx), abs(ty - gy)) != ring or not self.passable(tx, ty):
                        continue
                    d = (tx - sx) * (tx - sx) + (ty - sy) * (ty - sy)
                    if best is None or d < bd:
                        best, bd = (tx, ty), d
            if best is not None:
                return best
        return None

    # ---- A* ---------------------------------------------------------------------------------------------------
    def find_path(self, sx, sy, gx, gy, r, occ, max_nodes):
        w, h = self.w, self.h
        self.stats["astar_calls"] += 1
        if not self.passable(gx, gy):
            alt = self.nearest_free(gx, gy, sx, sy)
            if alt is None:
                return []
            gx, gy = alt
        start, goal = sy * w + sx, gy * w + gx
        if start == goal:
            return []
        self.gen += 1
        gen, gcost, came, stamp = self.gen, self.gcost, self.came, self.stamp
        memo = {}

        def valid(x, y):
            k = y * w + x
            v = memo.get(k)
            if v is None:
                v = self.tile_ok(x, y, r)
                if v and occ is not None:
                    v = not self.occupied(occ, (x + 0.5) * TILE, (y + 0.5) * TILE)
                memo[k] = v
            return v

        def hd(x, y):
            dx, dy = abs(x - gx), abs(y - gy)
            return (dx + dy) + (SQ2 - 2.0) * min(dx, dy)

        heap = Heap()
        stamp[start], gcost[start], came[start] = gen, 0.0, -1
        heap.push(hd(sx, sy), 0.0, start)
        best, besth = start, hd(sx, sy)
        expanded = 0
        while heap.n and expanded < max_nodes:
            _, g, n = heap.pop()
            if n == goal:
                best = goal
                break
            if g > gcost[n]:
                continue
            expanded += 1
            x, y = n % w, n // w
            for dx, dy, c in DIRS:
                nx, ny = x + dx, y + dy
                if nx < 0 or ny < 0 or nx >= w or ny >= h:
                    continue
                if not valid(nx, ny):
                    continue
                if dx != 0 and dy != 0 and (not valid(nx, y) or not valid(x, ny)):
                    continue
                ni = ny * w + nx
                ng = g + c
                if stamp[ni] != gen or ng < gcost[ni]:
                    stamp[ni], gcost[ni], came[ni] = gen, ng, n
                    hh = hd(nx, ny)
                    heap.push(ng + hh, ng, ni)
                    if hh < besth:
                        best, besth = ni, hh
        self.stats["astar_expanded"] += expanded
        out = []
        n = best
        while n != start and n != -1:
            out.append((n % w, n // w))
            n = came[n]
        out.reverse()
        return out

    def smooth(self, sx, sy, pts, r, occ):
        out = []
        cx, cy = sx, sy
        i, n = 0, len(pts)
        while i < n:
            j = min(n - 1, i + 6)
            while j > i and not self.clear_line(cx, cy, pts[j][0], pts[j][1], r, occ):
                j -= 1
            out.append(pts[j])
            cx, cy = pts[j]
            i = j + 1
        return out

    def unit_path(self, u):
        gx, gy = u.goal
        r = u.r
        sx, sy = int(math.floor(u.x / TILE)), int(math.floor(u.y / TILE))
        tx, ty = int(math.floor(gx / TILE)), int(math.floor(gy / TILE))
        if u.repaths == 0 and self.clear_line(u.x, u.y, gx, gy, r, None):
            return [(gx, gy)]
        occ = None
        if u.repaths > 0:
            reach = TILE * 4
            occ = []
            for v in self.query(u.x, u.y, reach):
                if v is not u and v.alive and dist(v.x, v.y, u.x, u.y) < reach:
                    rr = r + v.r - 0.01
                    occ.append((v.x, v.y, rr * rr))
            if occ:
                tiles = self.find_path(sx, sy, tx, ty, r, occ, OCC_NODES)
                if tiles and tiles[-1] == (tx, ty):
                    pts = [((a + 0.5) * TILE, (b + 0.5) * TILE) for a, b in tiles]
                    if self.clear_line(pts[-1][0], pts[-1][1], gx, gy, r, occ):
                        if pts[-1] != (gx, gy):
                            pts.append((gx, gy))
                        return self.smooth(u.x, u.y, pts, r, occ)
            else:
                occ = None
        if occ is None and self.clear_line(u.x, u.y, gx, gy, r, None):
            return [(gx, gy)]
        tiles = self.find_path(sx, sy, tx, ty, r, None, MAX_NODES)
        pts = [((a + 0.5) * TILE, (b + 0.5) * TILE) for a, b in tiles]
        if pts and pts[-1] != (gx, gy) and self.passable(tx, ty) and self.clear_line(pts[-1][0], pts[-1][1], gx, gy, r, None):
            pts.append((gx, gy))
        elif not pts and self.clear_line(u.x, u.y, gx, gy, r, None):
            return [(gx, gy)]
        return self.smooth(u.x, u.y, pts, r, None)

    # ---- térbeli index ------------------------------------------------------------------------------------
    def rebuild(self):
        for c in self.cells:
            c.clear()
        for u in self.units:
            if u.alive:
                self.cells[self.cell_of(u.x, u.y)].append(u)

    def cell_of(self, x, y):
        cx = min(self.cw - 1, max(0, int(math.floor(x / CELL))))
        cy = min(self.ch - 1, max(0, int(math.floor(y / CELL))))
        return cy * self.cw + cx

    def query(self, x, y, r):
        out = []
        cx0 = max(0, int(math.floor((x - r) / CELL)))
        cx1 = min(self.cw - 1, int(math.floor((x + r) / CELL)))
        cy0 = max(0, int(math.floor((y - r) / CELL)))
        cy1 = min(self.ch - 1, int(math.floor((y + r) / CELL)))
        for cy in range(cy0, cy1 + 1):
            for cx in range(cx0, cx1 + 1):
                out.extend(self.cells[cy * self.cw + cx])
        return out

    # ---- egységek ---------------------------------------------------------------------------------------------
    def set_path(self, u, gx, gy):
        u.goal = (gx, gy)
        u.stuck_t = 0.0
        u.lpx, u.lpy = u.x, u.y
        u.path = []
        u.pending = True
        if not u.queued:
            u.queued = True
            self.queue.append(u)
            self.stats["path_requests"] += 1

    def process_paths(self):
        budget = PATH_BUDGET
        while self.qhead < len(self.queue) and budget > 0:
            u = self.queue[self.qhead]
            self.qhead += 1
            u.queued = False
            if not u.alive or not u.pending or u.goal is None:
                u.pending = False
                continue
            budget -= 3 if u.repaths else 1
            u.path = self.unit_path(u)
            u.pending = False
        if self.qhead > 4096:
            del self.queue[:self.qhead]
            self.qhead = 0

    def blocker(self, u, nx, ny):
        cur = None
        for v in self.query(nx, ny, u.r + self.max_r):
            if v is u or not v.alive:
                continue
            m = u.r + v.r - 0.5
            dn = dist(nx, ny, v.x, v.y)
            if dn < m and dn < dist(u.x, u.y, v.x, v.y):
                return v
        return cur

    def follow(self, u):
        if u.pending:
            return False
        if not u.path:
            return True
        px, py = u.path[0]
        dx, dy = px - u.x, py - u.y
        d = math.sqrt(dx * dx + dy * dy)
        sp = self.types[u.t]["speed"]
        step = sp * self.dt
        if d <= step:
            nx, ny = px, py
        else:
            nx, ny = u.x + dx / d * step, u.y + dy / d * step
        tile_n = (int(math.floor(nx / TILE)), int(math.floor(ny / TILE)))
        tile_c = (int(math.floor(u.x / TILE)), int(math.floor(u.y / TILE)))
        if not self.fits(nx, ny, u.r) and (tile_n != tile_c or self.fits(u.x, u.y, u.r)):
            u.repaths = min(8, u.repaths + 1)
            g = u.goal if u.goal is not None else u.path[-1]
            self.set_path(u, g[0], g[1])
            return False
        if self.blocker(u, nx, ny) is not None:
            u.wait_t += self.dt
        elif d <= step:
            u.x, u.y = px, py
            u.path.pop(0)
            u.moving = True
            u.wait_t = max(0.0, u.wait_t - self.dt * 0.5)
        else:
            u.x, u.y = nx, ny
            u.moving = True
            u.wait_t = max(0.0, u.wait_t - self.dt * 0.5)
        u.stuck_t += self.dt
        if u.stuck_t >= 0.8 + min(4, u.repaths) * 0.15:
            ox, oy = u.lpx, u.lpy
            u.lpx, u.lpy = u.x, u.y
            u.stuck_t = 0.0
            moved = dist(u.x, u.y, ox, oy)
            progress = dist(px, py, ox, oy) - dist(px, py, u.x, u.y)
            if u.path and (moved < sp * 0.15 or (u.wait_t > 0.6 and progress < sp * 0.12)):
                u.repaths = min(8, u.repaths + 1)
                g = u.goal if u.goal is not None else u.path[-1]
                self.set_path(u, g[0], g[1])
            elif moved >= sp * 0.15:
                u.repaths = 0
        return not u.path and not u.pending

    def gap(self, a, b):
        return max(0.0, dist(a.x, a.y, b.x, b.y) - a.r - b.r)

    def visible(self, team, e):
        tx, ty = int(math.floor(e.x / TILE)), int(math.floor(e.y / TILE))
        return 0 <= tx < self.w and 0 <= ty < self.h and self.vis[team][ty * self.w + tx] == 1

    def find_enemy(self, u, radius):
        best, bestv = None, 0.0
        for e in self.query(u.x, u.y, radius + 32.0):
            if e.team == u.team or not e.alive or not self.visible(u.team, e):
                continue
            d = self.gap(u, e)
            if d > radius:
                continue
            if best is None or d < bestv:
                best, bestv = e, d
        return best

    def apply(self, team, ut, tgt, dmg):
        if not tgt.alive:
            return
        tt = self.types[tgt.t]["utype"]
        m = self.matrix[ut][tt] if ut >= 0 and tt >= 0 else 1.0
        d = max(0.5, dmg * m - self.types[tgt.t]["armor"])
        tgt.hp -= d
        if tgt.hp <= 0.0:
            tgt.alive = False
            self.stats["kills"] += 1

    def hit(self, team, ut, tgt, dmg, splash, x, y):
        if tgt is not None:
            self.apply(team, ut, tgt, dmg)
        if splash > 0.0:
            for e in self.query(x, y, splash + self.max_r):
                if e is tgt or not e.alive or e.team == team:
                    continue
                if dist(e.x, e.y, x, y) - e.r <= splash:
                    self.apply(team, ut, e, dmg * 0.5)

    def try_attack(self, u, tgt):
        if u.cd > 0.0:
            return
        ty = self.types[u.t]
        u.cd = ty["cooldown"]
        if ty["projectile"]:
            p = Proj()
            p.x, p.y, p.tgt, p.tx, p.ty = u.x, u.y, tgt, tgt.x, tgt.y
            p.dmg, p.team, p.ut, p.speed, p.splash, p.alive = ty["damage"], u.team, ty["utype"], ty["proj_speed"], ty["splash"], True
            self.projs.append(p)
        else:
            self.hit(u.team, ty["utype"], tgt, ty["damage"], ty["splash"], tgt.x, tgt.y)

    def engage(self, u, tgt):
        ty = self.types[u.t]
        if self.gap(u, tgt) <= ty["range"] + 3.0:
            u.path = []
            u.pending = False
            self.try_attack(u, tgt)
            return
        u.repath_t -= self.dt
        if (not u.path and not u.pending) or u.repath_t <= 0.0:
            if u.repath_t <= 0.0 or not u.path:
                u.repath_t = 0.8
                self.set_path(u, tgt.x, tgt.y)
        self.follow(u)

    def update_unit(self, u):
        ty = self.types[u.t]
        if u.cd > 0.0:
            u.cd -= self.dt
        u.moving = False
        sub = u.sub
        if sub is not None and (not sub.alive or self.gap(u, sub) > ty["sight"] * 1.6):
            sub = u.sub = None
            u.started = False
        if sub is None:
            u.scan_t -= self.dt
            if u.scan_t <= 0.0:
                u.scan_t = 0.4
                e = self.find_enemy(u, ty["sight"])
                if e is not None:
                    u.sub = e
                    return
            if u.done:
                return
            if not u.started or u.goal is None:
                u.started = True
                self.set_path(u, self.destx, self.desty)
                return
            if self.follow(u):
                u.done = True
                self.stats["arrived"] += 1
            return
        self.engage(u, sub)

    def update_projs(self):
        for p in self.projs:
            if p.tgt.alive:
                p.tx, p.ty = p.tgt.x, p.tgt.y
            dx, dy = p.tx - p.x, p.ty - p.y
            d = math.sqrt(dx * dx + dy * dy)
            step = p.speed * self.dt
            if d <= step:
                p.alive = False
                self.hit(p.team, p.ut, p.tgt if p.tgt.alive else None, p.dmg, p.splash, p.tx, p.ty)
            else:
                p.x += dx / d * step
                p.y += dy / d * step

    def separate(self):
        W, H = self.w * TILE, self.h * TILE
        for ci in range(self.cw * self.ch):
            lst = self.cells[ci]
            if not lst:
                continue
            cx, cy = ci % self.cw, ci // self.cw
            near = []
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    x2, y2 = cx + dx, cy + dy
                    if 0 <= x2 < self.cw and 0 <= y2 < self.ch:
                        near.extend(self.cells[y2 * self.cw + x2])
            for u in lst:
                if not u.alive:
                    continue
                for v in near:
                    if v.id <= u.id or not v.alive:
                        continue
                    ddx, ddy = v.x - u.x, v.y - u.y
                    m = u.r + v.r
                    if abs(ddx) >= m or abs(ddy) >= m:
                        continue
                    d = math.sqrt(ddx * ddx + ddy * ddy)
                    if d >= m:
                        continue
                    if d < 0.01:
                        if (u.id + v.id) % 2 == 0:
                            ddx, ddy = 1.0, 0.0
                        else:
                            ddx, ddy = 0.0, 1.0
                        d = 1.0
                    ov = (m - d) * 0.5
                    ku = 1.0 if u.moving else 0.0
                    kv = 1.0 if v.moving else 0.0
                    if ku + kv == 0.0:
                        ku = kv = 1.0
                    wu, wv = ku / (ku + kv), kv / (ku + kv)
                    nx, ny = ddx / d, ddy / d
                    ux, uy = u.x - nx * ov * wu, u.y - ny * ov * wu
                    vx, vy = v.x + nx * ov * wv, v.y + ny * ov * wv
                    if u.r <= ux <= W - u.r and u.r <= uy <= H - u.r and self.fits(ux, uy, u.r):
                        u.x, u.y = ux, uy
                    if v.r <= vx <= W - v.r and v.r <= vy <= H - v.r and self.fits(vx, vy, v.r):
                        v.x, v.y = vx, vy

    def update_fow(self):
        w, h = self.w, self.h
        for t in range(8):
            v = self.vis[t]
            for i in range(w * h):
                v[i] = 0
        for u in self.units:
            if not u.alive:
                continue
            s = self.types[u.t]["sight"]
            v = self.vis[u.team]
            ss = s * s
            tx0, tx1 = max(0, int(math.floor((u.x - s) / TILE))), min(w - 1, int(math.floor((u.x + s) / TILE)))
            ty0, ty1 = max(0, int(math.floor((u.y - s) / TILE))), min(h - 1, int(math.floor((u.y + s) / TILE)))
            for ty in range(ty0, ty1 + 1):
                dy = (ty + 0.5) * TILE - u.y
                for tx in range(tx0, tx1 + 1):
                    dx = (tx + 0.5) * TILE - u.x
                    if dx * dx + dy * dy <= ss:
                        v[ty * w + tx] = 1

    def step(self):
        self.tick += 1
        self.rebuild()
        self.process_paths()
        for u in self.units:
            if u.alive:
                self.update_unit(u)
        self.update_projs()
        self.separate()
        self.units = [u for u in self.units if u.alive]
        self.projs = [p for p in self.projs if p.alive]
        if self.tick % FOW_EVERY == 0:
            self.update_fow()

    def state_hash(self):
        h = 1469598103934665603
        for u in self.units:
            for v in (u.id, int(math.floor(u.x * 16.0)), int(math.floor(u.y * 16.0)), int(math.floor(u.hp * 16.0))):
                h = ((h ^ (v & MASK)) * 1099511628211) & MASK
        return h


class Heap:
    """Bináris kupac (f, g, n) lexikografikus rendezéssel – a C#/C++ változat ugyanezt valósítja meg."""
    __slots__ = ("a", "n")

    def __init__(self):
        self.a = []
        self.n = 0

    @staticmethod
    def less(x, y):
        return x[0] < y[0] or (x[0] == y[0] and (x[1] < y[1] or (x[1] == y[1] and x[2] < y[2])))

    def push(self, f, g, i):
        a = self.a
        item = (f, g, i)
        if self.n < len(a):
            a[self.n] = item
        else:
            a.append(item)
        k = self.n
        self.n += 1
        while k > 0:
            p = (k - 1) >> 1
            if Heap.less(a[k], a[p]):
                a[k], a[p] = a[p], a[k]
                k = p
            else:
                break

    def pop(self):
        a = self.a
        top = a[0]
        self.n -= 1
        if self.n > 0:
            a[0] = a[self.n]
            k = 0
            while True:
                l, r = 2 * k + 1, 2 * k + 2
                m = k
                if l < self.n and Heap.less(a[l], a[m]):
                    m = l
                if r < self.n and Heap.less(a[r], a[m]):
                    m = r
                if m == k:
                    break
                a[k], a[m] = a[m], a[k]
                k = m
        return top


def pct(vals, p):
    s = sorted(vals)
    i = min(len(s) - 1, max(0, int(round(p / 100.0 * (len(s) - 1)))))
    return round(s[i], 3)


def main():
    path = sys.argv[1]
    ticks, out = None, None
    args = sys.argv[2:]
    for i, a in enumerate(args):
        if a == "--ticks":
            ticks = int(args[i + 1])
        if a == "--out":
            out = args[i + 1]
    with open(path, encoding="utf-8") as f:
        sc = json.load(f)
    sim = Sim(sc)
    n = ticks or sc["ticks"]
    times, hashes = [], []
    start_units = len(sim.units)
    t_all = time.perf_counter()
    for _ in range(n):
        t0 = time.perf_counter()
        sim.step()
        times.append((time.perf_counter() - t0) * 1000.0)
        if sim.tick % HASH_EVERY == 0:
            hashes.append([sim.tick, format(sim.state_hash(), "016x")])
    total = time.perf_counter() - t_all
    res = {"lang": "python", "version": sys.version.split()[0], "scenario": sc["name"], "units_start": start_units,
           "ticks": n, "total_s": round(total, 3),
           "tick_ms": {"p50": pct(times, 50), "p95": pct(times, 95), "p99": pct(times, 99),
                       "max": round(max(times), 3), "mean": round(sum(times) / len(times), 3)},
           "alive_end": len(sim.units), **sim.stats, "hashes": hashes}
    try:
        import ctypes
        import ctypes.wintypes

        class PMC(ctypes.Structure):
            _fields_ = [("cb", ctypes.c_ulong), ("PageFaultCount", ctypes.c_ulong),
                        ("PeakWorkingSetSize", ctypes.c_size_t)] + [(f"f{i}", ctypes.c_size_t) for i in range(7)]
        c = PMC()
        c.cb = ctypes.sizeof(c)
        k32 = ctypes.windll.kernel32
        k32.GetCurrentProcess.restype = ctypes.c_void_p
        k32.K32GetProcessMemoryInfo.argtypes = [ctypes.c_void_p, ctypes.POINTER(PMC), ctypes.c_ulong]
        if k32.K32GetProcessMemoryInfo(k32.GetCurrentProcess(), ctypes.byref(c), c.cb):
            res["peak_mem_mib"] = round(c.PeakWorkingSetSize / 2 ** 20, 1)
    except Exception:
        pass
    text = json.dumps(res, ensure_ascii=False)
    if out:
        with open(out, "w", encoding="utf-8") as f:
            f.write(text)
    print(text)


if __name__ == "__main__":
    main()
