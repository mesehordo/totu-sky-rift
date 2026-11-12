"""Látványos pályák: kitaposott ösvények és talajfoltok, mozgó víz part menti habbal, tájankénti részecskék.

Minden itt tisztán kozmetikai: saját véletlenszám-generátort használ (a pálya magjából), a szimulációt nem érinti.
"""
import heapq
import math
import random

import numpy as np
import pygame

from . import canvas as gdraw
from .world import DIRT, GRASS, ROCK, TILE, WATER

PX = 8  # a foltréteg felbontása: képpont / csempe


# ---------------------------------------------------------------------------------------------------------------
# zaj és simítás
def _upsample(a, f):
    """Kétlineáris nagyítás f-szeresére (tengelyenként np.interp)."""
    h, w = a.shape

    def axis(n):
        p = np.clip((np.arange(n * f) + 0.5) / f - 0.5, 0, n - 1)
        i0 = np.floor(p).astype(np.int64)
        i1 = np.minimum(i0 + 1, n - 1)
        return i0, i1, (p - i0).astype(np.float32)
    x0, x1, wx = axis(w)
    y0, y1, wy = axis(h)
    a = a.astype(np.float32)
    t = a[:, x0] * (1 - wx) + a[:, x1] * wx
    return t[y0, :] * (1 - wy)[:, None] + t[y1, :] * wy[:, None]


