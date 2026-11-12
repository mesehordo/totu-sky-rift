"""Fajonkénti kezelőfelület: oldalsáv-textúra, keretszínek, díszítések és egérkurzorok.

Ha kézzel rajzolt kép van, azt használja:
    assets/ui/sidebar_<faj>.png            – az oldalsáv háttere (a sáv méretére nyújtva)
    assets/ui/cursors/<faj>_<fajta>.png    – kurzor 32×32 (fajták: arrow, select, attack, cast, build, gather)
Különben kódból rajzolja a faj stílusában.
"""
import math
import os

import numpy as np
import pygame

from .data import ROOT

# alap (r, g, b) a textúrához, keret világos/sötét, kiemelés (aktív fül), díszítés fajtája
THEMES = {
    "elves": {"base": (150, 128, 92), "light": (226, 190, 96), "dark": (84, 64, 30), "accent": (40, 170, 140),
              "fill": (40, 36, 28), "tab": (120, 98, 44), "orn": "sun", "pattern": "blocks"},
    "ursan": {"base": (92, 62, 40), "light": (150, 160, 176), "dark": (40, 28, 18), "accent": (120, 180, 255),
              "fill": (34, 26, 20), "tab": (96, 70, 40), "orn": "paw", "pattern": "planks"},
    "croftian": {"base": (66, 38, 80), "light": (226, 214, 186), "dark": (30, 12, 36), "accent": (150, 255, 120),
                 "fill": (30, 18, 36), "tab": (92, 50, 110), "orn": "spikes", "pattern": "veins"},
    "wullof": {"base": (140, 74, 46), "light": (236, 226, 206), "dark": (60, 30, 18), "accent": (214, 150, 72),
               "fill": (40, 24, 18), "tab": (130, 70, 40), "orn": "dots", "pattern": "dots"},
    "mycel": {"base": (64, 70, 52), "light": (226, 214, 186), "dark": (28, 30, 22), "accent": (80, 230, 205),
              "fill": (26, 30, 24), "tab": (70, 84, 60), "orn": "spores", "pattern": "threads"},
    "hukucha": {"base": (54, 96, 84), "light": (214, 226, 200), "dark": (22, 44, 40), "accent": (120, 230, 210),
                "fill": (22, 34, 32), "tab": (60, 110, 90), "orn": "dots", "pattern": "veins"},
    "ooni": {"base": (118, 40, 36), "light": (226, 190, 110), "dark": (44, 14, 12), "accent": (80, 200, 140),
             "fill": (34, 18, 16), "tab": (130, 50, 40), "orn": "rivets", "pattern": "planks"},
    "imperium": {"base": (120, 116, 110), "light": (196, 150, 80), "dark": (50, 44, 40), "accent": (120, 255, 190),
                 "fill": (34, 34, 36), "tab": (110, 90, 52), "orn": "rivets", "pattern": "marble"},
}
NEUTRAL = {"base": (90, 92, 98), "light": (196, 204, 214), "dark": (52, 56, 64), "accent": (110, 220, 255),
           "fill": (40, 42, 46), "tab": (80, 84, 92), "orn": "rivets", "pattern": "blocks"}
DEFAULT = {"base": (90, 86, 78), "light": (176, 138, 74), "dark": (60, 42, 22), "accent": (230, 190, 90),
           "fill": (42, 44, 48), "tab": (95, 80, 40), "orn": "rivets", "pattern": "blocks"}


def theme(fid):
    if fid == "neutral":
        return NEUTRAL
    return THEMES.get(fid, DEFAULT)


def _art(rel):
    p = os.path.join(ROOT, "assets", rel)
    if os.path.isfile(p):
        try:
            return pygame.image.load(p).convert_alpha()
        except pygame.error:
            return None
    return None


# ---------------------------------------------------------------- oldalsáv
def sidebar(fid, w, h):
    art = _art(f"ui/sidebar_{fid}.png")
    if art is not None:
        return pygame.transform.smoothscale(art, (w, h))
    t = theme(fid)
    surf = _texture(fid, w, h)
    light, dark = t["light"], t["dark"]
    for k, col in ((0, dark), (1, light), (2, tuple(int(c * 0.7) for c in light)), (3, dark)):
        pygame.draw.line(surf, col, (k, 0), (k, h))
    _ornaments(surf, t, w, h)
    return surf


