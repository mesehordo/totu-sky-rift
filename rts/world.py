"""Térkép: csempék, foglaltsági rács, nyersanyag-csomópontok, procedurális generátor."""
import math

from . import dmath
import random
from collections import deque

TILE = 32

GRASS, DIRT, WATER, ROCK = 0, 1, 2, 3
FREE, B_TERRAIN, B_BUILDING, B_RESOURCE = 0, 1, 2, 3


TREE_VARIANTS = 12  # a 4 és a 6 kockás fa-képlapnak is egyenletes (12 = lkkt)


def tree_variant(x, y, salt=0):
    """Fafajta egy mezőhöz: keverő hash, hogy a fajták ne csíkokban, hanem vegyesen álljanak."""
    h = (x * 374761393 + y * 668265263 + salt * 982451653) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return (h ^ (h >> 16)) % TREE_VARIANTS


class Resource:
    is_unit = False
    is_building = False
    is_resource = True
    flying = False

    def __init__(self, kind, tx, ty, size, amount, variant=0):
        self.kind = kind
        self.tx, self.ty = tx, ty
        self.w = self.h = size
        self.amount = amount
        self.max_amount = amount
        self.alive = True
        self.variant = variant
        self.x = (tx + size / 2) * TILE
        self.y = (ty + size / 2) * TILE
        self.radius = size * TILE / 2
        self.rect = (tx * TILE, ty * TILE, size * TILE, size * TILE)
        self.id = -1
        self.owner = None


class GameMap:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.terrain = bytearray(w * h)
        self.block = bytearray(w * h)
        self.res_grid = [None] * (w * h)
        self.resources = []
        self.starts = []
        self.version = 0  # nő, ha a statikus kép megváltozik (pl. kivágott fa)

    def inb(self, x, y):
        return 0 <= x < self.w and 0 <= y < self.h

    def passable(self, x, y):
        return 0 <= x < self.w and 0 <= y < self.h and self.block[y * self.w + x] == FREE

    def buildable_terrain(self, x, y):
        return self.inb(x, y) and self.terrain[y * self.w + x] in (GRASS, DIRT)

    def set_block(self, tx, ty, w, h, val):
        for y in range(ty, ty + h):
            for x in range(tx, tx + w):
                if self.inb(x, y):
                    self.block[y * self.w + x] = val

    def add_resource(self, r):
        if r.w <= 0 or r.h <= 0:
            return False
        for y in range(r.ty, r.ty + r.h):
            for x in range(r.tx, r.tx + r.w):
                i = y * self.w + x
                if not self.inb(x, y) or self.block[i] != FREE or self.res_grid[i] is not None:
                    return False
        self.resources.append(r)
        for y in range(r.ty, r.ty + r.h):
            for x in range(r.tx, r.tx + r.w):
                self.block[y * self.w + x] = B_RESOURCE
                self.res_grid[y * self.w + x] = r
        return True

    def remove_resource(self, r):
        if r not in self.resources:
            return
        r.alive = False
        for y in range(r.ty, r.ty + r.h):
            for x in range(r.tx, r.tx + r.w):
                i = y * self.w + x
                if self.res_grid[i] is r:
                    self.res_grid[i] = None
                    if self.block[i] == B_RESOURCE:
                        self.block[i] = FREE
        self.resources.remove(r)
        self.version += 1

    def nearest_free(self, tx, ty, from_xy=None, maxr=12):
        if self.passable(tx, ty):
            return tx, ty
        for r in range(1, maxr + 1):
            cands = []
            for x in range(tx - r, tx + r + 1):
                for y in (ty - r, ty + r):
                    if self.passable(x, y):
                        cands.append((x, y))
            for y in range(ty - r + 1, ty + r):
                for x in (tx - r, tx + r):
                    if self.passable(x, y):
                        cands.append((x, y))
            if cands:
                if from_xy:
                    fx, fy = from_xy
                    cands.sort(key=lambda c: dmath.sq(c[0] - fx) + dmath.sq(c[1] - fy))
                return cands[0]
        return None

    def reachable(self, a, b):
        w = self.w
        seen = bytearray(w * self.h)
        q = deque([a])
        seen[a[1] * w + a[0]] = 1
        while q:
            x, y = q.popleft()
            if (x, y) == b:
                return True
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if self.passable(nx, ny) and not seen[ny * w + nx]:
                    seen[ny * w + nx] = 1
                    q.append((nx, ny))
        return False


