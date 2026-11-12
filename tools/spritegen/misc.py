"""Nyersanyagok, lövedékek, effektek és terepcsempék."""
import math

import numpy as np
from PIL import Image

from .engine import Model, ellipsoid, particles

T = 32 * int(__import__('os').environ.get('TOTU_HD', '2') or 2)  # csempeméret (HD: 64)
K = T // 32


# --- nyersanyagok -----------------------------------------------------------
TREE_VARIANTS = 6


def tree(variant):
    """Fa: 0-2 lombos (dús, széles korona), 3-5 tűlevelű (magas, széles ágkoszorúk). A korona túlnyúlik a
    csempén, így a szomszédos fák egymásra lógnak, és összefüggő, sűrű erdőt adnak; a törzs helye csempénként
    kicsit eltolt, hogy ne legyen rácsszerű."""
    rng = np.random.default_rng(100 + variant)
    m = Model(f"tree{variant}")
    cx = T / 2 + rng.uniform(-3.5, 3.5)
    cy = T / 2 + rng.uniform(-3, 3)
    bark = (96, 66, 42)
    if variant < 3:
        m.capsule((cx, cy, 0), (cx + 0.8, cy, 22), 3.2, bark, r1=2.0, tex=0.1, stripe=(2, 1.6))
        for k in range(4):  # gyökerek
            a = k * 1.6 + variant
            m.capsule((cx, cy, 1.5), (cx + math.cos(a) * 6.5, cy + math.sin(a) * 5, 0.2), 1.3, bark, r1=0.4)
        greens = [(44, 108, 46), (60, 130, 52), (38, 92, 42), (74, 142, 58), (52, 118, 48)]
        for k in range(6):  # ágak
            a = k * 1.05 + variant
            m.capsule((cx, cy, 15 + k * 1.2), (cx + math.cos(a) * 11, cy + math.sin(a) * 9, 24 + k), 1.1, bark,
                      r1=0.5)
        for k in range(26):  # széles, dús korona csomókból
            a = rng.uniform(0, 2 * math.pi)
            rr = rng.uniform(0, 15)
            z = rng.uniform(22, 42) - rr * 0.45
            r = rng.uniform(5.5, 8.5)
            m.ellipsoid((cx + math.cos(a) * rr, cy + math.sin(a) * rr * 0.85, z), (r, r, r * 0.8),
                        greens[k % 5], tex=0.22, mat="fur")
        if variant == 2:  # virágzó fa: apró fehér-rózsaszín virágok a korona tetején
            for k in range(40):
                a = rng.uniform(0, 2 * math.pi)
                rr = rng.uniform(0, 14)
                m.ellipsoid((cx + math.cos(a) * rr, cy + math.sin(a) * rr * 0.85, 44 - rr * 0.6 + rng.uniform(0, 3)),
                            (0.7, 0.7, 0.5), (250, 214, 226) if k % 3 else (255, 250, 240), tex=0)
        m.height = 50
    else:
        m.capsule((cx, cy, 0), (cx, cy, 10), 2.6, bark, r1=2.0, tex=0.1)
        dark = (30 + variant * 3, 82, 50)
        for k in range(8):  # széles, lépcsős ágkoszorúk: minden réteg külön kúp, alul sötétebb peremmel
            z = 7 + k * 6.0
            r = 17.5 - k * 2.0
            col = dark if k % 2 else (40, 96, 58)
            m.capsule((cx, cy, z), (cx, cy, z + 9), r, col, r1=0.8, ends=False, tex=0.2, mat="fur")
            m.capsule((cx, cy, z - 0.6), (cx, cy, z + 0.6), r * 0.98, tuple(int(c * 0.6) for c in col), ends=False)
        m.capsule((cx, cy, 55), (cx, cy, 62), 1.6, dark, r1=0.1)
        m.height = 62
    return m


def rock_prop(variant):
    """Járhatatlan sziklacsempe kiemelkedő kőtömbjei."""
    rng = np.random.default_rng(300 + variant)
    m = Model(f"rock{variant}")
    for k in range(4 + variant % 2):
        x, y = rng.uniform(7, 25), rng.uniform(7, 25)
        r = rng.uniform(5, 8.5)
        c = int(rng.integers(98, 128))
        m.ellipsoid((x, y, r * 0.55), (r, r * 0.9, r * 0.95), (c, c - 4, c - 8), tex=0.18, spec=0.1)
    m.height = 16
    return m


