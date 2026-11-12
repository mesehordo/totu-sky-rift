"""Procedurális rajzolás: egységek, épületek, nyersanyagok, ikonok.

Nincs szükség képfájlokra – minden alakzat kódból készül. Az egységek a
definíciójuk "shape", az épületek a faj "style" és a saját "shape" mezője
alapján rajzolódnak, így modból új egység is azonnal megjelenik.
"""
import math

import pygame

from . import canvas as gdraw

from .world import TILE


def shade(c, f):
    return tuple(max(0, min(255, int(v * f))) for v in c[:3])


def mix(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def _rot(cx, cy, ang, pts):
    ca, sa = math.cos(ang), math.sin(ang)
    return [(cx + px * ca - py * sa, cy + px * sa + py * ca) for px, py in pts]


def _ellipse(cx, cy, ang, a, b, n=14):
    return _rot(cx, cy, ang, [(math.cos(i / n * 6.283) * a, math.sin(i / n * 6.283) * b) for i in range(n)])


def draw_shape(s, shape, cx, cy, r, base, team, accent, ang, t, moving=False):
    """Egység rajzolása (felülnézet, 'ang' irányba néz)."""
    dark = shade(base, 0.55)
    light = shade(base, 1.3)
    bob = math.sin(t * 10) * (r * 0.08) if moving else 0
    if shape == "worker":
        gdraw.circle(s, team, (cx, cy), r)
        gdraw.circle(s, base, (cx, cy), r * 0.78)
        hx, hy = _rot(cx, cy, ang, [(r * 0.9, r * 0.6 + bob)])[0]
        tx, ty = _rot(cx, cy, ang, [(r * 1.6, r * 0.9 + bob)])[0]
        gdraw.line(s, (120, 90, 50), (hx, hy), (tx, ty), max(1, int(r * 0.25)))
        gdraw.circle(s, (160, 160, 165), (tx, ty), max(2, r * 0.3))
    elif shape in ("warrior", "shieldbearer"):
        gdraw.circle(s, team, (cx, cy), r)
        gdraw.circle(s, base, (cx, cy), r * 0.8)
        gdraw.circle(s, light, _rot(cx, cy, ang, [(r * 0.2, 0)])[0], r * 0.35)
        if shape == "shieldbearer":
            p = _rot(cx, cy, ang, [(r * 0.9, -r * 1.0), (r * 1.25, -r * 0.4), (r * 1.25, r * 0.4), (r * 0.9, r * 1.0)])
            gdraw.lines(s, shade(team, 0.8), False, p, max(3, int(r * 0.45)))
            gdraw.lines(s, accent, False, p, max(1, int(r * 0.15)))
        else:
            a = _rot(cx, cy, ang, [(r * 0.4, r * 0.7 + bob), (r * 1.7, r * 0.9 + bob)])
            gdraw.line(s, (60, 50, 50), a[0], a[1], max(2, int(r * 0.3)))
            gdraw.line(s, (200, 220, 230), a[0], a[1], 1)
            gdraw.circle(s, team, _rot(cx, cy, ang, [(r * 0.2, -r * 0.8)])[0], r * 0.45)
    elif shape == "archer":
        gdraw.circle(s, team, (cx, cy), r)
        gdraw.circle(s, base, (cx, cy), r * 0.8)
        arc = _rot(cx, cy, ang, [(r * 0.9 + math.cos(a) * r * 0.5, math.sin(a) * r * 1.1)
                                 for a in [i / 6 * math.pi - math.pi / 2 for i in range(7)]])
        gdraw.lines(s, (130, 90, 40), False, arc, 2)
        a = _rot(cx, cy, ang, [(r * 0.3, 0), (r * 1.8, 0)])
        gdraw.line(s, light, a[0], a[1], 1)
    elif shape == "rider":
        body = _ellipse(cx, cy, ang, r * 1.15, r * 0.6)
        gdraw.polygon(s, dark, body)
        legs = math.sin(t * 14) * r * 0.35 if moving else 0
        for sx, sy in ((0.6, 0.6), (-0.6, 0.6), (0.6, -0.6), (-0.6, -0.6)):
            lx = sx * r + (legs if sx * sy > 0 else -legs)
            p = _rot(cx, cy, ang, [(lx * 0.8, sy * r * 0.5), (lx, sy * r * 0.85)])
            gdraw.line(s, dark, p[0], p[1], max(2, int(r * 0.2)))
        head = _rot(cx, cy, ang, [(r * 1.35, 0)])[0]
        gdraw.circle(s, dark, head, r * 0.38)
        gdraw.circle(s, accent, _rot(cx, cy, ang, [(r * 1.5, -r * 0.15)])[0], max(1, r * 0.1))
        # tollak a hátán
        for i in range(3):
            fp = _rot(cx, cy, ang, [(-r * 0.2 - i * r * 0.3, 0), (-r * 0.5 - i * r * 0.3, -r * 0.45)])
            gdraw.line(s, accent, fp[0], fp[1], 2)
        gdraw.circle(s, team, (cx, cy + bob), r * 0.48)
        gdraw.circle(s, base, (cx, cy + bob), r * 0.34)
    elif shape == "caster":
        glow = 0.6 + 0.4 * math.sin(t * 4)
        gdraw.circle(s, mix(accent, (0, 0, 0), 0.5), (cx, cy), r * (1.2 + glow * 0.15), 1)
        p = _rot(cx, cy, ang, [(r * 1.1, 0), (0, r * 0.85), (-r * 0.9, 0), (0, -r * 0.85)])
        gdraw.polygon(s, team, p)
        p2 = _rot(cx, cy, ang, [(r * 0.8, 0), (0, r * 0.6), (-r * 0.6, 0), (0, -r * 0.6)])
        gdraw.polygon(s, base, p2)
        gdraw.circle(s, mix(accent, (255, 255, 255), glow * 0.5), _rot(cx, cy, ang, [(r * 0.9, r * 0.5)])[0], r * 0.3)
    elif shape == "beast":
        body = _ellipse(cx, cy, ang, r * 1.1, r * 0.8, 18)
        gdraw.polygon(s, dark, body)
        gdraw.polygon(s, shade(base, 0.75), _ellipse(cx, cy, ang, r * 0.9, r * 0.62, 18))
        head = _rot(cx, cy, ang, [(r * 1.15, 0)])[0]
        gdraw.circle(s, dark, head, r * 0.45)
        for sy in (-1, 1):
            tk = _rot(cx, cy, ang, [(r * 1.3, sy * r * 0.25), (r * 1.75, sy * r * 0.45), (r * 1.8, sy * r * 0.15)])
            gdraw.lines(s, (235, 230, 210), False, tk, max(2, int(r * 0.12)))
        hw = _rot(cx, cy, ang, [(-r * 0.35, -r * 0.4), (r * 0.35, -r * 0.4), (r * 0.35, r * 0.4), (-r * 0.35, r * 0.4)])
        gdraw.polygon(s, team, hw)
        gdraw.polygon(s, accent, hw, 2)
    elif shape == "flyer":
        flap = math.sin(t * 9) * 0.35
        for sy in (-1, 1):
            w = _rot(cx, cy, ang, [(r * 0.3, 0), (-r * 0.3, sy * r * (1.6 + flap)), (-r * 0.7, sy * r * (1.2 + flap)), (-r * 0.4, 0)])
            gdraw.polygon(s, dark, w)
            gdraw.polygon(s, accent, w, 1)
        gdraw.polygon(s, team, _ellipse(cx, cy, ang, r * 0.9, r * 0.35))
        gdraw.polygon(s, base, _ellipse(cx, cy, ang, r * 0.65, r * 0.22))
        gdraw.circle(s, light, _rot(cx, cy, ang, [(r * 0.9, 0)])[0], r * 0.22)
    elif shape == "bug":
        for i in (-1, 0, 1):
            lw = math.sin(t * 18 + i) * r * 0.3 if moving else 0
            for sy in (-1, 1):
                p = _rot(cx, cy, ang, [(i * r * 0.4, 0), (i * r * 0.5 + lw * sy, sy * r * 1.15)])
                gdraw.line(s, dark, p[0], p[1], 1)
        gdraw.polygon(s, team, _ellipse(cx, cy, ang, r * 1.1, r * 0.7))
        gdraw.polygon(s, base, _ellipse(cx, cy, ang, r * 0.85, r * 0.5))
        for sy in (-1, 1):
            m = _rot(cx, cy, ang, [(r * 0.9, sy * r * 0.2), (r * 1.4, sy * r * 0.5)])
            gdraw.line(s, light, m[0], m[1], 2)
    elif shape == "spitter":
        gdraw.polygon(s, team, _ellipse(cx, cy, ang, r * 1.1, r * 0.85))
        gdraw.polygon(s, base, _ellipse(cx, cy, ang, r * 0.9, r * 0.65))
        for sy in (-1, 1):
            gdraw.circle(s, accent, _rot(cx, cy, ang, [(-r * 0.3, sy * r * 0.4)])[0], r * 0.25)
        tube = _rot(cx, cy, ang, [(r * 0.5, 0), (r * 1.5, 0)])
        gdraw.line(s, dark, tube[0], tube[1], max(2, int(r * 0.35)))
    elif shape == "artillery":
        gdraw.polygon(s, dark, _ellipse(cx, cy, ang, r * 1.1, r * 0.9, 16))
        sack = 0.8 + 0.2 * math.sin(t * 3)
        gdraw.circle(s, accent, _rot(cx, cy, ang, [(-r * 0.3, 0)])[0], r * 0.55 * sack)
        gdraw.circle(s, team, _rot(cx, cy, ang, [(-r * 0.3, 0)])[0], r * 0.55 * sack, 2)
        tube = _rot(cx, cy, ang, [(r * 0.3, 0), (r * 1.7, 0)])
        gdraw.line(s, base, tube[0], tube[1], max(3, int(r * 0.4)))
    elif shape == "horror":
        for i in range(7):
            a = i / 7 * 6.283
            wig = math.sin(t * 3 + i * 1.7) * 0.6
            pts = []
            for k in range(5):
                rr = r * (0.7 + k * 0.22)
                aa = a + wig * k * 0.15
                pts.append((cx + math.cos(aa) * rr, cy + math.sin(aa) * rr))
            gdraw.lines(s, dark, False, pts, max(2, int(r * 0.18)))
        gdraw.circle(s, team, (cx, cy), r * 0.85)
        gdraw.circle(s, base, (cx, cy), r * 0.7)
        for i in range(3):
            ep = _rot(cx, cy, ang, [(r * 0.35, (i - 1) * r * 0.3)])[0]
            gdraw.circle(s, accent, ep, max(1, r * 0.1))
    else:
        gdraw.circle(s, team, (cx, cy), r)
        gdraw.circle(s, base, (cx, cy), r * 0.75)


def draw_building(s, style, shape, rect, base, team, accent, t, progress=1.0, powered=True):
    """Épület rajzolása a képernyő-téglalapba."""
    x, y, w, h = rect
    cx, cy = x + w / 2, y + h / 2
    light = shade(base, 1.25)
    dark = shade(base, 0.6)
    if style == "aztec":
        steps = 4 if shape in ("hq", "barracks", "aerie", "observatory", "tech", "pen") else 2
        if shape == "obelisk":
            gdraw.rect(s, dark, (x + 4, y + 4, w - 8, h - 8))
            gdraw.polygon(s, light, [(cx, y + 2), (x + w - 6, cy), (cx, y + h - 2), (x + 6, cy)])
            glow = 0.6 + 0.4 * math.sin(t * 3)
            gdraw.circle(s, mix(accent, (255, 255, 255), glow * 0.4), (cx, cy), w * 0.14)
            gdraw.rect(s, team, (x + 4, y + 4, w - 8, h - 8), 2)
            return
        if shape == "tower":
            gdraw.rect(s, dark, (x + 3, y + 3, w - 6, h - 6))
            gdraw.circle(s, base, (cx, cy), w * 0.36)
            gdraw.circle(s, accent, (cx, cy), w * 0.2)
            gdraw.circle(s, team, (cx, cy), w * 0.36, 2)
            return
        for i in range(steps):
            m = i * min(w, h) * 0.11
            c = mix(dark, light, i / max(1, steps - 1))
            gdraw.rect(s, c, (x + 2 + m, y + 2 + m, w - 4 - 2 * m, h - 4 - 2 * m))
            gdraw.rect(s, shade(c, 0.7), (x + 2 + m, y + 2 + m, w - 4 - 2 * m, h - 4 - 2 * m), 1)
        m = steps * min(w, h) * 0.11
        top = (x + 2 + m, y + 2 + m, w - 4 - 2 * m, h - 4 - 2 * m)
        if shape == "hq":
            gdraw.rect(s, accent, top)
            gdraw.rect(s, team, top, 2)
        elif shape == "observatory":
            gdraw.circle(s, shade(accent, 0.8), (cx, cy), top[2] / 2 + 2)
            gdraw.line(s, light, (cx, cy), (cx + top[2] * 0.6, cy - top[3] * 0.6), 3)
        elif shape == "aerie":
            for i in range(5):
                a = i / 5 * 6.283 + t * 0.3
                gdraw.line(s, accent, (cx, cy), (cx + math.cos(a) * top[2] * 0.6, cy + math.sin(a) * top[3] * 0.6), 3)
        elif shape == "pen":
            gdraw.rect(s, (110, 85, 50), top, 3)
        else:
            gdraw.rect(s, shade(accent, 0.7), top)
        # tollas zászlók a sarkokon
        for px, py in ((x + 4, y + 4), (x + w - 5, y + 4), (x + 4, y + h - 5), (x + w - 5, y + h - 5)):
            gdraw.circle(s, team, (px, py), 3)
    elif style == "nordic":
        if shape == "tower":
            gdraw.rect(s, (110, 110, 115), (x + 2, y + 2, w - 4, h - 4))
            for i in range(4):
                gdraw.rect(s, (80, 80, 85), (x + 2 + i * (w - 4) / 4, y + 2, (w - 4) / 8, 6))
            gdraw.rect(s, base, (x + 8, y + 10, w - 16, h - 18))
            gdraw.rect(s, team, (x + 2, y + 2, w - 4, h - 4), 2)
            return
        if shape == "pen":
            gdraw.rect(s, (90, 120, 70), (x + 2, y + 2, w - 4, h - 4))
            for i in range(0, int(w), 8):
                gdraw.circle(s, dark, (x + 3 + i, y + 3), 3)
                gdraw.circle(s, dark, (x + 3 + i, y + h - 4), 3)
            for i in range(0, int(h), 8):
                gdraw.circle(s, dark, (x + 3, y + 3 + i), 3)
                gdraw.circle(s, dark, (x + w - 4, y + 3 + i), 3)
            gdraw.rect(s, base, (cx - w * 0.2, cy - h * 0.15, w * 0.4, h * 0.3))
            gdraw.rect(s, team, (cx - w * 0.2, cy - h * 0.15, w * 0.4, h * 0.3), 2)
            return
        if shape == "aerie":
            gdraw.polygon(s, (100, 100, 105), [(x + 3, y + h - 3), (x + w * 0.3, y + 4), (x + w * 0.75, y + 8), (x + w - 3, y + h - 3)])
            gdraw.circle(s, base, (cx, cy + 4), w * 0.22)
            gdraw.circle(s, team, (cx, cy + 4), w * 0.22, 2)
            return
        # hosszúház: tető gerinccel
        roof = shade(base, 0.8)
        gdraw.rect(s, dark, (x + 2, y + 2, w - 4, h - 4))
        gdraw.rect(s, roof, (x + 4, y + 4, w - 8, h - 8))
        horiz = w >= h
        for i in range(1, 7):
            if horiz:
                yy = y + 4 + (h - 8) * i / 7
                gdraw.line(s, shade(roof, 0.8), (x + 4, yy), (x + w - 5, yy), 1)
            else:
                xx = x + 4 + (w - 8) * i / 7
                gdraw.line(s, shade(roof, 0.8), (xx, y + 4), (xx, y + h - 5), 1)
        if horiz:
            gdraw.line(s, light, (x + 6, cy), (x + w - 7, cy), 3)
        else:
            gdraw.line(s, light, (cx, y + 6), (cx, y + h - 7), 3)
        if shape == "hq":
            for px in (x + 6, x + w - 7):
                gdraw.circle(s, accent, (px, cy), 5)
            for i in range(4):
                gdraw.circle(s, team, (x + w * (0.2 + i * 0.2), y + 6), 4)
                gdraw.circle(s, team, (x + w * (0.2 + i * 0.2), y + h - 7), 4)
        elif shape == "tech":
            glow = 0.6 + 0.4 * math.sin(t * 5)
            gdraw.circle(s, mix((255, 120, 30), (255, 230, 120), glow), (cx, cy), w * 0.13)
        elif shape == "dropoff":
            for i in range(3):
                gdraw.line(s, (120, 80, 40), (x + 6, y + h - 8 - i * 4), (x + w - 7, y + h - 8 - i * 4), 3)
        gdraw.rect(s, team, (x + 2, y + 2, w - 4, h - 4), 2)
    else:  # organic
        pulse = 1 + 0.04 * math.sin(t * 2 + x * 0.01)
        if shape == "tower":
            gdraw.circle(s, dark, (cx, cy), w * 0.42 * pulse)
            for i in range(6):
                a = i / 6 * 6.283 + t * 0.2
                gdraw.polygon(s, (220, 210, 190), [(cx + math.cos(a) * w * 0.15, cy + math.sin(a) * w * 0.15),
                                                         (cx + math.cos(a + 0.2) * w * 0.48, cy + math.sin(a + 0.2) * w * 0.48),
                                                         (cx + math.cos(a + 0.4) * w * 0.15, cy + math.sin(a + 0.4) * w * 0.15)])
            gdraw.circle(s, team, (cx, cy), w * 0.16)
            return
        lobes = 5 if shape == "hq" else 3
        for i in range(lobes):
            a = i / lobes * 6.283 + 0.5
            px, py = cx + math.cos(a) * w * 0.2, cy + math.sin(a) * h * 0.2
            gdraw.ellipse(s, dark, (px - w * 0.3 * pulse, py - h * 0.3 * pulse, w * 0.6 * pulse, h * 0.6 * pulse))
        gdraw.ellipse(s, base, (cx - w * 0.36 * pulse, cy - h * 0.36 * pulse, w * 0.72 * pulse, h * 0.72 * pulse))
        for i in range(5):
            a = i / 5 * 6.283 + t * 0.1
            gdraw.line(s, shade(base, 1.4), (cx, cy), (cx + math.cos(a) * w * 0.33, cy + math.sin(a) * h * 0.33), 1)
        if shape == "hq":
            gdraw.ellipse(s, (30, 10, 30), (cx - w * 0.12, cy - h * 0.08, w * 0.24, h * 0.16))
        if shape == "aerie":
            for i in range(4):
                a = i / 4 * 6.283
                gdraw.line(s, (230, 220, 200), (cx, cy), (cx + math.cos(a) * w * 0.48, cy + math.sin(a) * h * 0.48), 3)
        if shape in ("tech", "house"):
            g = 0.6 + 0.4 * math.sin(t * 2.5)
            gdraw.circle(s, mix(accent, (255, 255, 255), g * 0.3), (cx, cy), w * 0.12)
        gdraw.ellipse(s, team, (cx - w * 0.36 * pulse, cy - h * 0.36 * pulse, w * 0.72 * pulse, h * 0.72 * pulse), 2)
    if not powered:
        gdraw.line(s, (255, 60, 40), (x + 4, y + 4), (x + w - 4, y + h - 4), 3)


def draw_resource(s, r, sx, sy, t, color):
    if r.kind == "wood":
        v = r.variant % 4
        c = (30 + v * 6, 95 + v * 8, 40)
        gdraw.circle(s, (20, 50, 25), (sx + 16 + 2, sy + 16 + 3), 14)
        gdraw.circle(s, c, (sx + 16, sy + 16), 13)
        gdraw.circle(s, shade(c, 1.3), (sx + 13 + v, sy + 12), 7)
        gdraw.circle(s, shade(c, 1.55), (sx + 11 + v, sy + 10), 3)
    elif r.kind == "stone":
        w = r.w * TILE
        for i, (ox, oy, rr) in enumerate(((0.3, 0.35, 0.28), (0.68, 0.38, 0.25), (0.45, 0.7, 0.3), (0.78, 0.72, 0.18))):
            c = (150 - i * 10, 150 - i * 10, 158 - i * 8)
            gdraw.circle(s, shade(c, 0.6), (sx + ox * w + 2, sy + oy * w + 3), rr * w)
            gdraw.circle(s, c, (sx + ox * w, sy + oy * w), rr * w)
            gdraw.circle(s, shade(c, 1.3), (sx + ox * w - rr * w * 0.3, sy + oy * w - rr * w * 0.3), rr * w * 0.35)
    elif r.kind == "food":
        w = r.w * TILE
        fill = r.amount / r.max_amount
        for i, (ox, oy) in enumerate(((0.3, 0.3), (0.7, 0.3), (0.5, 0.55), (0.28, 0.75), (0.72, 0.75))):
            gdraw.circle(s, (40, 100, 45), (sx + ox * w, sy + oy * w), w * 0.2)
            if fill > i / 6:
                for k in range(3):
                    a = k * 2.1 + i
                    gdraw.circle(s, (210, 50, 70), (sx + ox * w + math.cos(a) * 5, sy + oy * w + math.sin(a) * 5), 3)
    elif r.kind == "meteor":
        w = r.w * TILE
        cx, cy = sx + w / 2, sy + w / 2
        gdraw.circle(s, (60, 50, 45), (cx, cy), w * 0.48)
        gdraw.circle(s, (40, 32, 30), (cx, cy), w * 0.38)
        g = 0.6 + 0.4 * math.sin(t * 2.5 + r.variant)
        for i in range(5):
            a = i * 1.26 + r.variant
            px, py = cx + math.cos(a) * w * 0.17, cy + math.sin(a) * w * 0.17
            pts = [(px, py - 9), (px + 5, py), (px, py + 7), (px - 5, py)]
            gdraw.polygon(s, mix(color, (255, 255, 255), g * 0.4), pts)
            gdraw.polygon(s, shade(color, 0.6), pts, 1)


ICON_W, ICON_H = 76, 56


def make_icon(fonts, defn, faction, team, kind="unit", sprite=None):
    surf = pygame.Surface((ICON_W, ICON_H))
    base = tuple(faction["color"])
    accent = tuple(faction["accent"])
    top = shade(base, 0.35)
    for yy in range(ICON_H):
        gdraw.line(surf, mix(top, (12, 14, 18), yy / ICON_H), (0, yy), (ICON_W, yy))
    if sprite is not None:
        sw, sh = sprite.get_size()
        sc = min((ICON_W - 4) / sw, (ICON_H - 16) / sh, 1.0)
        img = pygame.transform.smoothscale(sprite, (int(sw * sc), int(sh * sc))) if sc < 1 else sprite
        surf.blit(img, (ICON_W / 2 - img.get_width() / 2, (ICON_H - 14) / 2 - img.get_height() / 2))
    elif kind == "unit":  # még nincs grafika: nagy alak, felirat nélkül (kicsiben olvashatatlan; a név a súgóban)
        r = 17 if defn.get("radius", 0.4) < 0.7 else 21
        draw_shape(surf, defn.get("shape", ""), ICON_W / 2, ICON_H / 2, r, base, team, accent, -0.6, 1.0)
        gdraw.rect(surf, shade(base, 0.8), (0, 0, ICON_W, ICON_H), 1)
        return surf
    elif kind == "building":
        w, h = defn["size"]
        sc = 46 / max(w, h)
        bw, bh = w * sc, h * sc
        draw_building(surf, faction.get("style"), defn.get("shape"),
                      (ICON_W / 2 - bw / 2, ICON_H / 2 - bh / 2, bw, bh), base, team, accent, 1.0)
        gdraw.rect(surf, shade(base, 0.8), (0, 0, ICON_W, ICON_H), 1)
        return surf
    else:
        gdraw.polygon(surf, accent, [(ICON_W / 2, 6), (ICON_W / 2 + 16, 24), (ICON_W / 2 + 6, 24),
                                           (ICON_W / 2 + 6, 40), (ICON_W / 2 - 6, 40), (ICON_W / 2 - 6, 24),
                                           (ICON_W / 2 - 16, 24)])
    from .i18n import T
    name = T(defn["name"])
    f = fonts["tiny"]
    txt = f.render(name, True, (235, 235, 225))
    if txt.get_width() > ICON_W - 4:
        while name and f.size(name + ".")[0] > ICON_W - 4:
            name = name[:-1]
        txt = f.render(name + ".", True, (235, 235, 225))
    gdraw.rect(surf, (0, 0, 0), (0, ICON_H - 14, ICON_W, 14))
    surf.blit(txt, (ICON_W / 2 - txt.get_width() / 2, ICON_H - 14))
    gdraw.rect(surf, shade(base, 0.8), (0, 0, ICON_W, ICON_H), 1)
    return surf
