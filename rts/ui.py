"""Kezelőfelület: kamera, kijelölés, világ-rajzolás; a HUD (alsó sáv, felső sáv, parancsrács) az rts/hud.py-ban."""
import math

import pygame

from . import heroes as H
from . import canvas as gdraw
from . import scenery as SCN
from . import calm as CALM
from . import mission as MS
from . import shadows as SHD
from . import vfx as VFX
from .hud import HudMixin, KEY_SLOT
from .entities import in_resource_shelter, rect_dist
from .fonts import load_font
from .render import (draw_building, draw_resource, draw_shape, make_icon, shade)
from .sprites import SpriteBank, dir_index
from .world import DIRT, GRASS, ROCK, TILE, WATER


HELP = [
    "IRÁNYÍTÁS",
    "Bal klikk / húzás: kijelölés  (Shift: hozzáadás, dupla klikk: azonos típus)",
    "Jobb klikk: mozgás / támadás / gyűjtés / segít építeni / javít / gyülekezőpont",
    "Parancsrács (jobb alul, a betű minden ikon sarkában):  W E R T / S D F G / X C V B",
    "   W: mozgás  E: állj  R: támadás  T: helyben tartás  S…B: képességek, építés, kiképzés, kutatás",
    "   Munkás: S = Építés (B: vissza, V: következő oldal)   Épület: kiképzés/kutatás, B: utolsó visszavonása",
    "Kijelölés-rács: kattintás = csak ezek, Shift/jobb klikk = kivétel; Shift + kiképzés: 5×",
    "Ctrl+1..9: csoport mentése, 1..9: csoport kijelölése (dupla: odaugrás)",
    "Hősök: bal oldali portré (dupla: odaugrás), tárgyak: bal klikk = használat, jobb klikk = eldobás",
    "F2/F3/F4: 1./2./3. hős kijelölése   F8: következő tétlen munkás   Szóköz: bázis / utolsó riasztás",
    "Nyilak / képernyőszél / középső gomb: kamera   + / - : játéksebesség   P: szünet   Esc: mégse / menü",
    "Felül: szövetségesek (három alak), erőforrások, fogaskerék = menü",
]