def stone_pile():
    """Aranybánya (WC3-szerű): sziklahalom ácsolt tárnabejárattal, aranyerekkel, csillével és aranyhalommal.
    A bejárat délre (a néző felé) néz; a munkások kitermelés közben „bemennek” (a játék nem rajzolja őket)."""
    m = Model("stone")
    rng = np.random.default_rng(7)
    for k in range(10):  # a sziklahalom
        a = k / 10 * 2 * math.pi
        x, y = 32 + math.cos(a) * 15, 26 + math.sin(a) * 11
        r = rng.uniform(9, 13)
        c = int(rng.integers(98, 128))
        m.ellipsoid((x, y, r * 0.55), (r, r * 0.85, r * 0.8), (c + 12, c + 2, c - 12), tex=0.2, spec=0.1)
    m.ellipsoid((32, 26, 16), (17, 13, 13), (116, 104, 90), tex=0.2, spec=0.1)
    for k in range(9):  # aranyerek a sziklán
        a = rng.uniform(0, 2 * math.pi)
        d = rng.uniform(0.15, 0.8)
        dx, dy = math.cos(a) * d, math.sin(a) * d
        z = 16 + 13 * math.sqrt(max(0.0, 1 - dx * dx - dy * dy))  # a fő szikla felszínén
        m.ellipsoid((32 + dx * 17, 26 + dy * 13, z), (rng.uniform(3.5, 5.5), 1.6, 1.4), (252, 206, 64), tex=0.05, spec=0.9)
    # tárnabejárat: sötét nyílás, ácsolt keret
    m.box((32, 47, 7), (7, 5, 7), (18, 14, 12), tex=0.02)
    WOOD, WOOD2 = (120, 82, 46), (96, 64, 36)
    for x in (23.5, 40.5):
        m.cylinder((x, 52, 0), (x, 52, 16), 1.8, WOOD, tex=0.15, mat="logs")
    m.cylinder((21, 52, 16.5), (43, 52, 16.5), 2.0, WOOD2, tex=0.15, mat="logs")
    m.box((32, 52, 19.5), (5, 1.2, 1.6), WOOD, tex=0.1)  # tábla a gerendán
    for x in (28, 36):  # sínek
        m.box((x, 58, 0.5), (0.5, 7, 0.5), (90, 90, 96), spec=0.6)
    for y in range(52, 65, 3):
        m.box((32, 52 + (y - 52), 0.3), (6, 0.7, 0.3), WOOD2)
    # csille arannyal
    m.box((32, 60, 4), (4.5, 3.2, 2.4), (84, 60, 40), tex=0.1, mat="planks")
    m.ellipsoid((32, 60, 6.6), (4, 2.6, 1.8), (250, 204, 60), tex=0.05, spec=1.0)
    for x in (28.5, 35.5):
        for y in (57.8, 62.2):
            m.cylinder((x, y, 1.5), (x + (0.6 if x > 32 else -0.6), y, 1.5), 1.4, (50, 46, 44))
    # aranyhalom a bejárat mellett
    for k in range(14):
        x, y = rng.uniform(8, 20), rng.uniform(46, 60)
        g = rng.uniform(1.8, 3.2)
        m.ellipsoid((x, y, g * 0.6), (g, g * 0.85, g * 0.7), (246, 196, 52), tex=0.06, spec=1.0)
    m.height = 30
    return m


def berry_bush(fill):
    m = Model("food")
    rng = np.random.default_rng(11)
    for k, (x, y) in enumerate(((18, 18), (44, 18), (31, 32), (16, 46), (46, 46))):
        for j in range(4):
            a = j * 1.6 + k
            m.ellipsoid((x + math.cos(a) * 3, y + math.sin(a) * 3, 6 + j * 0.8), (6, 5.5, 5), (40 + k * 4 + j * 6, 100 + j * 8, 46),
                        tex=0.25)
        nb = int(round(6 * fill))
        for b in range(nb):
            a = rng.uniform(0, 2 * math.pi)
            m.ellipsoid((x + math.cos(a) * 6, y + math.sin(a) * 5, 8 + rng.uniform(0, 4)), (1.5, 1.5, 1.5),
                        (205, 40, 60), tex=0)
    m.height = 18
    return m


PURPLE = (176, 70, 255)       # a meteorkő színe mindenhol: lila, belülről izzó (kriptonit-szerű kristály)
PURPLE_DARK = (96, 30, 150)


