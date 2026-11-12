"""Faj-címerek (Warcraft III fajlogóinak hangulatát idéző, de saját tervezésű emblémák).

Minden címer rétegekből áll: egy réteg = alfa-maszk (1024×1024-es munkavásznon
rajzolva) + anyag. A maszkból domborzat lesz (elmosott magasságtérkép), arra
fémes megvilágítás, csillanás, gravírozás (barázda-maszk), sötét kontúr és
izzás kerül; a végén 4× élsimítással 256 px-re kicsinyítjük.
"""
import math

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

S = 1024
MATS = {  # (világos, sötét)
    "gold": ((255, 230, 150), (120, 70, 18)), "silver": ((238, 242, 248), (66, 72, 84)),
    "iron": ((176, 176, 186), (36, 36, 44)), "bronze": ((244, 196, 126), (86, 48, 20)),
    "jade": ((140, 246, 200), (8, 78, 58)), "wood": ((206, 156, 100), (66, 38, 18)),
    "fur": ((178, 118, 70), (40, 22, 10)), "ochre": ((246, 196, 116), (104, 56, 18)),
    "bone": ((252, 246, 230), (126, 112, 92)), "purple": ((206, 140, 236), (44, 16, 64)),
    "red": ((244, 96, 72), (86, 14, 8)), "turq": ((130, 236, 226), (8, 86, 88)),
    "black": ((96, 96, 104), (8, 8, 12)), "cap": ((244, 118, 86), (96, 26, 16)),
    "chitin": ((150, 100, 190), (24, 8, 40)), "sinew": ((236, 220, 190), (110, 90, 70)),
}
LIGHT = np.array([-0.5, -0.62, 0.6])
LIGHT /= np.linalg.norm(LIGHT)
HALF = (LIGHT + np.array([0, 0, 1.0]))
HALF /= np.linalg.norm(HALF)


class Canvas:
    def __init__(self):
        self.layers = []  # (maszk, anyag, barázdák, izzás-szín, domborzat-sugár)

    def new(self):
        m = Image.new("L", (S, S), 0)
        return m, ImageDraw.Draw(m)

    def add(self, mask, mat, grooves=None, glow=None, relief=14):
        self.layers.append((mask, mat, grooves, glow, relief))


def P(x, y):
    """Normalizált (-1..1) koordinátából vászonpixel."""
    return (S / 2 + x * S * 0.46, S / 2 + y * S * 0.46)


def circle(d, x, y, r, fill=255):
    cx, cy = P(x, y)
    rr = r * S * 0.46
    d.ellipse((cx - rr, cy - rr, cx + rr, cy + rr), fill=fill)


def ellipse(d, x, y, rx, ry, fill=255):
    cx, cy = P(x, y)
    d.ellipse((cx - rx * S * 0.46, cy - ry * S * 0.46, cx + rx * S * 0.46, cy + ry * S * 0.46), fill=fill)


def ring(d, x, y, r, w):
    circle(d, x, y, r + w / 2)
    circle(d, x, y, r - w / 2, 0)


def poly(d, pts, fill=255):
    d.polygon([P(x, y) for x, y in pts], fill=fill)


def line(d, pts, w, fill=255):
    pp = [P(x, y) for x, y in pts]
    d.line(pp, fill=fill, width=int(w * S * 0.46), joint="curve")
    for x, y in (pts[0], pts[-1]):
        circle(d, x, y, w / 2, fill)


def bezier(p0, p1, p2, p3, n=40):
    out = []
    for i in range(n + 1):
        t = i / n
        a = (1 - t) ** 3
        b = 3 * (1 - t) ** 2 * t
        c = 3 * (1 - t) * t * t
        e = t ** 3
        out.append((a * p0[0] + b * p1[0] + c * p2[0] + e * p3[0], a * p0[1] + b * p1[1] + c * p2[1] + e * p3[1]))
    return out


def taper(d, pts, w0, w1, fill=255):
    """Elvékonyodó vonal (csáp, toll, szarv)."""
    n = len(pts)
    for i, (x, y) in enumerate(pts):
        circle(d, x, y, (w0 + (w1 - w0) * i / max(1, n - 1)) / 2, fill)


def rot(pts, ang, cx=0.0, cy=0.0):
    c, s = math.cos(ang), math.sin(ang)
    return [(cx + (x - cx) * c - (y - cy) * s, cy + (x - cx) * s + (y - cy) * c) for x, y in pts]


