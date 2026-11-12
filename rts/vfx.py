"""Varázslat-látványok: elemenként saját megjelenés a képesség négy pillanatára.

    cast    – varázslás: rúnakör nyílik a varázsló alatt, az elem részecskéi szállnak fel
    bolt    – lövedék: elemre jellemző mag és csóva (tűzgömb parázzsal, jégszilánk dérrel, méregcsepp…)
    impact  – becsapódás / lökéshullám (nova): szétcsapó láng, szilánkok, villám-ívek, fröccs…
    area    – területi hatás a talajon, amíg tart (lángok, hóesés, bugyborékoló gáz, hullámok, örvény…)

Ha van festett képlap (assets/sprites/effects/vfx_<elem>_<pillanat>.json), a UI azt használja; ez a modul a
festett képek nélküli, kódból rajzolt változat (a játék minden varázslatnak ad látványt).
Az elemek: fire, ice, lightning, poison, water, root, sleep, fear, blind, wind, sunder, silence, mark;
elem nélküli képességnél „arcane” (a képesség saját színével).
"""
import math

import pygame

from . import canvas as gdraw

PAL = {  # (fő szín, világos, sötét kontúr)
    "fire": ((255, 130, 40), (255, 230, 120), (60, 18, 6)),
    "ice": ((150, 210, 255), (235, 250, 255), (16, 34, 70)),
    "lightning": ((170, 200, 255), (250, 250, 255), (20, 24, 60)),
    "poison": ((120, 220, 70), (210, 255, 160), (20, 50, 10)),
    "water": ((70, 160, 240), (200, 235, 255), (10, 30, 70)),
    "root": ((150, 110, 60), (140, 200, 90), (40, 26, 10)),
    "sleep": ((190, 170, 255), (240, 230, 255), (40, 30, 70)),
    "fear": ((150, 80, 200), (220, 170, 255), (30, 10, 40)),
    "blind": ((235, 235, 220), (255, 255, 250), (70, 70, 70)),
    "wind": ((215, 230, 220), (250, 255, 250), (60, 80, 70)),
    "sunder": ((220, 120, 60), (255, 210, 150), (50, 24, 10)),
    "silence": ((180, 140, 240), (235, 215, 255), (40, 20, 60)),
    "mark": ((255, 80, 70), (255, 200, 190), (60, 10, 10)),
}