def crater():
    """Meteorit-lelőhely: sötét, megolvadt becsapódási tál; a lila kristálycsoport (crater_crystals) izzik benne."""
    m = Model("meteor")
    for k in range(16):
        a = k / 16 * 2 * math.pi
        m.ellipsoid((32 + math.cos(a) * 23, 32 + math.sin(a) * 21, 2), (7, 6, 4.5), (86, 74, 70), tex=0.2,
                    mat="stones")
    m.ellipsoid((32, 32, 0), (21, 19, 1.5), (34, 26, 36), tex=0.1)
    for k in range(5):  # a peremre szórt kisebb kristályszilánkok (nem izzanak)
        a = k * 1.3 + 0.4
        x, y = 32 + math.cos(a) * 17, 32 + math.sin(a) * 15
        m.capsule((x, y, 0), (x + math.cos(a) * 2, y + math.sin(a) * 1.5, 4 + k % 2 * 2), 1.4, PURPLE_DARK, r1=0.2,
                  spec=0.8)
    m.height = 40
    return m


def _prism(x, y, h, r, lean_a, lean):
    """Hatszögletű, csúcsos kristályhasáb (kriptonit-szilánk) pontfelhője."""
    from .engine import _q
    tip = (x + math.cos(lean_a) * lean, y + math.sin(lean_a) * lean * 0.8, h)
    pts, ns = [], []
    st = 0.5 / _q()
    for i in range(6):  # 6 sík lap a tövétől a csúcs alatti gyűrűig, onnan a csúcsig
        a0, a1 = i / 6 * 2 * math.pi, (i + 1) / 6 * 2 * math.pi
        am = (a0 + a1) / 2
        nrm = np.array([math.cos(am), math.sin(am), 0.25])
        nrm /= np.linalg.norm(nrm)
        for t in np.arange(0, 1.0001, st / max(h, 1)):
            rr = r * (1 - t) if t > 0.72 else r
            if t > 0.72:
                rr = r * (1 - (t - 0.72) / 0.28)
            cxt = x + (tip[0] - x) * t
            cyt = y + (tip[1] - y) * t
            for u in np.arange(0, 1.0001, st / max(rr * 1.05, 0.3)):
                a = a0 + (a1 - a0) * u
                pts.append((cxt + math.cos(a) * rr, cyt + math.sin(a) * rr, t * h))
                ns.append(nrm)
    return np.array(pts, float), np.array(ns, float)


def crater_crystals(frame, n):
    """Lila, izzó kristálycsoport a kráter közepén (4 kockás lassú pulzálás)."""
    pts, nrms, cols, glows = [], [], [], []
    g = 0.78 + 0.22 * math.sin(frame / n * 2 * math.pi)
    rng = np.random.default_rng(5)
    spec = ((32, 32, 26, 4.2, 0, 0), (25, 29, 17, 3.0, 3.6, 4), (39, 30, 19, 3.2, 0.4, 4), (29, 38, 14, 2.6, 2.2, 4),
            (37, 38, 15, 2.6, 1.1, 4), (22, 36, 10, 2.0, 2.8, 4), (42, 36, 11, 2.2, 0.0, 4), (33, 24, 12, 2.2, 4.7, 3))
    for k, (x, y, h, r, la, ln) in enumerate(spec):
        x, y = 32 + (x - 32) * 1.35, 32 + (y - 32) * 1.35  # nagyobb, terebélyesebb kristálycsoport
        p, nn = _prism(x, y, h * 1.45, r * 1.35, la, ln * 1.4)
        light = 0.75 + 0.25 * (nn[:, 0] * -0.6 + nn[:, 1] * -0.4 + 0.6)
        c = np.array(PURPLE, float)[None] * (light[:, None] * g)
        edge = rng.random(len(p)) < 0.06  # csillanó élek
        c[edge] = np.minimum(255, c[edge] * 1.5 + 40)
        pts.append(p)
        nrms.append(nn)
        cols.append(c)
        glows.append(np.ones(len(p), bool))
    return np.concatenate(pts), np.concatenate(nrms), np.concatenate(cols), np.concatenate(glows)