def feather(d, g, x, y, ang, length, width):
    """Toll: elvékonyodó levél forma, közepén gravírozott gerinc."""
    pts = []
    for i in range(21):
        t = i / 20
        pts.append((t * length, math.sin(t * math.pi) ** 0.8 * width * (1 - 0.3 * t)))
    shape = pts + [(px, -py) for px, py in reversed(pts)]
    poly(d, rot([(x + px, y + py) for px, py in shape], ang, x, y))
    line(g, rot([(x, y), (x + length * 0.95, y)], ang, x, y), 0.012)
    for k in range(1, 7):
        t = k / 7
        bx = x + t * length
        line(g, rot([(bx, y), (bx + length * 0.08, y + width * 0.7)], ang, x, y), 0.006)
        line(g, rot([(bx, y), (bx + length * 0.08, y - width * 0.7)], ang, x, y), 0.006)


# ---------------------------------------------------------------------------
def elves(c):
    m, d = c.new()
    g, gd = c.new()
    for k in range(16):  # lépcsős napsugarak
        a = k / 16 * 2 * math.pi - math.pi / 2
        long = k % 2 == 0
        r0, r1 = 0.66, 0.98 if long else 0.86
        w = 0.11 if long else 0.08
        steps = [(r0, w), (r0 + (r1 - r0) * 0.45, w * 0.7), (r0 + (r1 - r0) * 0.75, w * 0.42), (r1, 0.0)]
        pts = [(r, ww) for r, ww in steps] + [(r, -ww) for r, ww in reversed(steps)]
        poly(d, [(math.cos(a) * r - math.sin(a) * ww, math.sin(a) * r + math.cos(a) * ww) for r, ww in pts])
    ring(d, 0, 0, 0.66, 0.1)
    ring(gd, 0, 0, 0.66, 0.012)
    c.add(m, "gold", g)
    m, d = c.new()
    g, gd = c.new()
    circle(d, 0, 0, 0.6)
    ring(gd, 0, 0, 0.45, 0.018)
    for k in range(4):  # lépcsős glifák
        a = k * math.pi / 2 + math.pi / 4
        x, y = math.cos(a) * 0.5, math.sin(a) * 0.5
        sq = [(-0.06, -0.06), (0.06, -0.06), (0.06, 0.0), (0.02, 0.0), (0.02, 0.06), (-0.06, 0.06)]
        line(gd, [(x + px, y + py) for px, py in rot(sq + [sq[0]], a)], 0.014)
    for k in range(12):
        a = k / 12 * 2 * math.pi
        circle(gd, math.cos(a) * 0.55, math.sin(a) * 0.55, 0.018)
    c.add(m, "jade", g, relief=10)
    m, d = c.new()
    g, gd = c.new()
    circle(d, 0, 0, 0.3)
    ring(gd, 0, 0, 0.24, 0.012)
    line(gd, [(-0.16, -0.1), (-0.04, -0.06)], 0.022)  # zord napisten-arc
    line(gd, [(0.16, -0.1), (0.04, -0.06)], 0.022)
    ellipse(gd, -0.09, -0.02, 0.04, 0.018)
    ellipse(gd, 0.09, -0.02, 0.04, 0.018)
    line(gd, [(-0.08, 0.12), (0.08, 0.12)], 0.022)
    line(gd, [(0, -0.02), (0, 0.06)], 0.018)
    for k in (-1, 1):
        line(gd, [(k * 0.06, 0.16), (k * 0.03, 0.2)], 0.012)
    c.add(m, "gold", g, relief=22)
    m, d = c.new()
    g, gd = c.new()
    for side in (-1, 1):  # tollak alul
        for k in range(3):
            feather(d, gd, side * 0.3, 0.62, math.pi / 2 - side * (0.55 + k * 0.3), 0.52 - k * 0.06, 0.075)
    c.layers.insert(0, (m, "turq", g, None, 10))


