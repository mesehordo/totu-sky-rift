"""A* útkeresés csemperácson (8 irány, sarokvágás nélkül) és útvonal-simítás."""
import heapq
import math

from . import dmath
from .world import TILE

SQ2 = math.sqrt(2)
DIRS = ((1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0),
        (1, 1, SQ2), (1, -1, SQ2), (-1, 1, SQ2), (-1, -1, SQ2))


def fits(gmap, x, y, radius):
    if not radius:
        return gmap.passable(int(x // TILE), int(y // TILE))
    if x - radius < 0 or y - radius < 0 or x + radius > gmap.w * TILE or y + radius > gmap.h * TILE:
        return False
    for ty in range(int((y-radius)//TILE), int((y+radius)//TILE)+1):
        for tx in range(int((x-radius)//TILE), int((x+radius)//TILE)+1):
            if gmap.passable(tx, ty):
                continue
            dx = max(tx*TILE-x, 0, x-(tx+1)*TILE)
            dy = max(ty*TILE-y, 0, y-(ty+1)*TILE)
            if dx*dx + dy*dy < radius*radius:
                return False
    return True


_UNKNOWN = 2


def _clearance_grid(gmap, radius):
    """Csempénként: elfér-e a `radius` sugarú egység a csempe közepén (a `fits` eredménye) – lustán töltött,
    sugaranként külön tárolt rács. A térkép blokkrácsának bármely változása (épület, kivágott fa, kitermelt kő)
    a rácsokat érvényteleníti; az összevetés bájtra pontos, így a gyorsítótár nem adhat elavult választ."""
    snap = getattr(gmap, "_clr_snap", None)
    if snap is None or snap != gmap.block:
        gmap._clr_snap = bytes(gmap.block)
        gmap._clr = {}
    grid = gmap._clr.get(radius)
    if grid is None:
        grid = gmap._clr[radius] = bytearray([_UNKNOWN]) * (gmap.w * gmap.h)
    return grid


def find_path(gmap, sx, sy, gx, gy, max_nodes=9000, radius=0, occupied=None):
    """Csempe-útvonal (start nélkül). Ha a cél elérhetetlen, a legközelebbi
    elért pontig vezet."""
    w, h = gmap.w, gmap.h
    if not gmap.passable(gx, gy):
        alt = gmap.nearest_free(gx, gy, from_xy=(sx, sy))
        if alt is None:
            return []
        gx, gy = alt
    grid = _clearance_grid(gmap, radius)
    clearance = {}

    def valid(x, y):
        key = (x, y)
        if key not in clearance:
            i = y * w + x
            ok = grid[i]
            if ok == _UNKNOWN:
                ok = grid[i] = 1 if fits(gmap, (x + .5) * TILE, (y + .5) * TILE, radius) else 0
            if ok and occupied is not None:
                ok = not occupied((x + .5) * TILE, (y + .5) * TILE)
            clearance[key] = bool(ok)
        return clearance[key]

    start = sy * w + sx
    goal = gy * w + gx
    if start == goal:
        return []

    def hdist(x, y):
        dx, dy = abs(x - gx), abs(y - gy)
        return (dx + dy) + (SQ2 - 2) * min(dx, dy)

    came = {start: -1}
    gcost = {start: 0.0}
    openh = [(hdist(sx, sy), 0.0, start)]
    best, besth = start, hdist(sx, sy)
    expanded = 0
    while openh and expanded < max_nodes:
        _, g, n = heapq.heappop(openh)
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
            ni = ny * w + nx
            if not valid(nx, ny):
                continue
            if dx and dy and (not valid(nx, y) or not valid(x, ny)):
                continue
            ng = g + c
            if ng < gcost.get(ni, 1e18):
                gcost[ni] = ng
                came[ni] = n
                hh = hdist(nx, ny)
                heapq.heappush(openh, (ng + hh, ng, ni))
                if hh < besth:
                    best, besth = ni, hh
    out = []
    n = best
    while n != start and n != -1:
        out.append((n % w, n // w))
        n = came[n]
    out.reverse()
    return out


def _clear_line(gmap, x0, y0, x1, y1, radius=9, occupied=None):
    dx, dy = x1 - x0, y1 - y0
    d = dmath.hypot(dx, dy)
    if d < 1:
        return True
    steps = int(d / 4) + 1
    for i in range(steps + 1):
        t = i / steps
        px, py = x0 + dx * t, y0 + dy * t
        if not fits(gmap, px, py, radius) or (occupied is not None and occupied(px, py)):
            return False
    return True


def smooth_path(gmap, start, pts, lookahead=6, radius=9, occupied=None):
    """Kihagyja a köztes pontokat, ahol szabad a rálátás."""
    out = []
    cur = start
    i = 0
    n = len(pts)
    while i < n:
        j = min(n - 1, i + lookahead)
        while j > i and not _clear_line(gmap, cur[0], cur[1], pts[j][0], pts[j][1], radius, occupied):
            j -= 1
        out.append(pts[j])
        cur = pts[j]
        i = j + 1
    return out


def local_detour(gmap, start, goal, radius, occupied, flying=False):
    # Rovid, pontos kitero a foglalt csempekozepek kozott.
    stride = TILE / 4
    sx, sy = start
    gx, gy = goal
    if dmath.hypot(gx - sx, gy - sy) > TILE * 6 or occupied(gx, gy):
        return []

    def clear(x, y, px, py):
        if not flying:
            return _clear_line(gmap, x, y, px, py, radius, occupied)
        if not (radius <= px <= gmap.w*TILE-radius and radius <= py <= gmap.h*TILE-radius):
            return False
        steps = int(dmath.hypot(px-x, py-y)/4) + 1
        return all(not occupied(x + (px-x)*i/steps, y + (py-y)*i/steps) for i in range(steps+1))

    if clear(sx, sy, gx, gy):
        return [goal]
    queue = [(dmath.hypot(gx - sx, gy - sy), 0, 0, 0)]
    costs = {(0, 0): 0}
    prev = {}
    for _ in range(210):
        if not queue:
            break
        _, cost, ix, iy = heapq.heappop(queue)
        if cost != costs[ix, iy]:
            continue
        x, y = sx + ix * stride, sy + iy * stride
        remaining = dmath.hypot(gx - x, gy - y)
        if remaining <= TILE * 2 and clear(x, y, gx, gy):
            out = [goal]
            while (ix, iy) != (0, 0):
                out.append((sx + ix * stride, sy + iy * stride))
                ix, iy = prev[ix, iy]
            return out[::-1] if flying else smooth_path(gmap, start, out[::-1], radius=radius, occupied=occupied)
        for dx, dy, weight in DIRS:
            nx, ny = ix + dx, iy + dy
            px, py = sx + nx * stride, sy + ny * stride
            new = cost + weight * stride
            if max(abs(nx), abs(ny)) > 24 or new >= costs.get((nx, ny), math.inf):
                continue
            # Az indulópont érintkezhet a blokkolóval. Az első lépés csak távolodhat.
            if (ix, iy) == (0, 0) and occupied(sx, sy):
                legal = not occupied(px, py) and (flying or fits(gmap, px, py, radius))
            else:
                legal = clear(x, y, px, py)
            if legal:
                costs[nx, ny] = new
                prev[nx, ny] = (ix, iy)
                heapq.heappush(queue, (new + dmath.hypot(gx - px, gy - py), new, nx, ny))
    return []