def _value_noise(rng, h, w, cell):
    g = rng.random((h // cell + 2, w // cell + 2))
    return _upsample(g, cell)[:h, :w]


def _fbm(rng, h, w, cells=(14, 6, 3), amps=(0.55, 0.3, 0.15)):
    out = np.zeros((h, w))
    for c, a in zip(cells, amps):
        out += _value_noise(rng, h, w, c) * a
    return out


def _blur(a, n=1):
    for _ in range(n):
        p = np.pad(a, 1, mode="edge")
        a = (p[1:-1, 1:-1] * 4 + p[:-2, 1:-1] + p[2:, 1:-1] + p[1:-1, :-2] + p[1:-1, 2:]) / 8
    return a


# ---------------------------------------------------------------------------------------------------------------
class Scenery:
    """Egy pálya díszítő rétegei (egyszer számolva, a megjelenítéshez)."""

    # rétegszínek (a táj-átfestés előtt; a tél/pusztaság/mocsár szűrő ezeket is átfesti)
    COLORS = {
        "path": (128, 104, 70),
        "path_rim": (78, 60, 40),
        "moss": (46, 92, 34),
        "dry": (150, 144, 74),
        "litter": (112, 70, 34),
        "mud": (58, 56, 40),
        "crack": (70, 56, 40),
        "bare": (104, 92, 76),
    }

    def __init__(self, game, theme):
        self.g = game
        self.theme = theme
        m = game.map
        self.w, self.h = m.w, m.h
        self.rng = np.random.default_rng((int(game.seed or 0) * 7919 + 1234567) & 0xFFFFFFFF)
        terr = np.array(m.terrain, dtype=np.int16).reshape(m.h, m.w)
        self.terr = terr
        wood = np.zeros((m.h, m.w))
        for r in m.resources:
            if r.kind == "wood":
                for y in range(r.ty, r.ty + r.w):
                    for x in range(r.tx, r.tx + r.w):
                        if 0 <= x < m.w and 0 <= y < m.h:
                            wood[y, x] = 1
        self.wood = wood
        self.forest = np.clip(_blur(wood, 3) * 3.0, 0, 1)        # erdő közelsége
        self.path = self._paths()
        self.vterr = self._organic_soil()
        self.meadow = _fbm(self.rng, m.h, m.w)                     # virágos rétek (díszletek csoportosítása)
        land = ((terr == GRASS) | (terr == DIRT)).astype(float)
        self.land = land
        self.water = terr == WATER
        self._overlay = None
        self._water_frames = {}
        self._foam_frames = {}
        self._shore = None

    def _organic_soil(self):
        """Megjelenítéshez: a fű és föld határa kerekded, zajos (a pályagenerátor foltjai szögletesek).
        Víz és szikla nem változik; a kezdőhelyek környéke is a régi marad."""
        m = self.g.map
        terr = self.terr
        dirt = (terr == DIRT).astype(float)
        soft = _blur(dirt, 3)
        n = _fbm(self.rng, m.h, m.w, cells=(4, 2), amps=(0.6, 0.4))
        v = soft + (n - 0.5) * 0.55
        land = (terr == GRASS) | (terr == DIRT)
        self.soil_field = np.where(land, v, 0.0)
        out = np.where(land, np.where(v > 0.5, DIRT, GRASS), terr)
        # a csempék alapja: minden szárazföld fű, a föld sima maszkkal kerül rá (paint_soil_*)
        self.vbase = [int(t) for t in np.where(land | (terr == WATER), GRASS, terr).reshape(-1)]  # a víz is maszkkal
        return [int(t) for t in out.reshape(-1)]

    # ---- ösvények ---------------------------------------------------------------------------------------------
    def _paths(self):
        """Kitaposott ösvények a kezdőhelyek, a középpont és a közeli aranybányák között (fél felbontású
        Dijkstra, kanyargós zajköltséggel)."""
        m = self.g.map
        H, W = (m.h + 1) // 2, (m.w + 1) // 2
        cost = np.full((H, W), 1.0)
        noise = _fbm(self.rng, H, W, cells=(5, 2), amps=(0.65, 0.35))
        for y in range(H):
            for x in range(W):
                c = 0.0
                for yy in (2 * y, 2 * y + 1):
                    for xx in (2 * x, 2 * x + 1):
                        if xx >= m.w or yy >= m.h:
                            continue
                        t = m.terrain[yy * m.w + xx]
                        if t in (WATER, ROCK):
                            c += 40
                        elif t == DIRT:
                            c += 0.15
                        else:
                            c += 0.25
                        r = m.res_grid[yy * m.w + xx]
                        if r is not None:
                            c += 6 if r.kind == "wood" else 40
                cost[y, x] = c + noise[y, x] ** 2 * 6.0  # kanyargós: a zaj „dombjait” kerüli
        starts = [(min(W - 1, int(sx) // 2), min(H - 1, int(sy) // 2)) for sx, sy in m.starts[:len(self.g.players)]]
        if not starts:
            starts = [(int(sx) // 2, int(sy) // 2) for sx, sy in m.starts]
        out = np.zeros((H, W))
        if not starts:
            return _upsample(out, 2)[:m.h, :m.w]
        center = (W // 2, H // 2)
        n = len(starts)
        ang = sorted(range(n), key=lambda i: math.atan2(starts[i][1] - center[1], starts[i][0] - center[0]))
        golds = [((r.tx + r.w // 2) // 2, (r.ty + r.w // 2) // 2) for r in m.resources if r.kind == "stone"]
        for k, i in enumerate(ang):
            s = starts[i]
            targets = [center]
            if n > 1:
                targets.append(starts[ang[(k + 1) % n]])
            near = sorted(golds, key=lambda gp: (gp[0] - s[0]) ** 2 + (gp[1] - s[1]) ** 2)[:1]
            targets += [gp for gp in near if (gp[0] - s[0]) ** 2 + (gp[1] - s[1]) ** 2 < 15 ** 2]
            pts = [(pt.tx // 2, pt.ty // 2) for pt in getattr(self.g, "points", ())]  # Hasadékkövekhez is
            targets += sorted(pts, key=lambda q: (q[0] - s[0]) ** 2 + (q[1] - s[1]) ** 2)[:2]
            prev = self._dijkstra(cost, s)
            for t in targets:
                node = t
                guard = 0
                while node is not None and guard < W * H:
                    out[node[1], node[0]] = 1.0
                    node = prev.get(node)
                    guard += 1
        out = _upsample(out, 2)[:m.h, :m.w]
        wid = 0.75 + self.rng.random() * 0.1 + _fbm(self.rng, m.h, m.w, cells=(8, 3), amps=(0.5, 0.5)) * 0.5
        return np.clip(out * 1.15 * wid, 0, 1)

    @staticmethod
    def _dijkstra(cost, src):
        H, W = cost.shape
        dist = {src: 0.0}
        prev = {src: None}
        pq = [(0.0, src)]
        nb = ((1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0),
              (1, 1, 1.41), (1, -1, 1.41), (-1, 1, 1.41), (-1, -1, 1.41))
        while pq:
            d, (x, y) = heapq.heappop(pq)
            if d > dist.get((x, y), 1e18):
                continue
            for dx, dy, f in nb:
                nx, ny = x + dx, y + dy
                if 0 <= nx < W and 0 <= ny < H:
                    nd = d + cost[ny, nx] * f
                    if nd < dist.get((nx, ny), 1e18):
                        dist[(nx, ny)] = nd
                        prev[(nx, ny)] = (x, y)
                        heapq.heappush(pq, (nd, (nx, ny)))
        return prev

    def on_path(self, x, y):
        return self.path[y, x] > 0.45

    def cluster(self, x, y):
        """Díszlet-sűrűség szorzó (0.2–2.2): réteken sűrű virág, máshol ritka – csoportosan, nem egyenletesen."""
        return 0.2 + 2.0 * max(0.0, (self.meadow[y, x] - 0.42) / 0.3) if self.meadow[y, x] > 0.42 else 0.25

    # ---- talajfoltok ------------------------------------------------------------------------------------------
    def overlay(self):
        """A teljes pálya foltrétege RGBA képként, PX képpont/csempe felbontásban."""
        if self._overlay is not None:
            return self._overlay
        h, w = self.h, self.w
        rng = self.rng
        th = self.theme
        grass = np.clip((0.5 - self.soil_field) * 4, 0, 1) * np.clip(1 - self.path * 1.5, 0, 1)  # fű: nem a földön
        soil = self.land
        # tájanként más foltok és arányok
        if th == "wasteland":
            plan = [("dry", grass, 0.48, 0.85), ("crack", soil, 0.6, 0.55), ("bare", soil, 0.55, 0.6)]
        elif th == "swamp":
            plan = [("moss", grass, 0.44, 0.75), ("mud", soil, 0.56, 0.7), ("litter", grass, 0.0, 0.5)]
        elif th == "winter":
            plan = [("bare", soil, 0.6, 0.7), ("moss", grass, 0.55, 0.5)]
        elif th == "autumn":
            plan = [("dry", grass, 0.5, 0.7), ("litter", grass, 0.0, 0.85), ("moss", grass, 0.62, 0.45)]
        elif th == "rift":
            plan = [("crack", soil, 0.5, 0.7), ("bare", soil, 0.5, 0.6), ("moss", grass, 0.6, 0.4)]
        else:
            plan = [("moss", grass, 0.5, 0.65), ("dry", grass, 0.6, 0.55), ("litter", grass, 0.0, 0.65)]
        H, Wp = h * PX, w * PX
        rgb = np.zeros((H, Wp, 3))
        alpha = np.zeros((H, Wp))
        grain = rng.random((H, Wp))
        keep = _blur(soil, 1)  # a nem szárazföldi csempék felé elhalványul (ott az átmenetek takarnak)
        for name, mask, thr, amax in plan:
            if name == "litter":
                field = self.forest * (0.6 + 0.4 * _fbm(rng, h, w, cells=(5, 2), amps=(0.6, 0.4)))
                thr = 0.35
            else:
                field = _fbm(rng, h, w)
            a = _upsample(np.clip((field - thr) / 0.18, 0, 1) * mask * keep, PX)[:H, :Wp]
            # szemcsés, organikus szél
            fine = grain if name != "crack" else (grain > 0.82).astype(float)
            a = np.clip((a + (fine - 0.5) * 0.35 - 0.3) / 0.35, 0, 1) * amax
            if name in ("litter", "crack"):
                a *= (rng.random((H, Wp)) > (0.45 if name == "litter" else 0.0))
            col = np.array(self.COLORS[name], dtype=float)
            rgb = rgb * (1 - a[..., None]) + col * a[..., None]
            alpha = alpha + a * (1 - alpha)
        # premultiplikált → egyenes alfa
        out_rgb = np.where(alpha[..., None] > 1e-3, rgb / np.maximum(alpha[..., None], 1e-3), 0)
        surf = pygame.Surface((Wp, H), pygame.SRCALPHA)
        px3 = pygame.surfarray.pixels3d(surf)
        px3[...] = np.clip(out_rgb, 0, 255).astype(np.uint8).transpose(1, 0, 2)
        del px3
        pa_ = pygame.surfarray.pixels_alpha(surf)
        pa_[...] = np.clip(alpha * 255, 0, 255).astype(np.uint8).T
        del pa_
        self._overlay = surf
        return surf

    def soil_masks(self):
        """A föld és az ösvények közös, csempefüggetlen maszkja (PX felbontás) és a sötét, festett pereme."""
        if getattr(self, "_soil", None) is not None:
            return self._soil
        h, w = self.h, self.w
        H, Wp = h * PX, w * PX
        rng = np.random.default_rng((self.g.seed + 99) & 0xFFFFFFFFFFFFFFFF)
        grain = rng.random((H, Wp))
        keep = _upsample(_blur(self.land, 1), PX)[:H, :Wp]
        d = _upsample(self.soil_field, PX)[:H, :Wp]
        da = np.clip((d + (grain - 0.5) * 0.22 - 0.5) / 0.08, 0, 1)
        p = _upsample(self.path, PX)[:H, :Wp] * keep
        pa = np.clip((p + (grain - 0.5) * 0.3 - 0.38) / 0.14, 0, 1)
        a = np.maximum(da, pa)
        b = _blur(a, 3)
        rim = np.clip((b - a) * 3.2, 0, 1) * 0.75
        out = []
        for col, al in (((255, 255, 255), a), (self.COLORS["path_rim"], rim)):
            sf = pygame.Surface((Wp, H), pygame.SRCALPHA)
            sf.fill(col)
            pa_ = pygame.surfarray.pixels_alpha(sf)
            pa_[...] = np.clip(al * 255, 0, 255).astype(np.uint8).T
            del pa_
            out.append(sf)
        self._soil = tuple(out)
        return self._soil

    def water_masks(self):
        """Lekerekített vízfelület (PX felbontás) és a homokos part sávja."""
        if getattr(self, "_wm", None) is not None:
            return self._wm
        h, w = self.h, self.w
        H, Wp = h * PX, w * PX
        rng = np.random.default_rng((self.g.seed + 7) & 0xFFFFFFFFFFFFFFFF)
        grain = rng.random((H, Wp))
        wt = _upsample(self.water.astype(float), PX)[:H, :Wp]
        wt = _blur(wt, 3)
        a = np.clip((wt + (grain - 0.5) * 0.06 - 0.5) / 0.06, 0, 1)
        self._wa = a
        far = _blur(a, 6)
        sand = np.clip((far - a) * 4.0 + (grain - 0.5) * 0.25, 0, 1) * 0.95
        sand = np.maximum(sand, 0) * (1 - a)
        out = []
        for al in (a, sand):
            sf = pygame.Surface((Wp, H), pygame.SRCALPHA)
            sf.fill((255, 255, 255))
            pa_ = pygame.surfarray.pixels_alpha(sf)
            pa_[...] = np.clip(al * 255, 0, 255).astype(np.uint8).T
            del pa_
            out.append(sf)
        self._wm = tuple(out)
        return self._wm

    def _paint_tex(self, surf, mask, x0, y0, x1, y1, ox, oy, T, tile_img):
        rw, rh = x1 - x0, y1 - y0
        area = (x0 * PX, y0 * PX, rw * PX, rh * PX)
        tex = pygame.Surface((rw * T, rh * T), pygame.SRCALPHA)
        for y in range(y0, y1):
            for x in range(x0, x1):
                tex.blit(tile_img(x, y), ((x - x0) * T, (y - y0) * T))
        tex.blit(pygame.transform.smoothscale(mask.subsurface(area), (rw * T, rh * T)), (0, 0),
                 special_flags=pygame.BLEND_RGBA_MULT)
        surf.blit(tex, (ox, oy))

    def _paint_water(self, surf, x0, y0, x1, y1, ox, oy, T, sand_img, water_img):
        if not self.water[max(0, y0 - 4):y1 + 4, max(0, x0 - 4):x1 + 4].any():  # a homoksáv messzebb ér
            return
        wm, sand = self.water_masks()
        self._paint_tex(surf, sand, x0, y0, x1, y1, ox, oy, T, sand_img)
        self._paint_tex(surf, wm, x0, y0, x1, y1, ox, oy, T, water_img)

    def paint_water_full(self, surf, sand_img, water_img):
        for y0 in range(0, self.h, 16):
            self._paint_water(surf, 0, y0, self.w, min(self.h, y0 + 16), 0, y0 * TILE, TILE, sand_img, water_img)

    def paint_water_chunk(self, surf, tx0, ty0, n, T, sand_img, water_img):
        x0, y0 = max(0, tx0 - 1), max(0, ty0 - 1)
        x1, y1 = min(self.w, tx0 + n + 1), min(self.h, ty0 + n + 1)
        if x1 > x0 and y1 > y0:
            self._paint_water(surf, x0, y0, x1, y1, (x0 - tx0) * T, (y0 - ty0) * T, T, sand_img, water_img)

    def _paint_soil(self, surf, x0, y0, x1, y1, ox, oy, T, tile_img):
        mask, rim = self.soil_masks()
        rw, rh = x1 - x0, y1 - y0
        area = (x0 * PX, y0 * PX, rw * PX, rh * PX)
        tex = pygame.Surface((rw * T, rh * T), pygame.SRCALPHA)
        for y in range(y0, y1):
            for x in range(x0, x1):
                tex.blit(tile_img(x, y), ((x - x0) * T, (y - y0) * T))
        tex.blit(pygame.transform.smoothscale(mask.subsurface(area), (rw * T, rh * T)), (0, 0),
                 special_flags=pygame.BLEND_RGBA_MULT)
        surf.blit(pygame.transform.smoothscale(rim.subsurface(area), (rw * T, rh * T)), (ox, oy))
        surf.blit(tex, (ox, oy))

    def paint_soil_full(self, surf, tile_img):
        step = 16
        for y0 in range(0, self.h, step):
            y1 = min(self.h, y0 + step)
            self._paint_soil(surf, 0, y0, self.w, y1, 0, y0 * TILE, TILE, tile_img)

    def paint_soil_chunk(self, surf, tx0, ty0, n, T, tile_img):
        x0, y0 = max(0, tx0 - 1), max(0, ty0 - 1)
        x1, y1 = min(self.w, tx0 + n + 1), min(self.h, ty0 + n + 1)
        if x1 > x0 and y1 > y0:
            self._paint_soil(surf, x0, y0, x1, y1, (x0 - tx0) * T, (y0 - ty0) * T, T, tile_img)

    def paint_full(self, surf):
        """A foltréteg a teljes (retró) terepképre, sávonként felnagyítva."""
        ov = self.overlay()
        f = TILE / PX
        step = 16
        for y0 in range(0, self.h, step):
            hh = min(step, self.h - y0)
            src = ov.subsurface((0, y0 * PX, self.w * PX, hh * PX))
            surf.blit(pygame.transform.smoothscale(src, (int(self.w * PX * f), int(hh * PX * f))), (0, y0 * TILE))

    def paint_chunk(self, surf, tx0, ty0, n, T):
        """A foltréteg egy HD terepdarabra (1 csempe ráhagyással, hogy a darabhatár ne látsszon)."""
        ov = self.overlay()
        x0, y0 = max(0, tx0 - 1), max(0, ty0 - 1)
        x1, y1 = min(self.w, tx0 + n + 1), min(self.h, ty0 + n + 1)
        if x1 <= x0 or y1 <= y0:
            return
        src = ov.subsurface((x0 * PX, y0 * PX, (x1 - x0) * PX, (y1 - y0) * PX))
        big = pygame.transform.smoothscale(src, ((x1 - x0) * T, (y1 - y0) * T))
        surf.blit(big, ((x0 - tx0) * T, (y0 - ty0) * T))

    # ---- víz ----------------------------------------------------------------------------------------------------
    WATER_FRAMES = 16

    def _water_tint(self):
        return {"winter": (150, 190, 230), "swamp": (120, 170, 110), "wasteland": (170, 190, 200),
                "rift": (200, 150, 255), "autumn": (190, 200, 210)}.get(
            self.theme, (150, 205, 255))

    def water_frames(self, k):
        """Csempényi, varratmentesen ismétlődő csillogás-képkockák (a mintázat periódusa pontosan egy csempe)."""
        fr = self._water_frames.get(k)
        if fr is not None:
            return fr
        S = int(round(TILE * k))
        u = (np.arange(S) + 0.5) / S
        X, Y = np.meshgrid(u, u, indexing="ij")
        tint = np.array(self._water_tint(), dtype=float)
        speed = 0.5 if self.theme == "swamp" else 1.0
        out = []
        for i in range(self.WATER_FRAMES):
            ph = 2 * math.pi * i / self.WATER_FRAMES
            v = (np.sin(2 * math.pi * (2 * X + Y) + ph) + np.sin(2 * math.pi * (X - 2 * Y) - ph * speed)
                 + np.sin(2 * math.pi * (3 * X + 1 * Y) + 2 * ph) * 0.6 + np.sin(2 * math.pi * (1 * X + 3 * Y) - ph) * 0.6)
            hi = np.clip((v - 1.55) / 1.0, 0, 1) ** 1.5
            s = pygame.Surface((S, S))
            a = pygame.surfarray.pixels3d(s)
            a[...] = (hi[..., None] * tint * (0.45 if self.theme == "winter" else 0.6)).astype(np.uint8)
            del a
            if k > 1:
                gdraw.mark_hd(s)
            out.append(s)
        self._water_frames[k] = out
        return out

    def interior(self):
        """Vízcsempék, amelyeknek mind a nyolc szomszédja is víz (a csillogás csak ezeken – a part lekerekített)."""
        if getattr(self, "_interior", None) is None:
            w = self.water
            p = np.pad(w, 1, mode="edge")
            out = w.copy()
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    out &= p[1 + dy:1 + dy + w.shape[0], 1 + dx:1 + dx + w.shape[1]]
            self._interior = out
        return self._interior

    def foam_layers(self):
        """Két hab-vonal a part mentén (PX felbontás): a partközeli és egy beljebb futó; felváltva lélegeznek."""
        if getattr(self, "_foam", None) is not None:
            return self._foam
        self.water_masks()
        a = self._wa
        e = _blur(a, 4)
        rng = np.random.default_rng((self.g.seed + 31) & 0xFFFFFFFFFFFFFFFF)
        speck = 0.55 + 0.45 * rng.random(a.shape)
        l1 = np.clip(1 - np.abs(e - 0.62) / 0.12, 0, 1) * a * speck
        l2 = np.clip(1 - np.abs(e - 0.86) / 0.06, 0, 1) * a * speck * 0.8
        col = (235, 245, 250) if self.theme != "swamp" else (205, 220, 185)
        out = []
        for al in (l1, l2):
            sf = pygame.Surface((a.shape[1], a.shape[0]), pygame.SRCALPHA)
            sf.fill(col)
            pa_ = pygame.surfarray.pixels_alpha(sf)
            pa_[...] = np.clip(al * 235, 0, 255).astype(np.uint8).T
            del pa_
            out.append(sf)
        self._foam = tuple(out)
        self._foam_cache = {}
        return self._foam

    CH = 8

    def _foam_chunk(self, ci, cj, k):
        key = (ci, cj, k)
        c = self._foam_cache.get(key)
        if c is not None or key in self._foam_cache:
            return c
        N = self.CH
        x0, y0 = ci * N, cj * N
        x1, y1 = min(self.w, x0 + N), min(self.h, y0 + N)
        if not self.water[max(0, y0 - 1):y1 + 1, max(0, x0 - 1):x1 + 1].any():
            self._foam_cache[key] = None
            return None
        area = (x0 * PX, y0 * PX, (x1 - x0) * PX, (y1 - y0) * PX)
        T = int(round(TILE * k))
        c = []
        for lay in self._foam:
            sc = pygame.transform.smoothscale(lay.subsurface(area), ((x1 - x0) * T, (y1 - y0) * T))
            r = sc.get_bounding_rect()  # a habréteg nagy része üres: csak a hab sávja marad
            if r.w <= 0 or r.h <= 0:
                c.append(None)
                continue
            sc = sc.subsurface(r).copy()
            if k > 1:
                gdraw.mark_hd(sc)
            c.append((sc, r.x / k, r.y / k))
        self._foam_cache[key] = c
        if len(self._foam_cache) > 160:
            self._foam_cache.pop(next(iter(self._foam_cache)))
        return c

    def draw_water(self, s, cx, cy, x0, y0, x1, y1, t, k, explored, W):
        if not self.water.any():
            return
        frames = self.water_frames(k)
        wf = frames[int(t * 7) % len(frames)]
        inner = self.interior()
        m = self.g.map
        for ty in range(y0, min(m.h, y1)):
            row = ty * W
            for tx in range(x0, x1):
                if inner[ty, tx] and explored[row + tx]:
                    s.blit(wf, (tx * TILE - cx, ty * TILE - cy), special_flags=pygame.BLEND_RGB_ADD)
        # hab: a sima partvonalon, két vonal felváltva (ki-be futó hullám)
        self.foam_layers()
        ph = math.sin(t * 1.4)
        al = (int(90 + 140 * max(0.0, ph)), int(90 + 140 * max(0.0, -ph)))
        N = self.CH
        for cj in range(max(0, y0 // N), min((self.h + N - 1) // N, y1 // N + 1)):
            for ci in range(max(0, x0 // N), min((self.w + N - 1) // N, x1 // N + 1)):
                c = self._foam_chunk(ci, cj, k)
                if c is None:
                    continue
                for lay, a in zip(c, al):
                    if lay is None:
                        continue
                    sf, ox, oy = lay
                    sf.set_alpha(a)
                    s.blit(sf, (ci * N * TILE - cx + ox, cj * N * TILE - cy + oy))


# ---------------------------------------------------------------------------------------------------------------
class Ambient:
    """Tájankénti környezeti részecskék a világ-koordinátákhoz rögzítve (a kamerával együtt mozognak)."""

    def __init__(self, theme, seed=0):
        self.theme = theme
        r = random.Random(seed * 31 + 5)
        self.parts = []
        specs = {
            "forest": [("firefly", 34), ("leaf", 22), ("pollen", 30)],
            "winter": [("snow", 150), ("snowbig", 30)],
            "wasteland": [("dust", 70), ("streak", 10), ("ember", 8)],
            "swamp": [("mist", 9), ("firefly", 40), ("spore", 30)],
            "autumn": [("leaf", 70), ("pollen", 12), ("firefly", 10)],
            "rift": [("mote", 46), ("spore", 24), ("mist", 4)],
        }.get(theme, [("firefly", 34), ("leaf", 22), ("pollen", 30)])
        for kind, n in specs:
            for _ in range(n):
                self.parts.append((kind, r.random(), r.random(), r.random(), r.random()))
        self._spr = {}

    def _sprite(self, kind, k, var=0):
        key = (kind, k, var)
        sp = self._spr.get(key)
        if sp is not None:
            return sp
        if kind == "glow":
            col = [(255, 240, 140), (190, 255, 150), (150, 255, 200), (190, 120, 255)][var]
            R = max(2, int(round(6 * k)))  # additív ragyogás: fekete háttér, a középpont felé fényesedő szín
            x = np.linspace(-1, 1, R * 2)[:, None]
            y = np.linspace(-1, 1, R * 2)[None, :]
            f = np.clip(1 - np.sqrt(x * x + y * y), 0, 1) ** 2.2
            sp = pygame.Surface((R * 2, R * 2))
            a3 = pygame.surfarray.pixels3d(sp)
            a3[...] = (f[..., None] * np.array(col, dtype=float) * 0.9).astype(np.uint8)
            del a3
        elif kind == "mist":
            w, h = int(220 * k), int(90 * k)
            sp = pygame.Surface((w, h), pygame.SRCALPHA)
            x = np.linspace(-1, 1, w)[:, None]
            y = np.linspace(-1, 1, h)[None, :]
            a = np.clip(1 - (x * x + y * y), 0, 1) ** 1.6 * 46
            sp.fill((200, 220, 205))
            pa = pygame.surfarray.pixels_alpha(sp)
            pa[...] = a.astype(np.uint8)
            del pa
        elif kind == "leaf":
            col = [(176, 110, 40), (196, 150, 52), (140, 80, 36)][var]
            sp = pygame.Surface((max(2, int(6 * k)), max(2, int(4 * k))), pygame.SRCALPHA)
            pygame.draw.ellipse(sp, col, sp.get_rect())
        elif kind == "dot":
            col = [(255, 255, 255), (230, 205, 150), (255, 160, 80), (240, 240, 200)][var]
            R = max(1, int(round(k)))
            sp = pygame.Surface((R * 2 + 1, R * 2 + 1), pygame.SRCALPHA)
            pygame.draw.circle(sp, col, (R, R), R)
        if k > 1:
            gdraw.mark_hd(sp)
        self._spr[key] = sp
        return sp

    def draw(self, s, cx, cy, vw, vh, t, k):
        M = 60
        BW, BH = vw + 2 * M, vh + 2 * M
        for kind, a, b, c, d in self.parts:
            if kind in ("firefly", "spore", "pollen", "mote"):
                vx, vy = (c - 0.5) * 14, (d - 0.5) * 10
                x = a * 4000 + vx * t + math.sin(t * (0.7 + c) + d * 9) * 18
                y = b * 4000 + vy * t + math.cos(t * (0.9 + d) + c * 7) * 12
                if kind in ("spore", "mote"):
                    y -= t * (9 if kind == "spore" else 14)  # a meteorpor lassan felfelé száll
            elif kind == "leaf":
                x = a * 4000 + t * (16 + c * 14) + math.sin(t * (1.2 + d) + c * 6) * 22
                y = b * 4000 + t * (22 + d * 14)
            elif kind in ("snow", "snowbig"):
                sp = 26 + c * 20 if kind == "snow" else 46 + c * 20
                x = a * 4000 + t * 10 + math.sin(t * (0.8 + d) + c * 9) * 10
                y = b * 4000 + t * sp
            elif kind in ("dust", "streak", "ember"):
                x = a * 4000 + t * (60 + c * 70) * (1.0 if kind != "ember" else 0.4)
                y = b * 4000 + math.sin(t * (0.6 + d) + c * 8) * 14 - (t * 12 if kind == "ember" else 0)
            else:  # mist
                x = a * 4000 + t * (6 + c * 6)
                y = b * 4000 + math.sin(t * 0.2 + d * 6) * 20
            # világhoz rögzítve, a látómező körüli dobozba csomagolva
            sx = (x - cx) % BW - M
            sy = (y - cy) % BH - M
            if kind == "mote":
                pulse = 0.5 + 0.5 * math.sin(t * (1.1 + c) + d * 20)
                g = self._sprite("glow", k, 3)
                gw = g.get_width() / k
                if pulse > 0.3:
                    s.blit(g, (sx - gw / 2, sy - gw / 2), special_flags=pygame.BLEND_RGB_ADD)
            elif kind == "firefly":
                pulse = 0.5 + 0.5 * math.sin(t * (1.5 + c * 2) + d * 20)
                if pulse < 0.25:
                    continue
                g = self._sprite("glow", k, 1 if self.theme == "swamp" else 0)
                g2 = g if pulse > 0.6 else pygame.transform.smoothscale(g, (max(2, int(g.get_width() * 0.6)),) * 2)
                if k > 1 and g2 is not g:
                    gdraw.mark_hd(g2)
                gw = g2.get_width() / k
                s.blit(g2, (sx - gw / 2, sy - gw / 2), special_flags=pygame.BLEND_RGB_ADD)
            elif kind in ("pollen", "spore"):
                g = self._sprite("dot", k, 3 if kind == "pollen" else 0)
                s.blit(g, (sx, sy))
            elif kind == "leaf":
                lf = self._sprite("leaf", k, int(c * 3) % 3)
                ang = (t * (90 + d * 120) + a * 360) % 360
                rl = pygame.transform.rotate(lf, ang)
                if k > 1:
                    gdraw.mark_hd(rl)
                s.blit(rl, (sx, sy))
            elif kind in ("snow", "snowbig"):
                g = self._sprite("dot", k, 0)
                s.blit(g, (sx, sy))
                if kind == "snowbig":
                    s.blit(g, (sx + 1, sy))
            elif kind == "dust":
                s.blit(self._sprite("dot", k, 1), (sx, sy))
            elif kind == "streak":
                gdraw.line(s, (200, 176, 120), (sx, sy), (sx + 26, sy + 1), 1)
            elif kind == "ember":
                if math.sin(t * 3 + d * 10) > -0.3:
                    s.blit(self._sprite("dot", k, 2), (sx, sy))
            else:
                ms = self._sprite("mist", k)
                s.blit(ms, (sx - 110, sy - 45))