def ursan(c):
    m, d = c.new()
    g, gd = c.new()
    for side in (-1, 1):  # keresztbe tett fejszék a pajzs mögött
        line(d, [(side * -0.85, 0.85), (side * 0.75, -0.75)], 0.07)
    c.add(m, "wood", relief=10)
    m, d = c.new()
    g, gd = c.new()
    for side in (-1, 1):
        x, y = side * 0.72, -0.72
        blade = [(0, 0), (side * 0.12, -0.26), (side * 0.32, -0.12), (side * 0.3, 0.12), (side * 0.12, 0.2)]
        poly(d, [(x + px, y + py) for px, py in rot(blade, side * -0.3)])
        line(gd, [(x + side * 0.27, y - 0.1), (x + side * 0.27, y + 0.12)], 0.012)
    c.add(m, "silver", g, relief=14)
    m, d = c.new()
    g, gd = c.new()
    circle(d, 0, 0.04, 0.78)  # deszkapajzs
    for k in range(-5, 6):
        line(gd, [(k * 0.14, -0.8), (k * 0.14, 0.8)], 0.012)
    c.add(m, "wood", g, relief=8)
    m, d = c.new()
    g, gd = c.new()
    ring(d, 0, 0.04, 0.78, 0.1)
    for k in range(16):
        a = k / 16 * 2 * math.pi
        circle(d, math.cos(a) * 0.78, 0.04 + math.sin(a) * 0.78, 0.035)
    c.add(m, "iron", relief=16)
    m, d = c.new()  # medvefej
    g, gd = c.new()
    for side in (-1, 1):
        circle(d, side * 0.33, -0.18, 0.13)
    ellipse(d, 0, 0.08, 0.4, 0.38)
    ellipse(d, 0, 0.35, 0.3, 0.16)
    for k in range(9):  # bunda-barázdák
        a = -0.9 + k * 0.22
        line(gd, [(math.sin(a) * 0.36, 0.08 + math.cos(a) * 0.3), (math.sin(a) * 0.42, 0.08 + math.cos(a) * 0.36)], 0.012)
    line(gd, [(-0.2, 0.0), (-0.08, 0.04)], 0.02)
    line(gd, [(0.2, 0.0), (0.08, 0.04)], 0.02)
    c.add(m, "fur", g, relief=20)
    m, d = c.new()
    g, gd = c.new()
    ellipse(d, 0, 0.27, 0.17, 0.13)
    line(gd, [(0, 0.3), (0, 0.38)], 0.012)
    line(gd, [(-0.07, 0.4), (0, 0.38), (0.07, 0.4)], 0.012)
    c.add(m, "bone", g, relief=14)
    m, d = c.new()
    ellipse(d, 0, 0.22, 0.07, 0.045)
    for side in (-1, 1):
        ellipse(d, side * 0.13, 0.02, 0.035, 0.03)
    c.add(m, "black", relief=8)
    m, d = c.new()  # szarvas sisak
    g, gd = c.new()
    poly(d, [(-0.36, -0.12), (-0.3, -0.34), (0.0, -0.44), (0.3, -0.34), (0.36, -0.12), (0.0, -0.08)])
    line(gd, [(0, -0.43), (0, -0.1)], 0.02)
    line(gd, [(-0.35, -0.13), (0.0, -0.09), (0.35, -0.13)], 0.016)
    c.add(m, "silver", g, relief=16)
    m, d = c.new()
    for side in (-1, 1):
        taper(d, bezier((side * 0.32, -0.26), (side * 0.6, -0.3), (side * 0.72, -0.55), (side * 0.6, -0.85), 50), 0.12, 0.01)
    c.add(m, "bone", relief=16)


def croftian(c):
    m, d = c.new()
    g, gd = c.new()
    for k in range(9):  # csápkoszorú
        a = k / 9 * 2 * math.pi
        p0 = (math.cos(a) * 0.3, math.sin(a) * 0.3)
        p1 = (math.cos(a) * 0.7, math.sin(a) * 0.7)
        p2 = (math.cos(a + 0.6) * 0.95, math.sin(a + 0.6) * 0.95)
        p3 = (math.cos(a + 1.0) * 0.72, math.sin(a + 1.0) * 0.72)
        pts = bezier(p0, p1, p2, p3, 60)
        taper(d, pts, 0.16, 0.02)
        for t in range(6, 54, 8):  # tapadókorong-barázdák
            circle(gd, pts[t][0], pts[t][1], 0.016)
    for side in (-1, 1):  # rágók
        taper(d, bezier((side * 0.16, 0.3), (side * 0.32, 0.55), (side * 0.18, 0.78), (side * 0.02, 0.88), 40), 0.12, 0.01)
    c.add(m, "chitin", g, relief=20)
    m, d = c.new()
    g, gd = c.new()
    poly(d, [(math.cos(t) * 0.42, math.sin(t) * 0.24 * (1 if t < math.pi else 1)) for t in
             [i / 40 * 2 * math.pi for i in range(40)]])
    ring(gd, 0, 0, 0.36, 0.012)
    c.add(m, "bone", g, relief=16)
    m, d = c.new()
    circle(d, 0, 0, 0.2)
    c.add(m, "jade", glow=(150, 255, 120), relief=10)
    m, d = c.new()
    ellipse(d, 0, 0, 0.045, 0.19)
    c.add(m, "black", relief=8)
    m, d = c.new()
    for k, (x, y, r) in enumerate(((-0.42, -0.42, 0.07), (0.44, -0.4, 0.06), (0.0, -0.56, 0.055))):
        circle(d, x, y, r)
    c.add(m, "jade", glow=(150, 255, 120), relief=8)