def hud_panel(fid, w, h):
    """Az alsó HUD egybefüggő sávja (assets/ui/hud_<faj>.png, ha megrajzoltad; különben a faj textúrája kerettel)."""
    art = _art(f"ui/hud_{fid}.png")
    if art is not None:
        return pygame.transform.smoothscale(art, (w, h))
    t = theme(fid)
    surf = _texture(fid, w, h)
    light, dark = t["light"], t["dark"]
    pygame.draw.rect(surf, (10, 8, 6), (0, 0, w, h), 1)
    for k, col in ((1, light), (2, tuple(int(c * 0.7) for c in light)), (3, dark)):
        pygame.draw.line(surf, col, (k, k), (w - 1 - k, k))
        pygame.draw.line(surf, col, (k, k), (k, h))
        pygame.draw.line(surf, col, (w - 1 - k, k), (w - 1 - k, h))
    acc = t["accent"]
    for x in range(40, w - 30, 120):  # visszafogott díszek a felső peremen
        pygame.draw.circle(surf, dark, (x, 3), 4)
        pygame.draw.circle(surf, light, (x, 2), 3)
        pygame.draw.circle(surf, acc, (x, 2), 1)
    return surf


_NS = {}


def nine_slice(rel, w, h, border):
    """9 szeletes keret (sarkok változatlanok, élek és közép nyújtva): assets/<rel>, ha van; különben None.
    border: a sarok mérete a célképen (a forráskép sarka a forrás szélességének negyede)."""
    key = (rel, w, h, border)
    if key in _NS:
        return _NS[key]
    art = _art(rel)
    out = None
    if art is not None and w > 2 * border and h > 2 * border:
        sw, sh = art.get_size()
        sb = sw // 4
        out = pygame.Surface((w, h), pygame.SRCALPHA)
        xs = [(0, sb, 0, border), (sb, sw - 2 * sb, border, w - 2 * border), (sw - sb, sb, w - border, border)]
        ys = [(0, sb, 0, border), (sb, sh - 2 * sb, border, h - 2 * border), (sh - sb, sb, h - border, border)]
        for sx, sww, dx, dw in xs:
            for sy, shh, dy, dh in ys:
                if sww > 0 and shh > 0 and dw > 0 and dh > 0:
                    piece = art.subsurface((sx, sy, sww, shh))
                    out.blit(pygame.transform.smoothscale(piece, (dw, dh)), (dx, dy))
    _NS[key] = out
    return out