class UI(HudMixin):
    zoom = 2               # a HD-képek natív felbontása (a retró mód megszűnt)
    _world_phase = False
    _wsurf = None

    def __init__(self, game, screen, player, fonts):
        self.g = game
        self.screen = screen
        self.p = player
        self.base_fonts = fonts  # a kódból rajzolt ikonokhoz (fix méret)
        self.fonts = fonts
        self.resize(screen)
        vw, vh = self._wv()
        self.cam_x = player.start_xy[0] - vw / 2
        self.cam_y = player.start_xy[1] - vh / 2
        self.clamp_cam()
        self.sel = []
        self.groups = {}
        self.last_group = (None, 0)
        self.mode = None       # None | ("place", bdef) | ("attack",) | ("cast", ab_id)
        self.learn_mode = False  # hős: a képességgombok tanulásra váltanak
        self.portraits = []      # (rect, hős) – a bal oldali hősportrék
        self.res_flash = {}      # nyersanyag -> utolsó hiány ideje (felső sáv jelzés)
        self.held = set()        # lenyomva tartott billentyűk: az automatikus ismétlés nem indít újra parancsot
        pygame.key.set_repeat()  # játékban nincs billentyűismétlés
        self.drag = None
        self.mid_drag = None
        self.minimap_drag = False
        self.regions = []
        self.hover = None
        self.icons = {}
        self.last_click = (0, None)
        self.paused = False
        self.show_help = False
        self.speed = 1.0
        self.seen = set()
        self.fog_key = None
        self.fog_scaled = None
        self.creep_key = None
        self.creep_scaled = None
        self.fog_src = None
        self.fog_vis_obj = None
        self.mini_t = 0
        self.mini = None
        self.mini_base_ver = -1
        self.t = 0.0
        self.quit_to_menu = False
        self.request = None   # ("quicksave",) | ("quickload",) – a fő ciklus kezeli
        self.settings = {"scroll_speed": 900, "edge_scroll": True, "show_fps": True}
        self.net = None          # LAN: lockstep-munkamenet; a parancsok ezen át mennek
        self.recorder = None     # egyéni csata: parancsnapló (rts/replay.py)
        self.sprites = SpriteBank()
        self.sprites.aliases = {d["id"]: d["sprite"] for d in game.data.defs.values() if d.get("sprite")}
        self._bake_terrain()

    # ------------------------------------------------------------------
    def resize(self, screen):
        self.screen = screen
        self.sw, self.sh = screen.get_size()
        self.vw = self.sw          # a játéktér a teljes képernyő; a HUD alul rárajzolódik
        self.vh = self.sh
        from .fonts import INGAME, scaled_fonts
        from .menu import ui_scale
        self.u = ui_scale((self.sw, self.sh))
        self.fonts = scaled_fonts(pygame, INGAME, self.u)  # olvasható méret minden felbontáson
        self.hud_resize()

    def clamp_cam(self):
        mw, mh = self.g.map.w * TILE, self.g.map.h * TILE
        vw, vh = self._wv()
        self.cam_x = max(-40, min(mw - vw + 40, self.cam_x))
        hud = self.L["hud"].h / self.E  # a térkép alja a HUD fölé görgethető
        self.cam_y = max(-40, min(mh - vh + 40 + hud, self.cam_y))

    MINI_COLS = {GRASS: (68, 110, 52), DIRT: (118, 98, 66), WATER: (38, 74, 120), ROCK: (78, 74, 72)}

    def _bake_terrain(self):
        """Díszítő rétegek és a kistérkép; a teljes (retró) terepkép csak akkor készül el, ha kell (_bake_full)."""
        m = self.g.map
        self.scenery = SCN.Scenery(self.g, self.theme_name())  # ösvények, talajfoltok, víz (kozmetika)
        self.ambient = SCN.Ambient(self.theme_name(), int(self.g.seed or 0))
        self.vbase = self.scenery.vbase
        self.vterr = self.scenery.vterr  # a megjelenített talaj: a fű/föld határ organikus (a játékmenetre nincs hatása)
        mini = pygame.Surface((m.w, m.h))
        for y in range(m.h):
            for x in range(m.w):
                mini.set_at((x, y), self.MINI_COLS[self.vterr[y * m.w + x]])
        self.mini_terrain = mini
        self.terrain = None

    def _bake_full(self):
        m = self.g.map
        import random
        rng = random.Random(self.g.seed)
        surf = pygame.Surface((m.w * TILE, m.h * TILE))
        cols = self.MINI_COLS
        ts = self._tiles()
        if ts is not None:
            rows = ts.meta["rows"]
            names = {GRASS: "grass", DIRT: "dirt", WATER: "water", ROCK: "rock"}
            for y in range(m.h):
                for x in range(m.w):
                    info = rows[names[self.vbase[y * m.w + x]]]
                    surf.blit(ts.frame(None, rng.randrange(info["variants"]), info["row"]), (x * TILE, y * TILE))
        for y in range(m.h if ts is None else 0):
            for x in range(m.w):
                t = self.vterr[y * m.w + x]
                c = cols[t]
                j = rng.uniform(0.92, 1.08)
                rect = (x * TILE, y * TILE, TILE, TILE)
                gdraw.rect(surf, shade(c, j), rect)
                if t == GRASS:
                    for _ in range(4):
                        px, py = x * TILE + rng.randrange(TILE), y * TILE + rng.randrange(TILE)
                        gdraw.line(surf, shade(c, 1.25), (px, py), (px + 1, py - 3))
                elif t == DIRT:
                    for _ in range(3):
                        gdraw.circle(surf, shade(c, 0.8), (x * TILE + rng.randrange(TILE), y * TILE + rng.randrange(TILE)), 1)
                elif t == WATER:
                    for _ in range(2):
                        px, py = x * TILE + rng.randrange(TILE - 8), y * TILE + rng.randrange(TILE)
                        gdraw.line(surf, (70, 110, 160), (px, py), (px + 7, py))
                elif t == ROCK:
                    gdraw.polygon(surf, (100, 96, 92), [(x * TILE + 4, y * TILE + 26), (x * TILE + 14, y * TILE + 5),
                                                             (x * TILE + 28, y * TILE + 22)])
                    gdraw.polygon(surf, (55, 52, 50), [(x * TILE + 14, y * TILE + 5), (x * TILE + 28, y * TILE + 22),
                                                            (x * TILE + 20, y * TILE + 28)])
        if ts is not None:
            drow = rows["dirt"]
            self.scenery.paint_soil_full(surf, lambda x, y: ts.frame(None, (x * 7 + y * 13) % drow["variants"], drow["row"]))
            self.scenery.paint_full(surf)
            srow, wrow = rows["sand"], rows["water"]
            self.scenery.paint_water_full(
                surf, lambda x, y: ts.frame(None, (x * 5 + y * 11) % srow["variants"], srow["row"]),
                lambda x, y: ts.frame(None, (x * 3 + y * 7) % wrow["variants"], wrow["row"]))
            self._decals(surf, range(m.w), range(m.h), 0, 0, TILE, lambda sh, i: sh.frame(None, i))
            self._terrain_transitions(surf, ts, rng)
        # sima átmenet: vízpart (csak sprite-ok nélkül)
        for y in range(m.h if ts is None else 0):
            for x in range(m.w):
                if self.vterr[y * m.w + x] == WATER:
                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        nx, ny = x + dx, y + dy
                        if m.inb(nx, ny) and self.vterr[ny * m.w + nx] != WATER:
                            r = pygame.Rect(x * TILE, y * TILE, TILE, TILE)
                            if dx == 1:
                                r = pygame.Rect(x * TILE + TILE - 3, y * TILE, 3, TILE)
                            elif dx == -1:
                                r = pygame.Rect(x * TILE, y * TILE, 3, TILE)
                            elif dy == 1:
                                r = pygame.Rect(x * TILE, y * TILE + TILE - 3, TILE, 3)
                            else:
                                r = pygame.Rect(x * TILE, y * TILE, TILE, 3)
                            gdraw.rect(surf, (150, 140, 100), r)
        self._apply_theme(surf, self.theme_name())
        self.terrain = surf

    def _tiles(self):
        return self.sprites.tiles(self.theme_name())

    def _tprop(self, kind):
        return self.sprites.themed("props", kind, self.theme_name()) or self.sprites.prop(kind)

    def _themed_art(self):
        """Van-e a tájnak saját csempekészlete (akkor nem kell a színeket átfesteni)."""
        return self.sprites.themed("tiles", "tileset", self.theme_name()) is not None

    def _decals(self, surf, xs, ys, tx0, ty0, T, frame_fn):
        """Talajmatricák (props/decals_<táj>): lapos, rálépehető 2×2-es foltok – romlap, gombakör, tócsa, virágszőnyeg."""
        dc = self.sprites.themed("props", "decals", self.theme_name())
        if dc is None:
            return
        n = dc.meta.get("frames", 1)
        sc = self.scenery
        m = self.g.map
        for y in ys:
            for x in xs:
                if x % 3 or y % 3:
                    continue
                rng = self._tile_rng(x, y, 7)
                if rng.random() > 0.12 or not sc.land[y, x] or sc.on_path(x, y) or m.res_grid[y * m.w + x] is not None:
                    continue
                if any(abs(x - sx) < 7 and abs(y - sy) < 7 for sx, sy in m.starts):
                    continue
                fr = frame_fn(dc, rng.randrange(n))
                jx, jy = rng.randrange(-T // 3, T // 3 + 1), rng.randrange(-T // 3, T // 3 + 1)
                surf.blit(fr, ((x - tx0) * T + jx - fr.get_width() // 2 + T, (y - ty0) * T + jy - fr.get_height() // 2 + T))

    def theme_name(self):
        theme = getattr(self.g, "options", {}).get("theme", "map")
        if theme in (None, "map"):
            theme = getattr(self.g.map, "theme", "forest")
        return theme

    # ------------------------------------------------------------------
    # HD edition: a terep 8×8 csempés darabokban, kétszeres felbontásban (csak a látható környék él)
    CHUNK = 8
    CHUNK_CACHE = 72

    VIEWS = (1.0, 0.875, 0.75, 0.625, 0.5)  # kamera-távolság (a csempe 64/56/48/40/32 képpont – mindig egész)

    @property
    def view(self):
        v = getattr(self, "settings", {}).get("view_scale", 0.75)
        return min(self.VIEWS, key=lambda x: abs(x - v))

    @property
    def E(self):
        """Képernyő-képpont / világ-képpont (a HD 2× és a kamera-távolság szorzata)."""
        return self.zoom * self.view

    def set_view(self, step):
        """Görgő: közelítés (-1) / távolítás (+1), a képernyő közepét megtartva."""
        vs = list(self.VIEWS)
        i = vs.index(self.view)
        j = max(0, min(len(vs) - 1, i + step))
        if j == i:
            return
        sw, sh = self.vw, self.L["hud"].y
        cx, cy = self.cam_x + sw / 2 / self.E, self.cam_y + sh / 2 / self.E
        self.settings["view_scale"] = vs[j]
        self.cam_x, self.cam_y = cx - sw / 2 / self.E, cy - sh / 2 / self.E
        self.clamp_cam()

    def wanted_zoom(self):
        """Mindig a HD-képek natív felbontása (2×). A retró mód és a pixelnagyítás megszűnt."""
        return 2

    def hd_on(self):
        """HD edition be van-e kapcsolva (beállítás + van HD grafika + 2× nagyítás)."""
        return self.zoom > 1 and self.sprites.has_hd()  # csak HD van (a retró mód megszűnt)

    def _hd_frame(self, sheet, col, row=0):
        """A sheet képkockája pontosan k-szoros (HD) méretben (ha a sheet maga nem HD, felnagyítva)."""
        k = self.E
        fr = sheet.frame(None, col, row)
        if sheet.scale == k:
            return fr
        key = ("hdfr", id(sheet), col, row)
        out = self._hd_cache.get(key)
        if out is None:
            f = k / sheet.scale
            out = pygame.transform.scale(fr, (int(fr.get_width() * f), int(fr.get_height() * f)))
            self._hd_cache[key] = out
        return out

    def _hd_anchor(self, sheet):
        f = self.E / sheet.scale
        return sheet.anchor_px[0] * f, sheet.anchor_px[1] * f

    def _tile_rng(self, x, y, layer):
        import random
        seed = int(self.g.seed or 0)
        return random.Random((seed * 1000003) ^ (x * 73856093) ^ (y * 19349663) ^ (layer * 83492791))

    def _tile_variant(self, x, y, layer, variants):
        cache = self.__dict__.setdefault("_terrain_variants", {})
        key = (self.g.seed, x, y, layer, variants)
        if key not in cache:
            if len(cache) >= 24000:
                cache.clear()
            cache[key] = self._tile_rng(x, y, layer).randrange(variants)
        return cache[key]

    def hd_chunk(self, ci, cj):
        """Egy terepdarab HD-képe (LRU-gyorsítótárban). A szomszédos csempék lelógó elemei is rákerülnek."""
        from collections import OrderedDict
        if not hasattr(self, "_chunks"):
            self._chunks = OrderedDict()
            self._hd_cache = {}
        key = (ci, cj, self.theme_name(), self.E)
        ch = self._chunks.get(key)
        if ch is not None:
            self._chunks.move_to_end(key)
            return ch
        started = pygame.time.get_ticks()
        m = self.g.map
        k = self.E
        T = int(round(TILE * k))
        N = self.CHUNK
        tx0, ty0 = ci * N, cj * N
        surf = pygame.Surface((N * T, N * T))
        surf.fill((0, 0, 0))
        mg = 2
        xs = range(max(0, tx0 - mg), min(m.w, tx0 + N + mg))
        ys = range(max(0, ty0 - mg), min(m.h, ty0 + N + mg))
        ts = self._tiles()
        names = {GRASS: "grass", DIRT: "dirt", WATER: "water", ROCK: "rock"}
        if ts is not None:
            rows = ts.meta["rows"]
            for y in ys:
                for x in xs:
                    info = rows[names[self.vbase[y * m.w + x]]]
                    v = self._tile_variant(x, y, 0, info["variants"])
                    surf.blit(self._hd_frame(ts, v, info["row"]), ((x - tx0) * T, (y - ty0) * T))
            drow = rows["dirt"]
            self.scenery.paint_soil_chunk(surf, tx0, ty0, N, T, lambda x, y: self._hd_frame(
                ts, self._tile_variant(x, y, 0, drow["variants"]), drow["row"]))
            self.scenery.paint_chunk(surf, tx0, ty0, N, T)
            srow, wrow = rows["sand"], rows["water"]
            self.scenery.paint_water_chunk(
                surf, tx0, ty0, N, T,
                lambda x, y: self._hd_frame(ts, self._tile_variant(x, y, 5, srow["variants"]), srow["row"]),
                lambda x, y: self._hd_frame(ts, self._tile_variant(x, y, 0, wrow["variants"]), wrow["row"]))
            self._decals(surf, xs, ys, tx0, ty0, T, lambda sh, i: self._hd_frame(sh, i))
            self._hd_transitions(surf, ts, xs, ys, tx0, ty0, T)
        props = self._tprop("rock")
        if props is not None:
            n = props.meta.get("frames", 1)
            ox, oy = self._hd_anchor(props)
            for y in ys:
                for x in xs:
                    if self.vterr[y * m.w + x] == ROCK:
                        fr = self._hd_frame(props, self._tile_variant(x, y, 1, n))
                        surf.blit(fr, ((x - tx0) * T - ox, (y - ty0) * T - oy))
        self._hd_doodads(surf, xs, ys, tx0, ty0, T)
        self._apply_theme(surf, self.theme_name())
        gdraw.mark_hd(surf)
        self._chunks[key] = surf
        elapsed = pygame.time.get_ticks() - started
        self._chunk_ms = (getattr(self, "_chunk_ms", elapsed) * 3 + elapsed) / 4
        while len(self._chunks) > self.CHUNK_CACHE:
            self._chunks.popitem(last=False)
        return surf

    def _hd_transitions(self, surf, ts, xs, ys, tx0, ty0, T):
        m = self.g.map
        masks = self.sprites.masks()
        if masks is None:
            return
        rows = ts.meta["rows"]
        order = masks.meta["order"]
        mrow = masks.meta["rows"]
        mask_tiles = {(w, kk): self._hd_frame(masks, i, info["row"]) for w, info in mrow.items()
                      for i, kk in enumerate(order)}
        prio = {GRASS: 0, DIRT: 1, ROCK: 2, WATER: 3}
        layers = [(DIRT, [("dirt", "narrow")]), (ROCK, [("rock", "narrow")]),
                  (WATER, [("sand", "wide"), ("water", "narrow")])]
        cache = self._hd_cache
        W = m.w
        sides = (("N", 0, -1), ("E", 1, 0), ("S", 0, 1), ("W", -1, 0))
        corners = (("NE", 1, -1, "N", "E"), ("SE", 1, 1, "S", "E"), ("SW", -1, 1, "S", "W"), ("NW", -1, -1, "N", "W"))
        for y in ys:
            for x in xs:
                t = self.vbase[y * W + x]
                for K, passes in layers:
                    if prio[t] >= prio[K]:
                        continue

                    def is_k(dx, dy):
                        nx, ny = x + dx, y + dy
                        return m.inb(nx, ny) and self.vbase[ny * W + nx] == K
                    on = [nm for nm, dx, dy in sides if is_k(dx, dy)]
                    on += [nm for nm, dx, dy, a, b in corners if is_k(dx, dy) and a not in on and b not in on]
                    if not on:
                        continue
                    for kind, width in passes:
                        key = ("ov", kind, width, tuple(on), (x * 7 + y * 3) % rows[kind]["variants"], T)
                        ov = cache.get(key)
                        if ov is None:
                            msk = pygame.Surface((T, T), pygame.SRCALPHA)
                            for nm in on:
                                msk.blit(mask_tiles[(width, nm)], (0, 0), special_flags=pygame.BLEND_RGBA_MAX)
                            ov = self._hd_frame(ts, key[4], rows[kind]["row"]).copy()
                            ov.blit(msk, (0, 0), special_flags=pygame.BLEND_RGBA_MULT)
                            if kind in ("dirt", "sand"):
                                import numpy as np
                                al = pygame.surfarray.array_alpha(msk).astype(np.float32)
                                band = np.clip(1 - np.abs(al - 128) / 70, 0, 1) * 150
                                rim = pygame.Surface((T, T), pygame.SRCALPHA)
                                rim.fill((52, 36, 22) if kind == "dirt" else (96, 80, 50))
                                ra = pygame.surfarray.pixels_alpha(rim)
                                ra[...] = band.astype(np.uint8)
                                del ra
                                ov.blit(rim, (0, 0))
                            cache[key] = ov
                        surf.blit(ov, ((x - tx0) * T, (y - ty0) * T))

    def _hd_doodads(self, surf, xs, ys, tx0, ty0, T):
        dd = self._tprop("doodads")
        if dd is None:
            return
        m = self.g.map
        k = self.E
        idx = {n: i for i, n in enumerate(dd.meta.get("names", []))}
        ox, oy = self._hd_anchor(dd)
        pick = {
            GRASS: (0.16, ["fucsomo", "fucsomo", "virag_sarga", "virag_piros", "virag_kek", "bokor", "kavics", "gomba",
                           "tusko", "farönk"]),
            DIRT: (0.1, ["kavics", "kavics", "kavics", "fucsomo", "fucsomo", "tusko", "csont"]),
        }

        def near(x, y, kind):
            return any(m.inb(x + dx, y + dy) and self.vterr[(y + dy) * m.w + x + dx] == kind
                       for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
        for y in ys:
            for x in xs:
                rng = self._tile_rng(x, y, 2)
                t = self.vterr[y * m.w + x]
                if t == WATER:
                    names = ["tavirozsa"] if near(x, y, GRASS) and rng.random() < 0.18 else []
                elif t in pick:
                    if near(x, y, WATER) and rng.random() < 0.35:
                        names = ["nad"]
                    elif self.scenery.on_path(x, y):  # ösvényen csak néhány kavics
                        names = ["kavics"] if rng.random() < 0.06 else []
                    else:
                        p, opts = pick[t]
                        if t == GRASS:  # csoportosan: réteken sűrű virág, az erdő szélén bokor, gomba, tuskó
                            if self.scenery.forest[y, x] > 0.3:
                                opts = ["bokor", "gomba", "tusko", "farönk", "fucsomo", "bokor", "gomba"]
                                p = 0.32
                            else:
                                p *= self.scenery.cluster(x, y)
                                if self.scenery.meadow[y, x] > 0.5:
                                    opts = ["virag_sarga", "virag_piros", "virag_kek", "fucsomo"]
                        names = [rng.choice(opts)] if rng.random() < p else []
                        if t == DIRT and not names and rng.random() < 0.006:
                            names = ["koponya"]
                else:
                    names = []
                for nm in names:
                    if nm in idx:
                        jx, jy = int(rng.randrange(-8, 9) * k), int(rng.randrange(-8, 9) * k)
                        surf.blit(self._hd_frame(dd, idx[nm]), ((x - tx0) * T - ox + jx, (y - ty0) * T - oy + jy))

    def draw_terrain_hd(self, s, cx, cy):
        span = TILE * self.CHUNK
        for cj in range(max(0, cy // span), min((self.g.map.h + self.CHUNK - 1) // self.CHUNK, (cy + self.vh) // span + 1)):
            for ci in range(max(0, cx // span), min((self.g.map.w + self.CHUNK - 1) // self.CHUNK,
                                                    (cx + self.vw) // span + 1)):
                s.blit(self.hd_chunk(ci, cj), (ci * span - cx, cj * span - cy))
        self._prefetch_terrain(cx, cy, span)

    def _prefetch_terrain(self, cx, cy, span):
        ox, oy = getattr(self, "_terrain_camera", (cx, cy))
        self._terrain_camera = cx, cy
        dx, dy = cx - ox, cy - oy
        budget = 1000 / max(1, int(self.settings.get("fps_limit", 60) or 60))
        if (not (dx or dy) or getattr(self, "_render_ms", budget) + getattr(self, "_chunk_ms", budget) > budget):
            return
        m = self.g.map
        if abs(dx) >= abs(dy):
            ahead = (((cx + self.vw) // span + 1 if dx > 0 else cx // span - 1), (cy + self.vh // 2) // span)
        else:
            ahead = ((cx + self.vw // 2) // span, ((cy + self.vh) // span + 1 if dy > 0 else cy // span - 1))
        x, y = ahead
        if (0 <= x * self.CHUNK < m.w and 0 <= y * self.CHUNK < m.h
                and (x, y, self.theme_name(), self.E) not in self._chunks):
            self.hd_chunk(x, y)

    def _terrain_transitions(self, surf, ts, rng):
        """WC2-szerű átmenetek: a magasabb rangú szomszéd terep zajos szélű maszkon át
        ráfolyik a csempére (fű < föld < szikla < víz; a víz előtt homokos part)."""
        m = self.g.map
        masks = self.sprites.masks()
        rows = ts.meta["rows"]
        if masks is not None:
            order = masks.meta["order"]
            mrow = masks.meta["rows"]
            mask_tiles = {(w, k): masks.frame(None, i, info["row"]) for w, info in mrow.items()
                          for i, k in enumerate(order)}
            prio = {GRASS: 0, DIRT: 1, ROCK: 2, WATER: 3}
            layers = [(DIRT, [("dirt", "narrow")]), (ROCK, [("rock", "narrow")]),
                      (WATER, [("sand", "wide"), ("water", "narrow")])]
            cache = {}
            W = m.w
            sides = (("N", 0, -1), ("E", 1, 0), ("S", 0, 1), ("W", -1, 0))
            corners = (("NE", 1, -1, "N", "E"), ("SE", 1, 1, "S", "E"), ("SW", -1, 1, "S", "W"), ("NW", -1, -1, "N", "W"))
            for y in range(m.h):
                for x in range(m.w):
                    t = self.vbase[y * W + x]
                    for K, passes in layers:
                        if prio[t] >= prio[K]:
                            continue

                        def is_k(dx, dy):
                            nx, ny = x + dx, y + dy
                            return m.inb(nx, ny) and self.vbase[ny * W + nx] == K
                        on = [nm for nm, dx, dy in sides if is_k(dx, dy)]
                        on += [nm for nm, dx, dy, a, b in corners if is_k(dx, dy) and a not in on and b not in on]
                        if not on:
                            continue
                        for kind, width in passes:
                            key = (kind, width, tuple(on), (x * 7 + y * 3) % rows[kind]["variants"])
                            ov = cache.get(key)
                            if ov is None:
                                msk = pygame.Surface((TILE, TILE), pygame.SRCALPHA)
                                for nm in on:
                                    msk.blit(mask_tiles[(width, nm)], (0, 0), special_flags=pygame.BLEND_RGBA_MAX)
                                ov = ts.frame(None, key[3], rows[kind]["row"]).copy()
                                ov.blit(msk, (0, 0), special_flags=pygame.BLEND_RGBA_MULT)
                                if kind in ("dirt", "sand"):  # festett, sötét perem a folt szélén (Loria/WC2)
                                    import numpy as np
                                    al = pygame.surfarray.array_alpha(msk).astype(np.float32)
                                    band = np.clip(1 - np.abs(al - 128) / 70, 0, 1) * 150
                                    rim = pygame.Surface((TILE, TILE), pygame.SRCALPHA)
                                    rim.fill((52, 36, 22) if kind == "dirt" else (96, 80, 50))
                                    ra = pygame.surfarray.pixels_alpha(rim)
                                    ra[...] = band.astype(np.uint8)
                                    del ra
                                    ov.blit(rim, (0, 0))
                                cache[key] = ov
                            surf.blit(ov, (x * TILE, y * TILE))
        # sziklacsempék kőtömbjei, hátulról előre (a magasság a fenti csempébe lóghat)
        props = self._tprop("rock")
        if props is not None:
            n = props.meta.get("frames", 1)
            ox, oy = props.anchor
            for y in range(m.h):
                for x in range(m.w):
                    if self.vterr[y * m.w + x] == ROCK:
                        surf.blit(props.frame(None, rng.randrange(n)), (x * TILE - ox, y * TILE - oy))
        self._scatter_doodads(surf, rng)

    def _apply_theme(self, surf, theme):
        """Évszak / táj: a kész terepkép színeinek átfestése (tél, pusztaság, mocsár) – ha a tájnak nincs saját grafikája."""
        if theme not in ("winter", "wasteland", "swamp", "autumn", "rift") or self._themed_art():
            return
        import numpy as np
        a = pygame.surfarray.pixels3d(surf)
        f = a.astype(np.float32)
        r, g, b = f[..., 0], f[..., 1], f[..., 2]
        lum = (r + g + b) / 3
        if theme == "winter":  # hó: kifehérített, kékes fény
            greenish = np.clip((g - np.maximum(r, b)) / 40.0, 0, 1)[..., None]
            snow = np.stack([lum * 0.55 + 120, lum * 0.55 + 128, lum * 0.5 + 142], -1)
            f = f * (1 - greenish * 0.85) + snow * greenish * 0.85
            f = f * 0.85 + np.array([20, 24, 34])
        elif theme == "autumn":  # ősz: a zöld rozsdás-aranyba hajlik
            greenish = np.clip((g - np.maximum(r, b)) / 30.0, 0, 1)
            warm = np.stack([g * 1.12 + 22, g * 0.78 + 8, b * 0.55], -1)
            f = f * (1 - greenish[..., None] * 0.8) + warm * greenish[..., None] * 0.8
        elif theme == "rift":  # Hasadékvidék: hideg, lilás, meteorporos táj
            f = np.stack([r * 0.72 + lum * 0.12 + 14, g * 0.62 + lum * 0.1, b * 0.8 + lum * 0.16 + 26], -1)
        elif theme == "wasteland":  # kiszáradt, okkeres puszta
            f = np.stack([r * 0.62 + g * 0.42 + 10, g * 0.78 + 6, b * 0.62], -1)
        else:  # mocsár: sötét, zöldeskék, párás
            f = np.stack([r * 0.7, g * 0.86 + 4, b * 0.84 + 14], -1)
        a[...] = np.clip(f, 0, 255).astype(np.uint8)
        del a

    def _scatter_doodads(self, surf, rng):
        """WC2-szerű apró terep-díszletek: virágok, fűcsomók, kövek, csontok, nád a parton, tavirózsa."""
        dd = self._tprop("doodads")
        if dd is None:
            return
        m = self.g.map
        idx = {n: i for i, n in enumerate(dd.meta.get("names", []))}
        ox, oy = dd.anchor
        pick = {
            GRASS: (0.16, ["fucsomo", "fucsomo", "virag_sarga", "virag_piros", "virag_kek", "bokor", "kavics", "gomba",
                           "tusko", "farönk"]),
            DIRT: (0.1, ["kavics", "kavics", "kavics", "fucsomo", "fucsomo", "tusko", "csont"]),
        }

        def near(x, y, kind):
            return any(m.inb(x + dx, y + dy) and self.vterr[(y + dy) * m.w + x + dx] == kind
                       for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
        for y in range(m.h):
            for x in range(m.w):
                t = self.vterr[y * m.w + x]
                if t == WATER:
                    names = ["tavirozsa"] if near(x, y, GRASS) and rng.random() < 0.18 else []
                elif t in pick:
                    if near(x, y, WATER) and rng.random() < 0.35:
                        names = ["nad"]
                    elif self.scenery.on_path(x, y):  # ösvényen csak néhány kavics
                        names = ["kavics"] if rng.random() < 0.06 else []
                    else:
                        p, opts = pick[t]
                        if t == GRASS:  # csoportosan: réteken sűrű virág, az erdő szélén bokor, gomba, tuskó
                            if self.scenery.forest[y, x] > 0.3:
                                opts = ["bokor", "gomba", "tusko", "farönk", "fucsomo", "bokor", "gomba"]
                                p = 0.32
                            else:
                                p *= self.scenery.cluster(x, y)
                                if self.scenery.meadow[y, x] > 0.5:
                                    opts = ["virag_sarga", "virag_piros", "virag_kek", "fucsomo"]
                        names = [rng.choice(opts)] if rng.random() < p else []
                        if t == DIRT and not names and rng.random() < 0.006:
                            names = ["koponya"]
                else:
                    names = []
                for nm in names:
                    if nm in idx:
                        jx, jy = rng.randrange(-8, 9), rng.randrange(-8, 9)
                        surf.blit(dd.frame(None, idx[nm]), (x * TILE - ox + jx, y * TILE - oy + jy))

    # ------------------------------------------------------------------
    # koordináták, kijelölés
    def to_world(self, sx, sy):
        return sx / self.E + self.cam_x, sy / self.E + self.cam_y

    def _wv(self):
        """A látható világ mérete (világ-képpontban)."""
        if self._world_phase:
            return self.vw, self.vh
        return self.vw / self.E, self.vh / self.E

    def mpos(self):
        """Egérpozíció az aktuális rajzolási térben (a nagyított játéktér rajzolásakor osztva)."""
        mx, my = pygame.mouse.get_pos()
        if self._world_phase:
            return mx / self.E, my / self.E
        return mx, my

    def entity_at(self, wx, wy):
        g, p = self.g, self.p
        best, bestd = None, 1e9
        for u in g.units:
            if u.inside is not None or not g.is_visible(p, u) or self.in_mine(u):
                continue
            yy = u.y - (14 if u.flying else 0)
            d = math.hypot(u.x - wx, yy - wy)
            if d <= u.radius + 6 and d < bestd:
                best, bestd = u, d
        if best:
            return best
        for b in g.buildings:
            if (b.owner is p or b.id in self.seen) and rect_dist(wx, wy, b.rect) <= 0:
                return b
        tx, ty = int(wx // TILE), int(wy // TILE)
        if g.map.inb(tx, ty) and p.explored[ty * g.map.w + tx]:
            r = g.map.res_grid[ty * g.map.w + tx]
            if r is not None:
                return r
            spike = g.spike_targets.get(ty * g.map.w + tx)
            if spike is not None and g.is_visible(p, spike):
                return spike
        return None

    in_mine = staticmethod(in_resource_shelter)

    def cmd(self, name, *args, **kw):
        """Játékparancs: egyjátékos módban azonnal, hálózaton a következő körökben (lockstep)."""
        if name in ("cmd_move", "cmd_smart", "cmd_attack", "cmd_gather") and args:
            us = [u for u in (args[0] if isinstance(args[0], list) else []) if getattr(u, "is_unit", False)]
            if us:
                ev = "attack_order" if name == "cmd_attack" or kw.get("attack") else "move"
                self.audio().play(ev, us[0].defn, us[0].owner.faction["id"], throttle_ms=900, key="ack")
        if self.net is not None:
            self.net.queue(name, args, kw)
            return True
        if self.recorder is not None:
            self.recorder.issue(self.p.id, name, args, kw)
        return getattr(self.g, name)(*args, **kw)

    def own_sel_units(self):
        return [e for e in self.sel if e.alive and e.is_unit and e.owner is self.p and e.inside is None]

    def own_sel_buildings(self):
        return [e for e in self.sel if e.alive and e.is_building and e.owner is self.p]

    def select(self, ents, add=False):
        if add:
            for e in ents:
                if e in self.sel:
                    self.sel.remove(e)
                else:
                    self.sel.append(e)
        else:
            self.sel = list(ents)

    # ------------------------------------------------------------------
    # bemenet
    def handle(self, ev):
        g, p = self.g, self.p
        mods = pygame.key.get_mods()
        shift = mods & pygame.KMOD_SHIFT
        ctrl = mods & pygame.KMOD_CTRL
        if ev.type == pygame.VIDEORESIZE:
            return
        d = g.director
        if d is not None and d.cine is not None and g.result is None:
            if ev.type == pygame.KEYDOWN:
                if ev.key == pygame.K_ESCAPE:
                    d.skip_cinematic()
                elif ev.key in (pygame.K_SPACE, pygame.K_RETURN):
                    d.next_shot()
            elif ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
                d.next_shot()
            return
        if g.result is not None:  # a meccs vége: felirat (Összesítő / Főmenü), majd az összesítő oldal
            if ev.type == pygame.KEYDOWN:
                if ev.key == pygame.K_ESCAPE:
                    self.quit_to_menu = True
                elif ev.key in (pygame.K_RETURN, pygame.K_SPACE):
                    if getattr(self, "summary", False):
                        self.quit_to_menu = True
                    else:
                        self.summary = True
                elif getattr(self, "summary", False) and ev.key in (pygame.K_LEFT, pygame.K_RIGHT, pygame.K_TAB):
                    self.sum_tab = (getattr(self, "sum_tab", 0) + (-1 if ev.key == pygame.K_LEFT else 1)) % 4
            elif ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
                for r, act in getattr(self, "result_btns", []):
                    if r.collidepoint(ev.pos):
                        if act == "menu":
                            self.quit_to_menu = True
                        else:
                            self.summary = True
                if getattr(self, "summary", False):
                    for r, i in getattr(self, "sum_tabs", []):
                        if r.collidepoint(ev.pos):
                            self.sum_tab = i
            return
        if g.director is not None and g.director.pending:  # döntés-ablak: csak a választás számít
            if ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
                for i, r in enumerate(getattr(self, "choice_rects", [])):
                    if r.collidepoint(ev.pos):
                        g.director.choose(i)
                        break
            elif ev.type == pygame.KEYDOWN and pygame.K_1 <= ev.key <= pygame.K_9:
                if ev.key - pygame.K_1 < len(g.director.pending["options"]):
                    g.director.choose(ev.key - pygame.K_1)
            return
        if self.path_pending():  # irányválasztó ablak
            pick = None
            if ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
                for r, pid in getattr(self, "path_rects", []):
                    if r.collidepoint(ev.pos):
                        pick = pid
            elif ev.type == pygame.KEYDOWN:
                if ev.key == pygame.K_ESCAPE:
                    self.path_hidden = True
                    return
                if pygame.K_1 <= ev.key <= pygame.K_3:
                    ks = list(p.faction["paths"])
                    if ev.key - pygame.K_1 < len(ks):
                        pick = ks[ev.key - pygame.K_1]
            if pick is not None:
                self.cmd("cmd_choose_path", p, pick)
            if ev.type in (pygame.MOUSEBUTTONDOWN, pygame.KEYDOWN):
                return
        if ev.type == pygame.KEYUP:
            self.held.discard(ev.key)
            return
        if ev.type == pygame.KEYDOWN:
            if ev.key in self.held:  # ugyanaz a lenyomás (ismétlés): nem hajtjuk végre újra
                return
            self.held.add(ev.key)
        if self.paused and ev.type == pygame.KEYDOWN:
            if ev.key in (pygame.K_ESCAPE, pygame.K_p):
                self.paused = False
            elif ev.key == pygame.K_q:
                self.quit_to_menu = True
            return
        if ev.type == pygame.KEYDOWN:
            k = ev.key
            if self.text_focus:  # szövegbevitel: a játékparancsok némák
                return
            if k == pygame.K_ESCAPE:
                if self.show_allies:
                    self.show_allies = False
                elif self.mode:
                    self.mode = None
                elif self.cmd_page or self.learn_mode:
                    self.cmd_page, self.page_no = None, 0
                    self.learn_mode = False
                elif self.mode:
                    self.mode = None
                elif self.show_help:
                    self.show_help = False
                else:
                    self.paused = True
            elif k == pygame.K_F5:
                self.request = ("quicksave",)
            elif k == pygame.K_F9:
                self.request = ("quickload",)
            elif k == pygame.K_F1:
                self.show_help = not self.show_help
            elif k == pygame.K_F8:
                self.select_idle_worker()
            elif k in (pygame.K_F2, pygame.K_F3, pygame.K_F4):
                hs = self.my_heroes()
                i = k - pygame.K_F2
                if i < len(hs):
                    if self.sel == [hs[i]]:
                        self.center_on(hs[i].x, hs[i].y)
                    self.sel = [hs[i]]
                    self.learn_mode = False
            elif k == pygame.K_p:
                self.paused = True
            elif k in (pygame.K_PLUS, pygame.K_KP_PLUS, pygame.K_EQUALS):
                self.change_speed(1)
            elif k in (pygame.K_MINUS, pygame.K_KP_MINUS):
                self.change_speed(-1)
            elif pygame.K_1 <= k <= pygame.K_9:
                n = k - pygame.K_0
                if ctrl:
                    self.groups[n] = [e for e in self.sel if e.owner is p]
                    g.notify(p, f"{n}. csoport mentve", (200, 200, 200))
                else:
                    grp = [e for e in self.groups.get(n, []) if e.alive]
                    if grp:
                        self.sel = grp
                        now = pygame.time.get_ticks()
                        if self.last_group[0] == n and now - self.last_group[1] < 400:
                            self.center_on(grp[0].x, grp[0].y)
                        self.last_group = (n, now)
            elif k == pygame.K_SPACE:
                mine = [a for a in g.alerts if len(a) < 4 or a[3] == p.id]
                if mine and g.time - mine[-1][2] < 10:
                    self.center_on(mine[-1][0], mine[-1][1])
                else:
                    self.center_on(*p.start_xy)
            elif k in KEY_SLOT:  # W E R T / S D F G / X C V B – ugyanaz, mint a kattintás
                self.hud_key(ev)
        elif ev.type == pygame.MOUSEBUTTONDOWN:
            mx, my = ev.pos
            if ev.button == 4 or ev.button == 5:
                if not self.over_hud(mx, my):  # görgő: kamera közelebb / távolabb
                    self.set_view(-1 if ev.button == 4 else 1)
                return
            if self.show_allies and ev.button == 1 and not self.over_hud(mx, my):
                self.show_allies = False  # kattintás máshová: a szövetséges-lista bezárul
            if self.over_hud(mx, my):
                self.hud_click(mx, my, ev.button)
                return
            for r, d in getattr(self, "speed_rects", []):  # sebesség −/+ gombok a bal felső sarokban
                if r.collidepoint(mx, my) and ev.button == 1:
                    self.change_speed(d)
                    return
            for r, h in self.portraits:
                if r.collidepoint(mx, my) and ev.button == 1:
                    now = pygame.time.get_ticks()
                    if self.last_click[1] is h and now - self.last_click[0] < 350:
                        self.center_on(h.x, h.y)
                    self.sel = [h]
                    self.learn_mode = False
                    self.last_click = (now, h)
                    return
            wx, wy = self.to_world(mx, my)
            if ev.button == 1:
                if self.mode and self.mode[0] == "place":
                    scale = self.E if self.zoom > 1 else 1
                    self.try_place(mx / scale + int(self.cam_x), my / scale + int(self.cam_y), shift)
                elif self.mode and self.mode[0] == "attack":
                    tgt = self.entity_at(wx, wy)
                    us = self.own_sel_units()
                    if tgt is not None and (getattr(tgt, "owner", None) not in (None, p)
                                            or getattr(tgt, "defn", {}).get("spike_tile")):
                        self.cmd("cmd_attack", us, tgt, queue=shift)
                        g.add_fx("ring", wx, wy, 0.4, (255, 80, 60))
                    else:
                        self.cmd("cmd_move", us, wx, wy, attack=True, queue=shift)
                    if not shift:
                        self.mode = None
                elif self.mode and self.mode[0] == "move":
                    self.cmd("cmd_move", self.own_sel_units(), wx, wy, False, shift)
                    if not shift:
                        self.mode = None
                elif self.mode and self.mode[0] == "cast":
                    self.cmd("cmd_cast", self.own_sel_units(), self.mode[1], wx, wy, target=self.entity_at(wx, wy))
                    if not shift:
                        self.mode = None
                elif self.mode and self.mode[0] == "ability_place":
                    ab_id = self.mode[1]
                    us = self.own_sel_units()
                    if g.placement_caster(us, ab_id, wx, wy) is not None:
                        self.cmd("cmd_place_ability", us, ab_id, wx, wy)
                        self.mode = None
                    else:
                        g.notify(p, "A képesség most nem használható ezen a helyen.")
                elif self.mode and self.mode[0] == "anchor_pick":
                    anchor = self.entity_at(wx, wy)
                    if g.anchor_available(p, anchor):
                        self.mode = ("anchor_drop", anchor)
                        g.notify(p, "Jelöld ki a Lélekhorgony lerakási helyét.")
                    else:
                        g.notify(p, "Válassz egy szabad saját Lélekhorgonyt.")
                elif self.mode and self.mode[0] == "anchor_drop":
                    anchor = self.mode[1]
                    if not g.anchor_available(p, anchor):
                        self.mode = None
                        g.notify(p, "Ez a Lélekhorgony már nem elérhető.")
                    elif g.anchor_drop_allowed(anchor, wx, wy):
                        self.cmd("cmd_anchor_rescue", self.own_sel_units(), anchor, wx, wy)
                        self.mode = None
                    else:
                        g.notify(p, "A Lélekhorgony nem rakható le ide.")
                elif self.mode and self.mode[0] == "gcast":  # Ö19 Égi kegy: célzás
                    self.cmd("cmd_gpower", self.p, self.mode[2], self.mode[1], wx, wy)
                    self.mode = None
                else:
                    self.drag = (mx, my)
            elif ev.button == 3:
                if self.mode:
                    self.mode = None
                    return
                self.right_click(wx, wy, shift)
            elif ev.button == 2:
                self.mid_drag = (mx, my, self.cam_x, self.cam_y)
        elif ev.type == pygame.MOUSEBUTTONUP:
            if ev.button == 1:
                self.minimap_drag = False
                if self.drag:
                    self.finish_drag(ev.pos, shift)
                    self.drag = None
            elif ev.button == 2:
                self.mid_drag = None
        elif ev.type == pygame.MOUSEMOTION:
            if self.mid_drag:
                mx, my, cx, cy = self.mid_drag
                self.cam_x = cx - (ev.pos[0] - mx)
                self.cam_y = cy - (ev.pos[1] - my)
                self.clamp_cam()
            if self.minimap_drag:
                self.minimap_jump(*ev.pos)

    def right_click(self, wx, wy, shift):
        g = self.g
        tgt = self.entity_at(wx, wy)
        us = self.own_sel_units()
        gi = g.item_at(wx, wy)
        if gi is not None and any(u.hero for u in us):
            tx, ty = int(gi.x // TILE), int(gi.y // TILE)
            if g.map.inb(tx, ty) and self.p.visible[ty * g.map.w + tx]:
                self.cmd("cmd_pickup", us, gi, queue=shift)
                g.add_fx("ring", gi.x, gi.y, 0.35, (255, 230, 140))
                return
        if us:
            self.cmd("cmd_smart", us, wx, wy, tgt, queue=shift)
            if tgt is not None:
                col = (255, 80, 60) if getattr(tgt, "owner", None) not in (None, self.p) else (90, 255, 120)
                g.add_fx("ring", wx, wy, 0.35, col)
            return
        bs = [b for b in self.own_sel_buildings() if b.defn.get("produces")]
        if bs:
            self.cmd("cmd_rally", bs, wx, wy, tgt if tgt is not None and getattr(tgt, "is_resource", False) else None)
            g.add_fx("ring", wx, wy, 0.35, (255, 230, 90))

    def finish_drag(self, pos, shift):
        g, p = self.g, self.p
        x0, y0 = self.drag
        x1, y1 = pos
        x1 = min(x1, self.vw - 1)
        y1 = min(y1, self.L["hud"].y - 1) if self.L["hud"].collidepoint(x1, y1) else y1
        if abs(x1 - x0) < 5 and abs(y1 - y0) < 5:
            wx, wy = self.to_world(x0, y0)
            e = self.entity_at(wx, wy)
            now = pygame.time.get_ticks()
            if e is not None and e.is_unit and e.owner is p and self.last_click[1] is e and now - self.last_click[0] < 350:
                same = [u for u in g.units if u.owner is p and u.defn is e.defn and self.on_screen(u.x, u.y)]
                self.select(same, False)
            elif e is not None:
                self.select([e], shift and e.owner is p)
            elif not shift:
                self.sel = []
            self.last_click = (now, e)
            return
        rx0, rx1 = sorted((x0, x1))
        ry0, ry1 = sorted((y0, y1))
        picked = []
        for u in g.units:
            if u.owner is not p or u.inside is not None:
                continue
            z = self.E
            sx, sy = (u.x - self.cam_x) * z, (u.y - self.cam_y - (14 if u.flying else 0)) * z
            if rx0 - u.radius * z <= sx <= rx1 + u.radius * z and ry0 - u.radius * z <= sy <= ry1 + u.radius * z:
                picked.append(u)
        if shift:
            self.sel += [u for u in picked if u not in self.sel]
        elif picked:
            self.sel = picked
        else:
            self.sel = []

    def on_screen(self, x, y):
        vw, vh = self._wv()
        return self.cam_x <= x <= self.cam_x + vw and self.cam_y <= y <= self.cam_y + vh

    def center_on(self, x, y):
        vw, vh = self._wv()
        self.cam_x = x - vw / 2
        self.cam_y = y - vh / 2
        self.clamp_cam()

    def minimap_world_pos(self, mx, my):
        r, m = self.mm_rect, self.g.map
        return (mx - r.x) * m.w * TILE / r.w, (my - r.y) * m.h * TILE / r.h

    def minimap_jump(self, mx, my):
        self.center_on(*self.minimap_world_pos(mx, my))

    def sel_abilities(self):
        out = []
        for u in self.own_sel_units():
            for ab in u.abilities:
                if ab not in out:
                    out.append(ab)
        return out

    def my_heroes(self):
        return [u for u in self.g.units if u.alive and u.hero and u.owner is self.p]

    def sel_hero(self):
        us = self.own_sel_units()
        return us[0] if len(us) == 1 and us[0].hero else None

    def learn(self, hero, ab_id):
        g = self.g
        if self.net is not None:
            self.cmd("cmd_learn", hero, ab_id)
            return
        if self.cmd("cmd_learn", hero, ab_id):
            ab = g.data.abilities[ab_id]
            g.notify(self.p, f"{hero.name} megtanulta: {ab['name']} ({hero.learned[ab_id]}. szint)", (255, 230, 150))
            g.add_fx("levelup", hero.x, hero.y, 0.8, (255, 220, 110))
        else:
            cur = hero.learned.get(ab_id, 0)
            if cur >= H.max_ability_level(g, ab_id):
                g.notify(self.p, "Ez a képesség már a legmagasabb szinten van.", (255, 160, 90))
            else:
                need = H.required_hero_level(g, ab_id, cur + 1)
                g.notify(self.p, f"Ehhez {need}. szintű hős kell.", (255, 160, 90))
        if hero.skill_points <= 0:
            self.learn_mode = False

    def use_ability(self, ab_id):
        g = self.g
        ab = g.data.abilities[ab_id]
        typ = ab.get("type")
        us = self.own_sel_units()
        if ab.get("target_placement"):
            self.mode = ("ability_place", ab_id)
        elif typ in ("area", "raise", "bolt", "blink"):
            self.mode = ("cast", ab_id)
        elif typ == "anchor_rescue":
            self.mode = ("anchor_pick",)
            g.notify(self.p, "Jelölj ki egy saját Lélekhorgonyt.")
        elif typ in ("passive", "aura"):
            return
        elif typ == "chant":
            self.cmd("cmd_toggle", us, ab_id)
        elif typ == "summon":
            self.cmd("cmd_summon", us, ab_id)
        elif typ == "nova":
            self.cmd("cmd_nova", us, ab_id)
        elif typ == "morph":
            self.cmd("cmd_morph", us, ab_id)
        elif typ == "garrison":
            self.cmd("cmd_garrison", us)
        elif typ in ("mount", "dismount"):
            self.cmd("cmd_mount", us, ab_id)
        elif typ == "merge":
            if not self.cmd("cmd_merge", us, ab_id) and len([u for u in us if u.has_ability(ab_id)]) < ab.get("count", 3):
                g.notify(self.p, f"{ab['name']}: legalább {ab.get('count', 3)} egység kell.", (255, 160, 90))
        elif typ == "toggle":
            self.cmd("cmd_toggle", us, ab_id)
        elif typ == "self_buff":
            self.cmd("cmd_buff", us, ab_id)
        elif typ == "overdrive":
            self.cmd("cmd_overdrive", us, ab_id)
        elif typ == "lay_mine":
            self.cmd("cmd_lay_mine", us, ab_id)
        elif typ == "ward":
            self.cmd("cmd_ward", us, ab_id)
        elif typ == "autoheal":
            g.notify(self.p, f"{ab['name']}: automatikus képesség.", (200, 200, 200))

    def place_pos(self, wx, wy, bdef):
        w, h = bdef["size"]
        tx, ty = int(wx // TILE), int(wy // TILE)
        if bdef.get("pond_tile"):
            w = h = self.p.rules.get("pond_size", self.p.faction.get("pond", {}).get("size", 2))
        if bdef.get("spike_tile"):
            w = h = self.p.rules.get("spike_size", 1)
        if bdef.get("on_point"):  # őrállás: a kurzor csempéjéhez legközelebbi Hasadékkőre illesztve
            pts = getattr(self.g, "points", [])
            if pts:
                x, y = (tx + .5) * TILE, (ty + .5) * TILE
                pt = min(pts, key=lambda q: (q.x - x) ** 2 + (q.y - y) ** 2)
                if (pt.x - x) ** 2 + (pt.y - y) ** 2 <= (2.5 * TILE) ** 2:
                    return pt.tx, pt.ty
        if bdef.get("on_resource"):  # bányaépület: a bánya alá illesztve
            m = self.g.map
            r = m.res_grid[ty * m.w + tx] if m.inb(tx, ty) else None
            if r is not None and r.kind in bdef["on_resource"]:
                return r.tx, r.ty
        return tx - w // 2, ty - h // 2

    def start_unpack(self):
        wagons = [u for u in self.own_sel_units() if getattr(u, "packed", None)]
        if wagons:
            self.mode = ("place", wagons[0].packed, wagons[0])

    def pack_selected(self):
        for b in self.own_sel_buildings():
            if b.defn.get("packable") and b.complete:
                u = self.cmd("cmd_pack", b)
                if u and u is not True:
                    self.sel = [u]

    def try_place(self, wx, wy, shift):
        g, p = self.g, self.p
        bdef = self.mode[1]
        tx, ty = self.place_pos(wx, wy, bdef)
        if len(self.mode) > 2:  # vándorszekér letelepítése
            self.submit_placement("cmd_unpack", (self.mode[2], tx, ty))
            return
        if bdef.get("foundation_tile"):  # Vasbirodalom-alapozás: a Fabrikátor rakja le; a mód nyitva marad
            ws = [u for u in self.own_sel_units() if u.can_build] or \
                [u for u in g.units if u.owner is p and u.can_build and (u.order is None or u.order["type"] == "gather")]
            if not ws:
                g.notify(p, "Nincs szabad Fabrikátor.", (255, 120, 90))
                return
            n = bdef["size"][0]
            cx, cy = (tx + n / 2) * TILE, (ty + n / 2) * TILE
            w = min(ws, key=lambda u: (u.order is not None and u.order["type"] == "found", math.hypot(u.x - cx, u.y - cy)))
            self.cmd("cmd_found", p, tx, ty, w, bdef["id"], queue=bool(shift))
            return
        if bdef.get("spike_tile"):  # Croftian mérgező tüskék: a Csápszolga rakja le; a mód nyitva marad
            ws = [u for u in self.own_sel_units() if u.can_build] or \
                [u for u in g.units if u.owner is p and u.can_build and u.order is None]
            if not ws:
                g.notify(p, "Nincs szabad Csápszolga.", (255, 120, 90))
                return
            n = p.rules.get("spike_size", 1)
            cx, cy = (tx + n / 2) * TILE, (ty + n / 2) * TILE
            w = min(ws, key=lambda u: (u.order is not None and u.order["type"] == "spike", math.hypot(u.x - cx, u.y - cy)))
            self.cmd("cmd_lay_spikes", p, tx, ty, w, queue=bool(shift))
            return
        if bdef.get("pond_tile"):  # Huk'ucha-tavacska: a Tóásó ássa ki; a mód nyitva marad
            ws = [u for u in self.own_sel_units() if u.can_build] or \
                [u for u in g.units if u.owner is p and u.can_build and (u.order is None or u.order["type"] == "gather")]
            if not ws:
                g.notify(p, "Nincs szabad Tóásó.", (255, 120, 90))
                return
            n = p.rules.get("pond_size", p.faction.get("pond", {}).get("size", 2))
            cx, cy = (tx + n / 2) * TILE, (ty + n / 2) * TILE
            w = min(ws, key=lambda u: (u.order is not None and u.order["type"] == "dig", math.hypot(u.x - cx, u.y - cy)))
            self.cmd("cmd_dig_pond", p, tx, ty, w, queue=bool(shift))
            return
        ok, reason = g.can_place(p, bdef, tx, ty)
        if not ok:
            g.notify(p, f"Nem lehet építeni: {reason}", (255, 120, 90))
            return
        workers = [u for u in self.own_sel_units() if u.can_build]
        if not workers:
            workers = [u for u in g.units if u.owner is p and u.can_build
                       and (u.order is None or u.order["type"] == "gather")]
            if not workers:
                workers = [u for u in g.units if u.owner is p and u.can_build]
        if not workers:
            g.notify(p, "Nincs szabad munkás.", (255, 120, 90))
            return
        cx, cy = (tx + bdef["size"][0] / 2) * TILE, (ty + bdef["size"][1] / 2) * TILE
        workers.sort(key=lambda u: (u.carry_amt > 0 and not self.own_sel_units(), math.hypot(u.x - cx, u.y - cy)))
        args = (p, bdef, tx, ty, workers[0])
        self.submit_placement("cmd_build", args, keep_open=shift, queue=shift and workers[0] in self.sel)

    def submit_placement(self, name, args, keep_open=False, **kw):
        if self.net is not None and not keep_open:
            mode = self.mode
            if getattr(self, "pending_placement", None) is mode:
                return
            self.pending_placement = mode

            def placed(accepted):
                if self.pending_placement is mode:
                    self.pending_placement = None
                if accepted and self.mode is mode:
                    self.mode = None

            self.net.queue(name, args, kw, on_result=placed)
        elif self.cmd(name, *args, **kw) and not keep_open:
            self.mode = None

    # ------------------------------------------------------------------
    # építés / kiképzés / kutatás (a parancsrács hívja)
    def icon(self, key, defn, kind):
        if key not in self.icons:
            if kind == "research":
                spr = self.sprites.research_art(defn["id"])  # kutatás-ikon, ha megrajzoltad
                self.icons[key] = make_icon(self.base_fonts, defn, self.p.faction, self.p.color, kind, spr)
            else:  # egység / épület: az 1:1 portré; ha még nincs, a kódból rajzolt alak
                spr = self.sprites.icon(defn["id"], self.p.color)
                self.icons[key] = spr if spr is not None else \
                    make_icon(self.base_fonts, defn, self.p.faction, self.p.color, kind, None)
        return self.icons[key]

    def path_visible(self, d):
        """Irányhoz kötött elem csak a választott irányban látszik (a hadjáratban mindig)."""
        pid = d.get("path")
        return not pid or self.p.path == pid or self.g.director is not None

    def path_pending(self):
        p = self.p
        return p.ascended and p.path is None and p.faction.get("paths") and not getattr(self, "path_hidden", False)

    def draw_path_choice(self):
        """Felemelkedés: 3 irány-kártya egymás mellett (kattintás vagy 1/2/3; Esc = később)."""
        g, p, s, F = self.g, self.p, self.screen, self.fonts
        paths = list(p.faction["paths"].items())
        dim = pygame.Surface((self.vw, self.vh), pygame.SRCALPHA)
        dim.fill((0, 0, 0, 150))
        s.blit(dim, (0, 0))
        u = self.u
        k = lambda v: int(round(v * u))  # noqa: E731
        cw = min(k(330), (self.vw - k(60)) // 3)
        head = F["big"].get_linesize() + F["tiny"].get_linesize() + k(16)
        foot = F["tiny"].get_linesize() + k(24)
        H = min(self.vh, self.L["hud"].y - k(4))  # a HUD fölötti terület: a lábléc se csússzon a HUD-ra
        top = self.L["topbar"].bottom + k(6)       # és a cím se a felső sávra
        ch = min(k(440), H - top - head - foot - k(8))
        gap = k(16)
        tot = cw * 3 + gap * 2
        y0 = max(top + head, top + head + (H - top - head - foot - ch) / 2)
        x0 = self.vw / 2 - tot / 2
        title = F["big"].render("Felemelkedés – válassz irányt", True, (255, 220, 130))
        ty = y0 - head
        s.blit(title, (self.vw / 2 - title.get_width() / 2, ty))
        sub = F["tiny"].render("A választás végleges: a másik két irány tartalma zárva marad.", True, (230, 220, 200))
        s.blit(sub, (self.vw / 2 - sub.get_width() / 2, ty + F["big"].get_linesize() + k(4)))
        mx, my = pygame.mouse.get_pos()
        self.path_rects = []
        acc = tuple(p.faction.get("accent", (200, 160, 80)))
        for i, (pid, pd) in enumerate(paths):
            r = pygame.Rect(int(x0 + i * (cw + gap)), int(y0), cw, ch)
            self.path_rects.append((r, pid))
            hover = r.collidepoint(mx, my)
            bg = pygame.Surface((r.w, r.h), pygame.SRCALPHA)
            bg.fill((40, 30, 20, 245) if hover else (16, 16, 20, 240))
            s.blit(bg, r.topleft)
            gdraw.rect(s, (255, 214, 120) if hover else acc, r, 3 if hover else 2)
            pad = k(12)
            iw = r.w - 2 * pad
            yy = r.y + pad
            es = k(48)
            em = self.sprites.art(f"paths/{p.faction['id']}_{pid}", (es, es))
            if em is not None:  # irány-címer, ha megrajzoltad
                s.blit(em, (r.right - es - k(10), r.y + k(8)))
            nw = iw - (es + k(6) if em is not None else 0)
            s.blit(F["med"].render(self.ellipsize(f"{i + 1}. {pd['name']}", F["med"], nw), True, (255, 236, 190)),
                   (r.x + pad, yy))
            yy += max(F["med"].get_linesize(), es - k(8) if em is not None else 0) + k(6)
            yy = self.wrap(pd.get("tagline", ""), r.x + pad, yy, iw, F["small"], (255, 200, 120), 2) + k(8)
            room = max(1, (r.bottom - yy - k(110)) // F["tiny"].get_linesize())
            yy = self.wrap(pd.get("desc", ""), r.x + pad, yy, iw, F["tiny"], (220, 218, 206), min(9, room)) + k(10)
            bd = g.data.defs.get(pd["building"])
            if bd:
                s.blit(F["tiny"].render(self.ellipsize("Épület: " + bd["name"], F["tiny"], iw), True, (200, 220, 255)),
                       (r.x + pad, yy))
                yy += F["tiny"].get_linesize() + k(4)
            ix = r.x + pad
            icw = k(44)
            for uid in pd.get("units", []):  # egység-ikonok
                d = g.data.defs.get(uid)
                if not d:
                    continue
                ic = self.icon(("path", uid), d, "unit")
                ics = pygame.transform.smoothscale(ic, (icw, icw))
                if ix + icw > r.right - k(8):
                    ix = r.x + pad
                    yy += icw + k(4)
                s.blit(ics, (ix, yy))
                ix += icw + k(6)
            yy += icw + k(6)
            lh = F["tiny"].get_linesize()
            for uid in pd.get("units", []):
                d = g.data.defs.get(uid)
                if d and yy + lh < r.bottom - k(4):
                    s.blit(F["tiny"].render(self.ellipsize("• " + d["name"], F["tiny"], iw), True, (230, 230, 220)),
                           (r.x + pad + k(2), yy))
                    yy += lh
        hint = F["tiny"].render("Esc: később (a főépület parancskártyáján újra megnyitható)", True, (210, 206, 196))
        hb = pygame.Rect(0, 0, hint.get_width() + k(24), hint.get_height() + k(10))
        hb.center = (self.vw / 2, y0 + ch + k(12) + hb.h / 2)
        bgh = pygame.Surface(hb.size, pygame.SRCALPHA)
        bgh.fill((10, 10, 14, 230))
        s.blit(bgh, hb)
        s.blit(hint, (hb.x + k(12), hb.y + k(5)))

    def entry_state(self, kind, d):
        """(elérhető, haladás 0..1 vagy None, darab, megjegyzés)"""
        g, p = self.g, self.p
        if kind == "building":
            miss = g.missing_requirements(p, d)
            return (not miss, None, 0, miss)
        if kind == "unit":
            prods = g.producers(p, d["id"])
            miss = g.missing_requirements(p, d)
            if not prods:
                miss = miss + ["termelő épület"]
            items = [it for b in g.buildings if b.owner is p for it in b.queue if it["id"] == d["id"]]
            prog = None
            if items:
                prog = max(it["progress"] / it["time"] for it in items)
            left = g.unit_limit_left(p, d)
            if left is not None and left <= 0 and not miss:
                miss = miss + [f"több {g.data.defs[d['per_building']['building']]['name']}"]
            return (not miss, prog, len(items), miss)
        if d["id"] in p.researched:
            return (False, None, 0, ["kész"])
        if g.research_blocked(p, d["id"]):
            return (False, None, 0, ["kizárva"])
        items = [it for b in g.buildings if b.owner is p for it in b.queue if it["id"] == d["id"]]
        prog = items[0]["progress"] / items[0]["time"] if items else None
        has = any(d["id"] in b.defn.get("researches", ()) and b.complete for b in g.buildings if b.owner is p)
        miss = ([] if has else ["épület"]) + g.missing_requirements(p, d)
        return (not miss, prog, len(items), miss)

    def click_entry(self, kind, d):
        g, p = self.g, self.p
        if kind == "building":
            miss = g.missing_requirements(p, d)
            if miss:
                g.notify(p, "Hiányzik: " + ", ".join(miss), (255, 160, 90))
                return
            if not p.can_afford(d["cost"]):
                g._notify_missing(p, d["cost"])
                return
            self.mode = ("place", d)
        elif kind == "unit":
            n = 5 if pygame.key.get_mods() & pygame.KMOD_SHIFT else 1
            for _ in range(n):
                if not self.cmd("cmd_train", p, d["id"]):
                    break
        else:
            self.cmd("cmd_research", p, d["id"])

    def right_entry(self, kind, d):
        if kind == "unit":
            self.cmd("cancel_unit_type", self.p, d["id"])
        elif kind == "research":
            for b in self.g.buildings:
                if b.owner is self.p:
                    for i, it in enumerate(b.queue):
                        if it["id"] == d["id"]:
                            self.cmd("cancel_item", b, i)
                            return
        elif kind == "building" and self.mode and self.mode[0] == "place":
            self.mode = None

    # ------------------------------------------------------------------
    def cursor_kind(self, mx, my):
        """Helyzetfüggő kurzor (egységes festett készlet, assets/ui/cursors): görgetőnyíl a képernyő szélén; a
        parancs-módokban támadás / varázslat / áthelyezés / építés (érvénytelen helyen tiltójel); a pálya fölött a
        mutatott dolog szerint: saját / ellenséges / semleges kijelölés, gyűjtés (nyersanyagonként), javítás."""
        if self.settings.get("edge_scroll", True) and not self.mode:
            ex = -1 if mx <= 2 else (1 if mx >= self.sw - 3 else 0)
            ey = -1 if my <= 2 else (1 if my >= self.sh - 3 else 0)
            if ex or ey:
                return "scroll_" + {(-1, -1): "nw", (0, -1): "n", (1, -1): "ne", (-1, 0): "w", (1, 0): "e",
                                    (-1, 1): "sw", (0, 1): "s", (1, 1): "se"}[(ex, ey)]
        if self.over_hud(mx, my):
            return "arrow"
        if self.mode:
            m = self.mode[0]
            if m in ("attack", "cast", "move", "gcast", "anchor_pick", "anchor_drop", "ability_place"):
                return "cast" if m in ("gcast", "anchor_pick", "anchor_drop", "ability_place") else m
            if m == "place":
                try:
                    bdef = self.mode[1]
                    wx, wy = self.to_world(mx, my)
                    tx, ty = self.place_pos(wx, wy, bdef)
                    if bdef.get("pond_tile") or bdef.get("foundation_tile") or bdef.get("spike_tile"):
                        return "build"
                    ok = self.g.can_place(self.p, bdef, tx, ty, check_req=len(self.mode) <= 2)[0]
                    return "build" if ok else "no"
                except Exception:
                    return "build"
            return "arrow"
        try:
            wx, wy = self.to_world(mx, my)
            e = self.entity_at(wx, wy)
        except Exception:
            return "arrow"
        if e is None:
            return "arrow"
        mine = self.own_sel_units()
        if getattr(e, "is_resource", False):
            if any(u.is_worker for u in mine):
                return {"wood": "gather_wood", "stone": "gather_stone", "meteor": "gather_meteor"}.get(
                    getattr(e, "kind", ""), "gather_stone")
            return "select_neutral"
        owner = getattr(e, "owner", None)
        if owner is not None and self.p.hostile(owner):
            return "attack" if any(u.attack for u in mine) else "select_enemy"
        if owner is self.p and e.hp < e.max_hp and any(u.is_worker for u in mine) and \
                (not e.is_unit or "mech" in getattr(e, "tags", ())):
            return "repair"
        return "select_friend" if owner is self.p or (owner is not None and not self.p.hostile(owner)) \
            else "select_neutral"

    def audio(self):
        from . import audio as AU
        return AU.get(self.settings)

    def play_sounds(self):
        """A szimuláció hangeseményei: ami a játékosnak látható vagy fontos."""
        g, p = self.g, self.p
        ev = getattr(g, "sound_events", None)
        A = self.audio()
        A.update()
        sel = {e.id for e in self.sel}
        if sel != getattr(self, "_snd_sel", set()):  # új kijelölés: a kijelölt (saját) egység megszólal
            new = [e for e in self.sel if e.id not in getattr(self, "_snd_sel", set()) and e.owner is p]
            if new:
                e0 = new[0]
                A.play("select", e0.defn, p.faction["id"], throttle_ms=300, key="select",
                       building=e0.is_building)
            self._snd_sel = sel
        if not ev:
            return
        vw, vh = self._wv()
        x0, y0 = self.cam_x - 80, self.cam_y - 80
        for kind, e in ev:
            onscreen = x0 <= e.x <= x0 + vw + 160 and y0 <= e.y <= y0 + vh + 160
            fid = e.owner.faction["id"] if getattr(e, "owner", None) is not None else None
            if kind == "under_attack":
                if e.owner is p and not onscreen:  # riasztás csak a képen kívüli támadásról, ritkán
                    A.play("under_attack", None, fid, throttle_ms=12000, key="alarm")
                continue
            if kind in ("trained", "building_done"):
                if e.owner is p:
                    A.play(kind, e.defn, fid, throttle_ms=400)
                continue
            if not onscreen or not g.is_visible(p, e):
                continue
            if kind == "attack":
                A.play("attack", e.defn, fid, vol=0.55, throttle_ms=120, key=("atk", e.defn["id"]))
            elif kind == "death":
                A.play("death", e.defn, fid, vol=0.8, throttle_ms=150, key=("death", e.defn["id"]),
                       building=e.is_building)
            elif kind.startswith("ability_"):
                A.play(kind, e.defn, fid, vol=0.85, throttle_ms=180)
            elif kind.startswith("impact_"):
                A.play(kind, e.defn, fid, vol=0.45, throttle_ms=95, key=kind)
            elif kind in ("gate_arrival", "building_pack", "building_unpack"):
                A.play(kind, e.defn, fid, throttle_ms=200)
        ev.clear()

    def update(self, dt):
        self.t += dt
        self.play_sounds()
        d = self.g.director
        if d is not None and d.cine is not None:
            d.tick_cinematic(dt)
            if d.cine is not None:
                sh = d.cine["shots"][d.cine["i"]]
                if sh.get("camera") is not None:
                    tx, ty = MS.world_xy(self.g, sh["camera"])
                    vw, vh = self._wv()
                    cx0, cy0 = self.cam_x + vw / 2, self.cam_y + vh / 2
                    a = min(1.0, dt * 3.0)
                    self.center_on(cx0 + (tx - cx0) * a, cy0 + (ty - cy0) * a)
            return
        mx, my = pygame.mouse.get_pos()
        if pygame.mouse.get_focused():
            from . import faction_ui as FUI
            FUI.set_cursor(self.p.faction["id"], self.cursor_kind(mx, my))
        keys = pygame.key.get_pressed()
        sp = self.settings.get("scroll_speed", 900) * dt
        focused = pygame.mouse.get_focused() and self.settings.get("edge_scroll", True)
        if keys[pygame.K_LEFT] or (focused and mx <= 2):
            self.cam_x -= sp
        if keys[pygame.K_RIGHT] or (focused and mx >= self.sw - 3):
            self.cam_x += sp
        if keys[pygame.K_UP] or (focused and my <= 2):
            self.cam_y -= sp
        if keys[pygame.K_DOWN] or (focused and my >= self.sh - 3):
            self.cam_y += sp
        self.clamp_cam()
        self.sel = [e for e in self.sel if e.alive]

    # ------------------------------------------------------------------
    # RAJZOLÁS
    def draw(self, fps=0):
        started = pygame.time.get_ticks()
        g, p = self.g, self.p
        tv = getattr(g, "terrain_version", 0)
        if tv != getattr(self, "_terrain_ver", 0):  # küldetés: omlás, elmozdult járat, gátnyitás
            self._terrain_ver = tv
            self._bake_terrain()
            if hasattr(self, "_chunks"):
                self._chunks.clear()
            self._pond_cache = None
        CALM.ENABLED = self.settings.get("calm_sprites", True)  # „nyugodt” figurák (csak a játéktéren)
        CALM.new_frame()
        if self.zoom != self.wanted_zoom():
            cx, cy = self.cam_x + self.vw / (2 * self.zoom), self.cam_y + self.vh / (2 * self.zoom)
            self.zoom = self.wanted_zoom()
            self.center_on(cx, cy)
        real, z = self.screen, self.zoom
        svw, svh = self.vw, self.vh
        hd = self.hd_on()
        self.sprites.hd = hd
        view = self.view
        if z > 1:  # HD: a képek a kamera-távolsághoz (E) egyszer kicsinyítve, közvetlenül a képernyőre
            E = z * view
            self.sprites.hd = E if hd else False
            W, H = int(-(-svw // E)), int(-(-svh // E))
            if hd:
                self.screen = gdraw.WorldCanvas(real, E, (W, H))
            else:
                if self._wsurf is None or self._wsurf.get_size() != (W, H):
                    self._wsurf = pygame.Surface((W, H))
                self.screen = self._wsurf
            self.vw, self.vh = W, H
            self._world_phase = True
        s = self.screen
        s.set_clip(pygame.Rect(0, 0, self.vw, self.vh))
        cx, cy = int(self.cam_x), int(self.cam_y)
        s.fill((0, 0, 0))
        if hd:
            self.draw_terrain_hd(s, cx, cy)
        else:
            if self.terrain is None:
                self._bake_full()
            s.blit(self.terrain, (0, 0), pygame.Rect(cx, cy, self.vw, self.vh))
        x0, y0 = max(0, cx // TILE - 2), max(0, cy // TILE - 1)
        x1 = min(g.map.w, (cx + self.vw) // TILE + 3)
        y1 = min(g.map.h, (cy + self.vh) // TILE + 2)
        if self.settings.get("water_anim", True):
            self.scenery.draw_water(s, cx, cy, x0, y0, x1, y1, self.t, self.E if hd else 1, p.explored, g.map.w)
        self.draw_creep(x0, y0, x1, y1)
        self.draw_foundations(x0, y0, x1, y1)
        self.draw_placement_area(cx, cy, x0, y0, x1, y1)
        self.draw_points()
        rcols = {k: tuple(v["color"]) for k, v in g.data.game["resources"].items()}
        m = g.map
        items = []  # (talajszint y, sorrend, objektum) – hátulról előre rajzolunk
        for ty in range(y0, min(m.h, y1 + 2)):
            row = ty * m.w
            for tx in range(x0, x1):
                r = m.res_grid[row + tx]
                if r is not None and r.tx == tx and r.ty == ty:
                    items.append((r.rect[1] + r.rect[3] * 0.5, 0, r))  # a törzs / a domb töve: a melletti lény elé kerül
        for b in g.buildings:
            if b.owner is not p:
                if g.is_visible(p, b):
                    self.seen.add(b.id)
                elif b.id not in self.seen:
                    continue
            bx, by = b.rect[0] - cx, b.rect[1] - cy
            if bx > self.vw + 40 or by > self.vh + 100 or bx + b.rect[2] < -60 or by + b.rect[3] < -20:
                continue
            items.append((b.rect[1] + b.rect[3], 1, b))
        vis_units = [u for u in g.units if u.inside is None and g.is_visible(p, u) and not self.in_mine(u)
                     and -60 < u.x - cx < self.vw + 60 and -60 < u.y - cy < self.vh + 90]
        sel_ids = {e.id for e in self.sel}
        for u in vis_units:
            items.append((u.y + (5000 if u.flying else 0), 2, u))
        for dc in getattr(g, "decor", ()):  # díszletépületek (tereptárgy)
            tx, ty = int(dc.x // TILE), int(dc.y // TILE)
            if m.inb(tx, ty) and p.explored[ty * m.w + tx] and \
                    -300 < dc.rect[0] - cx < self.vw + 60 and -60 < dc.rect[1] - cy < self.vh + 300:
                items.append((dc.rect[1] + dc.rect[3], 1, dc))
        for a in getattr(g, "actors", ()):  # ártalmatlan háttérszereplők
            tx, ty = int(a.x // TILE), int(a.y // TILE)
            if m.inb(tx, ty) and p.visible[ty * m.w + tx] and \
                    -60 < a.x - cx < self.vw + 60 and -60 < a.y - cy < self.vh + 90:
                items.append((a.y, 2, a))
        items.sort(key=lambda it: (it[0], it[1]))
        self.draw_corpses(cx, cy)
        if self.settings.get("shadows", True):
            live = {id(obj): obj for _, _, obj in items}
            shadows = []
            for obj_id, fr, dx, dy, foot, mode in getattr(self, "_shadow_items", ()):
                obj = live.get(obj_id)
                if obj is None or getattr(obj, "hidden", False) or getattr(obj, "burrowed", False):
                    continue
                shadows.append((fr, obj.x + dx, obj.y + dy, foot, mode))
            SHD.draw(s, shadows, cx, cy)
            for u in vis_units:
                if u.flying and not (u.hidden or u.burrowed) and self.sprites.unit(u.defn["id"]) is None:
                    r = u.radius
                    gdraw.ellipse(s, (20, 30, 20), (u.x - cx - r + 8, u.y - cy - r * 0.3 + 6,
                                                   r * 2, r * 0.8))
        self._shadow_next = []
        for u in vis_units:
            if u.id in sel_ids:
                col = (90, 255, 110) if u.owner is p else ((120, 200, 255) if p.allied(u.owner) else (255, 90, 80))
                w = max(24, u.radius * 2.5)
                img = self.sel_sigil(col, w)
                pulse = 0.82 + 0.18 * math.sin(self.t * 3.0)
                if pulse < 0.999:
                    img = img.copy()
                    img.set_alpha(int(255 * pulse))
                s.blit(img, (u.x - cx - img.get_width() / 2,
                             u.y - cy + u.radius * 0.45 + (0 if not u.flying else 2) - img.get_height() / 2))
        for b in self.own_sel_buildings():
            if b.rally:
                self.draw_rally_path(b, cx, cy, p.color)
        self.draw_move_indicator(cx, cy)
        if g.director is not None:  # küldetés-célpontok talaj-gyűrűje
            for mx_, my_, mr, label in g.director.markers():
                self.draw_objective_ring(mx_ - cx, my_ - cy, mr)
        self.draw_network(cx, cy)
        self.draw_ground_items(cx, cy)
        for e in g.effects:
            self.draw_area(e, e.x - cx, e.y - cy)
        mouse = pygame.mouse.get_pos()
        self._world_hover = self.mpos() if pygame.mouse.get_focused() and not self.over_hud(*mouse) else None
        self._hover_bar = None
        for _, kind, obj in items:
            if kind == 0:
                self.draw_res(obj, cx, cy, rcols)
            elif isinstance(obj, MS.Decor):
                self.draw_decor(obj, cx, cy)
            elif isinstance(obj, MS.Actor):
                self.draw_actor(obj, cx, cy)
            elif kind == 1:
                self.draw_building(obj, obj.rect[0] - cx, obj.rect[1] - cy)
            elif obj.flying and self.sprites.unit(obj.defn["id"]) is None:
                self.draw_unit(obj, obj.x - cx, obj.y - cy - 14)
            else:
                self.draw_unit(obj, obj.x - cx, obj.y - cy)
        self.draw_planned_buildings(cx, cy)
        for b in self.own_sel_buildings():  # a gyülekezési pont zászlaja (a nyíl-ösvény a talajon van)
            if b.rally:
                self.draw_rally_flag(b.rally[0] - cx, b.rally[1] - cy, p.color)
        self._shadow_items = self._shadow_next
        for pr in g.projectiles:
            self.draw_projectile(pr, cx, cy)
        self.draw_fx(cx, cy)
        if self.settings.get("ambient", True):
            self.ambient.draw(s, cx, cy, self.vw, self.vh, self.t, self.E if hd else 1)
        self.draw_fog(x0, y0, x1, y1)
        if g.director is not None:  # küldetés-célpontok: lebegő kristály és feliratos tábla (a köd fölött)
            for mx_, my_, mr, label in g.director.markers():
                self.draw_objective_beacon(mx_ - cx, my_ - cy, label)
        self.draw_hover_bar(cx, cy)
        self._world_hover = None
        for u in vis_units:
            if u.hero:
                sh = self.sprites.unit(u.defn["id"])
                top = sh.top_extent() + 4 if sh else u.radius + (18 if u.flying else 6)
                crown_y = u.y - cy - top - (4 if u.max_shield else 0)
                if self._hover_bar is not None and self._hover_bar[0] is u:
                    crown_y = min(crown_y, self._hover_bar[2] - (4 if u.max_shield else 0))
                self.draw_hero_crown(u, u.x - cx, crown_y)
        self.draw_mode_overlay(cx, cy)
        if self.drag:
            x0d, y0d = self.drag[0] / (z * view), self.drag[1] / (z * view)
            x1d, y1d = self.mpos()
            r = pygame.Rect(min(x0d, x1d), min(y0d, y1d), abs(x1d - x0d), abs(y1d - y0d))
            gdraw.rect(s, (90, 255, 110), r, 1)
        if z > 1:
            s.set_clip(None)
            self.screen, self.vw, self.vh = real, svw, svh
            self._world_phase = False
            if not hd:
                real.blit(pygame.transform.scale(self._wsurf, (self._wsurf.get_width() * z,
                                                               self._wsurf.get_height() * z)),
                          (0, 0), pygame.Rect(0, 0, svw, svh))
            s = real
            s.set_clip(pygame.Rect(0, 0, svw, svh))
        s.set_clip(None)
        self.draw_bottom_hud()
        self.draw_hud(fps)
        self.draw_portraits()
        if getattr(self, "net_wait", False) or getattr(self, "net_desync", False):
            msg = "Szinkronhiba a gépek között!" if self.net_desync else "Várakozás a többi játékosra…"
            img = self.fonts["med"].render(msg, True, (255, 220, 120))
            bg = pygame.Surface((img.get_width() + 20, img.get_height() + 10), pygame.SRCALPHA)
            bg.fill((0, 0, 0, 170))
            s.blit(bg, (self.vw / 2 - bg.get_width() / 2, 60))
            s.blit(img, (self.vw / 2 - img.get_width() / 2, 65))
        s.set_clip(None)
        self.draw_topbar()
        self.draw_tooltip()
        if self.show_help:
            self.draw_help()
        if self.paused:
            self.draw_center_box(["SZÜNET", "", "Esc / P: folytatás", "Q: kilépés a menübe"])
        if g.director is not None:
            self.draw_dialogue(g.director)
        if g.director is not None and g.director.pending:
            self.draw_choice(g.director.pending)
        elif self.path_pending():
            self.draw_path_choice()
        if g.result:
            self.draw_result()
        self._render_ms = pygame.time.get_ticks() - started

    _FOUND = None

    def foundation_tile(self):
        """Márvány-lapkő csempe (assets/ui/foundation.png, ha megrajzoltad; különben kódból)."""
        if UI._FOUND is None:
            art = self.sprites.art("ui/foundation")
            if art is not None:
                t = pygame.transform.smoothscale(art, (TILE * 2, TILE * 2)) if art.get_width() != TILE else art
                if t.get_width() == TILE * 2:
                    gdraw.mark_hd(t)
            else:
                import random
                rng = random.Random(7)
                t = pygame.Surface((TILE, TILE))
                t.fill((206, 200, 188))
                for _ in range(40):  # márványerezet
                    x, y = rng.randrange(TILE), rng.randrange(TILE)
                    pygame.draw.line(t, (182, 176, 168), (x, y), (x + rng.randrange(-6, 7), y + rng.randrange(2, 6)))
                pygame.draw.rect(t, (150, 144, 136), (0, 0, TILE, TILE), 1)
                pygame.draw.line(t, (232, 228, 220), (1, 1), (TILE - 2, 1))
                pygame.draw.line(t, (232, 228, 220), (1, 1), (1, TILE - 2))
            UI._FOUND = t
        return UI._FOUND

    _PONDS = None

    _SPIKES = None

    def spike_tile(self, x, y):
        """Mérgező tüskék csempéje (assets/ui/spikes_<0..3>.png, ha megrajzoltad; különben kódból, 4 változat)."""
        if UI._SPIKES is None:
            import random
            out = []
            for v in range(4):
                art = self.sprites.art(f"ui/spikes_{v}")
                if art is not None:
                    t = art if art.get_width() in (TILE, TILE * 2) else pygame.transform.smoothscale(art, (TILE * 2, TILE * 2))
                    if t.get_width() == TILE * 2:
                        gdraw.mark_hd(t)
                    out.append(t)
                    continue
                S = TILE * 2
                t = pygame.Surface((S, S), pygame.SRCALPHA)
                rng = random.Random(77 + v)
                pygame.draw.ellipse(t, (60, 30, 66, 120), (6, 10, S - 12, S - 18))  # nyálkás, lilás folt
                for _ in range(9):  # csont-színű tüskék zöld méreghegyekkel
                    bx, by = rng.randrange(10, S - 10), rng.randrange(18, S - 8)
                    hgt = rng.randrange(10, 22)
                    lean = rng.randrange(-4, 5)
                    pygame.draw.polygon(t, (40, 22, 30), [(bx - 4, by + 1), (bx + 4, by + 1), (bx + lean, by - hgt)])
                    pygame.draw.polygon(t, (220, 206, 176), [(bx - 3, by), (bx + 3, by), (bx + lean, by - hgt)])
                    pygame.draw.circle(t, (150, 255, 110), (bx + lean, by - hgt + 2), 2)
                gdraw.mark_hd(t)
                out.append(t)
            UI._SPIKES = out
        return UI._SPIKES[(x * 5 + y * 3) % 4]

    def pond_tile(self, x, y):
        """Tavirózsás víz-csempe (assets/ui/pond_<0..3>.png, ha megrajzoltad; különben kódból, 4 változat)."""
        if UI._PONDS is None:
            import random
            out = []
            for v in range(4):
                art = self.sprites.art(f"ui/pond_{v}")
                if art is not None:
                    t = art if art.get_width() in (TILE, TILE * 2) else pygame.transform.smoothscale(art, (TILE * 2, TILE * 2))
                    if t.get_width() == TILE * 2:
                        gdraw.mark_hd(t)
                    out.append(t)
                    continue
                rng = random.Random(31 + v)
                t = pygame.Surface((TILE, TILE), pygame.SRCALPHA)
                t.fill((46, 112, 120, 235))
                for _ in range(10):  # hullámfodrok
                    xx, yy = rng.randrange(TILE), rng.randrange(TILE)
                    pygame.draw.line(t, (90, 170, 170, 200), (xx, yy), (xx + 4, yy))
                if v in (0, 2):  # tavirózsalevél
                    cx, cy = rng.randrange(8, 24), rng.randrange(8, 24)
                    pygame.draw.circle(t, (60, 140, 60), (cx, cy), 6)
                    pygame.draw.polygon(t, (46, 112, 120), [(cx, cy), (cx + 7, cy - 2), (cx + 7, cy + 2)])
                    if v == 0:
                        pygame.draw.circle(t, (240, 180, 210), (cx - 2, cy - 2), 2)
                out.append(t)
            UI._PONDS = out
        return UI._PONDS[(x * 7 + y * 13) % 4]

    def draw_ponds(self, x0, y0, x1, y1):
        """Huk’ucha-tavacskák lágy, kerekded parttal (homoksáv + habvonal), mint a pálya vizei. A látott környékre
        (8 csempe ráhagyással) számolva; csak akkor rajzolódik újra, ha a tavak változnak vagy messzire görgetsz."""
        import numpy as np
        from .scenery import _upsample, _blur
        g = self.g
        m = g.map
        pd = g.pond
        k = self.screen.k if isinstance(self.screen, gdraw.WorldCanvas) else 1
        ver = (m.version, k)
        c = getattr(self, "_pond_cache", None)
        if not (c is not None and c[0] == ver and c[1] <= x0 and c[2] <= y0 and c[3] >= x1 and c[4] >= y1):
            fx0, fy0 = max(0, x0 - 8), max(0, y0 - 8)
            fx1, fy1 = min(m.w, x1 + 8), min(m.h, y1 + 8)
            surf = None
            w, h = fx1 - fx0, fy1 - fy0
            grid = np.zeros((h + 2, w + 2), dtype=np.float32)
            for yy in range(fy0 - 1, fy1 + 1):
                if 0 <= yy < m.h:
                    row = yy * m.w
                    for xx in range(fx0 - 1, fx1 + 1):
                        if 0 <= xx < m.w and pd[row + xx]:
                            grid[yy - fy0 + 1, xx - fx0 + 1] = 1.0
            if grid.any():
                PX = 8
                v = _blur(_upsample(grid, PX), 7)[PX:-PX, PX:-PX]  # erősebben lekerekített sarkok
                rng = np.random.default_rng((g.seed + fx0 * 31 + fy0) & 0xFFFFFFFFFFFFFFFF)
                n = (rng.random(v.shape) - 0.5) * 0.05
                water = np.clip((v + n - 0.5) / 0.07, 0, 1)
                outer = np.clip((_blur(v, 2) + n * 2 - 0.22) / 0.12, 0, 1)
                sand = np.clip(outer - water, 0, 1) * 0.9
                foam = np.clip(1 - np.abs(v + n - 0.52) / 0.035, 0, 1) * 0.75
                T = int(round(TILE * k))
                W2, H2 = w * T, h * T

                def layer(alpha, col=None):
                    sf = pygame.Surface((alpha.shape[1], alpha.shape[0]), pygame.SRCALPHA)
                    sf.fill(col or (255, 255, 255))
                    pa = pygame.surfarray.pixels_alpha(sf)
                    pa[...] = (alpha * 255).astype(np.uint8).T
                    del pa
                    return pygame.transform.smoothscale(sf, (W2, H2))
                surf = pygame.Surface((W2, H2), pygame.SRCALPHA)
                ts = self._tiles()
                if ts is not None and "sand" in ts.meta["rows"]:  # homokos part a tileset homokjából
                    srow = ts.meta["rows"]["sand"]
                    tex = pygame.Surface((W2, H2), pygame.SRCALPHA)
                    for yy in range(h):
                        for xx in range(w):
                            fr = self._hd_frame(ts, (xx * 5 + yy * 11) % srow["variants"], srow["row"]) if k > 1 else                                 ts.frame(None, (xx * 5 + yy * 11) % srow["variants"], srow["row"])
                            tex.blit(fr, (xx * T, yy * T))
                    tex.blit(layer(sand), (0, 0), special_flags=pygame.BLEND_RGBA_MULT)
                    surf.blit(tex, (0, 0))
                else:
                    surf.blit(layer(sand, (150, 136, 96)), (0, 0))
                tex = pygame.Surface((W2, H2), pygame.SRCALPHA)  # a tavacska vize (tavirózsás csempék)
                for yy in range(h):
                    for xx in range(w):
                        pt = self.pond_tile(fx0 + xx, fy0 + yy)
                        if pt.get_width() != T:
                            pt = pygame.transform.smoothscale(pt, (T, T))
                        tex.blit(pt, (xx * T, yy * T))
                tex.blit(layer(water), (0, 0), special_flags=pygame.BLEND_RGBA_MULT)
                surf.blit(tex, (0, 0))
                surf.blit(layer(foam, (230, 245, 240)), (0, 0))
                footprint = pygame.Surface((w, h), pygame.SRCALPHA)
                footprint.fill((255, 255, 255))
                alpha = pygame.surfarray.pixels_alpha(footprint)
                alpha[...] = (grid[1:-1, 1:-1] * 255).astype(np.uint8).T
                del alpha
                surf.blit(pygame.transform.scale(footprint, (W2, H2)), (0, 0),
                          special_flags=pygame.BLEND_RGBA_MULT)
                surf.set_alpha(255, pygame.RLEACCEL)
                if k > 1:
                    gdraw.mark_hd(surf)
            c = self._pond_cache = (ver, fx0, fy0, fx1, fy1, surf)
        if c[5] is not None:
            self.screen.blit(c[5], (c[1] * TILE - int(self.cam_x), c[2] * TILE - int(self.cam_y)))

    def _buildfx_tex(self):
        """A tavacska- és alapozás-építés animációjának képei (a meglévő csempékből, sziklákból, porból)."""
        key = self.theme_name()
        c = getattr(self, "_bfx_tex", None)
        if c is not None and c[0] == key:
            return c[1]
        ts = self._tiles()

        def row(name, fallback):
            if ts is not None and name in ts.meta.get("rows", {}):
                r = ts.meta["rows"][name]
                return [ts.image.subsurface((v * ts.fw, r["row"] * ts.fh, ts.fw, ts.fh)) for v in range(r["variants"])]
            t = pygame.Surface((TILE, TILE))
            t.fill(fallback)
            return [t]
        tex = {"dirt": row("dirt", (110, 84, 56)), "sand": row("sand", (176, 158, 112)),
               "pond": [self.pond_tile(v, 0) for v in range(4)], "marble": self.foundation_tile()}
        rk = self._tprop("rock")
        if rk is not None:
            tex["rocks"] = [rk.image.subsurface((i * rk.fw, 0, rk.fw, rk.fh)) for i in range(rk.meta.get("frames", 1))]
        du = self.sprites.effect("dust")
        if du is not None:
            tex["dust"] = [du.image.subsurface((i * du.fw, 0, du.fw, du.fh)) for i in range(du.image.get_width() // du.fw)]
        self._bfx_tex = (key, tex)
        return tex

    def draw_worksites(self, x0, y0, x1, y1):
        """Félkész tavacskák és alapozások: az ásás / lapkő-rakás haladása (a munkás parancsából)."""
        from . import buildfx as BF
        g, p = self.g, self.p
        sites = []
        m = g.map
        for s in getattr(g, "worksites", {}).values():
            n = s["n"]
            if s["tx"] + n < x0 - 1 or s["tx"] > x1 + 1 or s["ty"] + n < y0 - 1 or s["ty"] > y1 + 1:
                continue
            ci = min(m.h - 1, s["ty"] + n // 2) * m.w + min(m.w - 1, s["tx"] + n // 2)
            if s["owner"] is not p and not p.explored[ci]:
                continue
            if s["kind"] == "dig":
                prog = s["t"] / g.POND_TIME
            else:
                prog = s["t"] / max(0.1, g.data.defs[s["bid"]].get("time", 3))
            sites.append((s["kind"], s["tx"], s["ty"], n, min(1.0, prog), 0))
        if not sites:
            return
        k = self.screen.k if isinstance(self.screen, gdraw.WorldCanvas) else 1
        T = int(round(TILE * k))
        cache = self.__dict__.setdefault("_bfx_cache", {})
        if len(cache) > 64:
            cache.clear()
        tex = self._buildfx_tex()
        cx, cy = int(self.cam_x), int(self.cam_y)
        for kind, tx, ty, n, prog, uid in sites:
            q = (min(3, int(prog * 4)) + 1) / 4    # 4 fázis (25/50/75/100%), mint az épületek építése
            ck = (kind, tx, ty, n, T, q, self.theme_name())
            img = cache.get(ck)
            if img is None:
                if kind == "dig":
                    surf, M = BF.draw_pond(tex, n, T, q, "asas", seed=(tx * 7 + ty * 13) % 5)
                else:
                    surf, M = BF.draw_foundation(tex, n, T, q, "lapok", seed=(tx * 7 + ty * 13) % 5)
                img = surf.subsurface((M, M, n * T, n * T)).copy()
                if k > 1:
                    gdraw.mark_hd(img)
                cache[ck] = img
            self.screen.blit(img, (tx * TILE - cx, ty * TILE - cy))

    def _fit_world_tile(self, image):
        native = isinstance(self.screen, gdraw.WorldCanvas) and gdraw.is_hd(image)
        size = round(TILE * self.screen.k) if native else TILE
        if image.get_size() == (size, size):
            return image
        cache = self.__dict__.setdefault("_world_tile_sizes", {})
        key = (image, size, native)
        if key not in cache:
            if len(cache) >= 64:
                cache.clear()
            tile = pygame.transform.smoothscale(image, (size, size))
            cache[key] = gdraw.mark_hd(tile) if native else tile
        return cache[key]

    def draw_foundations(self, x0, y0, x1, y1):
        g, p = self.g, self.p
        pd = getattr(g, "pond", None)
        if pd and any(q.faction.get("pond") for q in g.players):
            self.draw_ponds(x0, y0, x1, y1)
        if any(q.faction.get("pond") or q.faction.get("foundation") for q in g.players):
            self.draw_worksites(x0, y0, x1, y1)
        sp = getattr(g, "spikes", None)
        if sp and any(sp):  # Croftian mérgező tüskék
            m = g.map
            cx, cy = int(self.cam_x), int(self.cam_y)
            for y in range(y0, y1):
                row = y * m.w
                for x in range(x0, x1):
                    o = sp[row + x]
                    if o and (o == p.id + 1 or p.explored[row + x]):
                        self.screen.blit(self._fit_world_tile(self.spike_tile(x, y)), (x * TILE - cx, y * TILE - cy))
        fd = getattr(g, "found", None)
        if not fd or not any(q.faction.get("foundation") for q in g.players):
            return
        m = g.map
        cx, cy = int(self.cam_x), int(self.cam_y)
        tile = self._fit_world_tile(self.foundation_tile())
        for y in range(y0, y1):
            row = y * m.w
            for x in range(x0, x1):
                o = fd[row + x]
                if o and (o == p.id + 1 or p.explored[row + x]):
                    self.screen.blit(tile, (x * TILE - cx, y * TILE - cy))

    def draw_creep(self, x0, y0, x1, y1):
        g = self.g
        creepers = [q for q in g.players if q.faction.get("mechanic") == "creep"]
        if not creepers or x1 <= x0 or y1 <= y0:
            return
        ts = self.sprites.tiles()
        if ts is not None and "creep" in ts.meta["rows"]:
            self.draw_creep_tiles(ts, creepers, x0, y0, x1, y1)
            return
        key = (x0, y0, x1, y1, g.creep_version)
        if key != self.creep_key:
            if self.creep_key is None or self.creep_key[4] != g.creep_version or not hasattr(self, "creep_src"):
                m = g.map
                n = m.w * m.h
                comb = bytearray(n)
                for q in creepers:
                    c = q.creep
                    comb = bytearray(a | b for a, b in zip(comb, c))
                buf = bytearray(n * 4)
                buf[0::4] = bytes([96]) * n
                buf[1::4] = bytes([44]) * n
                buf[2::4] = bytes([104]) * n
                buf[3::4] = comb.translate(bytes([0, 150] + [150] * 254))
                self.creep_src = pygame.image.frombuffer(bytes(buf), (m.w, m.h), "RGBA").convert_alpha()
            sub = self.creep_src.subsurface((x0, y0, x1 - x0, y1 - y0))
            self.creep_scaled = pygame.transform.smoothscale(sub, ((x1 - x0) * TILE, (y1 - y0) * TILE))
            self.creep_key = key
        self.screen.blit(self.creep_scaled, (x0 * TILE - int(self.cam_x), y0 * TILE - int(self.cam_y)))

    def draw_creep_tiles(self, ts, creepers, x0, y0, x1, y1):
        m = self.g.map
        info = ts.meta["rows"]["creep"]
        if getattr(self, "creep_tiles", (None, None, None))[2] is not ts:
            full = [ts.frame(None, v, info["row"]) for v in range(info["variants"])]
            edge = []
            for f in full:
                e = f.copy()
                e.set_alpha(150)
                if gdraw.is_hd(f):
                    gdraw.mark_hd(e)
                edge.append(e)
            self.creep_tiles = (full, edge, ts)
        full, edge, _ = self.creep_tiles
        if len(creepers) == 1:
            cm = creepers[0].creep
        else:
            cm = bytearray(creepers[0].creep)
            for q in creepers[1:]:
                for i, value in enumerate(q.creep):
                    cm[i] |= value
        cx, cy = int(self.cam_x), int(self.cam_y)
        blit = self.screen.blit
        nv = len(full)
        w = m.w
        for ty in range(y0, y1):
            row = ty * w
            for tx in range(x0, x1):
                if cm[row + tx]:
                    border = (tx == 0 or not cm[row + tx - 1] or tx == w - 1 or not cm[row + tx + 1]
                              or ty == 0 or not cm[row + tx - w] or ty == m.h - 1 or not cm[row + tx + w])
                    v = (tx * 7 + ty * 13) % nv
                    blit((edge if border else full)[v], (tx * TILE - cx, ty * TILE - cy))

    def draw_fog(self, x0, y0, x1, y1):
        g, p = self.g, self.p
        if x1 <= x0 or y1 <= y0:
            return
        signature = (bytes(p.visible), bytes(p.explored))
        if signature != self.fog_vis_obj:
            self.fog_vis_obj = signature
            n = g.map.w * g.map.h
            comb = bytes(a + b for a, b in zip(p.visible, p.explored))
            buf = bytearray(n * 4)
            buf[3::4] = comb.translate(bytes([250, 140, 0] + [0] * 253))
            self.fog_src = pygame.image.frombuffer(bytes(buf), (g.map.w, g.map.h), "RGBA").convert_alpha()
            self.fog_key = None
        k = self.screen.k if isinstance(self.screen, gdraw.WorldCanvas) else 1
        m = g.map
        if self.fog_key is not None and self.fog_key[4] == id(self.fog_src) and self.fog_key[5] == k and \
                self.fog_key[0] <= x0 and self.fog_key[1] <= y0 and self.fog_key[2] >= x1 and self.fog_key[3] >= y1:
            fx0, fy0 = self.fog_key[0], self.fog_key[1]
        else:  # a látott terület köré 8 csempés ráhagyás: görgetéskor ritkán kell újraszámolni
            fx0, fy0 = max(0, x0 - 8), max(0, y0 - 8)
            fx1, fy1 = min(m.w, x1 + 8), min(m.h, y1 + 8)
            sub = self.fog_src.subsurface((fx0, fy0, fx1 - fx0, fy1 - fy0))
            fs = pygame.transform.smoothscale(sub, (int(round((fx1 - fx0) * TILE * k)), int(round((fy1 - fy0) * TILE * k))))
            fs.set_alpha(255, pygame.RLEACCEL)  # a látott (átlátszó) részeket gyorsan átugorja
            if k > 1:
                gdraw.mark_hd(fs)
            self.fog_scaled = fs
            self.fog_key = (fx0, fy0, fx1, fy1, id(self.fog_src), k)
        self.screen.blit(self.fog_scaled, (fx0 * TILE - int(self.cam_x), fy0 * TILE - int(self.cam_y)))

    def unit_anim(self, u, sheet):
        g = self.g
        an = sheet.anims
        if g.time - u.last_cast < 0.6 and "cast" in an:
            return "cast", g.time - u.last_cast, True
        if g.time - u.last_attack < 0.5:
            return "attack", g.time - u.last_attack, True
        o = u.order
        walking = u.moving or g.time - getattr(u, "last_move", -9.0) < 0.25  # pillanatnyi megállásnál ne villanjon
        if o and "work" in an and not u.moving and (
                (o["type"] == "gather" and o.get("state") == "work") or o["type"] in ("construct", "repair")):
            return "work", u.anim, False
        if walking:
            return "walk", u.anim, False
        return "idle", u.anim, False

    HERO_MARK = "talaj+korona"   # változatok: "talaj", "korona", "talaj+korona" (a beállításokban is)

    def sel_sigil(self, color, w, inward=False):
        """Lágy belső fény, sötét kontúros kettős gyűrű és négy nyílhegy a tengelyeken."""
        key = ("selsig", tuple(color), int(w), inward)
        c = getattr(UI, "_HSIG", None)
        if c is None:
            c = UI._HSIG = {}
        img = c.get(key)
        if img is None:
            K = 3  # nagyban rajzolva, simán kicsinyítve (élsimítás)
            W, H = (int(w) + 12) * K, (int(w * 0.6) + 12) * K
            big = pygame.Surface((W, H), pygame.SRCALPHA)
            cx, cy = W / 2, H / 2
            rw, rh = w * K / 2, w * 0.3 * K
            for i in range(8, 0, -1):  # belső fény
                f = i / 8
                pygame.draw.ellipse(big, (*color, int(8 + 22 * (1 - f))), (cx - rw * f, cy - rh * f, 2 * rw * f, 2 * rh * f))
            pygame.draw.ellipse(big, (12, 14, 10, 190), (cx - rw - K, cy - rh - K, 2 * rw + 2 * K, 2 * rh + 2 * K), 4 * K)
            pygame.draw.ellipse(big, (*color, 255), (cx - rw, cy - rh, 2 * rw, 2 * rh), 2 * K)
            iw, ih = rw * 0.80, rh * 0.80
            pygame.draw.ellipse(big, (*color, 150), (cx - iw, cy - ih, 2 * iw, 2 * ih), K)
            light = tuple(min(255, v + 90) for v in color)
            direction = -1 if inward else 1
            for ang in (0, math.pi / 2, math.pi, 3 * math.pi / 2):
                ox, oy = math.cos(ang), math.sin(ang)
                bx, by = cx + ox * rw, cy + oy * rh
                tipx, tipy = bx + ox * 5 * K * direction, by + oy * 4 * K * direction
                px, py = -oy, ox
                bx -= ox * 2 * K * direction
                by -= oy * 2 * K * direction
                pts = [(bx + px * 4 * K, by + py * 3 * K), (tipx, tipy),
                       (bx - px * 4 * K, by - py * 3 * K)]
                pygame.draw.polygon(big, (12, 14, 10, 220), [(x + K, y + K) for x, y in pts])
                pygame.draw.polygon(big, (*light, 255), pts)
            img = pygame.transform.smoothscale(big, (W // K, H // K))
            c[key] = img
        return img

    def hero_sigil(self, color, w):
        """Hős talaj-jele: lágy fényudvar, kettős aranygyűrű és nyolc apró rovátka a játékos színével."""
        key = ("hsig", tuple(color), int(w))
        c = getattr(UI, "_HSIG", None)
        if c is None:
            c = UI._HSIG = {}
        img = c.get(key)
        if img is None:
            W, H = int(w) + 8, int(w * 0.56) + 8
            img = pygame.Surface((W, H), pygame.SRCALPHA)
            cx, cy = W / 2, H / 2
            for i in range(10, 0, -1):  # fényudvar
                f = i / 10
                pygame.draw.ellipse(img, (*color, int(10 + 26 * (1 - f))),
                                    (cx - w / 2 * f, cy - w * 0.28 * f, w * f, w * 0.56 * f))
            light = tuple(min(255, int(v * 0.6 + 110)) for v in color)
            pygame.draw.ellipse(img, (20, 14, 8, 200), (cx - w / 2, cy - w * 0.28, w, w * 0.56), 4)
            pygame.draw.ellipse(img, (*color, 245), (cx - w / 2, cy - w * 0.28, w, w * 0.56), 3)
            pygame.draw.ellipse(img, (*light, 220), (cx - w / 2 + 1, cy - w * 0.28 + 1, w - 2, w * 0.56 - 2), 1)
            iw = w * 0.78
            pygame.draw.ellipse(img, (*color, 200), (cx - iw / 2, cy - iw * 0.28, iw, iw * 0.56), 1)
            for k in range(8):
                a = k * math.pi / 4 + math.pi / 8
                x0, y0 = cx + math.cos(a) * w * 0.40, cy + math.sin(a) * w * 0.22
                x1, y1 = cx + math.cos(a) * w * 0.50, cy + math.sin(a) * w * 0.28
                pygame.draw.line(img, (255, 230, 150, 230), (x0, y0), (x1, y1), 2)
            c[key] = img
        return img

    @staticmethod
    def _faction_symbol(fid, S, col, dark):
        """A nép jelképe egyszerű, kis méretben is olvasható alakzatként (S×S, a csapat színével)."""
        img = pygame.Surface((S, S), pygame.SRCALPHA)
        c = S / 2

        def P(pts):
            return [(c + x * S, c + y * S) for x, y in pts]
        if fid == "elves":  # napkorong sugarakkal
            for k in range(8):
                a = k * math.pi / 4
                pygame.draw.polygon(img, col, [(c + math.cos(a - 0.22) * S * 0.24, c + math.sin(a - 0.22) * S * 0.24),
                                               (c + math.cos(a) * S * 0.47, c + math.sin(a) * S * 0.47),
                                               (c + math.cos(a + 0.22) * S * 0.24, c + math.sin(a + 0.22) * S * 0.24)])
            pygame.draw.circle(img, col, (c, c), S * 0.25)
            pygame.draw.circle(img, dark, (c, c), S * 0.12)
        elif fid == "ursan":  # medvemancs
            pygame.draw.ellipse(img, col, (c - S * 0.27, c - S * 0.02, S * 0.54, S * 0.42))
            for dx, dy in ((-0.30, -0.12), (-0.11, -0.30), (0.11, -0.30), (0.30, -0.12)):
                pygame.draw.circle(img, col, (c + dx * S, c + dy * S), S * 0.11)
        elif fid == "croftian":  # három görbe karom
            for dx in (-0.20, 0.08, 0.36):  # hajlított, hegyes karmok (balra görbülő ív)
                pts = []
                for k in range(9):
                    t = k / 8
                    pts.append((dx + 0.11 * (1 - t) ** 1.4 - 0.30 * t * t, 0.44 - 0.90 * t))
                for k in range(8, -1, -1):
                    t = k / 8
                    pts.append((dx - 0.05 * (1 - t) ** 1.4 - 0.30 * t * t, 0.44 - 0.90 * t))
                pygame.draw.polygon(img, col, P(pts))
        elif fid == "wullof":  # félhold
            pygame.draw.circle(img, col, (c, c), S * 0.44)
            pygame.draw.circle(img, (0, 0, 0, 0), (c + S * 0.18, c - S * 0.10), S * 0.38)
        elif fid == "imperium":  # kiterjesztett sasszárnyak
            for sg in (-1, 1):
                pygame.draw.polygon(img, col, P([(0, 0.05), (sg * 0.46, -0.30), (sg * 0.40, -0.05), (sg * 0.48, 0.02),
                                                 (sg * 0.34, 0.14), (sg * 0.38, 0.24), (0.0, 0.30)]))
            pygame.draw.circle(img, col, (c, c - S * 0.12), S * 0.10)
        elif fid == "hukucha":  # tavirózsalevél bevágással, rajta gyöngy
            pygame.draw.circle(img, col, (c, c + S * 0.04), S * 0.42)
            pygame.draw.polygon(img, (0, 0, 0, 0), P([(0, 0.06), (0.24, -0.5), (-0.24, -0.5)]))
            pygame.draw.circle(img, dark, (c, c + S * 0.12), S * 0.10)
        elif fid == "ooni":  # torii-kapu
            pygame.draw.polygon(img, col, P([(-0.48, -0.36), (0.48, -0.36), (0.42, -0.24), (-0.42, -0.24)]))
            pygame.draw.rect(img, col, (c - S * 0.34, c - S * 0.12, S * 0.68, S * 0.08))
            for dx in (-0.25, 0.17):
                pygame.draw.rect(img, col, (c + dx * S, c - S * 0.30, S * 0.08, S * 0.74))
        elif fid == "mycel":  # gombakalap tönkkel
            pygame.draw.ellipse(img, col, (c - S * 0.44, c - S * 0.40, S * 0.88, S * 0.52))
            pygame.draw.rect(img, col, (c - S * 0.10, c - S * 0.05, S * 0.20, S * 0.45))
        else:  # ismeretlen nép: csillag
            pts = []
            for k in range(10):
                r = S * (0.46 if k % 2 == 0 else 0.20)
                a = -math.pi / 2 + k * math.pi / 5
                pts.append((c + math.cos(a) * r, c + math.sin(a) * r))
            pygame.draw.polygon(img, col, pts)
        return img

    def hero_badge(self, fid, color, size):
        """Hős-jelvény a feje fölé: a nép jelképe a csapat színével, sötét körvonallal és vékony arany szegéllyel
        (nagyban rajzolva, majd simán kicsinyítve, hogy kis méretben is éles legyen)."""
        key = ("hbadge", fid, tuple(color), int(size))
        c = getattr(UI, "_HSIG", None)
        if c is None:
            c = UI._HSIG = {}
        img = c.get(key)
        if img is None:
            K = 6
            S = int(size) * K
            sym = self._faction_symbol(fid, S, tuple(color) + (255,), (30, 22, 12, 255))
            mask = pygame.mask.from_surface(sym)
            big = pygame.Surface((S + 6 * K, S + 6 * K), pygame.SRCALPHA)
            gold = mask.to_surface(setcolor=(255, 214, 110, 255), unsetcolor=(0, 0, 0, 0))
            dark = mask.to_surface(setcolor=(18, 12, 6, 255), unsetcolor=(0, 0, 0, 0))
            o = 3 * K
            for dx in range(-3 * K, 3 * K + 1, K):  # sötét körvonal
                for dy in range(-3 * K, 3 * K + 1, K):
                    if dx * dx + dy * dy <= (3 * K) ** 2:
                        big.blit(dark, (o + dx, o + dy))
            for dx in range(-K * 3 // 2, K * 3 // 2 + 1, K // 2):  # arany szegély
                for dy in range(-K * 3 // 2, K * 3 // 2 + 1, K // 2):
                    if dx * dx + dy * dy <= (K * 3 // 2) ** 2:
                        big.blit(gold, (o + dx, o + dy))
            big.blit(sym, (o, o))
            hl = pygame.Surface(big.get_size(), pygame.SRCALPHA)  # felső fény
            hl.blit(mask.to_surface(setcolor=(255, 255, 255, 60), unsetcolor=(0, 0, 0, 0)), (o, o - K))
            big.blit(hl, (0, 0))
            w = big.get_width() // K
            img = pygame.transform.smoothscale(big, (w, w))
            c[key] = img
        return img

    def draw_hero_ground(self, u, sx, sy):
        if not u.hero or "talaj" not in self.settings.get("hero_mark", self.HERO_MARK):
            return
        w = max(30, u.radius * 3.0)
        img = self.hero_sigil(u.owner.color, w)
        pulse = 0.85 + 0.15 * math.sin(self.t * 2.4 + u.id)
        if pulse < 0.999:
            img = img.copy()
            img.set_alpha(int(255 * pulse))
        self.screen.blit(img, (sx - img.get_width() / 2, sy + u.radius * 0.35 - img.get_height() / 2))

    def draw_hero_crown(self, u, sx, top_y):
        if not u.hero or "korona" not in self.settings.get("hero_mark", self.HERO_MARK):
            return
        size = 15
        img = self.hero_badge(u.owner.faction["id"], u.owner.color, size)
        bob = math.sin(self.t * 2.0 + u.id) * 1.5
        self.screen.blit(img, (sx - img.get_width() / 2, top_y - img.get_height() - 3 + bob))

    def _queue_shadow(self, obj, frame, x, y, foot=None, mode=None):
        self._shadow_next.append((id(obj), frame, x - obj.x, y - obj.y, foot, mode))

    def draw_unit(self, u, sx, sy):
        if u.hero and not u.flying:
            self.draw_hero_ground(u, sx, sy)
        self.draw_ground_marks(u, sx, sy)
        sheet = self.sprites.unit(u.defn["id"])
        if sheet is not None:
            anim, t, clamp = self.unit_anim(u, sheet)
            fr = sheet.anim_frame(u.owner.color, anim, t, dir_index(u.angle), clamp)
            pos = (sx - sheet.anchor[0], sy - sheet.anchor[1])
            st = self.unit_states(u)
            tint = self.status_tint(u, st)
            if tint is not None:  # állapot: a figura (pulzáló) színárnyalata – kikapcsolható
                hdf = gdraw.is_hd(fr)
                fr = fr.copy()
                fr.fill(tint, special_flags=pygame.BLEND_RGB_MULT)
                if hdf:
                    gdraw.mark_hd(fr)
            if u.hidden or u.burrowed:  # rejtett / beásott: áttetsző (beásva csak a hátát látni)
                hdf = gdraw.is_hd(fr)
                fr = fr.copy()
                fr.set_alpha(70 if u.burrowed else 120)
                if hdf:
                    gdraw.mark_hd(fr)
            if sheet.team is None and "pack" in sheet.meta and not u.flying:
                # csapatszín nélküli festett sprite: színes talpgyűrű jelzi a gazdát
                rr = pygame.Rect(0, 0, u.radius * 2.2, u.radius * 1.2)
                rr.center = (sx, sy + 1)
                gdraw.ellipse(self.screen, shade(u.owner.color, 0.85), rr, 2)
            if u.hidden or u.burrowed:
                self.screen.blit(fr, pos)
            else:
                gdraw.blit_trim(self.screen, fr, *pos)
                if self.settings.get("shadows", True):
                    SHD.blit_inner(self.screen, fr, *pos)
                    if hasattr(self, "_shadow_next"):
                        # A magasban repülő egység árnyéka a fény irányában, a talajra vetül.
                        self._queue_shadow(u, fr, pos[0] + int(self.cam_x) + (12 if u.flying else 0),
                                           pos[1] + int(self.cam_y) + (8 if u.flying else 0),
                                           int(sheet.anchor_px[1]))
            self.hover_sprite(u, fr, pos, sx, max(u.radius * 2.2, 22))
            if u.flash > 0:
                self.screen.blit(fr, pos, special_flags=pygame.BLEND_RGB_ADD)
            self.draw_unit_marks(u, sx, sy)
            self.draw_status_fx(u, sx, sy, sheet.top_extent(), st)
            return
        if self._portrait_token(u, sx, sy):  # képlap még nincs (pl. szövetséges népek): arckép-érme
            self.draw_unit_marks(u, sx, sy)
            self.draw_status_fx(u, sx, sy, max(18, u.radius * 2.6) * 1.05, self.unit_states(u))
            return
        f = u.owner.faction
        base = tuple(f["color"]) if u.flash <= 0 else (255, 255, 255)
        self.hover_shape(u, pygame.Rect(sx - u.radius, sy - u.radius, u.radius * 2, u.radius * 2),
                         sx, max(u.radius * 2.2, 22))
        draw_shape(self.screen, u.defn.get("shape", ""), sx, sy, u.radius, base, u.owner.color,
                   tuple(f["accent"]), u.angle, u.anim, u.moving)
        self.draw_unit_marks(u, sx, sy)
        self.draw_status_fx(u, sx, sy, u.radius * 2, self.unit_states(u))

    def _portrait_token(self, u, sx, sy):
        """Ideiglenes megjelenés képlap nélküli egységnek: kerek arckép csapatszínű kerettel, árnyékkal és
        egy kis lépés-billegéssel (amíg a festett képlap el nem készül)."""
        uid = u.defn["id"]
        cache = self.__dict__.setdefault("_tokens", {})
        d = max(18, int(u.radius * 2.6))
        key = (uid, d)
        img = cache.get(key, 0)
        if img == 0:
            img = None
            src = self.sprites.portrait(uid)
            if src is not None:
                sq = pygame.transform.smoothscale(src, (d, d))
                mask = pygame.Surface((d, d), pygame.SRCALPHA)
                pygame.draw.circle(mask, (255, 255, 255, 255), (d // 2, d // 2), d // 2)
                img = pygame.Surface((d, d), pygame.SRCALPHA)
                img.blit(sq, (0, 0))
                img.blit(mask, (0, 0), special_flags=pygame.BLEND_RGBA_MIN)
            cache[key] = img
        if img is None:
            return False
        bob = abs(math.sin(u.anim * 9)) * 2 if u.moving else 0
        cy = sy - d * 0.55 - bob
        gdraw.ellipse(self.screen, (0, 0, 0), (sx - d * 0.42, sy - d * 0.14, d * 0.84, d * 0.28))
        if getattr(u, "hidden", False) or getattr(u, "burrowed", False):
            img = img.copy()
            img.set_alpha(70 if getattr(u, "burrowed", False) else 120)
        pos = (sx - d / 2, cy - d / 2)
        self.screen.blit(img, pos)
        self.hover_sprite(u, img, pos, sx, max(u.radius * 2.2, 22))
        col = (255, 255, 255) if u.flash > 0 else tuple(u.owner.color)
        gdraw.circle(self.screen, col, (sx, cy), d // 2 + 1, 2)
        return True

    def _mark_img(self, kind, w, phase=0):
        """Talaj-jelek egységállapotokhoz (nagyban rajzolva, simán kicsinyítve; képkockánként gyorsítótárból).
        chant: forgó rúnagyűrű; buff: arany fény; slow: jeges lila gyűrű kristályszilánkokkal; hidden: pöttyözött."""
        key = ("mark", kind, int(w), phase)
        c = self.__dict__.setdefault("_marks", {})
        img = c.get(key)
        if img is not None:
            return img
        if len(c) > 300:
            c.clear()
        K = 3
        W, H = (int(w) + 14) * K, (int(w * 0.6) + 14) * K
        big = pygame.Surface((W, H), pygame.SRCALPHA)
        cx, cy = W / 2, H / 2
        rw, rh = w * K / 2, w * 0.3 * K

        def ell(col, f, width=0):
            pygame.draw.ellipse(big, col, (cx - rw * f, cy - rh * f, 2 * rw * f, 2 * rh * f), width)
        if kind == "chant":
            col = (150, 205, 255)
            ell((10, 16, 28, 150), 1.06, 3 * K)
            for i in range(24):  # szaggatott külső gyűrű
                if i % 3 == 2:
                    continue
                a0 = 2 * math.pi * i / 24 + phase * 0.13
                pygame.draw.arc(big, (*col, 230), (cx - rw, cy - rh, 2 * rw, 2 * rh), a0, a0 + 0.2, 2 * K)
            ell((*col, 90), 0.78, K)
            for i in range(6):  # rúnák a gyűrűn (kis rombuszok), lassan körbejárnak
                a = 2 * math.pi * i / 6 + phase * 0.26
                px, py = cx + math.cos(a) * rw * 0.89, cy + math.sin(a) * rh * 0.89
                s = 2.6 * K
                pygame.draw.polygon(big, (10, 16, 28, 220), [(px, py - s - K), (px + s * 0.7 + K, py),
                                                             (px, py + s + K), (px - s * 0.7 - K, py)])
                pygame.draw.polygon(big, (225, 240, 255, 255), [(px, py - s), (px + s * 0.7, py), (px, py + s),
                                                                (px - s * 0.7, py)])
        elif kind == "buff":
            col = (255, 214, 110)
            for i in range(10, 0, -1):
                f = i / 10
                ell((*col, int(10 + 30 * (1 - f))), f * 1.02)
            ell((60, 40, 10, 160), 1.04, 3 * K)
            ell((*col, 235), 1.0, 2 * K)
        elif kind == "slow":
            col = (186, 130, 255)
            ell((150, 190, 255, 45), 0.95)
            ell((20, 10, 34, 170), 1.05, 3 * K)
            ell((*col, 230), 1.0, 2 * K)
            for i in range(7):  # jégszilánkok a peremen
                a = 2 * math.pi * i / 7 + 0.3
                px, py = cx + math.cos(a) * rw, cy + math.sin(a) * rh
                h = (5 + (i % 3) * 2) * K
                pts = [(px - 2 * K, py), (px, py - h), (px + 2 * K, py)]
                pygame.draw.polygon(big, (20, 10, 34, 200), [(x, y + K) for x, y in pts])
                pygame.draw.polygon(big, (215, 235, 255, 245), pts)
        else:  # hidden
            for i in range(28):
                a = 2 * math.pi * i / 28
                pygame.draw.circle(big, (175, 180, 215, 170), (cx + math.cos(a) * rw, cy + math.sin(a) * rh), 1.6 * K)
        img = pygame.transform.smoothscale(big, (W // K, H // K))
        c[key] = img
        return img

    OBJ_GOLD = (255, 214, 110)

    def draw_objective_ring(self, x, y, r):
        """Küldetés-célpont a talajon: lapított (mint a kijelölés talaj-jele) arany gyűrű, lassan forgó szaggatott
        külső perem, rúna-rombuszok, lágy belső fény; sötét kontúr, hogy minden talajon olvasható legyen."""
        r = max(18, int(r))
        ph = int(self.t * 6) % 36
        key = ("objring", r, ph)
        c = self.__dict__.setdefault("_objring", {})
        img = c.get(key)
        if img is None:
            if len(c) > 160:
                c.clear()
            K = 2
            W, H = (2 * r + 20) * K, (int(r * 1.2) + 20) * K
            big = pygame.Surface((W, H), pygame.SRCALPHA)
            cx, cy = W / 2, H / 2
            rw, rh = r * K, r * 0.6 * K
            gold = self.OBJ_GOLD

            def ell(col, f, width=0):
                pygame.draw.ellipse(big, col, (cx - rw * f, cy - rh * f, 2 * rw * f, 2 * rh * f), width)
            for i in range(10, 0, -1):  # belső fény
                f = i / 10 * 0.86
                ell((*gold, int(4 + 26 * (1 - i / 10))), f)
            ell((30, 20, 6, 170), 1.0, 4 * K)
            ell((*gold, 255), 1.0, 2 * K)
            ell((*gold, 140), 0.84, K)
            seg = 28
            for i in range(seg):  # forgó, szaggatott külső perem
                if i % 2:
                    continue
                a0 = 2 * math.pi * i / seg + ph * 2 * math.pi / 36 / 4
                pygame.draw.arc(big, (255, 240, 180, 230), (cx - rw * 1.12, cy - rh * 1.12, 2 * rw * 1.12,
                                                             2 * rh * 1.12), a0, a0 + 0.14, max(1, K))
            for i in range(4):  # rúna-rombuszok a tengelyeken
                a = math.pi / 4 + i * math.pi / 2
                px, py = cx + math.cos(a) * rw, cy + math.sin(a) * rh
                sz = 4 * K
                pts = [(px, py - sz), (px + sz * 0.7, py), (px, py + sz), (px - sz * 0.7, py)]
                pygame.draw.polygon(big, (30, 20, 6, 220), [(qx + K, qy + K) for qx, qy in pts])
                pygame.draw.polygon(big, (255, 246, 210, 255), pts)
            img = pygame.transform.smoothscale(big, (W // K, H // K))
            c[key] = img
        pulse = 0.85 + 0.15 * math.sin(self.t * 3)
        if pulse < 0.999:
            img = img.copy()
            img.set_alpha(int(255 * pulse))
        self.screen.blit(img, (x - img.get_width() / 2, y - img.get_height() / 2))

    def draw_objective_beacon(self, x, y, label):
        """A célpont fölött lebegő, lassan billegő arany kristály, alatta halvány fénysugár; a felirat sötét,
        arany szegélyű táblán."""
        s = self.screen
        bob = math.sin(self.t * 2.4) * 3
        top = y - 46 + bob
        beam = self.__dict__.get("_objbeam")
        if beam is None:  # függőleges, lefelé halványuló fénysáv
            beam = pygame.Surface((10, 40), pygame.SRCALPHA)
            for yy in range(40):
                a = int(70 * (1 - yy / 40))
                pygame.draw.line(beam, (255, 230, 150, a), (2, yy), (7, yy))
            self._objbeam = beam
        s.blit(beam, (x - 5, top + 8))
        g = self.OBJ_GOLD
        w2, h1, h2 = 7, 9, 12   # kristály: felső és alsó gúla
        pts = [(x, top - h1), (x + w2, top), (x, top + h2), (x - w2, top)]
        gdraw.polygon(s, (30, 20, 6), [(px + 1, py + 1) for px, py in pts])
        gdraw.polygon(s, g, pts)
        gdraw.polygon(s, (255, 246, 210), [(x, top - h1), (x + w2, top), (x, top)])   # fényes oldal
        gdraw.polygon(s, (214, 160, 60), [(x, top), (x - w2, top), (x, top + h2)])    # árnyékos oldal
        if label:
            f = self.fonts["small"]
            img = f.render(label, True, (255, 240, 190))
            pw, ph = img.get_width() + 14, img.get_height() + 6
            rx, ry = x - pw / 2, top - h1 - 6 - ph
            gdraw.rect(s, (20, 16, 12), (rx + 2, ry + 2, pw, ph))
            gdraw.rect(s, (36, 28, 20), (rx, ry, pw, ph))
            gdraw.rect(s, g, (rx, ry, pw, ph), 1)
            gdraw.line(s, (120, 90, 40), (rx + 3, ry + ph - 3), (rx + pw - 3, ry + ph - 3), 1)
            s.blit(img, (rx + 7, ry + 3))

    def _draw_marching_arrows(self, x0, y0, rx, ry, color, margin):
        s = self.screen
        dx, dy = rx - x0, ry - y0
        d = math.hypot(dx, dy)
        if d <= margin:
            return
        ux, uy = dx / d, dy / d
        light = tuple(min(255, c + 70) for c in color)
        step = 15.0
        px, py = -uy, ux
        k = (self.t * 18) % step
        while k < d - margin:
            x, y = x0 + ux * k, y0 + uy * k
            sz = 4.2
            tip = (x + ux * sz, y + uy * sz * 0.6)
            l = (x - ux * sz * 0.6 + px * sz, y - uy * sz * 0.4 + py * sz * 0.6)
            r = (x - ux * sz * 0.6 - px * sz, y - uy * sz * 0.4 - py * sz * 0.6)
            gdraw.polygon(s, (14, 16, 12), [(q[0] + 1, q[1] + 1) for q in (tip, l, r)])
            gdraw.polygon(s, light, (tip, l, r))
            k += step

    def draw_rally_path(self, b, cx, cy, color):
        """A gyülekezési út az épület és az egységek alatt halad."""
        rx, ry = b.rally[0] - cx, b.rally[1] - cy
        if math.hypot(rx - (b.x - cx), ry - (b.y - cy)) <= 14:
            return
        self._draw_marching_arrows(b.x - cx, b.y - cy, rx, ry, color, 14)
        img = self.sel_sigil(tuple(color), 26)
        self.screen.blit(img, (rx - img.get_width() / 2, ry - img.get_height() / 2 + 2))
        self.draw_rally_flag_shadow(rx, ry)

    def draw_move_indicator(self, cx, cy):
        units = self.own_sel_units()
        orders = [u.order for u in units if u.order and u.order["type"] in ("move", "amove")
                  and not u.order.get("auto")]
        if not orders:
            return
        order = max(orders, key=lambda o: o.get("move_serial", -1))
        x, y = order.get("destination", (order["x"], order["y"]))
        x0 = sum(u.x for u in units) / len(units) - cx
        y0 = sum(u.y for u in units) / len(units) - cy
        rx, ry = x - cx, y - cy
        color = (60, 205, 92)
        self._draw_marching_arrows(x0, y0, rx, ry, color, 20)
        img = self.sel_sigil(color, 38, inward=True)
        self.screen.blit(img, (rx - img.get_width() / 2, ry - img.get_height() / 2))

    def draw_rally_flag(self, x, y, color):
        """A gyülekezési pont zászlaja: vékony rúd, lengő, kétágú (fecskefarkú) zászló a csapat színében.
        Külön képre rajzolva (a lengés 8 fázisa gyorsítótárból), így a figurákhoz hasonlóan árnyékot is vet."""
        ph = int(self.t * 5 / (2 * math.pi) * 8) % 8
        key = (tuple(color), ph)
        c = self.__dict__.setdefault("_flag_img", {})
        img = c.get(key)
        if img is None:
            K = 3
            W, H = 22, 32               # a talppont: (3, 30)
            big = pygame.Surface((W * K, H * K), pygame.SRCALPHA)
            bx, by = 3 * K, 30 * K
            top = by - 26 * K
            wave = math.sin(ph / 8 * 2 * math.pi) * 2 * K
            pygame.draw.line(big, (20, 16, 12), (bx + K, by), (bx + K, top + K), 3 * K)
            pygame.draw.line(big, (196, 176, 136), (bx, by), (bx, top), 2 * K)
            pygame.draw.circle(big, (240, 214, 140), (bx, top), 2 * K)
            pts = [(bx + K, top + 2 * K), (bx + 15 * K, top + 3 * K + wave * 0.5), (bx + 11 * K, top + 7 * K + wave * 0.3),
                   (bx + 15 * K, top + 11 * K + wave), (bx + K, top + 11 * K)]
            pygame.draw.polygon(big, (20, 16, 12), [(a + K, b + K) for a, b in pts])
            pygame.draw.polygon(big, tuple(color), pts)
            pygame.draw.line(big, tuple(min(255, v + 80) for v in color), (bx + 2 * K, top + 4 * K),
                             (bx + 12 * K, top + 4 * K + wave * 0.5), K)
            img = pygame.transform.smoothscale(big, (W, H))
            c[key] = img
        self.screen.blit(img, (x - 3, y - 30))

    def draw_rally_flag_shadow(self, x, y):
        """A zászló vetett árnyéka a talajon (a figurák alatt): a rúd és a zászló a fénnyel ellentétes irányba
        (jobbra-le) lapítva, mint a figurák árnyéka; puha szélű."""
        ph = int(self.t * 5 / (2 * math.pi) * 8) % 8
        c = self.__dict__.setdefault("_flag_sh", {})
        img = c.get(ph)
        if img is None:
            K = 3
            W, H = 40, 20
            big = pygame.Surface((W * K, H * K), pygame.SRCALPHA)
            ox, oy = 3 * K, 3 * K                      # a rúd töve a képen

            def gp(dx, h):                            # talaj-pont: dx oldalra, h magasan lévő pontból
                return ox + (dx + h * 0.62) * K, oy + h * 0.44 * K
            wave = math.sin(ph / 8 * 2 * math.pi) * 2
            pygame.draw.line(big, (8, 10, 14, 150), gp(0, 0), gp(0, 26), 2 * K)
            pts = [gp(1, 24), gp(15, 23 - wave * 0.5), gp(11, 19 - wave * 0.3), gp(15, 15 - wave), gp(1, 15)]
            pygame.draw.polygon(big, (8, 10, 14, 140), pts)
            img = pygame.transform.smoothscale(big, (W, H))
            img = pygame.transform.smoothscale(pygame.transform.smoothscale(img, (W // 2, H // 2)), (W, H))
            c[ph] = img
        self.screen.blit(img, (x - 3, y - 3))

    STATUS_TINT = {"poison": (150, 255, 120), "slow": (175, 195, 255), "sleep": (200, 200, 215),
                   "burn": (255, 170, 120), "stun": (255, 245, 170), "buff": (255, 235, 175),
                   "wet": (160, 205, 255), "root": (205, 180, 130), "fear": (215, 190, 255), "blind": (150, 150, 155),
                   "sunder": (230, 160, 130), "silence": (200, 170, 235), "mark": (255, 150, 150)}

    def status_tint(self, u, st):
        """A figura színárnyalata az állapot szerint, lassan pulzálva (a beállításokban kikapcsolható).
        Több hatás esetén ezek váltakoznak."""
        if not self.settings.get("status_pulse", True):
            return None
        kinds = [k for k in ("burn", "poison", "slow", "wet", "root", "fear", "blind", "sunder", "silence", "mark",
                             "stun", "buff", "sleep") if st.get(k)]
        if not kinds:
            return None
        ph = self.t * 1.6 + (u.id * 0.37) % 6.28
        kind = kinds[int(ph / math.pi) % len(kinds)] if len(kinds) > 1 else kinds[0]
        k = 0.35 + 0.65 * (0.5 + 0.5 * math.sin(ph * 2))   # 35%–100% erősségű árnyalat
        c = self.STATUS_TINT[kind]
        return tuple(int(255 - (255 - v) * k) for v in c)

    def _hostile_areas(self):
        """A sebző területi hatások (tűz-, villám-, méregmezők) képkockánként egyszer összegyűjtve."""
        key = self.g.tick
        c = getattr(self, "_areas_cache", None)
        if c is None or c[0] != key:
            c = (key, [e for e in self.g.effects if getattr(e, "ab", None) and e.ab.get("dps") and e.alive])
            self._areas_cache = c
        return c[1]

    def unit_states(self, u):
        """Az egységre ható állapotok a megjelenítéshez (a szabályokon nem változtat).
        burn: ellenséges sebző mezőben áll (a láng a mező színét kapja); sleep: táborában tétlenül álló szörny."""
        g = self.g
        st = {}
        now = g.time
        if u.poison is not None and u.poison[2] > now:
            st["poison"] = True
        b = getattr(u, "burn", None)
        if b is not None and b[2] > now:
            st["burn"] = (255, 130, 40)
        if u.slow_until > now:
            st["slow"] = True
        if u.stun_until > now:
            st["stun"] = True
        if any(t > now for t in u.buffs.values()):
            st["buff"] = True
        for key, attr in (("wet", "wet_until"), ("root", "rooted_until"), ("fear", "feared_until"),
                          ("blind", "blind_until"), ("sleep", "asleep_until"), ("sunder", "sunder_until"),
                          ("silence", "silenced_until"), ("mark", "marked_until")):
            if getattr(u, attr, 0.0) > now:
                st[key] = True
        for e in (self._hostile_areas() if not st.get("burn") else ()):
            if e.owner is not u.owner and e.owner.hostile(u.owner) and \
                    abs(u.x - e.x) <= e.radius and abs(u.y - e.y) <= e.radius and \
                    math.hypot(u.x - e.x, u.y - e.y) <= e.radius:
                st["burn"] = e.color
                break
        if getattr(u, "camp", None) is not None and u.order is None and not st.get("burn") and u.hp >= u.max_hp:
            st["sleep"] = True
        st["tint"] = "poison" if st.get("poison") else "slow" if st.get("slow") else \
            "burn" if st.get("burn") else "sleep" if st.get("sleep") else None
        return st

    def _zzz(self, size):
        c = self.__dict__.setdefault("_zzz_img", {})
        img = c.get(size)
        if img is None:
            f = load_font(pygame, size, bold=True)
            t = f.render("Z", True, (205, 218, 255))
            o = f.render("Z", True, (20, 24, 44))
            img = pygame.Surface((t.get_width() + 2, t.get_height() + 2), pygame.SRCALPHA)
            img.blit(o, (2, 2))
            img.blit(o, (0, 2))
            img.blit(t, (1, 0))
            c[size] = img
        return img

    def draw_status_fx(self, u, sx, sy, top, st):
        """Állapotok a testen (B változat): részecskék a figura körül, sötét kontúrral, hogy minden talajon
        olvashatók legyenek. Lassult: jégkristályok; ég: lángnyelvek; mérgezett: buborékok; alszik: Z Z Z;
        kábult: forgó csillagok; erősítve: szikrák."""
        if not st or not self.settings.get("status_fx", True):
            return
        s = self.screen
        t = self.t
        ph = (u.id * 0.6180339) % 1.0
        r = max(8, u.radius)
        if st.get("slow"):  # jégkristályok lassan körbe
            for i in range(4):
                a = t * 0.9 + i * math.pi / 2 + ph * 6
                px, py = sx + math.cos(a) * r * 1.05, sy - top * (0.25 + 0.18 * (i % 2)) + math.sin(a) * r * 0.35
                k = 3.2 if i % 2 else 2.4
                pts = [(px, py - k * 1.7), (px + k * 0.7, py), (px, py + k * 1.7), (px - k * 0.7, py)]
                gdraw.polygon(s, (24, 20, 46), [(x + 1, y + 1) for x, y in pts])
                gdraw.polygon(s, (226, 238, 255), pts)
        burn = st.get("burn")
        if burn:  # lángnyelvek a figura alsó felén, a mező színében
            light = tuple(min(255, c + 80) for c in burn)
            for i in range(5):
                q = (t * 1.6 + i * 0.37 + ph) % 1.0
                px = sx + (i - 2) * r * 0.42 + math.sin(t * 7 + i) * 1.5
                py = sy - top * (0.05 + 0.45 * q)
                h = (7 + 3 * (i % 2)) * (1 - q * 0.6)
                w = 3 * (1 - q * 0.5)
                pts = [(px, py - h), (px + w, py), (px - w, py)]
                gdraw.polygon(s, (40, 16, 6), [(x + 1, y + 1) for x, y in pts])
                gdraw.polygon(s, burn, pts)
                gdraw.polygon(s, light, [(px, py - h * 0.55), (px + w * 0.5, py), (px - w * 0.5, py)])
        if st.get("poison"):  # buborékok szállnak fel
            for i in range(4):
                q = (t * 0.7 + i * 0.25 + ph) % 1.0
                px = sx + math.sin(i * 2.3 + ph * 9) * r * 0.7 + math.sin(t * 3 + i) * 1.2
                py = sy - top * (0.15 + 0.8 * q)
                rr = 1.6 + 1.6 * (1 - abs(q - 0.5) * 2)
                gdraw.circle(s, (30, 70, 16), (px, py), rr + 1)
                gdraw.circle(s, (150, 240, 90), (px, py), rr)
                gdraw.circle(s, (220, 255, 190), (px - rr * 0.35, py - rr * 0.35), max(1, rr * 0.35))
        if st.get("wet"):  # hulló vízcseppek
            for i in range(4):
                q = (t * 1.3 + i * 0.25 + ph) % 1.0
                px = sx + (i - 1.5) * r * 0.55
                py = sy - top * (0.95 - 0.8 * q)
                gdraw.polygon(s, (16, 30, 60), [(px + 1, py - 3), (px + 3, py + 2), (px - 1, py + 2)])
                gdraw.polygon(s, (150, 205, 255), [(px, py - 4), (px + 2, py + 1), (px - 2, py + 1)])
                gdraw.circle(s, (150, 205, 255), (px, py + 1), 2)
        if st.get("root"):  # gyökérindák fonják körbe a lábat
            for i in range(5):
                a = i * 2 * math.pi / 5 + ph * 3
                bx, by = sx + math.cos(a) * r * 0.9, sy + math.sin(a) * r * 0.3
                tipx, tipy = sx + math.cos(a) * r * 0.35, sy - top * (0.22 + 0.06 * math.sin(t * 2 + i))
                midx, midy = (bx + tipx) / 2 + math.sin(a) * 4, (by + tipy) / 2
                for (x1, y1, x2, y2), col, w in (((bx, by, midx, midy), (40, 26, 10), 4), ((midx, midy, tipx, tipy), (40, 26, 10), 3),
                                                 ((bx, by, midx, midy), (120, 92, 50), 2), ((midx, midy, tipx, tipy), (150, 120, 70), 1)):
                    gdraw.line(s, col, (x1, y1), (x2, y2), w)
                gdraw.circle(s, (90, 150, 60), (tipx, tipy), 2)
        if st.get("fear"):  # felkiáltójel és verejtékcseppek
            bob = math.sin(t * 9) * 1.5
            ex, ey = sx, sy - top - 10 + bob
            gdraw.rect(s, (40, 10, 10), (ex - 2, ey - 9, 5, 10))
            gdraw.rect(s, (255, 90, 80), (ex - 1, ey - 8, 3, 8))
            gdraw.rect(s, (40, 10, 10), (ex - 2, ey + 2, 5, 4))
            gdraw.rect(s, (255, 90, 80), (ex - 1, ey + 3, 3, 2))
            for i in range(2):
                q = (t * 2 + i * 0.5) % 1.0
                px, py = sx + (r * 0.8 if i else -r * 0.8), sy - top * (0.9 - 0.3 * q)
                gdraw.circle(s, (180, 220, 255), (px, py), 2)
        if st.get("blind"):  # sötét fátyol a fej körül
            for i in range(5):
                a = t * 1.5 + i * 2 * math.pi / 5 + ph
                px, py = sx + math.cos(a) * r * 0.75, sy - top * 0.85 + math.sin(a) * 3
                gdraw.circle(s, (40, 40, 46), (px, py), 4)
                gdraw.circle(s, (110, 110, 118), (px - 1, py - 1), 2)
        if st.get("sunder"):  # repedt pajzs-jel és lehulló páncéldarabkák
            px, py = sx - r * 0.95, sy - top * 0.62
            pts = [(px - 5, py - 6), (px + 5, py - 6), (px + 5, py + 1), (px, py + 6), (px - 5, py + 1)]
            gdraw.polygon(s, (30, 14, 8), [(x + 1, y + 1) for x, y in pts])
            gdraw.polygon(s, (190, 96, 60), pts)
            gdraw.line(s, (40, 18, 10), (px - 1, py - 6), (px + 1, py - 1), 2)
            gdraw.line(s, (40, 18, 10), (px + 1, py - 1), (px - 2, py + 4), 2)
            for i in range(2):
                q = (t * 1.1 + i * 0.5 + ph) % 1.0
                fx_, fy_ = sx + (i * 2 - 1) * r * 0.5, sy - top * (0.6 - 0.55 * q)
                gdraw.rect(s, (30, 14, 8), (fx_, fy_, 4, 3))
                gdraw.rect(s, (170, 150, 140), (fx_, fy_, 3, 2))
        if st.get("silence"):  # áthúzott beszéd-buborék a fej mellett
            bx, by = sx + r * 0.9, sy - top - 6
            gdraw.ellipse(s, (30, 20, 44), (bx - 8, by - 6, 17, 13))
            gdraw.ellipse(s, (225, 210, 255), (bx - 7, by - 5, 15, 11))
            gdraw.polygon(s, (225, 210, 255), [(bx - 5, by + 3), (bx - 8, by + 8), (bx - 1, by + 4)])
            for k in (-3, 0, 3):
                gdraw.circle(s, (90, 70, 120), (bx + k, by + 1), 1)
            gdraw.line(s, (40, 10, 10), (bx - 8, by + 6), (bx + 8, by - 6), 3)
            gdraw.line(s, (240, 70, 70), (bx - 7, by + 5), (bx + 7, by - 5), 2)
        if st.get("mark"):  # piros, lassan forgó célkereszt a fej fölött
            mx, my = sx, sy - top - 12
            a0 = t * 1.5
            rr = 7
            gdraw.circle(s, (40, 6, 6), (mx, my), rr + 1, 3)
            gdraw.circle(s, (255, 70, 60), (mx, my), rr, 2)
            for i in range(4):
                a = a0 + i * math.pi / 2
                x1, y1 = mx + math.cos(a) * (rr - 3), my + math.sin(a) * (rr - 3)
                x2, y2 = mx + math.cos(a) * (rr + 4), my + math.sin(a) * (rr + 4)
                gdraw.line(s, (40, 6, 6), (x1 + 1, y1 + 1), (x2 + 1, y2 + 1), 2)
                gdraw.line(s, (255, 90, 80), (x1, y1), (x2, y2), 2)
            gdraw.circle(s, (255, 200, 190), (mx, my), 1)
        if st.get("sleep"):  # Z Z Z felfelé, egyre nagyobbak és halványabbak
            for i in range(3):
                q = (t * 0.35 + i / 3 + ph) % 1.0
                img = self._zzz(int(9 + 7 * q))
                img = img.copy()
                img.set_alpha(int(255 * (1 - q * 0.8)))
                s.blit(img, (sx + r * 0.4 + q * 14, sy - top - 2 - q * 22))
        if st.get("stun"):  # forgó csillagok a fej fölött
            for i in range(3):
                a = t * 4 + i * 2 * math.pi / 3
                px, py = sx + math.cos(a) * r * 0.9, sy - top - 3 + math.sin(a) * r * 0.25
                k = 3.4
                pts = []
                for j in range(10):
                    an = j * math.pi / 5 - math.pi / 2
                    rad = k if j % 2 == 0 else k * 0.45
                    pts.append((px + math.cos(an) * rad, py + math.sin(an) * rad))
                gdraw.polygon(s, (40, 30, 6), [(x + 1, y + 1) for x, y in pts])
                gdraw.polygon(s, (255, 236, 120), pts)
        if st.get("buff"):  # arany szikrák felszállnak
            for i in range(3):
                q = (t * 0.6 + i / 3 + ph) % 1.0
                px = sx + (i - 1) * r * 0.8 + math.sin(t * 2 + i) * 2
                py = sy - top * (0.1 + 0.85 * q)
                k = 2.6 * (1 - abs(q - 0.5))
                pts = [(px, py - k * 1.8), (px + k * 0.6, py), (px, py + k * 1.8), (px - k * 0.6, py)]
                gdraw.polygon(s, (60, 40, 10), [(x + 1, y + 1) for x, y in pts])
                gdraw.polygon(s, (255, 236, 150), pts)

    def draw_ground_marks(self, u, sx, sy):
        """Állapotjelek a figura alatt, a talajon (nem a figura fölött): ének / kapcsolt képesség, erősítés,
        lassítás, rejtőzés."""
        if u.flying:
            return
        w = max(22, u.radius * 2.7)
        kinds = []
        if u.toggles:
            kinds.append(("chant", int(self.t * 8) % 48))
        if u.hidden and u.owner is self.p:
            kinds.append(("hidden", 0))
        for i, (kind, ph) in enumerate(kinds):
            img = self._mark_img(kind, w + i * 8, ph)
            self.screen.blit(img, (sx - img.get_width() / 2, sy + 2 - img.get_height() / 2))

    def draw_unit_marks(self, u, sx, sy):
        n = u.passive(self.g, "split_kills") if u.owner is self.p or self.g.is_visible(self.p, u) else None
        if n:  # micélia: elfogyasztott ellenségek az osztódásig (pöttyök)
            top = sy - u.radius * 2.3 - 6
            for k in range(n):
                cx = sx - (n - 1) * 3.5 + k * 7
                gdraw.circle(self.screen, (20, 30, 10), (cx, top), 3)
                if k < u.meals:
                    gdraw.circle(self.screen, (170, 240, 110), (cx, top), 2)
        if u.camp is not None and u.order is None:  # alvó szörny
            pass
        if u.carry_amt:  # cipelt nyersanyag: kis zsák / köteg a nyersanyag színében, sötét kontúrral
            col = tuple(self.g.data.game["resources"][u.carry_kind]["color"])
            bx, by = sx + u.radius * 0.55, sy - u.radius * 1.25
            gdraw.ellipse(self.screen, (20, 16, 12), (bx - 5, by - 3, 10, 9))
            gdraw.ellipse(self.screen, col, (bx - 4, by - 2, 8, 7))
            gdraw.ellipse(self.screen, shade(col, 1.35), (bx - 2, by - 1, 3, 2))
            gdraw.line(self.screen, (20, 16, 12), (bx - 2, by - 3), (bx + 2, by - 3), 2)

    def draw_network(self, cx, cy):
        """Micélia spórafonalak: szálas, lüktető kötegek az összekötött épületek között."""
        g, p = self.g, self.p
        s = self.screen
        for q in g.players:
            if not q.links:
                continue
            acc = tuple(q.faction.get("accent", (90, 230, 200)))
            for a, b in q.links:
                if q is not p and not (a.id in self.seen and b.id in self.seen):
                    continue
                x0, y0, x1, y1 = a.x - cx, a.y - cy, b.x - cx, b.y - cy
                if max(x0, x1) < -20 or min(x0, x1) > self.vw + 20 or max(y0, y1) < -20 or min(y0, y1) > self.vh + 20:
                    continue
                d = math.hypot(x1 - x0, y1 - y0) or 1
                nx, ny = -(y1 - y0) / d, (x1 - x0) / d
                for k, (col, off) in enumerate((((70, 55, 45), 0), ((190, 175, 150), 1.5), ((150, 135, 110), -1.5))):
                    pts = []
                    n = max(4, int(d / 10))
                    for i in range(n + 1):
                        t = i / n
                        w = math.sin(t * math.pi * 3 + k * 2 + a.id) * 2.2 * math.sin(t * math.pi)
                        pts.append((x0 + (x1 - x0) * t + nx * (w + off), y0 + (y1 - y0) * t + ny * (w + off)))
                    gdraw.lines(s, col, False, pts, 2 if k == 0 else 1)
                ph = (self.t * 0.6 + a.id * 0.13) % 1.0
                gdraw.circle(s, acc, (x0 + (x1 - x0) * ph, y0 + (y1 - y0) * ph), 2)

    def draw_ground_items(self, cx, cy):
        """Földön heverő tárgyak: lebegő, csillogó ikon; egér fölött a neve."""
        g, p = self.g, self.p
        s = self.screen
        mx, my = self.mpos()
        for gi in g.items:
            tx, ty = int(gi.x // TILE), int(gi.y // TILE)
            if not g.map.inb(tx, ty) or not p.explored[ty * g.map.w + tx]:
                continue
            sx, sy = gi.x - cx, gi.y - cy
            if not (-30 < sx < self.vw + 30 and -30 < sy < self.vh + 30):
                continue
            bob = math.sin(self.t * 3 + gi.id) * 2
            rar = gi.defn.get("rarity", "common")
            col = {"common": (230, 220, 190), "rare": (110, 170, 255), "artifact": (255, 190, 70)}.get(rar)
            gdraw.ellipse(s, (0, 0, 0), (sx - 9, sy + 4, 18, 6))
            glow = pygame.Surface((36, 36), pygame.SRCALPHA)
            a = int(60 + 40 * math.sin(self.t * 4 + gi.id))
            gdraw.circle(glow, col + (a,), (18, 18), 15)
            s.blit(glow, (sx - 18, sy - 22 + bob))
            ic = self.sprites.item_icon(gi.item, 22)
            if ic is not None:
                s.blit(ic, (sx - 11, sy - 15 + bob))
            else:
                gdraw.rect(s, col, (sx - 6, sy - 10 + bob, 12, 12))
            if math.hypot(mx - sx, my - sy) < 16:
                img = self.fonts["small"].render(gi.defn["name"], True, col)
                bg = pygame.Surface((img.get_width() + 8, img.get_height() + 2), pygame.SRCALPHA)
                bg.fill((0, 0, 0, 170))
                s.blit(bg, (sx - img.get_width() / 2 - 4, sy - 38))
                s.blit(img, (sx - img.get_width() / 2, sy - 37))

    def draw_portraits(self):
        """Négyzetes hősportrék a jobb felső sarokban, szinttel és állapotsávokkal."""
        s = self.screen
        F = self.fonts
        self.portraits = []
        hs = self.my_heroes()
        k = self.L["s"]
        size = max(42, round(54 * k))
        pw, ph = size + 4, size + 13
        x = self.vw - pw - max(8, round(12 * k))
        y = self.L["topbar"].bottom + max(20, round(24 * k))
        for h in hs[:4]:
            r = pygame.Rect(x, y, pw, ph)
            sel = h in self.sel
            self.bevel(r, fill=(30, 26, 18) if not sel else (70, 58, 24), light=(230, 190, 90), dark=(60, 44, 20))
            portrait = self.def_icon(h.defn, "unit", size, h.owner.color)
            s.blit(portrait, (r.x + 2, r.y + 2))
            gdraw.rect(s, (10, 10, 10), (r.x + 2, r.bottom - 9, r.w - 4, 7))
            gdraw.rect(s, (80, 210, 80), (r.x + 2, r.bottom - 9, (r.w - 4) * h.hp / h.max_hp, 3))
            if h.max_energy:
                gdraw.rect(s, (150, 100, 255), (r.x + 2, r.bottom - 5, (r.w - 4) * h.energy / h.max_energy, 3))
            badge = pygame.Rect(r.right - 16, r.y - 4, 20, 16)
            gdraw.rect(s, (20, 16, 8), badge)
            gdraw.rect(s, (230, 190, 90), badge, 1)
            img = F["tiny"].render(str(h.level), True, (255, 230, 150))
            s.blit(img, (badge.centerx - img.get_width() / 2, badge.y + 1))
            if h.skill_points > 0:
                pulse = 0.5 + 0.5 * math.sin(self.t * 5)
                pr = pygame.Rect(r.x - 3, r.y - 4, 16, 16)
                gdraw.rect(s, (int(120 + 120 * pulse), 200, 80), pr)
                s.blit(F["tiny"].render("+", True, (10, 20, 0)), (pr.x + 4, pr.y))
            self.portraits.append((r, h))
            y += ph + 8

    def draw_corpses(self, cx, cy):
        g, p = self.g, self.p
        for defn, owner, x, y, ang, t0 in g.corpses:
            if not (-80 < x - cx < self.vw + 80 and -80 < y - cy < self.vh + 80):
                continue
            tx, ty = int(x // TILE), int(y // TILE)
            if not g.map.inb(tx, ty) or not p.visible[ty * g.map.w + tx]:
                continue
            sheet = self.sprites.unit(defn["id"])
            if sheet is None or "death" not in sheet.anims:
                continue
            fr = sheet.anim_frame(owner.color, "death", g.time - t0, dir_index(ang), True)
            self.screen.blit(fr, (x - cx - sheet.anchor[0], y - cy - sheet.anchor[1]))

    def rotting(self, r):
        """A fa éppen rothad-e (a Croftian mélysége emészti) – az utolsó 2 mp-ben fogyott a rothadástól."""
        return r.kind == "wood" and getattr(r, "rot_t", -99) > self.g.time - 2.0

    def _rot_sheet(self, base, level):
        """A fa rothadó változata: 3 fokozatban szürkül és barnul (level 1..3)."""
        cache = self.__dict__.setdefault("_rot_tint", {})
        key = (id(base), level)
        sh = cache.get(key)
        if sh is None:
            import copy
            import numpy as np
            sh = copy.copy(base)
            img = base.image.copy()
            a = pygame.surfarray.pixels3d(img)
            f = a.astype(np.float32)
            lum = f.mean(axis=2, keepdims=True)
            grey_brown = np.concatenate([lum * 1.05 + 46, lum * 0.92 + 30, lum * 0.78 + 22], axis=2)  # fakó, szürkésbarna
            k = (0.5, 0.7, 0.88)[level - 1]
            f = f * (1 - k) + grey_brown * k
            a[...] = np.clip(f, 0, 255).astype(np.uint8)
            del a
            sh.image = img
            sh._frames, sh._colored, sh._calm = {}, {}, {}
            cache[key] = sh
        return sh

    def draw_rot_bubbles(self, r, cx, cy):
        """Apró, lágyan felszálló buborékok a rothadó fa tövénél (zöldes-lilás)."""
        s = self.screen
        bx, by = r.x - cx, r.y - cy + TILE * 0.25
        seed = (r.tx * 73 + r.ty * 31) % 97
        for i in range(6):
            ph = (self.t * 0.45 + i / 6 + seed * 0.013) % 1.0
            x = bx + math.sin(i * 2.3 + seed) * 11 + math.sin(self.t * 2 + i) * 1.5
            y = by - ph * 22
            rad = 1.6 + 2.0 * (1 - abs(ph - 0.5) * 2)
            a = int(170 * (1 - ph))
            if a <= 10:
                continue
            col = (150, 210, 120) if i % 2 else (170, 120, 200)
            sp = pygame.Surface((10, 10), pygame.SRCALPHA)
            pygame.draw.circle(sp, (*col, a), (5, 5), max(1, int(round(rad))))
            pygame.draw.circle(sp, (235, 245, 220, a // 2), (4, 4), 1)
            s.blit(sp, (x - 5, y - 5))

    def _wood_sheet(self, r):
        """Fák a tájhoz: saját képlap (resources/wood_<táj>), vagy a meglévő fák átszínezve
        (tél: deres; ősz: arany / rozsdás / néhol örökzöld; Hasadékvidék: lilás)."""
        th = self.theme_name()
        own = self.sprites.themed("resources", "wood", th)
        if own is not None:
            return own
        base = self.sprites.resource("wood")
        if base is None or th not in ("winter", "autumn", "rift"):
            return base
        flav = (r.variant * 5 + r.tx + r.ty) % 3 if th == "autumn" else 0
        if th == "autumn" and flav == 2:
            return base
        cache = self.__dict__.setdefault("_wood_tint", {})
        key = (id(base), th, flav)
        sh = cache.get(key)
        if sh is None:
            import copy
            import numpy as np
            sh = copy.copy(base)
            img = base.image.copy()
            a = pygame.surfarray.pixels3d(img)
            f = a.astype(np.float32)
            r_, g_, b_ = f[..., 0], f[..., 1], f[..., 2]
            lum = (r_ + g_ + b_) / 3
            gr = np.clip((g_ - np.maximum(r_, b_)) / 25.0, 0, 1)[..., None]
            if th == "winter":
                tgt = np.stack([lum * 0.5 + 118, lum * 0.5 + 128, lum * 0.45 + 145], -1)
                f = f * (1 - gr * 0.45) + tgt * gr * 0.45
            elif th == "autumn":
                tgt = (np.stack([g_ * 1.25 + 30, g_ * 0.9 + 10, b_ * 0.4], -1) if flav == 0
                       else np.stack([g_ * 1.2 + 40, g_ * 0.55, b_ * 0.35], -1))
                f = f * (1 - gr * 0.9) + tgt * gr * 0.9
            else:
                tgt = np.stack([g_ * 0.75 + 58, g_ * 0.42 + 12, g_ * 0.95 + 70], -1)  # lilás-kékes lomb
                f = f * (1 - gr * 0.62) + tgt * gr * 0.62
            a[...] = np.clip(f, 0, 255).astype(np.uint8)
            del a
            sh.image = img
            sh._frames, sh._colored, sh._calm = {}, {}, {}
            cache[key] = sh
        return sh

    def draw_res(self, r, cx, cy, rcols):
        mine = getattr(r, "mine", None)
        if mine is not None and mine.alive:  # rajta áll a Napfúró / Fertőzött tárna
            return
        sheet = self._wood_sheet(r) if r.kind == "wood" else self.sprites.resource(r.kind)
        sx, sy = r.rect[0] - cx, r.rect[1] - cy
        if sheet is None:
            draw_resource(self.screen, r, sx, sy, self.t, rcols[r.kind])
            return
        n = sheet.meta.get("frames", 1)
        if r.kind == "food":
            fill = r.amount / r.max_amount
            i = 0 if fill > 0.6 else 1 if fill > 0.2 else 2
        elif sheet.meta.get("fps"):
            i = int(self.t * sheet.meta["fps"] + r.variant)
        else:
            i = r.variant
        rot = r.kind == "wood" and self.rotting(r)
        if rot or (r.kind == "wood" and getattr(r, "rot", False)):  # rothadó / rothadt fa: fakóbb, barnább
            used = 1 - r.amount / max(1, r.max_amount)
            sheet = self._rot_sheet(sheet, 1 if used < 0.33 else 2 if used < 0.66 else 3)
        fr = sheet.frame(None, i % n)
        gdraw.blit_trim(self.screen, fr, sx - sheet.anchor[0], sy - sheet.anchor[1])  # levágott, gyorsított kép
        if self.settings.get("shadows", True):
            SHD.blit_inner(self.screen, fr, sx - sheet.anchor[0], sy - sheet.anchor[1])
        if hasattr(self, "_shadow_next"):
            mode = "tree" if r.kind == "wood" else "col"
            self._queue_shadow(r, fr, sx - sheet.anchor[0] + cx, sy - sheet.anchor[1] + cy, mode=mode)
        if rot:
            self.draw_rot_bubbles(r, cx, cy)

    def draw_points(self):
        """Hasadékkövek: lebegő meteoritszilánk, a birtokos színű gyűrűvel és a foglalás állásával."""
        g = self.g
        pts = getattr(g, "points", None)
        if not pts:
            return
        s = self.screen
        cx, cy = int(self.cam_x), int(self.cam_y)
        ms = self.sprites.resource("meteor")
        bob = math.sin(self.t * 2.0)
        for pt in pts:
            sx, sy = pt.x - cx, pt.y - cy
            if sx < -80 or sy < -80 or sx > self.vw + 80 or sy > self.vh + 80:
                continue
            if not self.p.explored[pt.ty * g.map.w + pt.tx]:
                continue
            col = pt.owner.color if pt.owner is not None else (190, 150, 255)
            ring = pygame.Rect(0, 0, TILE * 1.9, TILE * 0.95)
            ring.center = (sx, sy + TILE * 0.2)
            gdraw.ellipse(s, (25, 15, 35), ring.inflate(4, 4), 2)
            gdraw.ellipse(s, col, ring, 2)
            if pt.cap_owner is not None and pt.progress > 0:  # foglalás: ív a gyűrű körül
                gdraw.arc(s, pt.cap_owner.color, ring.inflate(10, 6), -1.5708, -1.5708 + 6.283 * pt.progress, 3)
            if pt.guard is not None and pt.guard.alive:
                continue  # az őrállás épülete rajzolódik a kövön
            if ms is not None:
                fr = ms.frame(None, 0)
                w = int(TILE * 1.4)
                h = max(1, int(fr.get_height() * w / fr.get_width()))
                s.blit(pygame.transform.smoothscale(fr, (w, h)), (sx - w / 2, ring.centery - h * 0.45 + bob * 2))
            else:
                gdraw.polygon(s, (170, 90, 255), [(sx, sy - 22 + bob * 3), (sx + 10, sy), (sx, sy + 8), (sx - 10, sy)])

    def _wall_frame(self, b, sheet, fr, ox, oy):
        """Falszakasz tájolása a szomszédos saját falakból: ha csak északon / délen van fal, hosszanti kép
        (<id>_v képlap, ha van; különben a keresztben álló kép középső szeletéből mélységgel összerakva),
        ha mindkét irányban, a kettő együtt. Csak rajzolás: a játékállapotot nem érinti."""
        walls = self._walls_now()
        tx, ty = int(b.rect[0] // TILE), int(b.rect[1] // TILE)

        def has(dx, dy):
            o = walls.get((tx + dx, ty + dy))
            return o is not None and o.owner is b.owner
        ns, ew = has(0, -1) or has(0, 1), has(-1, 0) or has(1, 0)
        if not ns:
            return fr, ox, oy
        scale = self.screen.k if isinstance(getattr(self, "screen", None), gdraw.WorldCanvas) and gdraw.is_hd(fr) else 1
        vs = self.sprites.building(b.defn["id"] + "_v")
        if vs is not None:
            vf = vs.frame(b.owner.color, 0)
            vox, voy = vs.anchor
        else:
            cache = self.__dict__.setdefault("_wall_v", {})
            key = id(fr)
            got = cache.get(key)
            if got is None:
                w, h = fr.get_size()
                sw = max(4, int(w * 0.36))
                strip = fr.subsurface((w // 2 - sw // 2, 0, sw, h)).copy()
                dy = max(2, round(TILE * 0.32 * scale))
                img = pygame.Surface((w, h + dy), pygame.SRCALPHA)
                back = strip.copy()
                back.fill((185, 185, 185, 255), special_flags=pygame.BLEND_RGBA_MULT)  # a hátsó sor sötétebb
                img.blit(back, (w // 2 - sw // 2, 0))
                img.blit(strip, (w // 2 - sw // 2, dy))
                if gdraw.is_hd(fr):
                    gdraw.mark_hd(img)
                got = (img, dy)
                if len(cache) > 64:
                    cache.clear()
                cache[key] = got
            vf, dy = got
            vox, voy = ox, oy + dy / scale
        if not ew:
            return vf, vox, voy
        left, top = max(ox, vox), max(oy, voy)
        fx, fy = round((left - ox) * scale), round((top - oy) * scale)
        vx, vy = round((left - vox) * scale), round((top - voy) * scale)
        w = max(fx + fr.get_width(), vx + vf.get_width())
        h = max(fy + fr.get_height(), vy + vf.get_height())
        img = pygame.Surface((w, h), pygame.SRCALPHA)
        img.blit(vf, (vx, vy))
        img.blit(fr, (fx, fy))
        if gdraw.is_hd(fr):
            gdraw.mark_hd(img)
        return img, left, top

    def _walls_now(self):
        """A falszakaszok helye (képkockánként egyszer gyűjtve)."""
        t = (self.g.tick, len(self.g.buildings))
        c = getattr(self, "_walls_cache", None)
        if c is None or c[0] != t:
            c = (t, {(int(b.rect[0] // TILE), int(b.rect[1] // TILE)): b for b in self.g.buildings
                     if b.alive and b.defn.get("wall")})
            self._walls_cache = c
        return c[1]

    @staticmethod
    def building_frame(sheet, color, progress=None):
        animation = sheet.anims.get("build") if progress is not None else None
        if animation:
            index = min(animation["frames"] - 1, int(progress * animation["frames"]))
            return sheet.frame(color, animation["start"] + index)
        return sheet.frame(color, sheet.anims.get("idle", {}).get("start", 0))

    def draw_building(self, b, bx, by):
        s = self.screen
        sheet = self.sprites.building(b.defn["id"])
        if sheet is not None:
            ox, oy = sheet.anchor
            progress = None if b.complete else b.progress / b.build_time
            fr = self.building_frame(sheet, b.owner.color, progress)
            if b.defn.get("wall"):  # fal: a szomszédok szerint keresztben vagy hosszában (sarkon mindkettő)
                fr, ox, oy = self._wall_frame(b, sheet, fr, ox, oy)
            pos = (bx - ox, by - oy)
            s.blit(fr, pos)
            self.hover_sprite(b, fr, pos, bx + b.rect[2] / 2, b.rect[2] * 0.8)
            if self.settings.get("shadows", True):
                SHD.blit_inner(s, fr, bx - ox, by - oy)
            if hasattr(self, "_shadow_next"):  # az épület árnyéka oszloponként a saját tövétől indul
                self._queue_shadow(b, fr, bx - ox + int(self.cam_x), by - oy + int(self.cam_y), mode="col")
            if b.flash > 0:  # találati felvillanás
                s.blit(fr, (bx - ox, by - oy), special_flags=pygame.BLEND_RGB_ADD)
            if b.complete and not b.powered:
                r = pygame.Rect(bx, by, b.rect[2], b.rect[3])
                gdraw.line(s, (255, 60, 40), r.topleft, r.bottomright, 3)
                gdraw.line(s, (255, 60, 40), r.topright, r.bottomleft, 3)
            self.draw_building_marks(b, bx, by)
            return
        if b.defn.get("rift_source"):  # Égkő: amíg nincs saját képe, a meteorit képe kicsiben
            ms = self.sprites.resource("meteor")
            if ms is not None:
                fr = ms.frame(None, 0)
                w = int(b.rect[2] * 1.5)
                h = max(1, int(fr.get_height() * w / fr.get_width()))
                fr = pygame.transform.smoothscale(fr, (w, h))
                center = bx + b.rect[2] / 2
                pos = (center - w / 2, by + b.rect[3] - h * 0.8)
                s.blit(fr, pos)
                self.hover_sprite(b, fr, pos, center, b.rect[2] * 0.8)
                self.draw_building_marks(b, bx, by)
                return
        f = b.owner.faction
        base = tuple(f["color"]) if b.flash <= 0 else (255, 255, 255)
        rect = (bx, by, b.rect[2], b.rect[3])
        self.hover_shape(b, pygame.Rect(rect), bx + b.rect[2] / 2, b.rect[2] * 0.8)
        gdraw.rect(s, (25, 30, 20), (bx + 4, by + 5, b.rect[2], b.rect[3]))
        if b.complete:
            draw_building(s, f.get("style"), b.defn.get("shape"), rect, base, b.owner.color, tuple(f["accent"]),
                          self.t, 1.0, b.powered)
        else:
            frac = max(0.25, b.progress / b.build_time)
            gdraw.rect(s, (60, 55, 45), rect)
            clip = s.get_clip()
            h = b.rect[3] * frac
            s.set_clip(pygame.Rect(bx, by + b.rect[3] - h, b.rect[2], h).clip(clip))
            draw_building(s, f.get("style"), b.defn.get("shape"), rect, base, b.owner.color, tuple(f["accent"]), self.t)
            s.set_clip(clip)
            for i in range(0, int(b.rect[2]), 8):
                gdraw.line(s, (200, 190, 120), (bx + i, by), (bx + i + 4, by), 1)
                gdraw.line(s, (200, 190, 120), (bx + i, by + b.rect[3] - 1), (bx + i + 4, by + b.rect[3] - 1), 1)
        self.draw_building_marks(b, bx, by)

    def draw_building_marks(self, b, bx, by):
        s = self.screen
        if b.defn.get("larva") and b.complete and b.larva and self.g.is_visible(self.p, b):
            scale = s.k if isinstance(s, gdraw.WorldCanvas) else 1
            size = (max(1, round(17 * scale)), max(1, round(10 * scale)))
            larva = self.sprites.art("ui/cro_larva", size)
            if larva is not None:
                left = self.icons.get(("larva_left", size))
                if left is None:
                    left = self.icons["larva_left", size] = pygame.transform.flip(larva, True, False)
                if isinstance(s, gdraw.WorldCanvas):
                    gdraw.mark_hd(larva)
                    gdraw.mark_hd(left)
                spacing = b.rect[2] / b.larva
                for i in range(b.larva):
                    phase = self.t * 4.0 + i * 1.8
                    lx = bx + spacing * (i + 0.5) + math.sin(phase) * min(3, spacing * 0.15)
                    ly = by + b.rect[3] + 8 + math.sin(phase * 2) * 1.5
                    frame = larva if math.cos(phase) >= 0 else left
                    x, y = lx - 8.5, ly - 5
                    if self.settings.get("shadows", True) and hasattr(self, "_shadow_next"):
                        self._queue_shadow(b, frame, x + int(self.cam_x), y + int(self.cam_y))
                    s.blit(frame, (x, y))
        if b.queue and b.owner is self.p:
            it = b.queue[0]
            frac = it["progress"] / it["time"]
            gdraw.rect(s, (20, 20, 20), (bx + 4, by + b.rect[3] - 6, b.rect[2] - 8, 4))
            gdraw.rect(s, (240, 220, 90), (bx + 4, by + b.rect[3] - 6, (b.rect[2] - 8) * frac, 4))

    def hover_sprite(self, entity, frame, pos, center, width):
        """A rajzolási sorrendben legfelül levő látható képpont választ."""
        mouse = getattr(self, "_world_hover", None)
        if mouse is None:
            return
        canvas_scale = self.screen.k if isinstance(self.screen, gdraw.WorldCanvas) else 1
        native_scale = canvas_scale if gdraw.is_hd(frame) else 1
        if isinstance(self.screen, gdraw.WorldCanvas):
            x = math.floor((mouse[0] * canvas_scale - round(pos[0] * canvas_scale))
                           * native_scale / canvas_scale)
            y = math.floor((mouse[1] * canvas_scale - round(pos[1] * canvas_scale))
                           * native_scale / canvas_scale)
        else:
            x, y = math.floor(mouse[0] - int(pos[0])), math.floor(mouse[1] - int(pos[1]))
        if frame.get_rect().collidepoint(x, y) and frame.get_at((x, y)).a >= 32:
            top = frame.get_bounding_rect(min_alpha=32).top / native_scale
            self._hover_bar = (entity, center, pos[1] + top - 6, width)

    def hover_shape(self, entity, rect, center, width):
        mouse = getattr(self, "_world_hover", None)
        if mouse is not None and rect.collidepoint(mouse):
            self._hover_bar = (entity, center, rect.top - 6, width)

    def draw_hover_bar(self, cx, cy):
        mouse = self._world_hover
        if mouse is None:
            return
        if self._hover_bar is not None:
            entity, x, y, width = self._hover_bar
            if self.g.is_visible(self.p, entity):
                self.draw_bars(entity, x, y, width)
            return
        tx, ty = int((mouse[0] + cx) // TILE), int((mouse[1] + cy) // TILE)
        if self.g.map.inb(tx, ty):
            spike = self.g.spike_targets.get(ty * self.g.map.w + tx)
            if spike is not None and self.g.is_visible(self.p, spike):
                self.draw_bars(spike, spike.x - cx, spike.rect[1] - cy - 4, TILE * 0.8)

    def draw_bars(self, e, x, y, w):
        s = self.screen
        w = max(20, w)
        x0 = x - w / 2
        if e.max_shield:
            gdraw.rect(s, (10, 10, 30), (x0, y - 4, w, 3))
            gdraw.rect(s, (90, 170, 255), (x0, y - 4, w * max(0, e.shield) / e.max_shield, 3))
        frac = max(0, e.hp) / e.max_hp
        col = (80, 220, 80) if frac > 0.6 else (230, 200, 60) if frac > 0.3 else (230, 60, 50)
        gdraw.rect(s, (10, 10, 10), (x0, y, w, 4))
        gdraw.rect(s, col, (x0, y, w * frac, 4))
        show_energy = e.max_energy and e.is_unit and e.owner is self.p
        if show_energy:
            gdraw.rect(s, (10, 10, 10), (x0, y + 5, w, 3))
            gdraw.rect(s, (200, 110, 255), (x0, y + 5, w * e.energy / e.max_energy, 3))
        seconds = getattr(e, "summon_seconds", None)
        if seconds is not None:
            col = (255, 150, 100) if seconds <= 5 else (255, 220, 130)
            img = self.base_fonts["tiny"].render(f"{seconds} mp", True, col)
            tw, th = img.get_size()
            tx, ty = x - tw / 2, y + (9 if show_energy else 5)
            gdraw.rect(s, (14, 12, 10), (tx - 2, ty, tw + 4, th))
            s.blit(img, (tx, ty))

    def draw_projectile(self, pr, cx, cy):
        s = self.screen
        x, y = pr.x - cx, pr.y - cy
        dx, dy = pr.tx - pr.x, pr.ty - pr.y
        d = math.hypot(dx, dy) or 1
        k = pr.kind
        if pr.atk.get("spell") or (pr.atk.get("element") and k == "magic"):
            el = pr.atk.get("element")
            vs = self.sprites.effect(f"vfx_{el or 'arcane'}_bolt")
            if vs is not None:
                fr = vs.anim_frame(None, "play", self.t, row=0)
                s.blit(fr, (x - vs.anchor[0], y - vs.anchor[1]))
            else:
                col = pr.atk.get("color") or pr.owner.faction.get("accent", (220, 200, 255))
                VFX.draw_bolt(s, x, y, dx, dy, el, tuple(col), self.t, id(pr) % 997)
            return
        sheet = self.sprites.projectile(k)
        if sheet is not None:
            hgt = 0
            if k == "boulder":
                tot = math.hypot(pr.tx - pr.sx, pr.ty - pr.sy) or 1
                hgt = math.sin(max(0, min(1, 1 - d / tot)) * math.pi) * 30
                gdraw.ellipse(s, (25, 30, 20), (x - 4, y - 2, 8, 4))
            fr = sheet.anim_frame(None, "fly", self.t, dir_index(math.atan2(dy, dx)))
            s.blit(fr, (x - sheet.anchor[0], y - hgt - sheet.anchor[1]))
            return
        if k in ("arrow", "spear"):
            ln = 10 if k == "arrow" else 14
            gdraw.line(s, (230, 220, 190), (x, y), (x - dx / d * ln, y - dy / d * ln), 1 if k == "arrow" else 2)
        elif k == "boulder":
            tot = math.hypot(pr.tx - pr.sx, pr.ty - pr.sy) or 1
            frac = 1 - d / tot
            hgt = math.sin(max(0, min(1, frac)) * math.pi) * 30
            gdraw.circle(s, (30, 30, 30), (x, y), 3)
            gdraw.circle(s, (150, 140, 130), (x, y - hgt), 5)
        elif k == "boomerang":  # forgó, V alakú bumeráng
            a = self.t * 22
            for sgn in (1, -1):
                ex, ey = x + math.cos(a + sgn * 1.1) * 6, y - 6 + math.sin(a + sgn * 1.1) * 3
                gdraw.line(s, (96, 60, 34), (x, y - 6), (ex, ey), 3)
                gdraw.line(s, (214, 150, 84), (x, y - 6), (ex, ey), 1)
        elif k in ("fire", "frost"):  # Groomi-köpés: csóva farokkal
            c1, c2 = ((255, 120, 30), (255, 230, 120)) if k == "fire" else ((120, 200, 255), (235, 250, 255))
            for j in range(4):
                gdraw.circle(s, c1, (x - dx / d * j * 3, y - 6 - dy / d * j * 3), max(1, 5 - j))
            gdraw.circle(s, c2, (x, y - 6), 2)
        elif k == "acid":
            gdraw.circle(s, (120, 230, 60), (x, y), 4)
            gdraw.circle(s, (200, 255, 150), (x - 1, y - 1), 2)
        else:
            acc = tuple(pr.owner.faction["accent"])
            gdraw.circle(s, acc, (x, y), 5)
            gdraw.circle(s, (255, 255, 255), (x, y), 2)

    def draw_area(self, e, x, y):
        el = e.ab.get("element")
        life = 1 - e.t / max(0.01, e.dur)
        sheet = self.sprites.effect(e.ab["sprite"]) if e.ab.get("sprite") else None
        vsheet = None if sheet is not None else self.sprites.effect(f"vfx_{el or 'arcane'}_area")
        if vsheet is None:  # kódból rajzolt elem-terület (a festett varázslat-kép alá is: talajfény)
            VFX.draw_area(self.screen, x, y, e.radius, life, self.t, el, e.color, int(e.x * 7 + e.y * 13) % 997,
                          fade=0.6 if e.ab.get("sprite") else 1.0)
        if vsheet is not None:  # festett elem-terület: a terület méretére nagyítva (lapított)
            fr = vsheet.anim_frame(None, "play", e.dur - e.t)
            w = int(e.radius * 2.2)
            h = int(fr.get_height() * w / max(1, fr.get_width()))
            ck = (id(fr), w)
            cache = self.__dict__.setdefault("_vfx_area_cache", {})
            img = cache.get(ck)
            if img is None:
                if len(cache) > 120:
                    cache.clear()
                img = cache[ck] = pygame.transform.smoothscale(fr, (w, h))
            self.screen.blit(img, (x - w / 2, y - h / 2))
            return
        if sheet is not None:
            # nagyítás helyett több, eredeti méretű (éles pixeles) példány a terület mentén
            n = max(1, int((e.radius / 26) ** 2 * 1.2))
            pts = [(0.0, 0.0)] + [(math.cos(k / max(1, n - 1) * 6.283 + 0.4) * e.radius * 0.55,
                                   math.sin(k / max(1, n - 1) * 6.283 + 0.4) * e.radius * 0.45) for k in range(n - 1)]
            for k, (ox, oy) in enumerate(pts):
                fr = sheet.anim_frame(None, "play", e.dur - e.t + k * 0.17)
                self.screen.blit(fr, (x + ox - sheet.anchor[0], y + oy - sheet.anchor[1]))
            return
        return
        r = int(e.radius)
        surf = pygame.Surface((r * 2 + 4, r * 2 + 4), pygame.SRCALPHA)
        a = int(70 + 40 * math.sin(self.t * 8))
        gdraw.circle(surf, e.color + (a,), (r + 2, r + 2), r)
        gdraw.circle(surf, e.color + (200,), (r + 2, r + 2), r, 2)
        for i in range(6):
            ang = self.t * 3 + i
            rr = (i * 37 + int(self.t * 60)) % r
            gdraw.circle(surf, (255, 255, 255, 160), (r + 2 + math.cos(ang) * rr, r + 2 + math.sin(ang) * rr), 2)
        self.screen.blit(surf, (x - r - 2, y - r - 2))

    def draw_fx(self, cx, cy):
        s = self.screen
        for fxe in self.g.fx:
            kind, x, y, dur, rem, col = fxe[:6]
            if not self.g.map.inb(int(x // TILE), int(y // TILE)):
                continue
            if not self.p.visible[int(y // TILE) * self.g.map.w + int(x // TILE)]:
                continue
            f = 1 - rem / dur
            sx, sy = x - cx, y - cy
            if kind in ("cast", "enova", "impact"):
                el = fxe[7] if len(fxe) > 7 else None
                size = fxe[6] if len(fxe) > 6 else None
                vs = self.sprites.effect(f"vfx_{el or 'arcane'}_{'nova' if kind == 'enova' else kind}")
                if vs is not None:
                    fr = vs.anim_frame(None, "play", dur - rem, clamp=True)
                    ax, ay = vs.anchor
                    if size and kind in ("enova", "impact"):
                        factor = 2 * size * vs.scale / max(1, fr.get_width())
                        size_px = tuple(max(1, round(n * factor)) for n in fr.get_size())
                        fr = self.sprites.scaled_effect(kind, fr, size_px)
                        ax, ay = ax * factor, ay * factor
                    s.blit(fr, (sx - ax, sy - ay))
                    continue
                seed = int(x * 3 + y * 7) % 997
                if kind == "cast":
                    VFX.draw_cast(s, sx, sy, f, el, col, size, self.t, seed)
                elif kind == "enova":
                    VFX.draw_nova(s, sx, sy, f, el, col, size, self.t, seed)
                else:
                    VFX.draw_impact(s, sx, sy, f, el, col, size, self.t, seed)
                continue
            name = {"death": "explosion" if dur > 0.7 else "blood", "hit": "hit", "slash": "slash",
                    "heal": "heal", "levelup": "levelup"}.get(kind)
            sheet = self.sprites.effect(name) if name else None
            if sheet is not None:
                fr = sheet.anim_frame(None, "play", dur - rem, clamp=True)
                s.blit(fr, (sx - sheet.anchor[0], sy - sheet.anchor[1]))
                continue
            if kind == "death":
                gdraw.circle(s, (255, 200, 90), (sx, sy), 4 + f * 26, 2)
                for i in range(6):
                    a = i * 1.05
                    gdraw.circle(s, shade(col, 0.8), (sx + math.cos(a) * f * 22, sy + math.sin(a) * f * 22), 3 * (1 - f) + 1)
            elif kind == "hit":
                gdraw.circle(s, col, (sx, sy), 2 + f * 7, 1)
            elif kind == "slash":
                gdraw.arc(s, (255, 255, 255), (sx - 9, sy - 9, 18, 18), f * 3, f * 3 + 1.6, 2)
            elif kind == "heal":
                gdraw.line(s, col, (sx - 4, sy - f * 16), (sx + 4, sy - f * 16), 2)
                gdraw.line(s, col, (sx, sy - 4 - f * 16), (sx, sy + 4 - f * 16), 2)
            elif kind == "ring":
                gdraw.circle(s, col, (sx, sy), 4 + f * 14, 2)
            elif kind == "nova":
                rad = fxe[6] if len(fxe) > 6 else 60
                surf = pygame.Surface((rad * 2 + 8, rad * 2 + 8), pygame.SRCALPHA)
                rr = int(rad * (0.3 + 0.7 * f))
                gdraw.circle(surf, col + (int(120 * (1 - f)),), (rad + 4, rad + 4), rr)
                gdraw.circle(surf, col + (int(255 * (1 - f)),), (rad + 4, rad + 4), rr, 3)
                s.blit(surf, (sx - rad - 4, sy - rad - 4))
            elif kind == "levelup":
                for i in range(10):
                    a = i * 0.63 + f * 5
                    gdraw.circle(s, col, (sx + math.cos(a) * (6 + f * 10), sy - f * 30 + math.sin(a) * 4), 2)

    def _placement_tint(self, image, ok):
        cache = self.__dict__.setdefault("_placement_tints", {})
        key = (image, ok)
        if key not in cache:
            if len(cache) >= 64:
                cache.clear()
            tinted = image.copy()
            tinted.fill((95, 255, 120, 255) if ok else (255, 88, 88, 255),
                        special_flags=pygame.BLEND_RGBA_MULT)
            tinted.set_alpha(210)
            if gdraw.is_hd(image):
                gdraw.mark_hd(tinted)
            cache[key] = tinted
        return cache[key]

    def _draw_placement_grid(self, bx, by, w, h):
        color = tuple(self.p.color)
        tile = pygame.Surface((TILE, TILE), pygame.SRCALPHA)
        tile.fill((*color, 55))
        pygame.draw.rect(tile, (*color, 185), tile.get_rect(), 1)
        for y in range(h):
            for x in range(w):
                pos = bx + x * TILE, by + y * TILE
                self.screen.blit(tile, pos)

    def _draw_placement_building(self, bdef, bx, by, ok=None):
        p = self.p
        sheet = self.sprites.building(bdef["id"])
        if sheet is not None:
            fr = self.building_frame(sheet, p.color, 0 if ok is None else None)
            ox, oy = sheet.anchor
        else:
            w, h = (n * TILE for n in bdef["size"])
            fr = pygame.Surface((w, h), pygame.SRCALPHA)
            f = p.faction
            if ok is None:
                fr.fill((60, 55, 45))
                fr.set_clip((0, h - h // 4, w, h // 4))
            draw_building(fr, f.get("style"), bdef.get("shape"), (0, 0, w, h),
                          tuple(f["color"]), p.color, tuple(f["accent"]), 0)
            ox = oy = 0
        if ok is not None:
            fr = self._placement_tint(fr, ok)
        self.screen.blit(fr, (bx - ox, by - oy))

    def draw_planned_buildings(self, cx, cy):
        occupied = {(b.tx, b.ty) for b in self.g.buildings if b.alive}
        for u in self.g.units:
            if u.owner is not self.p or not u.alive:
                continue
            for order in (u.order, *u.queue):
                if not order or order["type"] not in ("build", "unpack"):
                    continue
                bdef = order["bdef"] if order["type"] == "build" else u.packed
                tx, ty = order["tx"], order["ty"]
                if bdef is None or (tx, ty) in occupied:
                    continue
                bx, by = tx * TILE - cx, ty * TILE - cy
                w, h = bdef["size"]
                if bx > self.vw + 40 or by > self.vh + 100 or bx + w * TILE < -60 or by + h * TILE < -20:
                    continue
                self._draw_placement_grid(bx, by, w, h)
                self._draw_placement_building(bdef, bx, by)
                occupied.add((tx, ty))

    def _can_preview_place(self, bdef, tx, ty):
        g, p = self.g, self.p
        if bdef.get("pond_tile"):
            return g.can_lay_pond(p, tx, ty)[0]
        if bdef.get("spike_tile"):
            return g.can_lay_spikes(p, tx, ty, p.rules.get("spike_size", 1))[0]
        if bdef.get("foundation_tile"):
            return g.can_lay_foundation(p, tx, ty, n=bdef["size"][0])[0]
        return g.can_place(p, bdef, tx, ty, check_req=len(self.mode) < 3)[0]

    def draw_placement_area(self, cx, cy, x0, y0, x1, y1):
        if not self.mode or self.mode[0] != "place":
            self._placement_area = None
            return
        bdef = self.mode[1]
        now = pygame.time.get_ticks()
        scale = self.screen.k if isinstance(self.screen, gdraw.WorldCanvas) else 1
        key = (self.mode, self.g.map.version, x0, y0, x1, y1, scale, tuple(self.p.color))
        cached = getattr(self, "_placement_area", None)
        if cached is None or cached[0] != key or now - cached[1] >= 120:
            w, h = bdef["size"]
            if bdef.get("pond_tile"):
                w = h = self.p.rules.get("pond_size", self.p.faction.get("pond", {}).get("size", 2))
            elif bdef.get("spike_tile"):
                w = h = self.p.rules.get("spike_size", 1)
            covered = set()
            for ty in range(max(0, y0 - h + 1), y1):
                for tx in range(max(0, x0 - w + 1), x1):
                    if self._can_preview_place(bdef, tx, ty):
                        covered.update((x, y)
                                       for y in range(max(y0, ty), min(y1, ty + h))
                                       for x in range(max(x0, tx), min(x1, tx + w)))
            layer = None
            if covered:
                color = tuple(self.p.color)
                size = round(TILE * scale)
                tile = pygame.Surface((size, size), pygame.SRCALPHA)
                tile.fill((*color, 55))
                pygame.draw.rect(tile, (*color, 175), tile.get_rect(), max(1, round(scale)))
                layer = pygame.Surface(((x1 - x0) * size, (y1 - y0) * size), pygame.SRCALPHA)
                for x, y in covered:
                    layer.blit(tile, ((x - x0) * size, (y - y0) * size))
                if isinstance(self.screen, gdraw.WorldCanvas):
                    gdraw.mark_hd(layer)
            cached = self._placement_area = key, now, layer
        if cached[2] is not None:
            self.screen.blit(cached[2], (x0 * TILE - cx, y0 * TILE - cy))

    def _draw_tile_placement_outline(self, screen, rect, ok):
        color = (90, 255, 110) if ok else (255, 70, 60)
        if isinstance(screen, gdraw.WorldCanvas):
            k = screen.k
            rect = tuple(round(value * k) for value in rect)
            pygame.draw.rect(screen.t, color, rect, round(2 * k))
        else:
            pygame.draw.rect(screen, color, rect, 2)

    def draw_mode_overlay(self, cx, cy):
        s = self.screen
        g, p = self.g, self.p
        mx, my = self.mpos()
        if not self.mode or mx >= self.vw:
            return
        wx, wy = mx + cx, my + cy
        if self.mode[0] == "place":
            bdef = self.mode[1]
            tx, ty = self.place_pos(wx, wy, bdef)
            w, h = bdef["size"]
            bx, by = tx * TILE - cx, ty * TILE - cy
            ok = self._can_preview_place(bdef, tx, ty)
            if bdef.get("pond_tile"):
                w = h = p.rules.get("pond_size", p.faction.get("pond", {}).get("size", 2))
                tile_art = lambda x, y: self.pond_tile(tx + x, ty + y)
            elif bdef.get("spike_tile"):
                w = h = p.rules.get("spike_size", 1)
                tile_art = lambda x, y: self.spike_tile(tx + x, ty + y)
            elif bdef.get("foundation_tile"):
                tile_art = lambda x, y: self.foundation_tile()
            else:
                tile_art = None
            if tile_art is None:
                self._draw_placement_building(bdef, bx, by, ok)
            else:
                for y in range(h):
                    for x in range(w):
                        image = self._fit_world_tile(tile_art(x, y))
                        tile_rect = (bx + x * TILE, by + y * TILE, TILE, TILE)
                        s.blit(self._placement_tint(image, ok), tile_rect[:2])
                        if bdef.get("spike_tile") and w > 1:
                            self._draw_tile_placement_outline(s, tile_rect, ok)
            self._draw_tile_placement_outline(s, (bx, by, w * TILE, h * TILE), ok)
            if bdef.get("attack"):
                gdraw.circle(s, (255, 230, 120), ((tx + w / 2) * TILE - cx, (ty + h / 2) * TILE - cy),
                                   bdef["attack"]["range"] * TILE, 1)
        elif self.mode[0] == "attack":
            gdraw.circle(s, (255, 70, 60), (mx, my), 10, 2)
            gdraw.line(s, (255, 70, 60), (mx - 14, my), (mx + 14, my))
            gdraw.line(s, (255, 70, 60), (mx, my - 14), (mx, my + 14))
        elif self.mode[0] == "ability_place":
            ab_id = self.mode[1]
            radius = g.placement_radius(g.data.abilities[ab_id])
            valid = g.placement_caster(self.own_sel_units(), ab_id, wx, wy) is not None
            gdraw.circle(s, (100, 240, 120) if valid else (255, 90, 75), (mx, my), radius, 2)
        elif self.mode[0] in ("anchor_pick", "anchor_drop"):
            wx, wy = mx + cx, my + cy
            if self.mode[0] == "anchor_pick":
                anchor = self.entity_at(wx, wy)
                valid = g.anchor_available(self.p, anchor)
                radius = anchor.radius if valid else 10
            else:
                anchor = self.mode[1]
                valid = g.anchor_available(self.p, anchor) and g.anchor_drop_allowed(anchor, wx, wy)
                radius = anchor.radius
            gdraw.circle(s, (100, 240, 120) if valid else (255, 90, 75), (mx, my), radius, 2)
        elif self.mode[0] in ("cast", "gcast"):
            ab = g.data.abilities[self.mode[1]]
            gdraw.circle(s, tuple(ab.get("color", (255, 200, 80))), (mx, my), ab.get("radius", 2) * TILE, 2)

    SPEEDS = [0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0]

    def cur_speed(self):
        return self.g.speed if self.net is not None else self.speed

    def change_speed(self, d):
        """Játéksebesség egy fokozattal fel/le. Hálózatban csak a gazda állíthatja (mindenkinél érvényes)."""
        cur = self.cur_speed()
        i = min(range(len(self.SPEEDS)), key=lambda k: abs(self.SPEEDS[k] - cur))
        new = self.SPEEDS[max(0, min(len(self.SPEEDS) - 1, i + d))]
        if self.net is not None:
            if self.p.id != 0:
                self.g.notify(self.p, "Hálózati játékban a sebességet a gazda állítja.", (255, 160, 90))
                return
            self.cmd("cmd_set_speed", self.p, new)
        else:
            self.g.speed = self.speed = new

    def draw_hud(self, fps):
        """Küldetéslista (bal felül), óra és sebesség (jobb felül), üzenetek (a HUD fölött)."""
        s = self.screen
        g = self.g
        self.quest_bottom = self.draw_quests()
        self.draw_clock(fps)
        F = self.F("small", False)
        lh = F.get_linesize() + 4
        y = self.L["hud"].y - lh - 6
        recent = [m for m in g.messages if g.time - m[2] < 8 and (len(m) < 4 or m[3] == self.p.id)][-6:]
        for text, col, *_ in reversed(recent):
            img = F.render(text, True, col)
            bg = pygame.Surface((img.get_width() + 10, img.get_height() + 2), pygame.SRCALPHA)
            bg.fill((0, 0, 0, 140))
            s.blit(bg, (8, y - 1))
            s.blit(img, (13, y))
            y -= lh

    # ------------------------------------------------------------------
    @property
    def th(self):
        """A saját faj kezelőfelület-témája (rts/faction_ui.py)."""
        from . import faction_ui as FUI
        return FUI.theme(self.p.faction["id"])

    def bevel(self, r, fill=(42, 44, 48), light=(110, 112, 118), dark=(16, 16, 18)):
        """Vésett, a faj anyagaiból keretezett panel (a régi RTS-ek kezelőfelülete szerint)."""
        s = self.screen
        r = pygame.Rect(r)
        th = self.th
        hi = tuple(int(c * 0.85) for c in th["light"])
        lo = tuple(int(c * 0.9) for c in th["dark"])
        if fill == (42, 44, 48):
            fill = th["fill"]
        gdraw.rect(s, fill, r)
        gdraw.rect(s, (12, 10, 8), r, 1)
        gdraw.line(s, hi, (r.x + 1, r.y + 1), (r.right - 2, r.y + 1))
        gdraw.line(s, hi, (r.x + 1, r.y + 1), (r.x + 1, r.bottom - 2))
        gdraw.line(s, lo, (r.x + 1, r.bottom - 2), (r.right - 2, r.bottom - 2))
        gdraw.line(s, lo, (r.right - 2, r.y + 1), (r.right - 2, r.bottom - 2))


    def clock_wipe(self, r, prog):
        surf = pygame.Surface((r.w, r.h), pygame.SRCALPHA)
        cx, cy = r.w / 2, r.h / 2
        R = max(r.w, r.h)
        pts = [(cx, cy)]
        a0 = -math.pi / 2 + prog * 2 * math.pi
        steps = 24
        for i in range(steps + 1):
            a = a0 + (2 * math.pi - prog * 2 * math.pi) * i / steps
            pts.append((cx + math.cos(a) * R, cy + math.sin(a) * R))
        if len(pts) > 2 and prog < 1:
            gdraw.polygon(surf, (0, 0, 0, 150), pts)
        self.screen.blit(surf, r)
        gdraw.rect(self.screen, (255, 220, 100), r, 1)

    def draw_minimap(self):
        g, p = self.g, self.p
        r = self.mm_rect
        s = self.screen
        if self.t - self.mini_t > 0.25 or self.mini is None:
            self.mini_t = self.t
            m = g.map
            if self.mini_base_ver != m.version:
                base = self.mini_terrain.copy()
                rcols = {k: tuple(v["color"]) for k, v in g.data.game["resources"].items()}
                for res in m.resources:
                    for yy in range(res.ty, res.ty + res.h):
                        for xx in range(res.tx, res.tx + res.w):
                            base.set_at((xx, yy), shade(rcols[res.kind], 0.8) if res.kind != "wood" else (30, 80, 35))
                self.mini_base = pygame.transform.scale(base, (r.w, r.h))
                self.mini_base_ver = m.version
            surf = self.mini_base.copy()
            sx, sy = r.w / (m.w * TILE), r.h / (m.h * TILE)
            for b in g.buildings:
                if b.owner is p or b.id in self.seen:
                    gdraw.rect(surf, b.owner.color, (b.rect[0] * sx, b.rect[1] * sy, max(3, b.rect[2] * sx), max(3, b.rect[3] * sy)))
            for u in g.units:
                if u.inside is None and not self.in_mine(u) and g.is_visible(p, u):
                    gdraw.rect(surf, shade(u.owner.color, 1.2), (u.x * sx - 1, u.y * sy - 1, 2, 2))
            for pt in getattr(g, "points", ()):  # Hasadékkövek
                col = pt.owner.color if pt.owner is not None else (200, 150, 255)
                gdraw.circle(surf, (20, 10, 30), (pt.x * sx, pt.y * sy), 4)
                gdraw.circle(surf, col, (pt.x * sx, pt.y * sy), 3)
            if self.fog_src is not None:
                surf.blit(pygame.transform.smoothscale(self.fog_src, (r.w, r.h)), (0, 0))
            if g.director is not None:
                for mx_, my_, mr, _ in g.director.markers():
                    gdraw.circle(surf, (255, 220, 90), (mx_ * sx, my_ * sy), 4, 2)
            for ax, ay, at, *who in g.alerts:
                if g.time - at < 4 and (not who or who[0] == self.p.id):
                    gdraw.circle(surf, (255, 60, 50), (ax * sx, ay * sy), 4 + (g.time - at) * 3, 1)
            self.mini = surf
        s.blit(self.mini, r)
        sx, sy = r.w / (g.map.w * TILE), r.h / (g.map.h * TILE)
        vw, vh = self._wv()
        clip = s.get_clip()
        s.set_clip(clip.clip(r))
        try:
            gdraw.rect(s, (255, 255, 255), (r.x + self.cam_x * sx, r.y + self.cam_y * sy, vw * sx, vh * sy), 1)
        finally:
            s.set_clip(clip)

    _ICONS = {}

    _ALIAS = {"build": "deploy", "next": "blink"}

    def cmd_icon(self, kind, passive=False, ab_id=None):
        key = (kind, passive, ab_id)
        img = self._ICONS.get(key)
        if img is None:
            # saját rajz, ha van: assets/abilities/<képesség>.png, majd assets/ui/icons/<fajta>.png
            # natív méretben (128×128) – a parancsrács egyszer, egyetlen kicsinyítéssel illeszti a cellába
            # (régen 37 képpontra kicsinyítve töltődött be, majd visszanagyítva: ettől voltak homályosak)
            img = (self.sprites.art("abilities/" + ab_id) if ab_id else None) or self.sprites.art("ui/icons/" + kind)
            if img is not None and passive:
                img = img.copy()
                img.fill((120, 120, 120, 255), special_flags=pygame.BLEND_RGBA_MULT)
            if img is None:
                img = draw_glyph_icon(self._ALIAS.get(kind, kind), 96, passive)
            self._ICONS[key] = img
        return img

    def bar(self, x, y, w, frac, col, label):
        s = self.screen
        gdraw.rect(s, (10, 10, 10), (x, y, w, 13))
        gdraw.rect(s, shade(col, 0.7), (x, y, w * max(0, min(1, frac)), 13))
        img = self.fonts["tiny"].render(label, True, (250, 250, 240))
        s.blit(img, (x + 4, y))

    def wrap(self, text, x, y, w, font, col, max_lines=20, surf=None):
        target = surf or self.screen
        from .i18n import T
        words = T(text).split()
        line = ""
        n = 0
        for wd in words:
            t = (line + " " + wd).strip()
            if font.size(t)[0] > w and line:
                target.blit(font.render(line, True, col), (x, y))
                y += font.get_linesize()
                n += 1
                if n >= max_lines:
                    return y
                line = wd
            else:
                line = t
        if line:
            target.blit(font.render(line, True, col), (x, y))
            y += font.get_linesize()
        return y

    def draw_tooltip(self):
        mx, my = pygame.mouse.get_pos()
        tip = None
        for rect, _, _, data in self.regions:
            if data and rect.collidepoint(mx, my):
                tip = data
                break
        if not tip:
            return
        g, p = self.g, self.p
        F = self.fonts
        s = self.screen
        hotkey = None
        if tip[0] == "slot":  # parancsrács: a billentyűt is kiírjuk
            hotkey, tip = tip[1], tip[2]
        if tip[0] in ("ability", "learn"):
            if tip[0] == "learn":
                ab_id, hero = tip[1], tip[2]
                cur = hero.learned.get(ab_id, 0)
                ab = H.ability(g, ab_id, min(cur + 1, H.max_ability_level(g, ab_id)))
                head = f"{ab['name']} – tanulás: {cur + 1}. szint"
                need = H.required_hero_level(g, ab_id, cur + 1)
                extra = [] if cur >= H.max_ability_level(g, ab_id) else [f"Szükséges hősszint: {need}"]
            else:
                ab = tip[1]
                head = ab["name"] + (f" ({tip[2]}. szint)" if len(tip) > 2 and tip[2] else "")
                extra = []
            lines = [(head, F["med"], (255, 230, 150))]
            if len(tip) > 3 and tip[3]:
                lines.append(("Passzív képesség", F["small"], (170, 170, 170)))
            if ab.get("faith"):
                fname = self.p.faction.get("faith", {}).get("name", "Áhítat")
                lines.append((f"Ára: {ab['faith']} {fname}", F["small"], (255, 214, 120)))
            if ab.get("energy"):
                lines.append((f"Energia: {ab['energy']}", F["small"], (200, 130, 255)))
            if ab.get("hp_cost"):
                lines.append((f"Ára: {ab['hp_cost']} életerő", F["small"], (255, 120, 110)))
            if ab.get("spores"):
                lines.append((f"Ára: {ab['spores']} spóra", F["small"], (150, 240, 110)))
            vals = []
            for k, nm in (("damage", "Sebzés"), ("dps", "Sebzés/mp"), ("count", "Darab"), ("amount", "Gyógyítás"),
                          ("radius", "Sugár"), ("duration", "Időtartam"), ("cooldown", "Újratöltés"),
                          ("heal", "Gyógyítás" if ab.get("type") == "nova" else "Gyógyítás/mp")):
                if ab.get(k):
                    vals.append(f"{nm}: {ab[k]:g}")
            names = {"speed": "Sebesség", "damage": "Sebzés", "armor": "Páncél", "ranged_taken": "Kapott távolsági sebzés",
                     "range": "Lőtáv", "regen": "Gyógyulás"}
            for k, v in ab.get("modifiers", {}).items():
                nm = names.get(k.rsplit("_", 1)[0], k)
                if k.endswith("_mult"):
                    vals.append("Nem mozog" if k == "speed_mult" and v == 0 else f"{nm} {(v - 1) * 100:+.0f}%")
                else:
                    vals.append(f"{nm} {v:+g}")
            if vals:
                lines.append(("  •  ".join(vals), F["tiny"], (190, 200, 210)))
            for e in extra:
                lines.append((e, F["tiny"], (255, 200, 120)))
            if ab.get("ultimate"):
                lines.append(("Végső képesség (6. szinttől)", F["tiny"], (255, 170, 80)))
            desc = ab.get("desc", "")
        elif tip[0] == "info":
            lines = [(tip[1], F["med"], (255, 230, 150))]
            desc = tip[2]
        elif tip[0] == "item":
            it = tip[1]
            rcol = {"common": (230, 220, 190), "rare": (110, 170, 255), "artifact": (255, 190, 70)}.get(it.get("rarity"))
            rname = {"common": "közönséges", "rare": "ritka", "artifact": "ereklye"}.get(it.get("rarity"), "")
            lines = [(it["name"], F["med"], rcol),
                     (f"{rname} • {'fogyóeszköz – bal klikk: használat' if it.get('kind') == 'consumable' else 'viselt tárgy'}",
                      F["tiny"], (180, 180, 170)), ("Jobb klikk: eldobás", F["tiny"], (140, 140, 130))]
            desc = it.get("desc", "")
        else:
            kind, d, miss = tip
            lines = [(d["name"], F["med"], (255, 230, 150))]
            desc = d.get("desc", "")
            if d.get("hero"):
                st = p.hero_states.get(d["id"])
                if st:
                    lines.append((f"Újraélesztés – {st.get('level', 1)}. szint", F["small"], (255, 220, 120)))
                else:
                    lines.append(("Hős – szintet lép, képességeket tanul, tárgyakat visel", F["tiny"], (255, 220, 120)))
        w = min(self.sw - 8, max(300, round(300 * self.u)))
        blits = []

        class TooltipLines:
            def blit(self, image, pos):
                blits.append((image, pos))

        box = TooltipLines()
        yy = 6
        for text, font, col in lines:  # minden sor tördelődik (a hosszú cím se lógjon ki)
            yy = self.wrap(text, 8, yy, w - 16, font, col, surf=box) + 2
        if tip[0] not in ("ability", "learn", "item", "info"):
            kind, d, miss = tip
            rd = g.data.game["resources"]
            cx = 8
            for k, v in d.get("cost", {}).items():
                col = tuple(rd[k]["color"]) if p.res.get(k, 0) >= v else (255, 80, 70)
                img = F["small"].render(f"{p.res_name(k, rd)} {v}", True, col)
                if cx > 8 and cx + img.get_width() > w - 8:  # nem fér ki: új sor
                    cx, yy = 8, yy + F["small"].get_linesize()
                box.blit(img, (cx, yy))
                cx += img.get_width() + 10
            yy += 20
            extra = []
            if kind == "unit":
                extra.append(f"Létszám: {d.get('supply', 1)}")
                if d.get("count", 1) > 1:
                    extra.append(f"{d['count']} egység")
            if d.get("time"):
                extra.append(f"Idő: {d['time']} mp")
            if kind == "building":
                extra.append(f"Méret: {d['size'][0]}×{d['size'][1]}")
            yy = self.wrap("  •  ".join(extra), 8, yy, w - 16, F["tiny"], (190, 190, 180), surf=box) + 2
            if miss and miss != ["kész"]:
                yy = self.wrap("Szükséges: " + ", ".join(miss), 8, yy, w - 16, F["tiny"], (255, 110, 90), surf=box)
            if kind == "building" and (d.get("pond_tile") or d.get("foundation_tile") or d.get("spike_tile")):
                yy = self.wrap("Klikk: a munkás odamegy és elkészíti • Shift: több egymás után • B / Esc: vissza",
                               8, yy, w - 16, F["tiny"], (140, 140, 130), surf=box)
            elif kind == "building":
                yy = self.wrap("Klikk / billentyű: lerakás • Shift: többször • B / Esc: vissza", 8, yy, w - 16, F["tiny"], (140, 140, 130), surf=box)
            elif kind == "unit":
                yy = self.wrap("Klikk / billentyű: kiképzés (Shift: 5×) • Jobb klikk: visszavonás", 8, yy, w - 16, F["tiny"], (140, 140, 130), surf=box)
        yy = self.wrap(desc, 8, yy + 4, w - 16, F["tiny"], (215, 215, 205), surf=box)
        if hotkey:
            yy = self.wrap(f"Billentyű: {hotkey}", 8, yy + 2, w - 16, F["small"], (255, 230, 150), surf=box)
        h = max(yy + 8, max((y + img.get_height() + 4 for img, (_, y) in blits), default=0))
        bg = pygame.Surface((w, h), pygame.SRCALPHA)
        bg.fill((10, 12, 14, 230))
        bg.blits(blits, doreturn=False)
        limit = max(1, self.L["hud"].y - 10 if my >= self.L["hud"].y else self.sh - 8)
        if h > limit:
            w, h = max(1, round(w * limit / h)), limit
            bg = pygame.transform.smoothscale(bg, (w, h))
        x = max(4, min(mx - w // 2, self.sw - w - 4))
        y = self.L["hud"].y - h - 6 if my >= self.L["hud"].y else min(my + 24, self.sh - h - 4)
        y = max(4, y)
        s.blit(bg, (x, y))
        gdraw.rect(s, (150, 130, 80), (x, y, w, h), 1)

    def draw_center_box(self, lines):
        s = self.screen
        F = self.fonts["med"]
        maxw = min(self.vw - 80, int(760 * self.u))
        wrapped = []
        for i, ln in enumerate(lines):  # hosszú sorok tördelése (pl. epilógus)
            if i == 0 or F.size(ln)[0] < maxw:
                wrapped.append(ln)
                continue
            cur = ""
            for wd in ln.split():
                t = (cur + " " + wd).strip()
                if F.size(t)[0] > maxw and cur:
                    wrapped.append(cur)
                    cur = wd
                else:
                    cur = t
            if cur:
                wrapped.append(cur)
        lines = wrapped
        big = self.fonts["big"]
        pad = int(20 * self.u)
        w = max([big.size(lines[0])[0]] + [F.size(ln)[0] for ln in lines[1:]]) + 2 * pad
        w = max(int(460 * self.u), min(self.vw - 40, w))
        h = 2 * pad + big.get_linesize() + F.get_linesize() * (len(lines) - 1)
        x, y = self.vw / 2 - w / 2, self.vh / 2 - h / 2
        box = pygame.Surface((w, h), pygame.SRCALPHA)
        box.fill((8, 10, 12, 230))
        s.blit(box, (x, y))
        gdraw.rect(s, (170, 140, 70), (x, y, w, h), 2)
        yy = y + pad
        for i, ln in enumerate(lines):
            f = big if i == 0 else F
            img = f.render(ln, True, (255, 230, 150) if i == 0 else (220, 220, 210))
            s.blit(img, (self.vw / 2 - img.get_width() / 2, yy))
            yy += f.get_linesize()

    def draw_help(self):
        s = self.screen
        u = self.u
        pad = int(16 * u)
        y = self.L["topbar"].bottom + int(10 * u)
        w = min(self.vw - 40, int(1000 * u))
        avail_h = self.L["hud"].y - y - int(10 * u)
        fb = self.fonts["small"]
        for f in (self.fonts["small"], self.fonts["tiny"]):  # a nagyobb betű, ha elfér
            fb = f
            n = sum(self._wrap_count(ln, f, w - 2 * pad - int(20 * u)) for ln in HELP[1:])
            if 2 * pad + self.fonts["med"].get_linesize() + n * f.get_linesize() + int(6 * u) <= avail_h:
                break
        lines = []
        for ln in HELP[1:]:
            ind = len(ln) - len(ln.lstrip())
            first = True
            for part in self._wrap_lines(ln.strip(), fb, w - 2 * pad - int(20 * u)):
                lines.append((part, ind > 0 or not first))
                first = False
        h = min(avail_h, 2 * pad + self.fonts["med"].get_linesize() + len(lines) * fb.get_linesize() + int(6 * u))
        x = self.vw / 2 - w / 2
        box = pygame.Surface((w, h), pygame.SRCALPHA)
        box.fill((8, 10, 12, 235))
        s.blit(box, (x, y))
        gdraw.rect(s, (170, 140, 70), (x, y, w, h), 2)
        s.blit(self.fonts["med"].render(HELP[0], True, (255, 230, 150)), (x + pad, y + pad))
        yy = y + pad + self.fonts["med"].get_linesize() + int(6 * u)
        for txt, indent in lines:
            if yy + fb.get_linesize() > y + h - pad // 2:
                break
            s.blit(fb.render(txt, True, (220, 220, 210)), (x + pad + (int(20 * u) if indent else 0), yy))
            yy += fb.get_linesize()

    @staticmethod
    def _wrap_lines(text, font, w):
        from .i18n import T
        out, line = [], ""
        for wd in T(text).split():
            t = (line + " " + wd).strip()
            if font.size(t)[0] > w and line:
                out.append(line)
                line = wd
            else:
                line = t
        return out + ([line] if line else [])

    def _wrap_count(self, text, font, w):
        return max(1, len(self._wrap_lines(text.strip(), font, w)))

    def draw_decor(self, dc, cx, cy):
        """Díszletépület: az épület képe, csapatszín nélkül, életcsík és jelölés nélkül."""
        sheet = self.sprites.building(dc.defn_id) or self.sprites._sheet("decor", dc.defn_id)
        bx, by = dc.rect[0] - cx, dc.rect[1] - cy
        if sheet is None:
            gdraw.rect(self.screen, (90, 80, 64), (bx, by, dc.rect[2], dc.rect[3]))
            return
        ox, oy = sheet.anchor
        fr = sheet.frame((128, 120, 104), dc.frame % max(1, sheet.meta.get("frames", 1)))
        if dc.ruin:  # rom: sötétebb, kopottabb
            fr = fr.copy()
            fr.fill((150, 140, 128), special_flags=pygame.BLEND_RGB_MULT)
            if sheet.scale > 1:
                gdraw.mark_hd(fr)
        self.screen.blit(fr, (bx - ox, by - oy))
        if self.settings.get("shadows", True):
            SHD.blit_inner(self.screen, fr, bx - ox, by - oy)
        if hasattr(self, "_shadow_next"):
            self._queue_shadow(dc, fr, bx - ox + int(self.cam_x), by - oy + int(self.cam_y), mode="col")

    def draw_actor(self, a, cx, cy):
        """Háttérszereplő: egy meglévő egység képe járás / ácsorgás animációval, gazda-szín nélkül."""
        from .sprites import dir_index
        sheet = self.sprites.unit(a.defn_id)
        sx, sy = a.x - cx, a.y - cy
        if sheet is None:
            gdraw.circle(self.screen, (150, 140, 120), (sx, sy), 6)
            return
        anim = "walk" if a.moving and "walk" in sheet.anims else "idle"
        fr = sheet.anim_frame((150, 140, 120), anim, a.anim, dir_index(a.angle))
        pos = (sx - sheet.anchor[0], sy - sheet.anchor[1])
        gdraw.blit_trim(self.screen, fr, *pos)
        if hasattr(self, "_shadow_next"):
            self._queue_shadow(a, fr, pos[0] + int(self.cam_x), pos[1] + int(self.cam_y),
                               int(sheet.anchor_px[1]))

    def draw_dialogue(self, d):
        """Párbeszéd: alul középen portré + beszélő + szöveg; átvezető alatt szélesvásznú sávok."""
        s = self.screen
        cine = d.cine is not None
        if cine:
            bar = int(self.vh * 0.09)
            s.fill((0, 0, 0), (0, 0, self.vw, bar))
            s.fill((0, 0, 0), (0, self.vh - bar, self.vw, bar))
            F = self.F("tiny", False)
            hint = F.render("Szóköz / kattintás: tovább   •   Esc: átvezető kihagyása", True, (170, 160, 140))
            s.blit(hint, (self.vw - hint.get_width() - 12, self.vh - bar + (bar - hint.get_height()) // 2))
        ln = d.current_line()
        if not ln:
            return
        u = self.u
        k = lambda v: int(round(v * u))  # noqa: E731
        w = min(self.vw - k(40), k(760))
        ps = k(84)
        Fn, Ft = self.F("name"), self.F("small", False)
        lines = self._wrap_lines(ln.get("text", ""), Ft, w - ps - k(36))[:5]
        h = max(ps + k(16), k(14) + Fn.get_linesize() + len(lines) * Ft.get_linesize() + k(12))
        x = self.vw // 2 - w // 2
        y = self.L["hud"].y - h - k(54)   # a játéktér alján, a HUD fölött (átvezetőben is)
        bg = pygame.Surface((w, h), pygame.SRCALPHA)
        bg.fill((10, 10, 14, 225))
        s.blit(bg, (x, y))
        gdraw.rect(s, (190, 150, 80), (x, y, w, h), 2)
        pr = pygame.Rect(x + k(8), y + k(8), ps, ps)
        art = self.sprites.portrait(ln["portrait"]) if ln.get("portrait") else None
        if art is not None:
            s.blit(pygame.transform.smoothscale(art, pr.size), pr)
        else:
            gdraw.rect(s, (40, 36, 30), pr)
        gdraw.rect(s, (150, 120, 70), pr, 1)
        tx = pr.right + k(14)
        who = ln.get("who", "")
        if ln.get("to"):
            who += f"  →  {ln['to']}"
        s.blit(Fn.render(who, True, tuple(ln.get("color", (255, 214, 120)))), (tx, y + k(8)))
        yy = y + k(10) + Fn.get_linesize()
        for t in lines:
            s.blit(Ft.render(t, True, (236, 232, 220)), (tx, yy))
            yy += Ft.get_linesize()

    def draw_choice(self, ch):
        """Döntés-ablak: cím, helyzetleírás, opciók (kattintás vagy 1/2/3)."""
        s = self.screen
        F = self.fonts
        u = self.u
        k = lambda v: int(round(v * u))  # noqa: E731
        w = min(self.vw - 60, k(800))
        x = self.vw / 2 - w / 2
        opts = ch.get("options", [])
        dim = pygame.Surface((self.vw, self.vh), pygame.SRCALPHA)
        dim.fill((0, 0, 0, 120))
        s.blit(dim, (0, 0))
        tmp = pygame.Surface((w, self.vh), pygame.SRCALPHA)
        yy = k(16)
        tmp.blit(F["big"].render(self.ellipsize(ch.get("title", "Döntés"), F["big"], w - k(40)), True, (255, 220, 130)),
                 (k(20), yy))
        yy += F["big"].get_linesize() + k(8)
        yy = self.wrap(ch.get("text", ""), k(20), yy, w - k(40), F["small"], (230, 228, 214), 12, surf=tmp) + k(12)
        self.choice_rects = []
        top = yy
        for i, o in enumerate(opts):
            nh = min(3, self._wrap_count(o.get("hint", ""), F["tiny"], w - k(80))) if o.get("hint") else 0
            hgt = k(10) + F["med"].get_linesize() + nh * F["tiny"].get_linesize() + k(4)
            self.choice_rects.append(pygame.Rect(x + k(20), 0, w - k(40), hgt))
            yy += hgt + k(10)
        h = yy + k(10)
        y = self.vh / 2 - h / 2
        bg = pygame.Surface((w, h), pygame.SRCALPHA)
        bg.fill((10, 12, 16, 240))
        s.blit(bg, (x, y))
        gdraw.rect(s, (200, 160, 80), (x, y, w, h), 2)
        s.blit(tmp, (x, y), pygame.Rect(0, 0, w, top))
        mx, my = pygame.mouse.get_pos()
        yy = y + top
        for i, (o, r) in enumerate(zip(opts, self.choice_rects)):
            r.y = yy
            hover = r.collidepoint(mx, my)
            gdraw.rect(s, (110, 84, 40) if hover else (48, 44, 40), r)
            gdraw.rect(s, (255, 214, 120) if hover else (140, 110, 70), r, 1)
            s.blit(F["med"].render(self.ellipsize(f"{i + 1}. {o.get('label', '')}", F["med"], r.w - k(24)), True,
                                   (255, 240, 200)), (r.x + k(12), r.y + k(6)))
            if o.get("hint"):
                self.wrap(o["hint"], r.x + k(28), r.y + k(6) + F["med"].get_linesize(), r.w - k(40), F["tiny"],
                          (200, 200, 190), 3)
            yy += r.h + k(10)

    # ---- a meccs vége: felirat két gombbal, majd az összesítő oldal (RTS-szerűen) ---------------------------
    SUM_TABS = ("Áttekintés", "Gazdaság", "Katonaság", "Grafikon")

    def _res_button(self, r, text, hover, primary=False):
        s, F = self.screen, self.fonts
        bg = pygame.Surface(r.size, pygame.SRCALPHA)
        bg.fill((70, 54, 30, 240) if hover else (24, 20, 16, 235))
        s.blit(bg, r.topleft)
        gold = (255, 214, 120)
        pygame.draw.rect(s, gold if hover or primary else (150, 124, 72), r, 2)
        img = F["big"].render(text, True, (255, 236, 190) if hover else (236, 220, 180))
        s.blit(img, img.get_rect(center=r.center))

    def draw_result(self):
        if getattr(self, "summary", False):
            return self.draw_summary()
        g, p = self.g, self.p
        s = self.screen
        W, H = s.get_size()
        kind = g.result[0]
        state = "victory" if kind == "win" else "defeat"
        # háttérkép nincs: a csatatér látszik, a felirat a sötét sávon
        title = "GYŐZELEM!" if kind == "win" else "VERESÉG"
        if g.director is not None:
            title = "KÜLDETÉS TELJESÍTVE" if kind == "win" else "A KÜLDETÉS ELBUKOTT"
        u = self.u
        k = lambda v: int(round(v * u))  # noqa: E731
        F = self.fonts
        img = F["huge"].render(title, True, (255, 220, 120) if kind == "win" else (230, 120, 100))
        band = pygame.Rect(0, int(H * 0.36), W, img.get_height() + k(110))
        sh = pygame.Surface(band.size, pygame.SRCALPHA)
        for y in range(band.h):  # felül-alul elhalványuló sötét sáv a felirat mögött
            a = int(200 * min(1.0, min(y, band.h - y) / max(1, band.h * 0.3)))
            sh.fill((6, 5, 4, a), (0, y, W, 1))
        s.blit(sh, band.topleft)
        s.blit(F["huge"].render(title, True, (0, 0, 0)), (W / 2 - img.get_width() / 2 + 3, band.y + k(12) + 3))
        s.blit(img, (W / 2 - img.get_width() / 2, band.y + k(12)))
        bw, bh = k(220), k(48)
        y = band.y + k(24) + img.get_height()
        mx, my = pygame.mouse.get_pos()
        self.result_btns = []
        for n, (text, act) in enumerate((("ÖSSZESÍTŐ", "summary"), ("FŐMENÜ", "menu"))):
            r = pygame.Rect(0, 0, bw, bh)
            r.center = (W / 2 + (n - 0.5) * (bw + k(24)), y + bh / 2)
            self._res_button(r, text, r.collidepoint(mx, my), primary=n == 0)
            self.result_btns.append((r, act))
        hint = F["tiny"].render("Enter: összesítő  •  Esc: főmenü", True, (200, 190, 170))
        s.blit(hint, (W / 2 - hint.get_width() / 2, y + bh + k(10)))

    @staticmethod
    def _score(q):
        st = getattr(q, "stats", {}) or {}
        mil = st.get("killed_value", 0) // 10 + st.get("units_killed", 0) * 2
        eco = int(sum(q.gathered.values())) // 10
        bld = st.get("buildings_built", 0) * 15 + st.get("buildings_killed", 0) * 20
        return mil + eco + bld, mil, eco, bld

    def draw_summary(self):
        """Összesítő oldal: fülek (áttekintés, gazdaság, katonaság, grafikon), játékosonként egy sor."""
        g, p = self.g, self.p
        s = self.screen
        W, H = s.get_size()
        F = self.fonts
        u = self.u
        k = lambda v: int(round(v * u))  # noqa: E731
        dim = pygame.Surface((W, H), pygame.SRCALPHA)
        dim.fill((0, 0, 0, 175))
        s.blit(dim, (0, 0))
        pw, ph = min(W - k(40), k(1180)), min(H - k(40), k(680))
        P = pygame.Rect(0, 0, pw, ph)
        P.center = (W / 2, H / 2)
        bg = pygame.Surface(P.size, pygame.SRCALPHA)
        bg.fill((14, 12, 10, 236))
        s.blit(bg, P.topleft)
        pygame.draw.rect(s, (170, 140, 80), P, 2)
        t = int(g.time)
        won = g.result[0] == "win"
        head = F["big"].render("ÖSSZESÍTŐ", True, (255, 220, 130))
        s.blit(head, (P.x + k(20), P.y + k(14)))
        sub = f"{'Győzelem' if won else 'Vereség'}  •  Játékidő: {t // 60}:{t % 60:02d}"
        if g.director is not None:
            sub = g.director.m.get("title", "") + "  •  " + sub
        si = F["small"].render(self.ellipsize(sub, F["small"], P.w - head.get_width() - k(60)), True, (220, 210, 190))
        s.blit(si, (P.right - si.get_width() - k(20), P.y + k(20)))
        # fülek
        mx, my = pygame.mouse.get_pos()
        tab = getattr(self, "sum_tab", 0)
        tx, ty = P.x + k(20), P.y + k(14) + head.get_height() + k(10)
        self.sum_tabs = []
        for i, name in enumerate(self.SUM_TABS):
            img = F["med"].render(name, True, (255, 236, 190) if i == tab else (200, 190, 170))
            r = pygame.Rect(tx, ty, img.get_width() + k(28), img.get_height() + k(12))
            pygame.draw.rect(s, (60, 46, 26) if i == tab else ((40, 34, 26) if r.collidepoint(mx, my) else (24, 20, 16)), r)
            pygame.draw.rect(s, (200, 166, 90) if i == tab else (110, 92, 60), r, 1)
            s.blit(img, (r.x + k(14), r.y + k(6)))
            self.sum_tabs.append((r, i))
            tx = r.right + k(6)
        body = pygame.Rect(P.x + k(20), ty + k(44), P.w - k(40), P.bottom - ty - k(44) - k(76))
        pygame.draw.line(s, (110, 92, 60), (P.x + k(12), body.y - k(8)), (P.right - k(12), body.y - k(8)))
        players = [q for q in g.players if not getattr(q, "neutral", False)]
        rd = g.data.game["resources"]
        survivors = [q for q in players if not q.defeated]
        winner = p if won else (
            survivors[0] if survivors and all(q is survivors[0] or survivors[0].allied(q) for q in survivors)
            and not p.allied(survivors[0]) else None
        )

        def outcome(q):
            if q.defeated:
                return "kiesett"
            return "győztes" if winner is not None and (q is winner or winner.allied(q)) else "életben"

        if tab == 3:
            self._summary_graphs(body, players)
        else:
            if tab == 0:
                cols = [("Pontszám", lambda q: self._score(q)[0]), ("Katonai", lambda q: self._score(q)[1]),
                        ("Gazdasági", lambda q: self._score(q)[2]), ("Építkezés", lambda q: self._score(q)[3]),
                        ("Eredmény", outcome)]
            elif tab == 1:
                cols = [(p.res_name(kk, rd) if kk != "faith" else "Áhítat", (lambda q, kk=kk: int(q.gathered.get(kk, 0))))
                        for kk in ("wood", "stone", "meteor")]
                if any(q.gathered.get("faith") for q in players):
                    cols.append(("Áhítat", lambda q: int(q.gathered.get("faith", 0))))
                cols += [("Összes gyűjtött", lambda q: int(sum(q.gathered.values()))),
                         ("Elköltött", lambda q: int(sum(v for kk, v in getattr(q, "spent", {}).items()
                                                          if kk != "faith")))]
            else:
                st = lambda q, key: (getattr(q, "stats", {}) or {}).get(key, 0)  # noqa: E731
                cols = [("Kiképzett", lambda q: q.units_made), ("Megölt", lambda q: st(q, "units_killed")),
                        ("Elesett", lambda q: st(q, "units_lost")), ("Épített", lambda q: st(q, "buildings_built")),
                        ("Lerombolt", lambda q: st(q, "buildings_killed")),
                        ("Elvesztett ép.", lambda q: st(q, "buildings_lost"))]
            self._summary_table(body, players, cols)
            if tab == 0 and g.director is not None and won:
                story = " ".join(g.director.victory_text())
                if story:
                    yy = body.y + k(48) + len(players) * k(40) + k(16)
                    self.wrap(story, body.x, yy, body.w, F["small"], (220, 214, 196), max_lines=5)
        # gomb
        r = pygame.Rect(0, 0, k(200), k(46))
        r.bottomright = (P.right - k(20), P.bottom - k(16))
        self._res_button(r, "FŐMENÜ", r.collidepoint(mx, my), primary=True)
        self.result_btns = [(r, "menu")]

    def _summary_table(self, body, players, cols):
        s, F = self.screen, self.fonts
        u = self.u
        k = lambda v: int(round(v * u))  # noqa: E731
        name_w = int(body.w * 0.34)
        cw = (body.w - name_w) // max(1, len(cols))
        y = body.y
        for i, (name, _) in enumerate(cols):
            img = F["small"].render(self.ellipsize(name, F["small"], cw - k(8)), True, (230, 200, 130))
            s.blit(img, (body.x + name_w + i * cw + cw - img.get_width() - k(8), y))
        y += k(34)
        vals = [[fn(q) for _, fn in cols] for q in players]
        best = []
        for ci in range(len(cols)):
            nums = [v[ci] for v in vals if isinstance(v[ci], (int, float))]
            best.append(max(nums) if nums else None)
        for q, row in zip(players, vals):
            rr = pygame.Rect(body.x, y - k(4), body.w, k(36))
            bg = pygame.Surface(rr.size, pygame.SRCALPHA)
            bg.fill((40, 34, 26, 150) if q is self.p else (24, 22, 18, 120))
            s.blit(bg, rr.topleft)
            pygame.draw.rect(s, q.color, (body.x + k(6), y + k(4), k(18), k(18)))
            nm = f"{q.name} – {q.faction['name']}"
            s.blit(F["small"].render(self.ellipsize(nm, F["small"], name_w - k(40)), True, (236, 228, 210)),
                   (body.x + k(32), y + k(2)))
            for ci, v in enumerate(row):
                hi = best[ci] is not None and v == best[ci] and v
                img = F["small"].render(str(v), True, (255, 220, 120) if hi else (220, 214, 200))
                s.blit(img, (body.x + name_w + ci * cw + cw - img.get_width() - k(8), y + k(2)))
            y += k(40)

    def _summary_graphs(self, body, players):
        """Két grafikon: a sereg értéke és az összes gyűjtött nyersanyag az idő függvényében."""
        s, F = self.screen, self.fonts
        u = self.u
        k = lambda v: int(round(v * u))  # noqa: E731
        gw = (body.w - k(24)) // 2
        for gi, (title, idx) in enumerate((("A sereg értéke", 2), ("Összes gyűjtött nyersanyag", 3))):
            r = pygame.Rect(body.x + gi * (gw + k(24)), body.y + k(30), gw, body.h - k(40))
            s.blit(F["small"].render(title, True, (230, 200, 130)), (r.x, body.y))
            pygame.draw.rect(s, (20, 18, 15), r)
            pygame.draw.rect(s, (110, 92, 60), r, 1)
            series = [[(row[0], row[idx]) for row in getattr(q, "timeline", [])] for q in players]
            tmax = max([pt[0] for ser in series for pt in ser] + [1])
            vmax = max([pt[1] for ser in series for pt in ser] + [1])
            for f in (0.25, 0.5, 0.75):  # rácsvonalak
                yy = r.bottom - int(r.h * f)
                pygame.draw.line(s, (40, 36, 30), (r.x + 1, yy), (r.right - 2, yy))
            lab = F["tiny"].render(str(int(vmax)), True, (170, 160, 140))
            s.blit(lab, (r.x + k(4), r.y + k(4)))
            for q, ser in zip(players, series):
                if len(ser) < 2:
                    continue
                pts = [(r.x + k(4) + (r.w - k(8)) * t / tmax, r.bottom - k(4) - (r.h - k(12)) * v / vmax) for t, v in ser]
                pygame.draw.lines(s, q.color, False, pts, max(2, k(2)))
            mins = F["tiny"].render(f"{int(tmax // 60)} perc", True, (170, 160, 140))
            s.blit(mins, (r.right - mins.get_width() - k(4), r.bottom - mins.get_height() - k(4)))


# ---------------------------------------------------------------------------
# parancskártya-ikonok: egyszerű, festett hatású vektoros glifák
_GLYPH_COL = {
    "move": (120, 220, 120), "stop": (230, 90, 80), "hold": (220, 200, 120), "attack": (240, 120, 90),
    "blink": (120, 230, 255), "aim": (120, 230, 180), "spear": (230, 220, 170), "dive": (180, 200, 255),
    "roar": (240, 200, 90), "deploy": (200, 170, 120), "spearwall": (210, 190, 140), "harpoon": (200, 200, 210),
    "rage": (255, 90, 70), "rune": (130, 200, 255), "stomp": (200, 160, 110), "burrow": (170, 120, 80),
    "spines": (230, 230, 200), "acid": (150, 255, 90), "sweep": (200, 110, 190), "charge": (255, 170, 80),
    "song": (255, 220, 140), "axe": (220, 200, 180), "lightning": (200, 220, 255), "shield": (170, 190, 220),
    "burst": (255, 200, 110), "rain": (150, 210, 120), "bomb": (255, 160, 90), "bolt": (200, 170, 255),
    "network": (180, 230, 140), "repair": (230, 200, 120), "anchor": (120, 255, 200), "split": (170, 240, 110),
    "forest": (90, 180, 90), "eye": (200, 120, 230), "ghost": (160, 230, 255), "cocoon": (210, 150, 190),
    "tent": (210, 170, 120), "pack": (200, 160, 110), "heal": (120, 255, 140), "summon": (180, 200, 255),
    "merge": (190, 220, 130), "house": (210, 180, 130), "aura": (255, 230, 150), "learn": (255, 220, 110),
    "back": (200, 200, 200),
}


def draw_glyph_icon(kind, size, passive=False):
    S = size * 3
    surf = pygame.Surface((S, S), pygame.SRCALPHA)
    col = _GLYPH_COL.get(kind, (220, 210, 190))
    for k in range(S):  # sötét, enyhén színezett háttér
        f = k / S
        gdraw.line(surf, (int(18 + 26 * (1 - f)), int(18 + 22 * (1 - f)), int(22 + 20 * (1 - f))), (0, k), (S, k))
    glow = pygame.Surface((S, S), pygame.SRCALPHA)
    gdraw.circle(glow, col + (40,), (S // 2, S // 2), S // 2 - 4)
    surf.blit(glow, (0, 0))
    c, m = S // 2, S // 2
    w = max(4, S // 14)
    dark = tuple(max(0, v // 3) for v in col)

    def poly(pts, fill=col):
        gdraw.polygon(surf, dark, [(x + 3, y + 3) for x, y in pts])
        gdraw.polygon(surf, fill, pts)

    def line(a, b, ww=w, fill=col):
        gdraw.line(surf, dark, (a[0] + 3, a[1] + 3), (b[0] + 3, b[1] + 3), ww)
        gdraw.line(surf, fill, a, b, ww)

    def circ(x, y, r, fill=col, wd=0):
        gdraw.circle(surf, dark, (x + 3, y + 3), r, wd)
        gdraw.circle(surf, fill, (x, y), r, wd)

    q = S / 10
    if kind == "move":
        poly([(2 * q, 5 * q), (6 * q, 2 * q), (6 * q, 4 * q), (8.5 * q, 4 * q), (8.5 * q, 6 * q), (6 * q, 6 * q), (6 * q, 8 * q)])
    elif kind == "stop":
        poly([(3 * q, 3 * q), (7 * q, 3 * q), (7 * q, 7 * q), (3 * q, 7 * q)])
    elif kind in ("hold", "shield", "spearwall"):
        poly([(c, 1.5 * q), (8 * q, 3 * q), (7.5 * q, 6 * q), (c, 8.7 * q), (2.5 * q, 6 * q), (2 * q, 3 * q)])
        if kind == "spearwall":
            for k in range(3):
                line((3.5 * q + k * 1.5 * q, 8.5 * q), (3.5 * q + k * 1.5 * q, 0.8 * q), w // 2, (240, 230, 200))
    elif kind in ("attack", "axe"):
        line((2.5 * q, 7.5 * q), (7.5 * q, 2.5 * q), w + 2)
        line((3.2 * q, 5.2 * q), (4.8 * q, 6.8 * q), w)
        if kind == "axe":
            poly([(6.2 * q, 1.8 * q), (8.6 * q, 2.6 * q), (7.6 * q, 4.6 * q)])
    elif kind in ("blink", "dive", "charge"):
        for k in range(3):
            line((2 * q + k * q, 7.5 * q - k * 0.4 * q), (6.5 * q + k * q, 3 * q - k * 0.4 * q), w // 2 + 1)
        poly([(6.5 * q, 1.5 * q), (8.8 * q, 2 * q), (8 * q, 4.2 * q)])
    elif kind in ("aim", "eye"):
        circ(c, m, int(3.2 * q), col, w // 2 + 1)
        circ(c, m, int(1.2 * q))
        if kind == "aim":
            for a, b in (((c, 0.8 * q), (c, 2.4 * q)), ((c, 7.6 * q), (c, 9.2 * q)), ((0.8 * q, m), (2.4 * q, m)),
                         ((7.6 * q, m), (9.2 * q, m))):
                line(a, b, w // 2)
    elif kind in ("spear", "harpoon", "bolt"):
        line((2 * q, 8 * q), (7.5 * q, 2.5 * q), w // 2 + 2)
        poly([(7 * q, 1.5 * q), (8.8 * q, 1.2 * q), (8.5 * q, 3 * q)])
    elif kind in ("roar", "song"):
        for k in range(3):
            gdraw.arc(surf, col, (2 * q - k * q, 2 * q - k * q, 6 * q + 2 * k * q, 6 * q + 2 * k * q), -0.9, 0.9,
                            w // 2 + 1)
        circ(int(3 * q), m, int(1.4 * q))
    elif kind in ("deploy", "tent", "house", "pack"):
        poly([(c, 1.8 * q), (8.5 * q, 7.5 * q), (1.5 * q, 7.5 * q)])
        poly([(4.3 * q, 7.5 * q), (c, 5 * q), (5.7 * q, 7.5 * q)], dark)
    elif kind in ("rage", "burst", "bomb", "stomp", "lightning"):
        pts = []
        for k in range(16):
            a = k * math.pi / 8
            r = (4.2 if k % 2 == 0 else 2.0) * q
            pts.append((c + math.cos(a) * r, m + math.sin(a) * r))
        poly(pts)
    elif kind == "rune":
        poly([(c, 1.2 * q), (7.5 * q, 3 * q), (7 * q, 8.6 * q), (3 * q, 8.6 * q), (2.5 * q, 3 * q)], (150, 150, 160))
        line((c, 2.5 * q), (c, 7.5 * q), w // 2, col)
        line((c, 4 * q), (6 * q, 3 * q), w // 2, col)
        line((c, 5.5 * q), (4 * q, 4.5 * q), w // 2, col)
    elif kind == "burrow":
        line((1 * q, 7 * q), (9 * q, 7 * q), w, (140, 100, 60))
        poly([(c, 6.6 * q), (3 * q, 2.5 * q), (4.3 * q, 2.5 * q), (4.3 * q, 1 * q), (5.7 * q, 1 * q), (5.7 * q, 2.5 * q),
              (7 * q, 2.5 * q)])
    elif kind in ("spines", "rain", "acid"):
        for k in range(5):
            x = 2 * q + k * 1.5 * q
            line((x, 2 * q + (k % 2) * q), (x - 0.6 * q, 7.5 * q), w // 2 + 1)
        if kind == "acid":
            circ(c, int(7.6 * q), int(1.4 * q))
    elif kind == "sweep":
        gdraw.arc(surf, col, (1.5 * q, 1.5 * q, 7 * q, 7 * q), 0.3, 5.4, w)
    elif kind in ("heal", "learn"):
        poly([(4 * q, 1.5 * q), (6 * q, 1.5 * q), (6 * q, 4 * q), (8.5 * q, 4 * q), (8.5 * q, 6 * q), (6 * q, 6 * q),
              (6 * q, 8.5 * q), (4 * q, 8.5 * q), (4 * q, 6 * q), (1.5 * q, 6 * q), (1.5 * q, 4 * q), (4 * q, 4 * q)])
    elif kind in ("summon", "ghost", "pack"):
        circ(c, int(3.2 * q), int(1.6 * q))
        poly([(2.5 * q, 8.6 * q), (3.5 * q, 5 * q), (6.5 * q, 5 * q), (7.5 * q, 8.6 * q)])
    elif kind in ("split", "merge"):
        circ(int(3.5 * q), m, int(2.2 * q))
        circ(int(6.6 * q), m, int(2.2 * q), tuple(min(255, v + 40) for v in col))
    elif kind in ("network", "forest"):
        if kind == "forest":
            for k in range(3):
                x = 2.5 * q + k * 2.5 * q
                poly([(x, 2 * q + (k % 2) * q), (x + 1.8 * q, 7.5 * q), (x - 1.8 * q, 7.5 * q)])
        else:
            pts = [(2 * q, 3 * q), (c, 2 * q), (8 * q, 4 * q), (6.5 * q, 8 * q), (3 * q, 7.5 * q)]
            for a in pts:
                for b in pts:
                    line(a, b, 2)
            for a in pts:
                circ(int(a[0]), int(a[1]), int(0.9 * q))
    elif kind == "repair":
        line((2.5 * q, 7.5 * q), (6.5 * q, 3.5 * q), w + 1)
        circ(int(7 * q), int(3 * q), int(1.6 * q), col, w // 2 + 1)
    elif kind == "anchor":
        line((c, 1.8 * q), (c, 8 * q), w)
        gdraw.arc(surf, col, (2 * q, 3.5 * q, 6 * q, 5 * q), math.pi, 2 * math.pi, w)
        circ(c, int(1.6 * q), int(0.9 * q), col, 3)
    elif kind == "cocoon":
        gdraw.ellipse(surf, col, (3 * q, 1.5 * q, 4 * q, 7 * q))
    elif kind == "aura":
        for k in range(3):
            circ(c, m, int((1.2 + k * 1.3) * q), col, 3)
    elif kind == "back":
        poly([(7 * q, 2 * q), (7 * q, 8 * q), (2.5 * q, 5 * q)])
    else:
        circ(c, m, int(3 * q))
    img = pygame.transform.smoothscale(surf, (size, size))
    if passive:
        img.fill((150, 150, 150, 255), special_flags=pygame.BLEND_RGBA_MULT)
    return img