# --- lövedékek ---------------------------------------------------------------
def projectile(kind):
    m = Model(kind)
    if kind == "arrow":
        m.capsule((-5, 0, 0), (4, 0, 0), 0.45, (150, 110, 60), tex=0)
        m.capsule((4, 0, 0), (6, 0, 0), 0.8, (200, 200, 210), r1=0.1, tex=0)
        m.capsule((-5, 0, 0), (-7, 0, 0.5), 0.7, (230, 230, 230), tex=0)
    elif kind == "spear":
        m.capsule((-7, 0, 0), (5, 0, 0), 0.6, (130, 90, 50), tex=0)
        m.capsule((5, 0, 0), (9, 0, 0), 1.2, (180, 185, 195), r1=0.1, tex=0)
    elif kind == "boulder":
        m.ellipsoid((0, 0, 0), (4, 3.6, 3.4), (140, 135, 128), tex=0.2)
    elif kind == "acid":
        m.ellipsoid((0, 0, 0), (3, 2.4, 2.4), (130, 230, 70), glow=True, tex=0.1)
        m.ellipsoid((-3, 0, 0), (2, 1.4, 1.4), (90, 180, 50), glow=True)
    else:  # magic
        m.ellipsoid((0, 0, 0), (3, 3, 3), (255, 255, 230), glow=True)
    m.height = 0
    return m


# --- effektek -------------------------------------------------------------------
def effect_frame(kind, i, n):
    rng = np.random.default_rng(1000 + i * 17 + hash(kind) % 97)
    t = i / max(1, n - 1)
    parts = []
    if kind == "explosion":
        parts.append(particles(int(10 * (1 - t)) + 3, (0, 0, 6 + t * 6), 3 + t * 9, rng, (255, 200 - t * 120, 60),
                               (1.2, 3.0 - t)))
        if t > 0.2:
            parts.append(particles(6, (0, 0, 10 + t * 10), 4 + t * 8, rng, (90, 85, 80), (2, 4), glow=False))
    elif kind == "hit":
        parts.append(particles(5, (0, 0, 2), 1 + t * 3, rng, (255, 240, 170), (0.6, 1.2 - t * 0.4)))
    elif kind == "blood":
        parts.append(particles(6, (0, 0, 3 + 4 * t * (1 - t)), 1 + t * 4, rng, (170, 20, 20), (0.6, 1.2), glow=False))
    elif kind == "heal":
        for k in range(4):
            a = k / 4 * 2 * math.pi + t * 3
            parts.append(particles(1, (math.cos(a) * 5, math.sin(a) * 5, 2 + t * 16), 0.3, rng, (140, 255, 160),
                                   (1.0, 1.4)))
    elif kind == "napvihar":
        for k in range(14):
            ph = (t + k / 14) % 1.0
            x, y = (k * 37 % 50) - 25, (k * 23 % 46) - 23
            parts.append(particles(1, (x + ph * 8, y, 40 * (1 - ph)), 0.2, rng, (255, 200 - ph * 120, 60), (1.2, 2.0)))
        parts.append(particles(10, (0, 0, 1), 16, rng, (255, 120, 40), (1.0, 2.2)))
    elif kind == "oruletkod":
        for k in range(12):
            a = k / 12 * 2 * math.pi + t * 2 * math.pi / 3
            r = 12 + 8 * math.sin(k * 1.3 + t * 6)
            parts.append(particles(1, (math.cos(a) * r, math.sin(a) * r, 4 + 3 * math.sin(a * 2)), 1.0, rng,
                                   (170 + (k % 3) * 20, 90, 220), (3.0, 5.0), glow=k % 2 == 0))
    elif kind == "slash":
        for k in range(6):
            a = -1.2 + (k / 5) * 2.4 + t * 1.5
            parts.append(particles(1, (math.cos(a) * 6, math.sin(a) * 6, 8), 0.1, rng, (255, 255, 255), (0.7, 0.9)))
    elif kind == "gyokerkotes":
        for k in range(10):
            a = k / 10 * 2 * math.pi + t * 0.3
            r = 18 * (0.4 + 0.6 * ((k * 7) % 10) / 10)
            hgt = 3 + 7 * math.sin(math.pi * min(1.0, (t * 2 + k * 0.1) % 1.0))
            for j in range(4):
                z = hgt * j / 3
                parts.append(particles(1, (math.cos(a) * r + math.sin(j + k) * 1.2, math.sin(a) * r, z), 0.1, rng,
                                       (110 + j * 10, 90 + j * 8, 60), (1.0, 1.4), glow=False))
        parts.append(particles(6, (0, 0, 2), 12, rng, (120, 220, 140), (0.6, 1.0)))
    elif kind == "hosszutel":
        for k in range(16):
            ph = (t + k / 16) % 1.0
            x, y = (k * 37 % 56) - 28, (k * 23 % 50) - 25
            parts.append(particles(1, (x - ph * 6, y, 36 * (1 - ph)), 0.2, rng, (220, 240, 255), (0.8, 1.3)))
        parts.append(particles(8, (0, 0, 1), 18, rng, (150, 200, 240), (1.2, 2.4), glow=False))
    elif kind == "lelegzet":
        for k in range(12):
            a = k / 12 * 2 * math.pi + t * 2
            r = 8 + (k % 3) * 7
            parts.append(particles(1, (math.cos(a) * r, math.sin(a) * r, 2 + ((t + k / 12) % 1.0) * 26), 0.2, rng,
                                   (150, 255, 200), (0.9, 1.4)))
    elif kind == "levelup":
        for k in range(14):
            a = k / 14 * 2 * math.pi + t * 3
            parts.append(particles(1, (math.cos(a) * (6 + t * 8), math.sin(a) * (6 + t * 8), 2 + t * 22), 0.1, rng,
                                   (255, 226, 110), (0.8, 1.3)))
    elif kind == "dust":
        parts.append(particles(6, (0, 0, 2 + t * 4), 3 + t * 6, rng, (150, 130, 100), (1.5, 3.0 - t), glow=False))
    pts = np.concatenate([p[0] for p in parts])
    nrm = np.concatenate([p[1] for p in parts])
    col = np.concatenate([p[2] for p in parts])
    glw = np.concatenate([p[3] for p in parts])
    return pts, nrm, col, glw


