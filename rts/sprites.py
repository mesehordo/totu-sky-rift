"""Sprite-ok betöltése és kiszolgálása.

Az assets/sprites/manifest.json alapján automatikusan betölti az egységek,
épületek, nyersanyagok, lövedékek, effektek, csempék és ikonok sheet-jeit.
A csapatszín külön maszkból (…_team.png) kerül rá játékosszínenként, egyszer
kiszámolva és gyorsítótárazva. Amihez nincs sprite (pl. egy mod új egysége),
arra a None visszatérés jelzi a UI-nak, hogy a procedurális rajzolást használja.
"""
import json
import math
import os

import pygame

from . import calm, canvas
from .data import ROOT
from .fonts import load_font

ASSET_DIR = os.path.join(ROOT, "assets", "sprites")
# A grafika HD-ben (2×) készül; a retró (1×) változatot a betöltő állítja elő kicsinyítéssel.
PACK_DIR = os.path.join(ROOT, "assets", "pack")   # importált, kézzel festett csomag (tools/import_pack.py)


def dir_index(angle):
    """Szög (radián, képernyő-koordináták) -> 0..7 (K, DK, D, DNy, Ny, ÉNy, É, ÉK)."""
    return int(round(angle / (math.pi / 4))) % 8


def tint(team_surf, color):
    t = team_surf.copy()
    t.fill(tuple(color) + (255,), special_flags=pygame.BLEND_RGBA_MULT)
    t.blit(t.copy(), (0, 0), special_flags=pygame.BLEND_RGB_ADD)  # világosítás (szürke*szín*2)
    return t