def wullof(c):
    m, d = c.new()
    g, gd = c.new()
    ring(d, 0, -0.1, 0.66, 0.1)  # álomfogó-abroncs, bőrrel tekerve
    for k in range(40):
        a = k / 40 * 2 * math.pi
        line(gd, [(math.cos(a) * 0.61, -0.1 + math.sin(a) * 0.61), (math.cos(a + 0.12) * 0.71, -0.1 + math.sin(a + 0.12) * 0.71)],
             0.012)
    c.add(m, "wood", g, relief=14)
    m, d = c.new()
    for k in range(8):  # háló
        a = k / 8 * 2 * math.pi
        b = (k + 3) / 8 * 2 * math.pi
        line(d, [(math.cos(a) * 0.6, -0.1 + math.sin(a) * 0.6), (math.cos(b) * 0.6, -0.1 + math.sin(b) * 0.6)], 0.012)
    c.add(m, "sinew", relief=4)
    m, d = c.new()  # félhold
    circle(d, 0.24, -0.32, 0.22)
    circle(d, 0.33, -0.39, 0.2, 0)
    c.add(m, "silver", relief=14)
    m, d = c.new()  # vonító dingó profilból
    g, gd = c.new()
    head = [(-0.26, 0.46), (-0.22, 0.22), (-0.2, 0.04), (-0.25, -0.12), (-0.38, -0.38), (-0.33, -0.43),
            (-0.17, -0.25), (-0.06, -0.21), (0.0, -0.24), (0.1, -0.12), (0.15, 0.06), (0.16, 0.24), (0.22, 0.46)]
    poly(d, head)
    poly(d, [(-0.04, -0.2), (0.02, -0.42), (0.11, -0.12)])  # hegyes fül
    poly(d, [(-0.3, -0.33), (-0.22, -0.3), (-0.26, -0.24), (-0.31, -0.28)], 0)  # nyitott pofa
    line(gd, [(0.03, -0.36), (0.06, -0.18)], 0.014)
    for k in range(7):  # nyak-szőrzet
        line(gd, [(-0.18 + k * 0.05, 0.44), (-0.14 + k * 0.05, 0.12)], 0.01)
    c.add(m, "ochre", g, relief=18)
    m, d = c.new()
    line(d, [(-0.2, -0.16), (-0.06, -0.14)], 0.03)  # harci festés
    line(d, [(-0.18, -0.06), (-0.02, -0.05)], 0.025)
    c.add(m, "red", relief=6)
    m, d = c.new()
    circle(d, -0.12, -0.22, 0.022)
    c.add(m, "black", relief=4)
    m, d = c.new()
    g, gd = c.new()
    for k, x in enumerate((-0.35, 0.0, 0.35)):  # függő tollak gyöngyökkel
        line(d, [(x * 0.9, 0.52), (x, 0.66)], 0.012)
        feather(d, gd, x, 0.66, math.pi / 2 + (k - 1) * 0.12, 0.34, 0.07)
    c.add(m, "bone", g, relief=10)
    m, d = c.new()
    for x in (-0.35, 0.0, 0.35):
        circle(d, x * 0.95, 0.62, 0.04)
    c.add(m, "turq", relief=10)