def pal(el, col):
    if el in PAL:
        return PAL[el]
    c = tuple(col or (220, 200, 255))
    return c, tuple(min(255, v + 90) for v in c), tuple(max(0, v // 5) for v in c)


def _rnd(seed, i):
    """Determinisztikus 0..1 érték (a részecskék helye effektenként állandó)."""
    v = math.sin(seed * 12.9898 + i * 78.233) * 43758.5453
    return v - math.floor(v)


def _layer(w, h):
    return pygame.Surface((max(1, int(w)), max(1, int(h))), pygame.SRCALPHA)


def _ell(surf, col, cx, cy, rx, ry, width=0):
    pygame.draw.ellipse(surf, col, (cx - rx, cy - ry, 2 * rx, 2 * ry), width)


def _diamond(s, x, y, k, fill, dark, tall=1.7):
    pts = [(x, y - k * tall), (x + k * 0.7, y), (x, y + k * tall), (x - k * 0.7, y)]
    gdraw.polygon(s, dark, [(a + 1, b + 1) for a, b in pts])
    gdraw.polygon(s, fill, pts)


def _flame(s, x, y, h, w, c1, c2, dark):
    pts = [(x, y - h), (x + w, y), (x - w, y)]
    gdraw.polygon(s, dark, [(a + 1, b + 1) for a, b in pts])
    gdraw.polygon(s, c1, pts)
    gdraw.polygon(s, c2, [(x, y - h * 0.55), (x + w * 0.5, y), (x - w * 0.5, y)])


def _zigzag(s, x0, y0, x1, y1, seed, c, light, width=2, segs=6):
    pts = [(x0, y0)]
    for i in range(1, segs):
        f = i / segs
        off = (_rnd(seed, i) - 0.5) * 14
        nx, ny = -(y1 - y0), (x1 - x0)
        d = math.hypot(nx, ny) or 1
        pts.append((x0 + (x1 - x0) * f + nx / d * off, y0 + (y1 - y0) * f + ny / d * off))
    pts.append((x1, y1))
    gdraw.lines(s, (20, 24, 60), False, [(a + 1, b + 1) for a, b in pts], width + 2)
    gdraw.lines(s, c, False, pts, width + 1)
    gdraw.lines(s, light, False, pts, max(1, width - 1))


def particles(s, el, x, y, f, t, seed, spread, rise, n=6):
    """Az elem részecskéi (felfelé szálló, a varázsláskor és a területen)."""
    c, light, dark = pal(el, None)
    for i in range(n):
        q = (f * 1.4 + t * 0.4 + _rnd(seed, i)) % 1.0
        px = x + (_rnd(seed, i + 20) - 0.5) * spread * 2 + math.sin(t * 3 + i) * 2
        py = y - q * rise
        fade = 1 - q
        if el == "fire":
            _flame(s, px, py, 6 * fade + 2, 2.2 * fade + 1, c, light, dark)
        elif el == "ice":
            _diamond(s, px, py, 2.4 * fade + 1, light, dark, 1.4)
        elif el == "lightning":
            if _rnd(seed + int(t * 8), i) < 0.5:
                _zigzag(s, px, py, px + 6, py - 8, seed + i, c, light, 1, 3)
        elif el == "poison":
            r = 2 + 2 * fade
            gdraw.circle(s, dark, (px, py), r + 1)
            gdraw.circle(s, c, (px, py), r)
            gdraw.circle(s, light, (px - r * 0.4, py - r * 0.4), max(1, r * 0.4))
        elif el == "water":
            gdraw.polygon(s, dark, [(px + 1, py - 3), (px + 3, py + 2), (px - 1, py + 2)])
            gdraw.polygon(s, c, [(px, py - 4), (px + 2, py + 1), (px - 2, py + 1)])
            gdraw.circle(s, light, (px, py + 1), 2)
        elif el == "root":
            gdraw.ellipse(s, dark, (px - 3, py - 2, 7, 4))
            gdraw.ellipse(s, light, (px - 2, py - 1, 5, 3))
        elif el == "wind":
            gdraw.arc(s, light, (px - 6, py - 3, 12, 6), 0.2 + t * 4 + i, 2.4 + t * 4 + i, 1)
        elif el in ("sleep", "silence", "fear"):
            gdraw.circle(s, dark, (px, py), 3)
            gdraw.circle(s, light, (px, py), 2)
        elif el == "blind":
            gdraw.circle(s, (200, 200, 190), (px, py), 2 + 2 * fade)
        else:
            _diamond(s, px, py, 2 * fade + 1, light, dark)


# ------------------------------------------------------------------ lökéshullám / becsapódás
def draw_nova(s, x, y, f, el, col, rad, t, seed):
    """Elem-lökéshullám: kifelé futó lapított gyűrű, a peremén az elem jellegzetes alakzataival."""
    c, light, dark = pal(el, col)
    rad = rad or 60
    rr = rad * (0.25 + 0.75 * f)
    a = int(255 * (1 - f))
    surf = _layer(rad * 2 + 16, rad * 1.3 + 16)
    cx, cy = surf.get_width() / 2, surf.get_height() / 2
    _ell(surf, (*c, int(a * 0.35)), cx, cy, rr, rr * 0.6)
    _ell(surf, (*dark, int(a * 0.8)), cx, cy, rr + 2, rr * 0.6 + 2, 5)
    _ell(surf, (*c, a), cx, cy, rr, rr * 0.6, 3)
    _ell(surf, (*light, a), cx, cy, rr * 0.92, rr * 0.55, 1)
    s.blit(surf, (x - cx, y - cy))
    n = 10
    for i in range(n):  # a perem alakzatai
        an = i * 2 * math.pi / n + _rnd(seed, i) * 0.4
        px, py = x + math.cos(an) * rr, y + math.sin(an) * rr * 0.6
        k = 1 - f
        if el == "fire":
            _flame(s, px, py, 12 * k + 3, 4 * k + 1, c, light, dark)
        elif el == "ice":
            _diamond(s, px, py - 4 * k, 3.5 * k + 1, light, dark, 2.2)
        elif el == "lightning":
            if i % 2 == 0:
                _zigzag(s, x, y - 4, px, py, seed + i, c, light, 2, 5)
        elif el == "poison":
            gdraw.circle(s, dark, (px, py - 3 * k), 4 * k + 2)
            gdraw.circle(s, c, (px, py - 3 * k), 4 * k + 1)
        elif el == "water":
            gdraw.arc(s, light, (px - 5, py - 8 * k - 4, 10, 10), 0.3, 2.8, 2)
            gdraw.circle(s, c, (px + 3, py - 10 * k), 2)
        elif el in ("root", "sunder"):  # repedések / kőszilánkok
            gdraw.line(s, dark, (x + math.cos(an) * rr * 0.4, y + math.sin(an) * rr * 0.24), (px, py), 3)
            gdraw.line(s, c, (x + math.cos(an) * rr * 0.4, y + math.sin(an) * rr * 0.24), (px, py), 1)
            if el == "sunder":
                gdraw.rect(s, dark, (px - 2, py - 8 * k - 2, 5, 4))
                gdraw.rect(s, light, (px - 1, py - 8 * k - 1, 3, 2))
        elif el == "wind":
            gdraw.arc(s, light, (px - 8, py - 4, 16, 8), an + 1.2, an + 3.0, 2)
        else:
            _diamond(s, px, py - 3 * k, 2.6 * k + 1, light, dark)


def draw_impact(s, x, y, f, el, col, size, t, seed):
    """Becsapódás a célponton (lövedék, célzott varázslat)."""
    c, light, dark = pal(el, col)
    k = 1 - f
    r = (size or 14) * (0.4 + 0.8 * f)
    if el == "fire":
        surf = _layer(r * 2 + 6, r * 2 + 6)
        pygame.draw.circle(surf, (*c, int(200 * k)), (r + 3, r + 3), r)
        pygame.draw.circle(surf, (*light, int(230 * k)), (r + 3, r + 3), r * 0.55)
        s.blit(surf, (x - r - 3, y - r - 3))
        for i in range(7):
            an = i * 0.9 + _rnd(seed, i)
            _flame(s, x + math.cos(an) * r, y + math.sin(an) * r * 0.7 - 3, 6 * k + 2, 2 * k + 1, c, light, dark)
    elif el == "ice":
        for i in range(8):
            an = i * math.pi / 4 + _rnd(seed, i) * 0.3
            px, py = x + math.cos(an) * r, y + math.sin(an) * r * 0.75
            gdraw.line(s, dark, (x, y), (px + 1, py + 1), 3)
            gdraw.line(s, light, (x, y), (px, py), 2)
            _diamond(s, px, py, 2.5 * k + 1, light, dark, 1.8)
    elif el == "lightning":
        for i in range(4):
            an = i * math.pi / 2 + _rnd(seed, i)
            _zigzag(s, x, y, x + math.cos(an) * r * 1.3, y + math.sin(an) * r, seed + i, c, light, 2, 4)
        if f < 0.3:
            _zigzag(s, x + 4, y - 60, x, y, seed + 9, c, light, 3, 7)
        gdraw.circle(s, light, (x, y), 4 * k + 1)
    elif el in ("poison", "water"):
        for i in range(8):
            an = i * 0.8 + _rnd(seed, i)
            d = r * (0.6 + 0.6 * _rnd(seed, i + 9))
            px, py = x + math.cos(an) * d, y + math.sin(an) * d * 0.6 - 8 * math.sin(f * math.pi)
            rr = 2.5 * k + 1
            gdraw.circle(s, dark, (px, py), rr + 1)
            gdraw.circle(s, c, (px, py), rr)
        if el == "water":
            gdraw.arc(s, light, (x - r, y - r * 0.5, 2 * r, r), 0, math.pi, 2)
    elif el == "mark":
        rr = 14 - 6 * f
        gdraw.circle(s, dark, (x, y - 10), rr + 1, 3)
        gdraw.circle(s, c, (x, y - 10), rr, 2)
        for i in range(4):
            an = i * math.pi / 2 + f
            gdraw.line(s, c, (x + math.cos(an) * (rr - 4), y - 10 + math.sin(an) * (rr - 4)),
                       (x + math.cos(an) * (rr + 5), y - 10 + math.sin(an) * (rr + 5)), 2)
    else:  # csillag-szikra
        for i in range(8):
            an = i * math.pi / 4 + f
            gdraw.line(s, dark, (x, y), (x + math.cos(an) * r + 1, y + math.sin(an) * r + 1), 3)
            gdraw.line(s, light, (x, y), (x + math.cos(an) * r, y + math.sin(an) * r), 1)
        gdraw.circle(s, c, (x, y), 3 * k + 1)


# ------------------------------------------------------------------ lövedék
def draw_bolt(s, x, y, dx, dy, el, col, t, seed):
    """Varázslövedék: mag + csóva az elem szerint (a földtől kissé felemelve)."""
    c, light, dark = pal(el, col)
    d = math.hypot(dx, dy) or 1
    ux, uy = dx / d, dy / d
    y -= 6
    for j in range(7, 0, -1):  # csóva
        tx, ty = x - ux * j * 4, y - uy * j * 4
        rr = max(1, 5 - j * 0.6)
        if el == "fire":
            _flame(s, tx + (_rnd(seed + int(t * 20), j) - 0.5) * 3, ty, rr * 1.8, rr, c, light, dark)
        elif el == "ice":
            gdraw.circle(s, light, (tx, ty), rr * 0.7)
        elif el == "lightning":
            pass
        elif el == "poison":
            gdraw.circle(s, c, (tx, ty + j), rr * 0.8)
        else:
            gdraw.circle(s, c, (tx, ty), rr * 0.75)
    if el == "lightning":
        _zigzag(s, x - ux * 26, y - uy * 26, x, y, seed + int(t * 30), c, light, 2, 4)
        gdraw.circle(s, light, (x, y), 4)
    elif el == "ice":  # hegyes szilánk a repülés irányába
        px, py = -uy, ux
        pts = [(x + ux * 9, y + uy * 9), (x + px * 3, y + py * 3), (x - ux * 7, y - uy * 7), (x - px * 3, y - py * 3)]
        gdraw.polygon(s, dark, [(a + 1, b + 1) for a, b in pts])
        gdraw.polygon(s, light, pts)
    else:
        gdraw.circle(s, dark, (x, y), 6)
        gdraw.circle(s, c, (x, y), 5)
        gdraw.circle(s, light, (x - 1, y - 1), 2.5)


# ------------------------------------------------------------------ varázskörök (elemenként saját minta)
_CIRCLE = {}


def magic_circle(el, col, size, phase):
    """Elemenként megtervezett, forgó varázskör: álló helyzetben, nagyban rajzolva, majd a talajra lapítva
    (2:1,2) és simán kicsinyítve. phase: 0..47 (a forgás fázisa) – gyorsítótárazható."""
    key = (el, col if el not in PAL else None, int(size), phase)
    img = _CIRCLE.get(key)
    if img is not None:
        return img
    if len(_CIRCLE) > 600:
        _CIRCLE.clear()
    c, light, dark = pal(el, col)
    K = 3
    R = int(size) * K
    D = 2 * R + 12 * K
    big = pygame.Surface((D, D), pygame.SRCALPHA)
    o = D / 2
    rot = phase / 48 * 2 * math.pi

    def P(r, a):
        return o + math.cos(a) * r, o + math.sin(a) * r

    def ring(r, w, colr, alpha=255):
        pygame.draw.circle(big, (*colr, alpha), (o, o), r, w)

    def line(a, b, w, colr, alpha=255):
        pygame.draw.line(big, (*colr, alpha), a, b, w)

    def poly(pts, colr, alpha=255, w=0):
        pygame.draw.polygon(big, (*colr, alpha), pts, w)

    # közös alap: sötét kontúr + fő gyűrű + lágy belső fény
    for i in range(6, 0, -1):
        pygame.draw.circle(big, (*c, int(10 + 16 * (1 - i / 6))), (o, o), R * 0.95 * i / 6)
    ring(R + 2 * K, 5 * K, dark, 180)
    ring(R, 3 * K, c)
    ring(R * 0.86, K, light, 200)
    if el == "fire":  # napsugaras gyűrű + lángjel
        for i in range(12):
            a = rot + i * math.pi / 6
            poly([P(R * 0.86, a - 0.12), P(R * 1.12, a), P(R * 0.86, a + 0.12)], light if i % 2 else c)
        ring(R * 0.55, 2 * K, c)
        poly([P(R * 0.42, -math.pi / 2), P(R * 0.18, -0.3), P(R * 0.3, 0.5), P(0.0, 0), P(R * 0.3, math.pi - 0.5),
              P(R * 0.18, math.pi + 0.3)], c)
        poly([P(R * 0.28, -math.pi / 2), P(R * 0.1, 0), P(R * 0.1, math.pi)], light)
    elif el == "ice":  # hatágú hópehely
        for i in range(6):
            a = rot * 0.5 + i * math.pi / 3
            line(P(0, 0), P(R * 0.8, a), 2 * K, light)
            for f in (0.45, 0.65):
                b = P(R * 0.8 * f, a)
                line(b, (b[0] + math.cos(a + 0.7) * R * 0.15, b[1] + math.sin(a + 0.7) * R * 0.15), K + 1, light)
                line(b, (b[0] + math.cos(a - 0.7) * R * 0.15, b[1] + math.sin(a - 0.7) * R * 0.15), K + 1, light)
        poly([P(R * 0.62, rot * 0.5 + i * math.pi / 3) for i in range(6)], c, 255, 2 * K)
    elif el == "lightning":  # cikcakkos gyűrű, ívek
        pts = [P(R * (0.72 if i % 2 else 0.6), rot + i * math.pi / 12) for i in range(24)]
        poly(pts, light, 255, 2 * K)
        for i in range(3):
            a = rot * 2 + i * 2.1
            line(P(R * 0.2, a), P(R * 0.55, a + 0.3), 2 * K, light)
            line(P(R * 0.55, a + 0.3), P(R * 0.4, a + 0.5), 2 * K, light)
            line(P(R * 0.4, a + 0.5), P(R * 0.8, a + 0.7), 2 * K, light)
    elif el == "poison":  # buborékgyűrű + hármas levél
        for i in range(10):
            a = rot + i * math.pi / 5
            pygame.draw.circle(big, (*light, 255), P(R * 0.72, a), R * (0.07 + 0.03 * (i % 2)), K + 1)
        for i in range(3):
            a = -rot + i * 2.094
            pygame.draw.circle(big, (*c, 230), P(R * 0.22, a), R * 0.17)
            pygame.draw.circle(big, (*light, 255), P(R * 0.22, a), R * 0.17, K)
    elif el == "water":  # hullámzó gyűrűk
        for j, rr in enumerate((0.7, 0.5, 0.3)):
            pts = [P(R * rr + math.sin(i * 0.8 + rot * 2 + j) * K * 3, i * math.pi / 24) for i in range(48)]
            poly(pts, light if j % 2 == 0 else c, 255, 2 * K)
    elif el == "root":  # tövises indakör + triskelion
        pts = [P(R * 0.74 + math.sin(i * 1.3 + rot * 3) * K * 4, rot * 0.5 + i * math.pi / 18) for i in range(36)]
        poly(pts, c, 255, 3 * K)
        for i in range(9):
            a = rot * 0.5 + i * 0.7
            poly([P(R * 0.74, a - 0.05), P(R * 0.88, a + 0.08), P(R * 0.74, a + 0.12)], light)
        for i in range(3):
            a = rot + i * 2.094
            for k in range(8):
                f = k / 8
                pygame.draw.circle(big, (*light, 255), P(R * 0.4 * f, a + f * 2.2), K + 1)
    elif el == "sleep":  # holdsarló és csillagok
        pygame.draw.circle(big, (*light, 255), (o, o), R * 0.38)
        pygame.draw.circle(big, (0, 0, 0, 0), (o + R * 0.14, o - R * 0.08), R * 0.34)
        for i in range(7):
            a = rot * 0.6 + i * 0.9
            sx_, sy_ = P(R * 0.66, a)
            k = R * 0.06
            poly([(sx_, sy_ - k * 1.6), (sx_ + k * 0.5, sy_), (sx_, sy_ + k * 1.6), (sx_ - k * 0.5, sy_)], light)
    elif el == "fear":  # csipkés sötét gyűrű + szem
        pts = [P(R * (0.8 if i % 2 else 0.62), rot + i * math.pi / 10) for i in range(20)]
        poly(pts, dark, 220)
        poly(pts, c, 255, 2 * K)
        pygame.draw.ellipse(big, (*light, 255), (o - R * 0.3, o - R * 0.14, R * 0.6, R * 0.28))
        pygame.draw.circle(big, (*dark, 255), (o, o), R * 0.11)
    elif el == "blind":  # napkitörés
        for i in range(16):
            a = rot + i * math.pi / 8
            line(P(R * 0.2, a), P(R * (0.85 if i % 2 else 0.6), a), 2 * K, light)
        pygame.draw.circle(big, (*light, 255), (o, o), R * 0.18)
    elif el == "wind":  # forgó spirálkarok
        for i in range(4):
            pts = [P(R * 0.85 * f, rot * 1.5 + i * math.pi / 2 + f * 2.6) for f in [k / 14 for k in range(1, 15)]]
            pygame.draw.lines(big, (*light, 255), False, pts, 2 * K)
    elif el == "sunder":  # szögletes rúnák repedésekkel
        for i in range(8):
            a = rot + i * math.pi / 4
            cx_, cy_ = P(R * 0.68, a)
            k = R * 0.08
            poly([(cx_ - k, cy_ - k), (cx_ + k, cy_ - k), (cx_ + k, cy_ + k), (cx_ - k, cy_ + k)], light, 255, K + 1)
        for i in range(5):
            a = i * 1.26 + 0.3
            line(P(R * 0.1, a), P(R * 0.5, a + 0.2), 2 * K, c)
    elif el == "silence":  # áthúzott rúnagyűrű
        for i in range(12):
            a = rot + i * math.pi / 6
            cx_, cy_ = P(R * 0.68, a)
            k = R * 0.06
            line((cx_ - k, cy_ - k), (cx_ + k, cy_ + k), K + 1, light)
            line((cx_ - k, cy_ + k), (cx_ + k, cy_), K + 1, light)
        ring(R * 0.35, 2 * K, light)
        line(P(R * 0.35, math.pi * 0.75), P(R * 0.35, -math.pi * 0.25), 3 * K, (240, 80, 80))
    elif el == "mark":  # célkereszt
        ring(R * 0.5, 2 * K, light)
        for i in range(4):
            a = rot + i * math.pi / 2
            line(P(R * 0.2, a), P(R * 0.8, a), 2 * K, light)
    else:  # arcane: forgó hatágú csillag rúnákkal
        for sgn in (0, math.pi / 3):
            poly([P(R * 0.72, rot + sgn + i * 2.094) for i in range(3)], light, 255, 2 * K)
        ring(R * 0.4, K + 1, c)
        for i in range(8):
            a = -rot + i * math.pi / 4
            cx_, cy_ = P(R * 0.86 + 0.0, a)
            pygame.draw.circle(big, (*light, 255), (cx_, cy_), K * 1.5)
    W = D // K
    flat = pygame.transform.smoothscale(big, (W, int(W * 0.6)))
    _CIRCLE[key] = flat
    return flat


def draw_cast(s, x, y, f, el, col, size, t, seed):
    """Varázslás: az elem varázsköre kinyílik a varázsló alatt, forog, majd elhalványul; az elem részecskéi
    felszállnak."""
    rad = (size or 26) * (0.55 + 0.45 * min(1.0, f * 3))
    a = int(255 * (1 - max(0.0, f - 0.6) / 0.4))
    ph = int(t * 14) % 48
    img = magic_circle(el, None if el in PAL else tuple(col or (220, 200, 255)), max(10, int(rad)), ph)
    if a < 250:
        img = img.copy()
        img.set_alpha(a)
    s.blit(img, (x - img.get_width() / 2, y - img.get_height() / 2))
    particles(s, el, x, y, f, t, seed, rad * 0.7, 34, 7)


# ------------------------------------------------------------------ terület
_PUFF = {}


def _puff(s, x, y, r, col, alpha):
    """Lágy szélű, áttetsző folt (köd, gáz, izzás) – méret és szín szerint gyorsítótárból."""
    r = max(2, int(r))
    a = max(0, min(255, int(alpha)))
    key = (r, col, a // 8)
    img = _PUFF.get(key)
    if img is None:
        if len(_PUFF) > 400:
            _PUFF.clear()
        img = pygame.Surface((r * 2, r * 2), pygame.SRCALPHA)
        for i in range(8, 0, -1):
            k = i / 8
            pygame.draw.circle(img, (*col, int(a * (1 - k) * 0.9 + a * 0.05)), (r, r), r * k)
        _PUFF[key] = img
    s.blit(img, (x - r, y - r * 0.75))


def _vine(s, x, y, h, t, i, dark, c, leaf):
    """Tekergő inda levéllel."""
    pts = []
    for j in range(6):
        q = j / 5
        pts.append((x + math.sin(t * 2 + i + q * 4) * 5 * q, y - h * q))
    gdraw.lines(s, dark, False, [(a + 1, b + 1) for a, b in pts], 4)
    gdraw.lines(s, c, False, pts, 2)
    lx, ly = pts[3]
    gdraw.ellipse(s, dark, (lx - 1, ly - 3, 8, 5))
    gdraw.ellipse(s, leaf, (lx, ly - 2, 6, 3))
    gdraw.circle(s, leaf, pts[-1], 2)


def draw_area(s, x, y, rad, life, t, el, col, seed, fade=1.0):
    """Területi hatás a talajon: az elem forgó varázsköre a peremen, lágy talajfény és élő elem-animáció.
    life: 0 (kezdet) .. 1 (vége)."""
    c, light, dark = pal(el, col)
    a_in = min(1.0, life * 6) * min(1.0, (1 - life) * 5) * fade
    if a_in <= 0.02:
        return
    rx, ry = rad, rad * 0.6
    glow = {"fire": (255, 110, 30), "sunder": (120, 80, 50)}.get(el, c)
    gl = _layer(rad * 2 + 4, rad * 1.2 + 4)          # lapított, lágy talajfény
    gcx, gcy = gl.get_width() / 2, gl.get_height() / 2
    for i in range(8, 0, -1):
        k = i / 8
        _ell(gl, (*glow, int(a_in * (10 + 60 * (1 - k)))), gcx, gcy, rx * k, ry * k)
    s.blit(gl, (x - gcx, y - gcy))
    ph = int(t * 8) % 48
    img = magic_circle(el, None if el in PAL else tuple(col or (220, 200, 255)), max(12, int(rad)), ph)
    if a_in < 0.98:
        img = img.copy()
        img.set_alpha(int(220 * a_in))
    s.blit(img, (x - img.get_width() / 2, y - img.get_height() / 2))
    n = max(8, int(rad / 5.5))

    def spot(i):
        an = _rnd(seed, i) * 2 * math.pi
        dd = math.sqrt(_rnd(seed, i + 50)) * 0.9
        return x + math.cos(an) * rx * dd, y + math.sin(an) * ry * dd

    if el == "fire":
        for i in range(n):
            px, py = spot(i)
            h = (14 + 10 * _rnd(seed, i + 7)) * (0.65 + 0.35 * math.sin(t * 10 + i * 1.7))
            _flame(s, px, py, h, 4.5, c, light, dark)
        for i in range(n // 2):
            q = (t * 0.8 + _rnd(seed, i + 70)) % 1.0
            px, py = spot(i + 200)
            gdraw.circle(s, (255, 220, 120), (px + math.sin(t * 4 + i) * 4, py - 10 - q * 40), 2 * (1 - q) + 0.5)
    elif el == "ice":
        for i in range(n // 2):
            px, py = spot(i)
            for k in range(3):
                _diamond(s, px + (k - 1) * 5, py - 4 - (k % 2) * 3, 3.6 - (k % 2), light, dark, 2.4)
        for i in range(n):
            q = (t * 0.35 + _rnd(seed, i + 90)) % 1.0
            px, py = spot(i + 300)
            gdraw.circle(s, (245, 250, 255), (px + math.sin(t * 2 + i) * 5, py - 50 + q * 50), 2 * (1 - q * 0.5))
    elif el == "lightning":
        for i in range(3):
            k = int(t * 3) + i * 7
            px, py = spot(k)
            phs = (t * 3) % 1.0
            if phs < 0.6:
                _puff(s, px, py, 16, (200, 220, 255), 160 * a_in * (1 - phs))
                _zigzag(s, px + 4, py - 90, px, py, seed + k, c, light, 3, 8)
    elif el == "poison":
        for i in range(5):
            px, py = spot(i)
            _puff(s, px + math.sin(t * 0.6 + i) * 8, py - 10, rad * 0.32, (120, 210, 80), 120 * a_in)
        for i in range(n):
            q = (t * 0.7 + _rnd(seed, i + 90)) % 1.0
            px, py = spot(i + 10)
            r = 2.5 + 3.5 * math.sin(q * math.pi)
            gdraw.circle(s, dark, (px, py - q * 22), r + 1)
            gdraw.circle(s, c, (px, py - q * 22), r)
            gdraw.circle(s, light, (px - r * 0.4, py - q * 22 - r * 0.4), max(1, r * 0.35))
    elif el == "water":
        for i in range(n):
            px, py = spot(i)
            q = (t * 0.8 + _rnd(seed, i + 9)) % 1.0
            gdraw.arc(s, dark, (px - 7, py - 4 - q * 3 + 1, 14, 8), 0.3, 2.8, 3)
            gdraw.arc(s, light, (px - 7, py - 4 - q * 3, 14, 8), 0.3, 2.8, 2)
    elif el == "root":
        for i in range(n):
            px, py = spot(i)
            _vine(s, px, py, 14 + 8 * _rnd(seed, i + 4), t, i, dark, c, light)
    elif el == "sleep":
        for i in range(5):
            px, py = spot(i)
            _puff(s, px + math.sin(t * 0.5 + i) * 6, py - 8, rad * 0.3, (200, 185, 255), 110 * a_in)
        for i in range(n // 2):
            q = (t * 0.3 + _rnd(seed, i + 9)) % 1.0
            px, py = spot(i + 30)
            _diamond(s, px, py - q * 26, 2.4 * (1 - q) + 0.8, (245, 240, 255), dark, 1.2)
    elif el == "wind":
        for i in range(n):
            ang = t * 3.2 + i * 2.4
            rr = (0.25 + 0.75 * ((i * 0.37) % 1.0)) * rx
            ex, ey = x + math.cos(ang) * rr, y + math.sin(ang) * rr * 0.6
            gdraw.arc(s, dark, (ex - 10, ey - 5 + 1, 20, 10), ang + 1.0, ang + 2.8, 3)
            gdraw.arc(s, light, (ex - 10, ey - 5, 20, 10), ang + 1.0, ang + 2.8, 2)
            if i % 3 == 0:
                gdraw.ellipse(s, (40, 60, 20), (ex - 3, ey - 2, 7, 4))
                gdraw.ellipse(s, (130, 180, 80), (ex - 2, ey - 1, 5, 3))
    elif el == "sunder":
        for i in range(n // 2):
            q = (t * 0.9 + _rnd(seed, i + 9)) % 1.0
            px, py = spot(i)
            hh = math.sin(q * math.pi) * 14
            gdraw.rect(s, dark, (px - 3, py - hh - 3, 7, 5))
            gdraw.rect(s, (190, 170, 150), (px - 2, py - hh - 2, 5, 3))
    elif el == "blind":
        for i in range(5):
            px, py = spot(i)
            _puff(s, px, py - 8, rad * 0.3, (235, 235, 225), 130 * a_in)
    else:  # arcane, fear, silence, mark: felszálló szikrák
        for i in range(n):
            q = (t * 0.55 + _rnd(seed, i + 9)) % 1.0
            px, py = spot(i)
            _diamond(s, px, py - q * 30, 3.2 * (1 - q) + 0.8, light, dark, 1.6)