def _down(img, k):
    """HD kép retró (1/k) méretre."""
    return pygame.transform.smoothscale(img, (max(1, img.get_width() // k), max(1, img.get_height() // k)))


class Sheet:
    def __init__(self, meta_path, retro=False, target=None):
        base = os.path.dirname(meta_path)
        with open(meta_path, encoding="utf-8") as f:
            self.meta = m = json.load(f)
        self.image = pygame.image.load(os.path.join(base, m["image"])).convert_alpha()
        tm = m.get("team_mask")
        self.team = pygame.image.load(os.path.join(base, tm)).convert_alpha() if tm else None
        self.fw, self.fh = m.get("frame_size", m.get("tile_size", (32, 32)))
        self.anims = m.get("animations", {})
        self.scale = int(m.get("scale", 1) or 1)   # HD: 2 – a képek k-szorosak, a horgony világ-képpontban
        self.anchor_px = tuple(m.get("anchor") or m.get("footprint_offset") or (0, 0))
        if retro and self.scale > 1:  # retró mód: a HD képlapból kicsinyítve
            k = self.scale
            self.image = _down(self.image, k)
            if self.team is not None:
                self.team = _down(self.team, k)
            self.fw, self.fh = self.fw // k, self.fh // k
            self.anchor_px = tuple(v / k for v in self.anchor_px)
            self.scale = 1
        if self.fw <= 0 or self.fh <= 0:
            raise ValueError("érvénytelen képkockaméret")
        self.cols = self.image.get_width() // self.fw
        self.rows = self.image.get_height() // self.fh
        if not self.cols or not self.rows:
            raise ValueError("a képlapon nincs teljes képkocka")
        # célméret (kamera-távolság): a kockák egyszer, jó minőségben kicsinyítve (nem képkockánként)
        self.fx = 1.0
        if target and not retro and abs(target - self.scale) > 1e-6:
            self.fx = target / self.scale
            self.anchor_px = tuple(v * self.fx for v in self.anchor_px)
            self.scale = target
        self.anchor = tuple(v / self.scale for v in self.anchor_px)
        self._colored = {}
        self._frames = {}
        self._calm = {}
        self.calm = calm.applies(meta_path)   # játéktér-figura: futás közben „nyugodt” feldolgozást kap
        self.top = None

    def surface(self, color):
        if color is None or self.team is None:
            return self.image
        key = tuple(color)
        s = self._colored.get(key)
        if s is None:
            s = self.image.copy()
            if self.meta.get("team_colour_mode") == "shade":
                layer = self.image.copy()
                layer.fill(key + (255,), special_flags=pygame.BLEND_RGBA_MULT)
                layer.blit(self.team, (0, 0), special_flags=pygame.BLEND_RGBA_MULT)
            else:
                layer = tint(self.team, color)
            s.blit(layer, (0, 0))
            self._colored[key] = s
        return s

    def frame(self, color, col, row=0):
        col = max(0, min(self.cols - 1, col))
        row = max(0, min(self.rows - 1, row))
        key = (tuple(color) if color else None, col, row)
        f = self._frames.get(key)
        if f is None:
            f = self.surface(color).subsurface((col * self.fw, row * self.fh, self.fw, self.fh))
            if self.fx != 1.0:
                f = pygame.transform.smoothscale(f, (max(1, round(self.fw * self.fx)), max(1, round(self.fh * self.fx))))
            if self.scale > 1:
                canvas.mark_hd(f)
            self._frames[key] = f
        if self.calm and calm.ENABLED:
            c = calm.get(self._calm, key, f)
            if c is not f and self.scale > 1:
                canvas.mark_hd(c)
            return c
        return f

    def anim_frame(self, color, anim, t, row=0, clamp=False):
        a = self.anims.get(anim) or self.anims.get("idle") or next(iter(self.anims.values()), None)
        if not a:
            return self.frame(color, 0, row)
        n = max(1, a["frames"])
        i = int(t * a.get("fps", 8))
        i = min(i, n - 1) if (clamp or not a.get("loop", True)) else i % n
        return self.frame(color, a["start"] + i, row)

    def top_extent(self):
        """A horgony feletti látható magasság (életerő-csík elhelyezéséhez)."""
        if self.top is None:
            row = min(2, max(0, self.image.get_height() // self.fh - 1))
            r = self.frame(None, self.anims.get("idle", {"start": 0})["start"], row).get_bounding_rect()
            self.top = (self.anchor_px[1] - r.top if r.h else self.fh * self.fx // 2) / self.scale
        return self.top


_SRC = {}  # betöltött (natív HD) képlapok: lemezről csak egyszer


def _scaled_sheet(path, retro, target):
    """A képlap a kért célméretben. A natív lapot csak egyszer olvassa be; a többi méret ebből készül
    (a képkockák kicsinyítése lustán, kockánként – így a kamera-távolság váltása nem akad)."""
    import copy
    src = _SRC.get(path)
    if src is None:
        src = _SRC[path] = Sheet(path)
    if retro:
        return Sheet(path, retro=True)
    if not target or abs(target - src.scale) < 1e-6:
        return src
    sh = copy.copy(src)
    sh.fx = target / src.scale
    sh.anchor_px = tuple(v * sh.fx for v in src.anchor_px)
    sh.scale = target
    sh.anchor = tuple(v / sh.scale for v in sh.anchor_px)
    sh._frames, sh._calm, sh.top = {}, {}, None   # a színezett teljes lapok (_colored) közösek maradhatnak
    return sh


class SpriteBank:
    def __init__(self):
        self.ok = False
        self.manifest = {}
        self.cache = {}
        path = os.path.join(ASSET_DIR, "manifest.json")
        if os.path.isfile(path):
            with open(path, encoding="utf-8") as f:
                self.manifest = json.load(f)
            self.ok = True
        self.hd = False            # a UI kapcsolja (HD edition); HD-ben a sprites_hd előnyt élvez
        self.pack = {}
        pp = os.path.join(PACK_DIR, "manifest.json")
        if os.path.isfile(pp) and os.environ.get("RTS_PACK"):  # a festett csomag csak kérésre (RTS_PACK=1)
            with open(pp, encoding="utf-8") as f:
                self.pack = json.load(f)
        self.icons = {}
        self.scaled = {}
        self.aliases = {}  # definíció-id -> másik egység sprite-ja (pl. hősök)
        self.mission_backgrounds = {}
        catalog = os.path.join(ROOT, "assets", "ui", "campaign", "MISSION_BACKGROUNDS.json")
        try:
            with open(catalog, encoding="utf-8") as f:
                for entry in json.load(f)["entries"]:
                    path = entry["game_path"]
                    if path.startswith("assets/ui/campaign/missions/") and path.endswith(".png"):
                        self.mission_backgrounds[entry["mission_id"]] = path[7:-4]
        except (OSError, ValueError, KeyError, TypeError):
            pass

    def has_hd(self):
        return bool(self.manifest)

    def _sheet(self, cat, key):
        ck = (cat, key, self.hd)
        if ck in self.cache:
            return self.cache[ck]
        for manifest, base in ((self.pack, PACK_DIR), (self.manifest, ASSET_DIR)):
            entry = manifest.get(cat, {}).get(key)
            if not entry:
                continue
            try:
                rel = entry["meta"] if isinstance(entry, dict) else entry
                path = os.path.join(base, rel)
                if os.path.isfile(path):
                    sheet = _scaled_sheet(path, not self.hd, self.hd or None)
                    self.cache[ck] = sheet
                    return sheet
            except (pygame.error, OSError, KeyError, TypeError, ValueError) as e:
                print(f"[sprite] nem tölthető: {key}: {e}")
        self.cache[ck] = None
        return None

    def unit(self, uid):
        return self._sheet("units", uid) or (self._sheet("units", self.aliases[uid]) if uid in self.aliases else None)

    def building(self, bid):
        return self._sheet("buildings", bid) or (self._sheet("buildings", self.aliases[bid]) if bid in self.aliases else None)

    def resource(self, kind):
        return self._sheet("resources", kind)

    def projectile(self, kind):
        return self._sheet("projectiles", kind)

    def effect(self, kind):
        return self._sheet("effects", kind)

    def tiles(self, theme=None):
        return (self.themed("tiles", "tileset", theme) if theme else None) or self._sheet("tiles", "tileset")

    def themed(self, cat, base, theme):
        """Tájhoz saját képlap (pl. tiles/tileset_winter.json, props/doodads_swamp.json, props/decals_forest.json),
        ha van; különben None. A manifestben nem kell szerepelnie: a fájl megléte elég."""
        key = f"{base}_{theme}"
        ck = (cat, key, self.hd)
        if ck in self.cache:
            return self.cache[ck]
        sheet = self._sheet(cat, key)
        if sheet is None:
            p = os.path.join(ASSET_DIR, cat, key + ".json")
            if os.path.isfile(p):
                try:
                    sheet = _scaled_sheet(p, not self.hd, self.hd or None)
                except (pygame.error, OSError, KeyError, ValueError) as e:
                    print(f"[sprite] nem tölthető: {key}: {e}")
            self.cache[ck] = sheet
        return sheet

    def masks(self):
        return self._sheet("tiles", "masks")

    def prop(self, kind):
        return self._sheet("props", kind)

    def icon(self, did, color):
        """Az egység / épület ikonja mindenhol (gyártósor, kijelölés, karakterlap): az 1:1-es portré
        (assets/portraits/<id>.png), a csapatszín-maszkkal (<id>_csapatszin.png) a játékos színére festve.
        A régi, fekvő ikonokat (sprites/icons) már nem használja; ha nincs portré, None (a hívó kódból rajzol)."""
        pdir = os.path.join(ROOT, "assets", "portraits")
        if not os.path.isfile(os.path.join(pdir, did + ".png")) and did in self.aliases:
            did = self.aliases[did]
        key = (did, tuple(color), "portre")
        if key in self.icons:
            return self.icons[key]
        img = None
        p = os.path.join(pdir, did + ".png")
        if os.path.isfile(p):
            try:
                img = pygame.image.load(p).convert_alpha()
                tp = os.path.join(pdir, did + "_csapatszin.png")
                if os.path.isfile(tp):
                    img.blit(tint(pygame.image.load(tp).convert_alpha(), color), (0, 0))
            except (pygame.error, OSError):
                img = None
        self.icons[key] = img
        return img

    def art(self, rel, size=None):
        """Kézzel rajzolt kiegészítő kép (assets/<rel>.png), ha létezik; különben None.
        Ezek: arcképek (portraits/<egység>), képesség-ikonok (abilities/<képesség>), parancsikonok
        (ui/icons/<fajta>), irány-címerek (paths/<faj>_<irány>)."""
        key = ("art", rel, size)
        if key in self.cache:
            return self.cache[key]
        p = os.path.join(ROOT, "assets", rel + ".png")
        img = None
        if os.path.isfile(p):
            try:
                img = pygame.image.load(p).convert_alpha()
                if size and img.get_size() != tuple(size):
                    img = pygame.transform.smoothscale(img, tuple(size))
            except pygame.error:
                img = None
        self.cache[key] = img
        return img

    def research_art(self, rid):
        """Kutatás-ikon. A szintes kutatások II. és III. foka (…_2, …_3), ha nincs saját képe, az alapkutatás ikonját
        kapja, a jobb alsó sarkában római számmal (a négyzetre vágott közepén belül, hogy a parancsrácson is látsszon)."""
        img = self.art("research/" + rid)
        if img is not None:
            return img
        if len(rid) > 2 and rid[-2] == "_" and rid[-1] in "23":
            key = ("tier", rid)
            if key in self.cache:
                return self.cache[key]
            base = self.art("research/" + rid[:-2])
            out = None
            if base is not None:
                out = base.copy()
                w, h = out.get_size()
                m = min(w, h)
                x1, y1 = (w + m) // 2, (h + m) // 2          # a középső négyzet jobb alsó sarka
                txt = "II" if rid[-1] == "2" else "III"
                fs = max(10, int(m * 0.42))
                f = load_font(pygame, fs, bold=True)
                t = f.render(txt, True, (255, 226, 140))
                bw, bh = t.get_width() + int(m * 0.12), t.get_height() + int(m * 0.04)
                r = pygame.Rect(x1 - bw - int(m * 0.04), y1 - bh - int(m * 0.04), bw, bh)
                plate = pygame.Surface(r.size, pygame.SRCALPHA)
                plate.fill((18, 14, 10, 225))
                out.blit(plate, r)
                pygame.draw.rect(out, (214, 170, 80), r, max(1, m // 48))
                sh = f.render(txt, True, (20, 12, 6))
                out.blit(sh, (r.centerx - t.get_width() // 2 + 1, r.centery - t.get_height() // 2 + 2))
                out.blit(t, (r.centerx - t.get_width() // 2, r.centery - t.get_height() // 2 + 1))
            self.cache[key] = out
            return out
        return None

    def portrait(self, uid, size=None):
        return self.art("portraits/" + uid, size)

    def cover(self, rel, size):
        key = ("cover", rel, tuple(size))
        if key not in self.cache:
            image = self.art(rel)
            result = None
            if image:
                width, height = size
                scale = max(width / image.get_width(), height / image.get_height())
                scaled = pygame.transform.smoothscale(
                    image, (math.ceil(image.get_width() * scale),
                            math.ceil(image.get_height() * scale)))
                result = pygame.Surface(size)
                result.blit(scaled, ((width - scaled.get_width()) // 2,
                                     (height - scaled.get_height()) // 2))
            self.cache[key] = result
        return self.cache[key]

    def mission_cover(self, mission_id, size):
        rel = self.mission_backgrounds.get(mission_id)
        return self.cover(rel, size) if rel else None

    def item_icon(self, item_id, size=48):
        """Tárgy-ikon: assets/items/<tárgy>.png (keretezett, 48 px)."""
        key = ("item", item_id, size)
        if key in self.cache:
            return self.cache[key]
        p = os.path.join(ROOT, "assets", "items", item_id + ".png")
        img = None
        if os.path.isfile(p):
            try:
                img = pygame.image.load(p).convert_alpha()
                if size != img.get_width():
                    img = pygame.transform.smoothscale(img, (size, size))
            except pygame.error:
                img = None
        self.cache[key] = img
        return img

    def emblem(self, faction, size=128):
        """Faj-címer: assets/emblems/<faj>.png, vagy a faj 'emblem' mezőjében megadott kép (modokhoz)."""
        fid = faction["id"] if isinstance(faction, dict) else faction
        key = ("emblem", fid, size)
        if key in self.cache:
            return self.cache[key]
        cands = []
        if isinstance(faction, dict) and faction.get("emblem"):
            cands.append(os.path.join(ROOT, faction["emblem"]))
        cands.append(os.path.join(ROOT, "assets", "emblems", fid + ".png"))
        img = None
        for p in cands:
            if os.path.isfile(p):
                try:
                    img = pygame.image.load(p).convert_alpha()
                    img = pygame.transform.smoothscale(img, (size, size))
                    break
                except pygame.error:
                    img = None
        self.cache[key] = img
        return img

    def scaled_effect(self, kind, frame_surf, size):
        size = (size, size) if isinstance(size, int) else tuple(size)
        key = (kind, frame_surf, size)
        image = self.scaled.get(key)
        if image is None:
            image = pygame.transform.smoothscale(frame_surf, size)
            if canvas.is_hd(frame_surf):
                canvas.mark_hd(image)
            if len(self.scaled) >= 400:
                self.scaled.clear()
            self.scaled[key] = image
        return image