def mycel(c):
    m, d = c.new()
    rng = np.random.default_rng(5)
    for k in range(28):  # spórafonal-gyűrű: kígyózó szálak
        a = k / 28 * 2 * math.pi
        pts = bezier((math.cos(a) * 0.72, math.sin(a) * 0.72), (math.cos(a + 0.12) * 0.86, math.sin(a + 0.12) * 0.86),
                     (math.cos(a + 0.2) * 0.62, math.sin(a + 0.2) * 0.62), (math.cos(a + 0.3) * 0.78, math.sin(a + 0.3) * 0.78), 20)
        line(d, pts, 0.022)
    for k in range(6):
        a = k / 6 * 2 * math.pi + 0.3
        line(d, bezier((0, 0.3), (math.cos(a) * 0.3, 0.3 + math.sin(a) * 0.2), (math.cos(a) * 0.6, math.sin(a) * 0.6),
                       (math.cos(a) * 0.74, math.sin(a) * 0.74), 30), 0.018)
    c.add(m, "sinew", relief=6)
    m, d = c.new()
    g, gd = c.new()
    for x, h, w in ((-0.36, 0.22, 0.07), (0.38, 0.2, 0.06), (0.0, 0.42, 0.11)):  # tönkök
        poly(d, [(x - w, 0.56), (x - w * 0.8, 0.56 - h), (x + w * 0.8, 0.56 - h), (x + w, 0.56)])
    c.add(m, "bone", relief=12)
    m, d = c.new()
    g, gd = c.new()
    for x, y, rx, ry in ((-0.36, 0.3, 0.22, 0.13), (0.38, 0.33, 0.2, 0.12), (0.0, 0.08, 0.38, 0.24)):
        poly(d, [(x + math.cos(t) * rx, y - abs(math.sin(t)) * ry * (1 if math.sin(t) < 0 else 0.15)) for t in
                 [i / 40 * 2 * math.pi for i in range(40)]])
        for k in range(9):  lemezek = None  # noqa: E701 (csak olvashatóság)
    for x, y, rx in ((-0.36, 0.3, 0.22), (0.38, 0.33, 0.2), (0.0, 0.08, 0.38)):
        for k in range(1, 8):
            t = k / 8
            line(gd, [(x - rx + 2 * rx * t, y + 0.02), (x - rx * 0.6 + 1.2 * rx * t, y + 0.06)], 0.008)
    c.add(m, "cap", g, relief=18)
    m, d = c.new()
    for x, y, r in ((-0.12, -0.04, 0.05), (0.1, -0.08, 0.04), (0.2, 0.02, 0.035), (-0.24, 0.04, 0.03), (0.0, -0.12, 0.03),
                    (-0.4, 0.22, 0.03), (0.42, 0.26, 0.03)):
        circle(d, x, y, r)
    c.add(m, "bone", relief=8)
    m, d = c.new()
    for k in range(10):
        a = k / 10 * 2 * math.pi
        circle(d, math.cos(a) * 0.55, -0.1 + math.sin(a) * 0.5, 0.025)
    c.add(m, "turq", glow=(80, 230, 205), relief=6)


def imperium(c):
    m, d = c.new()
    g, gd = c.new()
    teeth = []
    for k in range(18):  # fogaskerék
        a0 = k / 18 * 2 * math.pi
        for da, r in ((0.0, 0.74), (0.05, 0.88), (0.14, 0.88), (0.19, 0.74)):
            teeth.append((math.cos(a0 + da) * r, math.sin(a0 + da) * r))
    poly(d, teeth)
    circle(d, 0, 0, 0.6, 0)
    for k in range(18):
        a = k / 18 * 2 * math.pi + 0.1
        circle(gd, math.cos(a) * 0.67, math.sin(a) * 0.67, 0.02)
    c.add(m, "iron", g, relief=16)
    m, d = c.new()
    g, gd = c.new()
    for side in (-1, 1):  # babérkoszorú
        for k in range(8):
            a = math.pi / 2 + side * (0.25 + k * 0.17)
            x, y = math.cos(a) * 0.62, math.sin(a) * 0.62
            leaf = [(0, 0), (0.07, -0.03), (0.14, 0), (0.07, 0.03)]
            poly(d, [(x + px, y + py) for px, py in rot(leaf, a + side * 1.9)])
    c.add(m, "gold", relief=10)
    m, d = c.new()  # aquila
    g, gd = c.new()
    for side in (-1, 1):
        for k in range(7):  # kiterjesztett szárny tolllépcsői
            x0 = side * (0.1 + k * 0.1)
            top = -0.3 - k * 0.075
            poly(d, [(x0, top), (x0 + side * 0.12, top - 0.02), (x0 + side * 0.1, top + 0.36 - k * 0.025), (x0, top + 0.3)])
            line(gd, [(x0 + side * 0.02, top + 0.04), (x0 + side * 0.06, top + 0.28)], 0.008)
    ellipse(d, 0, 0.0, 0.13, 0.26)
    poly(d, [(-0.12, 0.2), (0.12, 0.2), (0.16, 0.44), (0.0, 0.38), (-0.16, 0.44)])  # farok
    circle(d, -0.02, -0.3, 0.1)
    poly(d, [(-0.1, -0.32), (-0.24, -0.27), (-0.1, -0.24)])  # csőr
    for k in range(4):
        line(gd, [(-0.1, 0.02 + k * 0.06), (0.1, 0.02 + k * 0.06)], 0.008)
    c.add(m, "bronze", g, relief=20)
    m, d = c.new()
    poly(d, [(-0.3, 0.46), (0.3, 0.46), (0.28, 0.52), (-0.28, 0.52)])
    c.add(m, "gold", relief=8)
    m, d = c.new()
    circle(d, -0.05, -0.32, 0.025)
    poly(d, [(0, -0.06), (0.07, 0.04), (0, 0.14), (-0.07, 0.04)])
    c.add(m, "jade", glow=(120, 255, 190), relief=10)