def generate_map(data, seed, size=None):
    rng = random.Random(seed)
    g = data.game
    W, H = g["map"]["width"], g["map"]["height"]
    if size and size[0] and size[1]:
        W, H = size
    rdefs = g["resources"]
    m = GameMap(W, H)
    s0 = (11, 11)
    s1 = (W - 12, H - 12)
    m.starts = [s0, s1]

    def mirror(x, y):
        return W - 1 - x, H - 1 - y

    def near_start(x, y, d):
        return any(dmath.hypot(x - sx, y - sy) < d for sx, sy in m.starts)

    def set_t(x, y, t):
        for px, py in ((x, y), mirror(x, y)):
            if m.inb(px, py):
                m.terrain[py * W + px] = t

    def blob(cx, cy, r, t, keep_out=0):
        for y in range(int(cy - r - 1), int(cy + r + 2)):
            for x in range(int(cx - r - 1), int(cx + r + 2)):
                if dmath.hypot(x - cx, y - cy) <= r + rng.uniform(-0.8, 0.8):
                    if keep_out and near_start(x, y, keep_out):
                        continue
                    set_t(x, y, t)

    for _ in range(45):
        blob(rng.randrange(W), rng.randrange(H), rng.uniform(1.5, 4.5), DIRT)
    for _ in range(5):
        cx, cy = rng.randrange(W), rng.randrange(H // 2 + 6)
        if near_start(cx, cy, 18):
            continue
        blob(cx, cy, rng.uniform(2, 4.5), WATER, keep_out=14)
    for _ in range(7):  # sziklagerincek
        x, y = rng.uniform(0, W), rng.uniform(0, H / 2 + 6)
        ang = rng.uniform(0, math.pi * 2)
        for _ in range(rng.randrange(5, 12)):
            ix, iy = int(x), int(y)
            if not near_start(ix, iy, 14):
                set_t(ix, iy, ROCK)
                if rng.random() < 0.5:
                    set_t(ix + 1, iy, ROCK)
            ang += rng.uniform(-0.5, 0.5)
            x += dmath.cos(ang)
            y += dmath.sin(ang)
    # bázisok környéke tiszta füves terület
    for sx, sy in m.starts:
        for y in range(sy - 6, sy + 7):
            for x in range(sx - 6, sx + 7):
                if m.inb(x, y) and m.terrain[y * W + x] in (WATER, ROCK):
                    m.terrain[y * W + x] = GRASS
    for i in range(W * H):
        if m.terrain[i] in (WATER, ROCK):
            m.block[i] = B_TERRAIN

    def place(kind, tx, ty, size=2):
        amount = rdefs[kind]["node_amount"]
        v = rng.randrange(4)
        a = Resource(kind, tx, ty, size, amount, v)
        mx, my = W - tx - size, H - ty - size
        b = Resource(kind, mx, my, size, amount, v)
        for r in (a, b):
            for y in range(r.ty, r.ty + size):
                for x in range(r.tx, r.tx + size):
                    if m.inb(x, y) and m.terrain[y * W + x] in (WATER, ROCK):
                        m.terrain[y * W + x] = GRASS
                        m.block[y * W + x] = FREE
        if m.add_resource(a):
            m.add_resource(b)

    sx, sy = s0
    place("stone", sx + 7, sy - 6)
    place("stone", sx - 6, sy + 7)
    place("meteor", sx + 8, sy + 8)
    # külső bázishelyek és középső kráterek
    place("stone", 8, H // 2 - 2)
    place("stone", W // 2 - 12, H // 2 + 6)
    place("meteor", W // 2 - 8, 9)
    place("meteor", W // 2 - 4, H // 2 + 3)
    place("meteor", W // 2 + 6, H // 2 - 6)

    def tree(x, y):
        if near_start(x, y, 7):
            return
        for px, py in ((x, y), mirror(x, y)):
            if m.inb(px, py) and m.terrain[py * W + px] in (GRASS, DIRT):
                m.add_resource(Resource("wood", px, py, 1, rdefs["wood"]["node_amount"], rng.randrange(TREE_VARIANTS)))

    # bázis menti erdősávok a térkép szélén
    for x in range(0, 30):
        for y in range(0, 4):
            if rng.random() < 0.8 - y * 0.12:
                tree(x, y)
    for y in range(0, 30):
        for x in range(0, 4):
            if rng.random() < 0.8 - x * 0.12:
                tree(x, y)
    for _ in range(22):
        cx, cy = rng.randrange(W), rng.randrange(H // 2 + 8)
        if near_start(cx, cy, 11):
            continue
        r = rng.uniform(1.5, 3.5)
        for y in range(int(cy - r), int(cy + r) + 1):
            for x in range(int(cx - r), int(cx + r) + 1):
                if dmath.hypot(x - cx, y - cy) <= r and rng.random() < 0.85:
                    tree(x, y)

    # összeköttetés biztosítása a két bázis között
    for _ in range(3):
        if m.reachable(s0, s1):
            break
        steps = int(dmath.hypot(s1[0] - s0[0], s1[1] - s0[1]))
        for i in range(steps + 1):
            cx = s0[0] + (s1[0] - s0[0]) * i / steps
            cy = s0[1] + (s1[1] - s0[1]) * i / steps
            for y in range(int(cy) - 1, int(cy) + 2):
                for x in range(int(cx) - 1, int(cx) + 2):
                    if not m.inb(x, y):
                        continue
                    idx = y * W + x
                    r = m.res_grid[idx]
                    if r is not None and r.kind == "wood":
                        m.remove_resource(r)
                    if m.terrain[idx] in (WATER, ROCK):
                        m.terrain[idx] = GRASS
                        m.block[idx] = FREE
    return m