def banner(fid, w, h, color, accent, emblem=None):
    """Keskeny, álló fajbanner (elválasztó): assets/ui/banner_<faj>.png, különben kódból rajzolt zászló a címerrel."""
    art = _art(f"ui/banner_{fid}.png")
    surf = pygame.Surface((w, h), pygame.SRCALPHA)
    if art is not None:
        surf.blit(pygame.transform.smoothscale(art, (w, h)), (0, 0))
        return surf
    t = theme(fid)
    cloth = tuple(int(c * 0.55) for c in color)
    tip = max(6, w // 2)
    pts = [(1, 3), (w - 2, 3), (w - 2, h - tip), (w // 2, h - 2), (1, h - tip)]
    pygame.draw.polygon(surf, cloth, pts)
    for y in range(6, h - tip, 6):  # szövetcsíkok
        pygame.draw.line(surf, tuple(int(c * 0.47) for c in color), (3, y), (w - 4, y))
    pygame.draw.polygon(surf, accent, pts, 2)
    pygame.draw.line(surf, t["light"], (0, 2), (w - 1, 2), 2)  # rúd
    if emblem is not None:
        e = pygame.transform.smoothscale(emblem, (w - 4, w - 4))
        surf.blit(e, (2, h // 2 - (w - 4) // 2 - tip // 2))
    else:
        pygame.draw.circle(surf, accent, (w // 2, h // 2 - tip // 2), max(3, w // 3), 2)
    return surf


def _texture(fid, w, h):
    t = theme(fid)
    rng = np.random.default_rng(sum(map(ord, fid)) + 7)
    n = rng.random((h // 4 + 2, w // 4 + 2))
    n = np.kron(n, np.ones((4, 4)))[:h, :w]
    fine = rng.random((h, w))
    v = 0.62 + n * 0.22 + fine * 0.12
    yy, xx = np.mgrid[0:h, 0:w]
    pat = t["pattern"]
    if pat == "planks":  # függőleges deszkák résekkel
        gap = (xx % 44) < 2
        v = np.where(gap, v * 0.45, v * (0.92 + 0.12 * np.sin(yy * 0.08 + (xx // 44) * 1.7)))
    elif pat == "blocks":  # faragott kőtömbök
        row = yy // 36
        mortar = ((yy % 36) < 2) | (((xx + (row % 2) * 40) % 80) < 2)
        v = np.where(mortar, v * 0.55, v)
    elif pat == "veins":  # organikus erek
        vein = np.abs(np.sin(xx * 0.05 + np.sin(yy * 0.03) * 2.5)) < 0.05
        v = np.where(vein, v * 1.35, v)
    elif pat == "dots":  # pöttyfestés körökben
        cx, cy = (xx % 60) - 30, (yy % 60) - 30
        ring = np.abs(np.hypot(cx, cy) - 16) < 2.2
        dot = ring & (((np.arctan2(cy, cx) * 4) % 1.0) < 0.5)
        v = np.where(dot, v * 1.8, v)
    elif pat == "threads":  # világító gombafonalak
        th = np.abs(np.sin(xx * 0.09 + yy * 0.02) * np.cos(yy * 0.05 - xx * 0.01)) < 0.03
        v = np.where(th, v * 1.6, v)
    elif pat == "marble":  # márványerezet + lemezvasalás
        vein = np.abs(np.sin((xx + yy * 0.6) * 0.04 + n * 3)) < 0.04
        v = np.where(vein, v * 1.25, v)
        v = np.where((yy % 96) < 2, v * 0.6, v)
    b = np.array(t["base"], float)
    arr = (v[..., None] * b[None, None, :]).clip(0, 255).astype(np.uint8)
    if pat == "threads":
        glow = np.abs(np.sin(xx * 0.09 + yy * 0.02) * np.cos(yy * 0.05 - xx * 0.01)) < 0.012
        arr[glow] = (np.array(t["accent"]) * 0.7).astype(np.uint8)
    return pygame.surfarray.make_surface(arr.transpose(1, 0, 2))


def _ornaments(surf, t, w, h):
    light, dark, acc = t["light"], t["dark"], t["accent"]
    orn = t["orn"]
    for y in range(24, h, 64):  # a bal oldali keret díszei
        if orn == "sun":
            pygame.draw.circle(surf, dark, (3, y + 1), 4)
            pygame.draw.circle(surf, light, (2, y), 3)
            pygame.draw.circle(surf, acc, (2, y), 1)
        elif orn == "paw":
            pygame.draw.circle(surf, dark, (3, y + 1), 3)
            pygame.draw.circle(surf, light, (2, y), 2)
            pygame.draw.line(surf, acc, (5, y - 6), (5, y + 6), 1)
        elif orn == "spikes":
            pygame.draw.polygon(surf, light, [(0, y - 4), (9, y), (0, y + 4)])
            pygame.draw.circle(surf, acc, (2, y), 1)
        elif orn == "dots":
            for k in range(3):
                pygame.draw.circle(surf, light, (3, y - 6 + k * 6), 1)
        elif orn == "spores":
            pygame.draw.circle(surf, dark, (4, y + 1), 4)
            pygame.draw.circle(surf, (170, 60, 50), (3, y), 3)
            pygame.draw.circle(surf, light, (2, y - 1), 1)
        else:
            pygame.draw.circle(surf, dark, (3, y + 1), 3)
            pygame.draw.circle(surf, light, (2, y), 2)
    for cy in (0, h):  # sarokdíszek
        sgn = 1 if cy == 0 else -1
        if orn == "sun":
            c = (16, cy + sgn * 16)
            for a in range(12):
                ang = a / 12 * 2 * math.pi
                pygame.draw.line(surf, light, c, (c[0] + math.cos(ang) * 15, c[1] + math.sin(ang) * 15), 2)
            pygame.draw.circle(surf, light, c, 8)
            pygame.draw.circle(surf, acc, c, 4)
        elif orn == "paw":
            c = (16, cy + sgn * 18)
            pygame.draw.ellipse(surf, dark, (c[0] - 9, c[1] - 6, 18, 14))
            pygame.draw.ellipse(surf, light, (c[0] - 8, c[1] - 5, 16, 12), 2)
            for k in range(4):
                pygame.draw.circle(surf, light, (c[0] - 9 + k * 6, c[1] - sgn * 10), 3)
        elif orn == "spikes":
            for k in range(4):
                x = 4 + k * 9
                pygame.draw.polygon(surf, light, [(x, cy), (x + 7, cy), (x + 3, cy + sgn * (22 - k * 4))])
            pygame.draw.circle(surf, acc, (14, cy + sgn * 10), 4)
        elif orn == "dots":
            c = (18, cy + sgn * 18)
            for r in (6, 11, 16):
                n = int(r * 1.4)
                for k in range(n):
                    ang = k / n * 2 * math.pi
                    pygame.draw.circle(surf, light if r != 11 else acc,
                                       (int(c[0] + math.cos(ang) * r), int(c[1] + math.sin(ang) * r)), 1)
        elif orn == "spores":
            c = (16, cy + sgn * 16)
            pygame.draw.ellipse(surf, (170, 60, 50), (c[0] - 14, c[1] - 7, 28, 14))
            for k in range(4):
                pygame.draw.circle(surf, light, (c[0] - 8 + k * 5, c[1] - 2 + (k % 2) * 3), 2)
            pygame.draw.circle(surf, acc, (c[0], c[1] + sgn * 10), 2)
        else:
            pts = [(0, cy), (34, cy), (22, cy + sgn * 8), (12, cy + sgn * 12), (6, cy + sgn * 26), (0, cy + sgn * 34)]
            pygame.draw.polygon(surf, tuple(int(c * 0.7) for c in light), pts)
            pygame.draw.polygon(surf, light, pts, 2)
            for k in range(3):
                pygame.draw.circle(surf, dark, (6 + k * 9, cy + sgn * 4), 2)
            pygame.draw.circle(surf, acc, (9, cy + sgn * 9), 3)


# ---------------------------------------------------------------- kurzorok
KINDS = ("arrow", "select", "attack", "cast", "build", "gather")
HOT = {"arrow": (1, 1), "select": (16, 16), "attack": (16, 16), "cast": (16, 16), "build": (4, 4), "gather": (4, 4)}


def _cursor_surface(fid, kind):
    t = theme(fid)
    s = pygame.Surface((32, 32), pygame.SRCALPHA)
    light, dark, acc = t["light"], t["dark"], t["accent"]
    outline = (12, 10, 8)
    if kind == "arrow":  # a faj saját nyila
        tip = [(1, 1), (1, 22), (6, 17), (10, 26), (14, 24), (10, 15), (17, 15)]
        if fid == "ursan":  # medvekarom
            tip = [(1, 1), (3, 20), (7, 15), (12, 24), (16, 21), (11, 13), (18, 12)]
        elif fid == "croftian":  # kitinkasza
            tip = [(1, 1), (2, 24), (7, 17), (16, 26), (12, 15), (20, 12)]
        elif fid == "wullof":  # lándzsahegy
            tip = [(1, 1), (5, 20), (9, 14), (15, 22), (17, 19), (11, 12), (20, 9)]
        pygame.draw.polygon(s, outline, [(x + 1, y + 1) for x, y in tip])
        pygame.draw.polygon(s, light, tip)
        pygame.draw.polygon(s, dark, tip, 1)
        pygame.draw.circle(s, acc, (4, 8), 2)
        if fid == "wullof":
            for k in range(3):
                pygame.draw.circle(s, (240, 236, 220), (5 + k * 2, 12 + k * 2), 1)
        elif fid == "mycel":
            pygame.draw.ellipse(s, (170, 60, 50), (0, 0, 10, 6))
        elif fid == "imperium":
            pygame.draw.circle(s, (120, 40, 34), (6, 12), 2)
    elif kind in ("select", "attack", "cast"):
        col = {"select": (110, 230, 110), "attack": (240, 70, 60), "cast": acc}[kind]
        pygame.draw.circle(s, outline, (16, 16), 12, 3)
        pygame.draw.circle(s, col, (16, 16), 11, 2)
        for a in range(4):
            ang = a * math.pi / 2 + (math.pi / 4 if kind == "select" else 0)
            p0 = (16 + math.cos(ang) * 6, 16 + math.sin(ang) * 6)
            p1 = (16 + math.cos(ang) * 15, 16 + math.sin(ang) * 15)
            pygame.draw.line(s, outline, p0, p1, 4)
            pygame.draw.line(s, light if kind == "select" else col, p0, p1, 2)
        if kind == "cast":
            pygame.draw.circle(s, light, (16, 16), 3)
        if kind == "attack":
            pygame.draw.circle(s, col, (16, 16), 2)
    elif kind == "build":  # kalapács
        pygame.draw.line(s, outline, (6, 26), (22, 10), 5)
        pygame.draw.line(s, (130, 92, 54), (6, 26), (22, 10), 3)
        pygame.draw.polygon(s, outline, [(14, 4), (26, 16), (30, 12), (18, 0)])
        pygame.draw.polygon(s, light, [(15, 4), (26, 15), (29, 12), (18, 1)])
    elif kind == "gather":  # csákány
        pygame.draw.line(s, outline, (6, 28), (24, 6), 5)
        pygame.draw.line(s, (130, 92, 54), (6, 28), (24, 6), 3)
        pygame.draw.arc(s, outline, (8, 0, 26, 18), 0.2, 2.9, 5)
        pygame.draw.arc(s, light, (8, 0, 26, 18), 0.2, 2.9, 3)
    return s


_CACHE = {}
# a festett készlet (assets/ui/cursors/<fajta>.png, 64×64) fajtái; ha valamelyik hiányzik, ennek a kódból rajzolt
# megfelelője áll be
FALLBACK = {"select_friend": "select", "select_enemy": "select", "select_neutral": "select", "gather_wood": "gather",
            "gather_stone": "gather", "gather_meteor": "gather", "repair": "build", "move": "select", "no": "arrow"}
POINTERS = ("arrow", "select_friend", "select_enemy", "select_neutral")   # hegyes mutatók: a hegyük a kattintási pont
SIZE = [0]   # 0 = automatikus (a képernyő magasságához)


def cursor_size():
    if SIZE[0]:
        return SIZE[0]
    try:
        h = pygame.display.get_surface().get_height()
    except (pygame.error, AttributeError):
        h = 768
    return 32 if h < 860 else (40 if h < 1150 else 48 if h < 1500 else 64)


def _tip(surf):
    """A hegyes mutató hegye: a bal felső sarokhoz legközelebbi átlátszatlan képpont."""
    w, h = surf.get_size()
    best = (0, 0)
    for d in range(w + h):
        for x in range(max(0, d - h + 1), min(w, d + 1)):
            if surf.get_at((x, d - x))[3] > 100:
                return (x, d - x)
    return best


def cursor(fid, kind, size=None):
    size = size or cursor_size()
    key = (fid, kind, size)
    if key not in _CACHE:
        src = _art(f"ui/cursors/{fid}_{kind}.png") or _art(f"ui/cursors/{kind}.png")
        if src is not None:
            sw, sh = src.get_size()
            meta = os.path.join(ROOT, "assets", "ui", "cursors", kind + ".json")
            if kind in POINTERS or kind.startswith("scroll_") is False and not os.path.isfile(meta):
                hx, hy = _tip(src) if kind in POINTERS else (sw // 2, sh // 2)
            else:
                import json
                try:
                    with open(meta, encoding="utf-8") as f:
                        m = json.load(f)
                    hx, hy = (v * sw / m.get("size", [32, 32])[0] for v in m.get("hotspot", [sw // 2, sh // 2]))
                except (OSError, ValueError):
                    hx, hy = sw // 2, sh // 2
            k = size / sw
            surf = src if sw == size else (pygame.transform.scale(src, (size, size)) if sw % size == 0 and
                                           sw // size in (1, 2) and False else
                                           pygame.transform.smoothscale(src, (size, size)))
            hot = (min(size - 1, int(hx * k)), min(size - 1, int(hy * k)))
        else:
            base = FALLBACK.get(kind, kind if kind in KINDS else "arrow")
            surf = _cursor_surface(fid, base)
            hot = HOT.get(base, (0, 0))
            if size != 32:
                surf = pygame.transform.smoothscale(surf, (size, size))
                hot = (hot[0] * size // 32, hot[1] * size // 32)
        try:
            _CACHE[key] = pygame.cursors.Cursor(hot, surf)
        except (pygame.error, TypeError, AttributeError):
            _CACHE[key] = None
    return _CACHE[key]


_CURRENT = [None]


def set_cursor(fid, kind):
    """Kurzor beállítása (csak ha változott)."""
    key = (fid, kind, cursor_size())
    if _CURRENT[0] == key:
        return
    c = cursor(fid, kind, key[2])
    if c is None:
        return
    try:
        pygame.mouse.set_cursor(c)
        _CURRENT[0] = key
    except pygame.error:
        pass


def reset_cursor():
    """Semleges kurzor (menük, nyitókép – egyik fajhoz sem tartozik): assets/ui/cursors/neutral_arrow.png vagy
    kódból rajzolt acélszürke nyíl meteorkék drágakővel."""
    set_cursor("neutral", "arrow")