EFFECTS = {  # név: (képkockák, fps, méret, ismétlődik)
    "explosion": (7, 12, 72, False), "hit": (4, 16, 24, False), "blood": (5, 12, 24, False),
    "heal": (6, 10, 32, False), "napvihar": (6, 10, 128, True), "oruletkod": (6, 8, 128, True),
    "slash": (4, 16, 32, False), "dust": (5, 12, 40, False), "gyokerkotes": (6, 8, 128, True),
    "hosszutel": (6, 10, 128, True), "lelegzet": (6, 10, 128, True), "levelup": (8, 12, 64, False),
}


# --- terepcsempék (Pillow) -------------------------------------------------------
def _noise_tile(rng, palette, weights):
    idx = rng.choice(len(palette), size=(T, T), p=weights)
    return np.array(palette, np.uint8)[idx]


def _clump(arr, rng, n=2):
    for _ in range(n):
        dy, dx = rng.integers(-1, 2, 2)
        mask = rng.random((T, T)) < 0.35
        arr[mask] = np.roll(np.roll(arr, dy, 0), dx, 1)[mask]
    return arr


def make_tile(kind, v):
    rng = np.random.default_rng(500 + v * 13 + sum(map(ord, kind)) * 7)
    if kind == "grass":
        arr = _noise_tile(rng, [(64, 106, 48), (72, 116, 52), (58, 98, 44), (80, 126, 58), (68, 110, 44)],
                          [0.3, 0.25, 0.2, 0.15, 0.1])
        arr = _clump(arr, rng, 3)
        for _ in range(10 * K * K):  # fűcsomók
            x, y = rng.integers(1, T - 1, 2)
            arr[y, x] = (100, 150, 68)
            arr[y - 1, x] = (90, 140, 62)
            if x + 1 < T:
                arr[y, x + 1] = (52, 88, 40)
        if v in (3, 4):
            for _ in range(3 * K * K):  # virágok
                x, y = rng.integers(2, T - 2, 2)
                arr[y, x] = [(236, 224, 120), (230, 120, 150), (200, 200, 250)][int(rng.integers(3))]
        if v == 5:
            for _ in range(3 * K * K):  # kavics
                x, y = rng.integers(2, T - 2, 2)
                arr[y, x] = (150, 146, 130)
                arr[y + 1, x] = (90, 86, 74)
    elif kind == "dirt":
        arr = _noise_tile(rng, [(124, 102, 70), (114, 94, 64), (134, 112, 78), (102, 84, 58)], [0.35, 0.3, 0.2, 0.15])
        arr = _clump(arr, rng, 3)
        for _ in range(7 * K * K):
            x, y = rng.integers(1, T - 2, 2)
            arr[y, x] = (156, 146, 126)
            arr[y + 1, x + 1] = (80, 66, 46)
    elif kind == "sand":
        arr = _noise_tile(rng, [(196, 176, 126), (186, 166, 118), (206, 188, 138)], [0.4, 0.35, 0.25])
        arr = _clump(arr, rng, 2)
    elif kind == "water":
        arr = _noise_tile(rng, [(34, 70, 118), (38, 76, 126), (30, 64, 110), (42, 82, 132)], [0.35, 0.3, 0.2, 0.15])
        arr = _clump(arr, rng, 3)
        ph = v / 4 * 2 * math.pi
        for k in range(6 * K):
            y = int((k * 6 + v * 2 * K) % T)
            x0 = int((k * 11 + 6 * K * math.sin(ph + k)) % (T - 8 * K))
            arr[y, x0:x0 + 6 * K] = (74, 116, 166)
            arr[y, x0 + K:x0 + 4 * K] = (112, 152, 198)
    elif kind == "rock":
        arr = _noise_tile(rng, [(88, 84, 80), (80, 76, 72), (96, 92, 86), (72, 68, 64)], [0.35, 0.3, 0.2, 0.15])
        arr = _clump(arr, rng, 3)
    else:  # creep
        arr = _noise_tile(rng, [(84, 42, 92), (96, 50, 104), (72, 34, 80), (110, 60, 116)], [0.35, 0.3, 0.2, 0.15])
        arr = _clump(arr, rng, 3)
        for _ in range(5 * K * K):
            x, y = rng.integers(1, T - 1, 2)
            arr[y, x] = (150, 230, 120)
        for _ in range(2):  # erek
            y = int(rng.integers(2, T - 2))
            for x in range(T):
                yy = int(y + 2 * K * math.sin(x * 0.4 / K + v))
                if 0 <= yy < T:
                    arr[yy, x] = (130, 60, 120)
    return Image.fromarray(arr.astype(np.uint8), "RGB").convert("RGBA")