DESIGNS = {"elves": elves, "ursan": ursan, "croftian": croftian, "wullof": wullof, "mycel": mycel,
           "imperium": imperium}


# ---------------------------------------------------------------------------
def _shade(mask, mat, grooves, relief):
    a = np.asarray(mask, np.float32) / 255.0
    h = np.asarray(mask.filter(ImageFilter.GaussianBlur(relief)), np.float32) / 255.0
    h = np.minimum(h, a) * 1.0
    if grooves is not None:
        gg = np.asarray(grooves.filter(ImageFilter.GaussianBlur(2.5)), np.float32) / 255.0
        h = h - gg * 0.35
    gy, gx = np.gradient(h * 60.0)
    n = np.stack([-gx, -gy, np.ones_like(h)], -1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    diff = np.clip(n @ LIGHT, 0, 1)
    spec = np.clip(n @ HALF, 0, 1) ** 30
    light, dark = (np.array(c, np.float32) for c in MATS[mat])
    yy = np.linspace(0.12, -0.12, S)[:, None]  # fentről világosabb (WC3-as aranyfény)
    t = np.clip(diff * 0.95 + yy + 0.05, 0, 1)[..., None]
    col = dark + (light - dark) * t + spec[..., None] * 255 * 0.85
    if grooves is not None:
        gm = np.asarray(grooves, np.float32)[..., None] / 255.0
        col = col * (1 - 0.45 * gm)
    return np.clip(col, 0, 255), a


def render_emblem(fid, size=256):
    c = Canvas()
    DESIGNS[fid](c)
    out = np.zeros((S, S, 4), np.float32)
    union = Image.new("L", (S, S), 0)
    for mask, *_ in c.layers:
        union = Image.fromarray(np.maximum(np.asarray(union), np.asarray(mask)))
    # vetett árnyék az egész címer alatt
    sh = np.asarray(union.filter(ImageFilter.GaussianBlur(14)), np.float32) / 255.0
    sh = np.roll(np.roll(sh, 14, 0), 10, 1)
    out[..., 3] = sh * 150
    for mask, mat, grooves, glow, relief in c.layers:
        if glow is not None:  # izzás-glória
            gl = np.asarray(mask.filter(ImageFilter.GaussianBlur(26)), np.float32) / 255.0 * 1.6
            gl = np.clip(gl, 0, 1)
            ga = gl * 200
            na = ga + out[..., 3] * (1 - ga / 255)
            out[..., :3] = (np.array(glow, np.float32) * ga[..., None] + out[..., :3] * (out[..., 3] * (1 - ga / 255))[..., None]) \
                / np.maximum(na, 1e-3)[..., None]
            out[..., 3] = na
        # sötét kontúr
        edge = mask.filter(ImageFilter.MaxFilter(9))
        e = np.asarray(edge, np.float32) / 255.0
        ea = e * 255
        na = ea + out[..., 3] * (1 - ea / 255)
        out[..., :3] = (np.array((14, 10, 8), np.float32) * ea[..., None] + out[..., :3] * (out[..., 3] * (1 - ea / 255))[..., None]) \
            / np.maximum(na, 1e-3)[..., None]
        out[..., 3] = na
        col, a = _shade(mask, mat, grooves, relief)
        aa = a * 255
        na = aa + out[..., 3] * (1 - a)
        out[..., :3] = (col * aa[..., None] + out[..., :3] * (out[..., 3] * (1 - a))[..., None]) / np.maximum(na, 1e-3)[..., None]
        out[..., 3] = na
    img = Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), "RGBA")
    return img.resize((size, size), Image.LANCZOS)