def edge_masks(width):
    """Átmeneti alfa-maszkok zajos határral: 4 oldal (N,E,S,W) + 4 belső sarok (NE,SE,SW,NW)."""
    rng = np.random.default_rng(900 + width)
    out = {}
    xs = np.arange(T)
    noise = np.convolve(rng.normal(0, 1.6, T + 6), np.ones(4) / 4, mode="same")[3:T + 3]
    Y, X = np.mgrid[0:T, 0:T]
    base = Y < (width + noise[X])                                   # északi sáv
    for name, k in (("N", 0), ("W", 1), ("S", 2), ("E", 3)):
        out[name] = np.rot90(base, k)
    d = np.hypot(X - 0, Y - 0)
    corner = d < (width + 1 + noise[(X + Y) % T] * 0.8)
    for name, k in (("NW", 0), ("SW", 1), ("SE", 2), ("NE", 3)):
        out[name] = np.rot90(corner, k)
    return {k: Image.fromarray((v * 255).astype(np.uint8), "L") for k, v in out.items()}


# --- terep-díszletek (WC2-szerű apró tárgyak a csempéken) -------------------------------
DOODADS = ["virag_sarga", "virag_piros", "virag_kek", "fucsomo", "kavics", "gomba", "csont", "tusko", "farönk",
           "bokor", "nad", "tavirozsa", "kristaly", "koponya"]


def doodad(kind):
    rng = np.random.default_rng(sum(map(ord, kind)))
    m = Model(kind)
    c = 16
    if kind.startswith("virag"):
        col = {"virag_sarga": (240, 210, 70), "virag_piros": (220, 70, 80), "virag_kek": (120, 150, 240)}[kind]
        for k in range(5):
            x, y = c + rng.uniform(-6, 6), c + rng.uniform(-5, 5)
            m.capsule((x, y, 0), (x, y, 3 + k % 2), 0.35, (60, 120, 50))
            for j in range(5):
                a = j * 1.26
                m.ellipsoid((x + math.cos(a) * 0.9, y + math.sin(a) * 0.9, 3.4 + k % 2), (0.7, 0.7, 0.35), col)
            m.ellipsoid((x, y, 3.6 + k % 2), (0.45,) * 3, (250, 230, 120))
    elif kind == "fucsomo":
        for k in range(12):
            a = rng.uniform(0, 2 * math.pi)
            r = rng.uniform(0, 4)
            x, y = c + math.cos(a) * r, c + math.sin(a) * r
            m.capsule((x, y, 0), (x + rng.uniform(-1.5, 1.5), y + rng.uniform(-1, 1), rng.uniform(4, 7)), 0.5,
                      (70 + k * 3, 130 + k * 2, 52), r1=0.1)
    elif kind == "kavics":
        for k in range(5):
            r = rng.uniform(1.2, 2.6)
            m.ellipsoid((c + rng.uniform(-6, 6), c + rng.uniform(-5, 5), r * 0.4), (r, r * 0.8, r * 0.6),
                        (int(rng.integers(120, 160)),) * 3, tex=0.15)
    elif kind == "gomba":
        for k in range(3):
            x, y = c + (k - 1) * 3, c + (k % 2) * 2
            m.capsule((x, y, 0), (x, y, 2 + k), 0.6, (230, 220, 200))
            m.ellipsoid((x, y, 2.4 + k), (1.8, 1.8, 1.0), (200, 60, 50), spec=0.3)
            m.ellipsoid((x + 0.6, y - 0.4, 3.2 + k), (0.35,) * 3, (250, 245, 230))
    elif kind in ("csont", "koponya"):
        if kind == "koponya":
            m.ellipsoid((c, c, 1.8), (2.4, 2.0, 1.8), (232, 226, 206))
            m.ellipsoid((c + 1.8, c - 0.8, 2.0), (0.5,) * 3, (30, 26, 24))
            m.ellipsoid((c + 1.8, c + 0.8, 2.0), (0.5,) * 3, (30, 26, 24))
        for k in range(3):
            a = rng.uniform(0, math.pi)
            x, y = c + rng.uniform(-4, 4), c + rng.uniform(-4, 4)
            m.capsule((x - math.cos(a) * 3, y - math.sin(a) * 3, 0.6), (x + math.cos(a) * 3, y + math.sin(a) * 3, 0.6), 0.5,
                      (226, 218, 196))
    elif kind == "tusko":
        m.cylinder((c, c, 0), (c, c, 3.5), 3.2, (110, 76, 46), stripe=(2, 1.0))
        m.cylinder((c, c, 3.5), (c, c, 3.7), 3.0, (190, 160, 110))
        for k in range(3):
            a = k * 2.1
            m.capsule((c, c, 0.6), (c + math.cos(a) * 5, c + math.sin(a) * 5, 0.2), 0.8, (100, 70, 42), r1=0.3)
    elif kind == "farönk":
        m.cylinder((c - 7, c, 1.8), (c + 7, c + 1, 1.8), 1.8, (116, 80, 48), stripe=(0, 1.4))
        m.ellipsoid((c - 2, c - 1, 3.4), (1.6, 1.0, 0.6), (80, 130, 60))
    elif kind == "bokor":
        for k in range(6):
            a = k * 1.05
            m.ellipsoid((c + math.cos(a) * 3, c + math.sin(a) * 2.6, 3.2), (3.2, 3.0, 2.8), (50 + k * 4, 110 + k * 3, 48), tex=0.25)
    elif kind == "nad":
        for k in range(9):
            x, y = c + rng.uniform(-5, 5), c + rng.uniform(-4, 4)
            h = rng.uniform(6, 10)
            m.capsule((x, y, 0), (x + rng.uniform(-1, 1), y, h), 0.35, (110, 140, 70), r1=0.15)
            if k % 3 == 0:
                m.capsule((x, y, h - 2.5), (x, y, h - 0.8), 0.7, (120, 80, 50))
    elif kind == "tavirozsa":
        for k in range(3):
            x, y = c + (k - 1) * 4.5, c + (k % 2) * 3
            m.ellipsoid((x, y, 0.2), (2.6, 2.4, 0.3), (60, 130, 70))
            if k == 1:
                m.ellipsoid((x, y, 0.8), (1.0, 1.0, 0.8), (250, 200, 220))
    elif kind == "kristaly":
        for k in range(4):
            a = k * 1.4
            m.capsule((c, c, 0), (c + math.cos(a) * 2.5, c + math.sin(a) * 2, 4 + k), 1.1, PURPLE, r1=0.1, glow=True)
    m.height = 10
    return m


TILE_KINDS = {"grass": 6, "dirt": 4, "sand": 3, "water": 4, "rock": 4, "creep": 4}
