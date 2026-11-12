"""Épületmodellek fajonkénti stílusban – 2. változat.

Stílusok: aztec (szürke elfek), nordic (ursanok), organic (croftianok),
tribal (wullof), fungal (micéliák), imperial (vasbirodalom). A definíció
"shape" mezője választja ki a formát: hq, house, obelisk, barracks, tech, pen,
observatory, aerie, tower, dropoff, node. Koordináták: az alapterület bal-felső
sarka (0,0,0), 1 csempe = 32 egység.
"""
import math

import numpy as np

from .engine import Model, capsule, ellipsoid

T = 32
AZ = dict(sand=(206, 188, 146), sand2=(170, 152, 114), dark=(120, 105, 80), jade=(40, 196, 156), gold=(234, 188, 62),
          hay=(196, 168, 96),
          red=(170, 62, 42), obsidian=(52, 46, 62), feather=(60, 170, 210), wood=(120, 85, 50))
NO = dict(hide=(214, 192, 150), hide2=(120, 88, 56),
          wood=(132, 92, 54), wood2=(92, 60, 34), roof=(112, 76, 42), turf=(96, 116, 62), stone=(140, 140, 146),
          stone2=(108, 108, 116), metal=(166, 170, 180), ember=(255, 140, 40), hay=(208, 184, 92), bone=(232, 226, 206))
OR = dict(shell=(112, 66, 138), shell2=(72, 42, 92), flesh=(204, 122, 162), glow=(150, 255, 120), bone=(228, 216, 186),
          dark=(45, 15, 40), slime=(90, 160, 70))
TR = dict(hide=(214, 190, 150), hide2=(186, 160, 120), red=(190, 54, 38), turq=(52, 170, 168), black=(40, 34, 32),
          wood=(126, 88, 52), wood2=(90, 60, 36), stone=(132, 126, 118), ember=(255, 150, 50), bone=(232, 224, 200),
          feather=(240, 240, 232), hay=(206, 180, 96), bison=(90, 62, 40))
FU = dict(stalk=(228, 216, 190), stalk2=(192, 178, 148), cap=(178, 74, 60), cap2=(216, 198, 162), spot=(246, 238, 216),
          gill=(140, 104, 92), glow=(80, 230, 205), blue=(70, 120, 210), purple=(140, 96, 170), root=(120, 92, 70),
          moss=(84, 120, 66))
IM = dict(marble=(214, 208, 196), marble2=(176, 170, 160), bronze=(186, 140, 72), iron=(84, 84, 92), metal=(160, 156, 150),
          red=(170, 40, 34), soul=(120, 255, 190), roof=(176, 88, 60), smoke=(90, 90, 90), ember=(255, 140, 50))


EMBLEM = {"aztec": "sun", "nordic": "paw", "organic": "eye", "tribal": "moon", "fungal": "spore", "imperial": "eagle"}
_CUR = {"emblem": "paw"}


def _banner(m, x, y, z, h, pole, team=True):
    m.capsule((x, y, z), (x, y, z + h), 0.7, pole)
    m.box((x + 3.4, y, z + h - 4), (2.8, 0.5, 3.8), (200, 200, 200), team=team)
    m.emblem((x + 3.4, y + 0.56, z + h - 4), 2.3, 3.1, 1, _CUR["emblem"], (240, 236, 224), tex=0.03)
    m.ellipsoid((x, y, z + h + 0.8), (1.0, 1.0, 1.0), (230, 190, 70), spec=0.9)


def _brazier(m, x, y, z, col=(255, 150, 50)):
    m.cylinder((x, y, z), (x, y, z + 3), 2.2, (90, 80, 70), spec=0.4)
    m.ellipsoid((x, y, z + 3.4), (1.8, 1.8, 1.2), col, glow=True)


# ---------------------------------------------------------------------------
def aztec(shape, w, h):
    m = Model(shape)
    W, H = w * T, h * T
    cx, cy = W / 2, H / 2
    P = AZ
    anim = []
    if shape in ("hq", "aerie"):
        tiers = 4 if shape == "hq" else 3
        th = 8 if shape == "hq" else 7
        for k in range(tiers):
            hx = W / 2 - 4 - k * (W * 0.09)
            hy = H / 2 - 4 - k * (H * 0.09)
            col = P["sand"] if k % 2 == 0 else P["sand2"]
            m.box((cx, cy, k * th + th / 2), (hx, hy, th / 2), col, tex=0.06, mat="blocks")
            m.box((cx, cy + hy - 0.4, k * th + th - 1.2), (hx - 1, 0.8, 1.0), P["red"])
            m.box((cx, cy + hy - 0.2, k * th + 2.5), (hx - 3, 0.5, 0.6), P["jade"], spec=0.4)
        top = tiers * th
        for k in range(tiers * 3):  # lépcső
            m.box((cx, H - 5 - k * 1.7, k * th / 3 + th / 6), (W * 0.1, 1.6, th / 6), P["sand2"], tex=0.04)
        for sx in (-1, 1):  # kígyófejek a lépcső aljánál
            m.ellipsoid((cx + sx * W * 0.12, H - 3, 3), (2.6, 3.4, 2.6), P["jade"], spec=0.5)
            m.ellipsoid((cx + sx * W * 0.12, H - 0.6, 3.2), (0.8, 0.6, 0.8), P["red"], glow=True)
        if shape == "hq":
            m.box((cx, cy, top + 6), (W * 0.16, H * 0.14, 6), P["jade"], spec=0.3, mat="bricks")
            m.box((cx, cy + H * 0.14, top + 4), (3, 0.6, 4), P["obsidian"])
            m.roof((cx, cy, top + 12), W * 0.18, H * 0.16, 9, P["hay"], axis=0, mat="thatch")
            m.box((cx, cy, top + 12), (W * 0.19, H * 0.17, 0.8), P["gold"], spec=0.9)
            for sx in (-1, 1):
                _brazier(m, cx + sx * W * 0.13, cy + H * 0.18, top)
                _banner(m, cx + sx * W * 0.34, cy + H * 0.34, 8, 22, P["wood"])
            anim.append(("embers", (cx - W * 0.13, cy + H * 0.18, top + 4), 3, (255, 160, 60)))
            anim.append(("embers", (cx + W * 0.13, cy + H * 0.18, top + 4), 3, (255, 160, 60)))
        else:
            for i in range(12):
                a = i / 12 * 2 * math.pi
                m.capsule((cx + math.cos(a) * 9, cy + math.sin(a) * 9, top), (cx + math.cos(a) * 16, cy + math.sin(a) * 16,
                          top + 9), 1.6, P["feather"] if i % 2 else P["jade"], r1=0.6, team=(i % 3 == 0))
            m.ellipsoid((cx, cy, top + 4), (5, 4, 5), (238, 228, 204), spec=0.4)
        height = top + 18
    elif shape == "obelisk":
        m.box((cx, cy, 3), (W / 2 - 6, H / 2 - 6, 3), P["sand2"], mat="bricks")
        for k in range(5):
            r = 7 - k * 0.9
            m.box((cx, cy, 6 + k * 8 + 4), (r, r, 4), P["sand"] if k % 2 == 0 else P["sand2"], tex=0.05)
            m.box((cx, cy + r, 6 + k * 8 + 4), (r * 0.7, 0.3, 1.0), P["jade"], glow=k == 2)
        m.box((cx, cy, 30), (5.5, 5.5, 1.2), P["jade"], glow=True)
        m.capsule((cx, cy, 46), (cx, cy, 55), 3.4, P["gold"], r1=0.3, glow=True)
        height = 58
        anim.append(("motes", (cx, cy, 40), 9, P["jade"]))
    elif shape == "tower":
        m.box((cx, cy, 4), (W / 2 - 5, H / 2 - 5, 4), P["sand2"], mat="bricks")
        m.cylinder((cx, cy, 8), (cx, cy, 34), 7, P["jade"], spec=0.4, stripe=(2, 3))
        m.box((cx, cy, 36), (8, 8, 2), P["gold"], spec=0.9)
        m.ellipsoid((cx, cy, 43), (4.5, 4.5, 4.5), (190, 255, 220), glow=True)
        height = 50
        anim.append(("motes", (cx, cy, 43), 7, P["jade"]))
    elif shape == "observatory":
        m.box((cx, cy, 4), (W / 2 - 4, H / 2 - 4, 4), P["sand2"], mat="bricks")
        m.cylinder((cx, cy, 8), (cx, cy, 26), 14, P["sand"], tex=0.05, mat="bricks")
        m.box((cx, cy + 13.5, 14), (3, 1.5, 6), P["obsidian"])
        m.ellipsoid((cx, cy, 30), (14, 14, 10), P["jade"], spec=0.5)
        m.capsule((cx - 4, cy, 34), (cx + 12, cy - 6, 44), 2.2, P["gold"], spec=0.9)
        for sx in (-1, 1):
            _banner(m, cx + sx * W * 0.38, cy + H * 0.36, 8, 18, P["wood"])
        height = 46
    elif shape == "pen":
        for sx in (-1, 1):
            m.box((cx + sx * (W / 2 - 4), cy, 5), (2.5, H / 2 - 3, 5), P["sand2"], mat="bricks")
        m.box((cx, 4, 5), (W / 2 - 3, 2.5, 5), P["sand2"], mat="bricks")
        m.box((cx - W * 0.25, H - 4, 5), (W * 0.2, 2.5, 5), P["sand2"], mat="bricks")
        m.box((cx + W * 0.25, H - 4, 5), (W * 0.2, 2.5, 5), P["sand2"], mat="bricks")
        m.box((cx, cy - 6, 8), (W * 0.25, H * 0.15, 8), P["sand"], mat="bricks")
        m.roof((cx, cy - 6, 16), W * 0.28, H * 0.18, 6, P["red"], axis=0, stripe=(1, 2.4))
        for k in range(3):
            m.ellipsoid((cx - 14 + k * 9, cy + 14, 3), (3, 2.4, 3.4), (230, 220, 190), spec=0.3)
        _banner(m, cx, cy - 6, 22, 12, P["wood"])
        height = 36
    else:  # barracks, tech
        m.box((cx, cy, 3), (W / 2 - 3, H / 2 - 3, 3), P["sand2"], mat="bricks")
        m.box((cx, cy - 4, 14), (W / 2 - 10, H / 2 - 12, 8), P["sand"], tex=0.06, mat="blocks")
        m.box((cx, cy - 4 + H / 2 - 12, 10), (4, 0.6, 4), P["obsidian"])
        for k in range(5):
            x = 12 + k * (W - 24) / 4
            m.cylinder((x, H - 14, 6), (x, H - 14, 22), 2.4, P["sand2"], stripe=(2, 2))
        m.box((cx, cy, 24), (W / 2 - 6, H / 2 - 8, 2), P["sand2"], mat="blocks")
        m.box((cx, cy, 26.1), (W / 2 - 8, H / 2 - 10, 0.2), (150, 92, 66), mat="blocks")  # festett tetőterasz
        # faragott fríz: vörös-jade lépcsős meander a párkány alatt
        fy = cy + H / 2 - 8 + 0.3
        m.box((cx, fy, 21.2), (W / 2 - 6, 0.5, 1.0), P["red"])
        for k in range(int((W - 14) / 4)):
            x = 9 + k * 4
            m.box((x, fy + 0.4, 21.2 + (0.5 if k % 2 else -0.5)), (1.2, 0.4, 0.5), P["jade"], spec=0.3)
        # tetőterasz lépcsős pártázattal (almena) körben
        ex, ey = W / 2 - 7, H / 2 - 9
        n_x, n_y = int(ex / 3.2), int(ey / 3.2)
        for k in range(-n_x, n_x + 1):
            for sy in (-1, 1):
                x = cx + k * ex / n_x
                m.box((x, cy + sy * ey, 27.6), (1.3, 1.0, 1.6), P["sand"], mat="blocks")
                m.box((x, cy + sy * ey, 29.9), (0.7, 0.8, 0.8), P["jade"] if k % 2 else P["red"], spec=0.3)
        for k in range(-n_y + 1, n_y):
            for sx in (-1, 1):
                y = cy + k * ey / n_y
                m.box((cx + sx * ex, y, 27.6), (1.0, 1.3, 1.6), P["sand"], mat="blocks")
                m.box((cx + sx * ex, y, 29.9), (0.8, 0.7, 0.8), P["jade"] if k % 2 else P["red"], spec=0.3)
        # tetőszentély nádtetővel
        m.box((cx - W * 0.12, cy - 2, 30), (W * 0.16, H * 0.14, 4), P["sand"], mat="blocks")
        m.box((cx - W * 0.12, cy - 2 + H * 0.14 + 0.2, 29.5), (2.2, 0.5, 3.0), P["obsidian"])
        m.roof((cx - W * 0.12, cy - 2, 34), W * 0.17 + 1.5, H * 0.14 + 2, 8, P["hay"], axis=0, mat="thatch")
        if shape == "tech":
            m.cylinder((cx + W * 0.25, cy - H * 0.2, 28), (cx + W * 0.25, cy - H * 0.2, 44), 4.5, P["obsidian"],
                       spec=0.6, mat="blocks")
            m.ellipsoid((cx + W * 0.25, cy - H * 0.2, 45), (3.4, 3.4, 2), (255, 160, 60), glow=True)
            anim.append(("smoke", (cx + W * 0.25, cy - H * 0.2, 47), 0, (90, 90, 90)))
            height = 58
        else:
            m.ellipsoid((cx + W * 0.2, cy + 2, 30), (3, 3, 2), (255, 170, 70), glow=True)  # áldozati tűz
            anim.append(("embers", (cx + W * 0.2, cy + 2, 31), 3, (255, 160, 60)))
            for sx in (-1, 1):
                _banner(m, cx + sx * W * 0.36, H - 10, 6, 26, P["wood"])
            height = 40
    m.height = height
    m.anim = anim
    return m


def _patch(m, cx, cy, rx, ry, hz, col, seed=0, rough=0.16, **kw):
    """Szabálytalan szélű, lapos talajfolt (letaposott föld, nyálka, moha) – nem tökéletes korong."""
    from .engine import _q
    rng = np.random.default_rng(seed)
    ph = rng.uniform(0, 2 * math.pi, 4)
    st = 0.55 / _q()
    X, Y = np.meshgrid(np.arange(-rx, rx + 0.01, st), np.arange(-ry, ry + 0.01, st), indexing="ij")
    X, Y = X.ravel(), Y.ravel()
    a = np.arctan2(Y / ry, X / rx)
    edge = 1 - rough * (0.5 + 0.5 * (np.sin(3 * a + ph[0]) * 0.5 + np.sin(5 * a + ph[1]) * 0.3 + np.sin(9 * a + ph[2]) * 0.2))
    d = np.hypot(X / rx, Y / ry) / edge
    msk = d < 1
    X, Y, d = X[msk], Y[msk], d[msk]
    Z = hz * np.sqrt(np.clip(1 - d * d, 0, 1)) * 0.8 + 0.1
    p = np.stack([cx + X, cy + Y, Z], 1)
    n = np.stack([X / rx * 0.25 * d, Y / ry * 0.25 * d, np.ones_like(X)], 1)
    n /= np.linalg.norm(n, axis=1, keepdims=True)
    m.add((p, n), col, **kw)


def _tent(x, y, z, r0, r1, h, power=1.0):
    """Kúppalást (sátor/tipi/jurtatető): a sugár r0-ról r1-re fogy; power<1: domború, >1: homorú."""
    from .engine import _q
    st = 0.5 / _q()
    pts, ns = [], []
    for t in np.arange(0, 1.0001, st / max(h, 1)):
        rr = r1 + (r0 - r1) * (1 - t) ** power
        k = max(8, int(2 * math.pi * rr / st))
        a = np.linspace(0, 2 * math.pi, k, endpoint=False)
        dr = (r0 - r1) * power * (1 - t) ** max(power - 1, 0) / max(h, 1e-6)  # -dr/dz
        nz = dr / math.sqrt(1 + dr * dr)
        nr = 1 / math.sqrt(1 + dr * dr)
        pts.append(np.stack([x + np.cos(a) * rr, y + np.sin(a) * rr, np.full(k, z + t * h)], 1))
        ns.append(np.stack([np.cos(a) * nr, np.sin(a) * nr, np.full(k, nz)], 1))
    return np.concatenate(pts), np.concatenate(ns)


def _yurt(m, x, y, r, wall_h, dome_h, P, door=True, team=True):
    """Ősemberi jurta: alacsony deszkafal, bőrkupola kötélfonattal, kilógó rudak, csontíves bejárat."""
    m.cylinder((x, y, 0), (x, y, wall_h), r, P["wood"], tex=0.18, mat="planks")
    # sátorkúp (nem gömb): enyhén domború kúp bőrből, rajta szőrmék
    m.add(_tent(x, y, wall_h, r + 1.6, 2.0, dome_h * 1.25, 1.35), P["hide"], tex=0.22, mat="cloth")
    n_ = max(10, int(r * 1.2))
    for k in range(n_):  # bundaperem: a sátor alsó szélén körbefutó szőrme
        a0, a1 = k / n_ * 2 * math.pi, (k + 1) / n_ * 2 * math.pi
        m.capsule((x + math.cos(a0) * (r + 1.5), y + math.sin(a0) * (r + 1.5), wall_h + 0.4),
                  (x + math.cos(a1) * (r + 1.5), y + math.sin(a1) * (r + 1.5), wall_h + 0.4), 1.1, P["hide2"], mat="fur")
    for k in range(3):  # ráterített állatbőrök
        a = k * 2.1 + 0.4
        m.ellipsoid((x + math.cos(a) * r * 0.62, y + math.sin(a) * r * 0.62, wall_h + dome_h * 0.3),
                    (r * 0.36, r * 0.26, 1.4), P["hide2"] if k != 1 else (150, 120, 84), mat="fur")
    H_ = dome_h * 1.25
    for k in range(8):  # kötél-pántok a sátortetőn (a kúp felületét követik)
        a = k / 8 * 2 * math.pi + 0.2
        prev = None
        for t in np.linspace(0, 0.92, 6):
            rr = 2.0 + (r + 1.6 - 2.0) * (1 - t) ** 1.35 + 0.3
            pt = (x + math.cos(a) * rr, y + math.sin(a) * rr, wall_h + t * H_ + 0.9)
            if prev is not None:
                m.capsule(prev, pt, 0.6, P["wood2"])
            prev = pt
    for k in range(5):  # füstnyílásból kiálló rudak
        a = k / 5 * 2 * math.pi + 0.3
        m.capsule((x + math.cos(a) * 1.5, y + math.sin(a) * 1.5, wall_h + dome_h * 1.25 - 1.5),
                  (x + math.cos(a) * 3.6, y + math.sin(a) * 3.6, wall_h + dome_h * 1.25 + 4.5), 0.55, P["wood2"])
    if door:
        dy = y + r + 0.8
        m.box((x, dy, wall_h * 0.55), (2.8, 0.6, wall_h * 0.55), (200, 200, 200), team=team, tex=0.15)
        for sx in (-1, 1):  # mamutagyar-ív
            m.capsule((x + sx * 4.2, dy + 0.6, 0), (x + sx * 1.4, dy + 1.4, wall_h + 4), 1.0, P["bone"], r1=0.4)
    return wall_h + dome_h * 1.25 + 5


def nordic(shape, w, h):
    m = Model(shape)
    W, H = w * T, h * T
    cx, cy = W / 2, H / 2
    P = NO
    anim = []
    if shape in ("hq", "house", "barracks"):  # ursan: viking csarnok és ősemberi jurta keveréke
        if shape == "hq":
            # hosszúcsarnok-szárny gyeptetővel + nagy központi jurta
            m.box((cx - 10, cy - 12, 6), (W / 2 - 16, 10, 6), P["wood"], mat="logs")
            m.roof((cx - 10, cy - 12, 12), W / 2 - 13, 12, 11, P["turf"], axis=0, tex=0.2, mat="thatch")
            for k in range(5):  # tetőt tartó ferde gerendák a fal előtt
                x = cx - 10 - (W / 2 - 18) + k * (W - 36) / 4
                m.capsule((x, cy - 12 + 11.5, 0), (x, cy - 12 + 10.2, 11), 0.9, P["wood2"])
            for sx in (-1, 1):  # sárkányfejes oromdísz
                x = cx - 10 + sx * (W / 2 - 13)
                m.capsule((x, cy - 12, 22), (x + sx * 5, cy - 12, 28), 1.4, P["wood2"], r1=0.9)
                m.ellipsoid((x + sx * 6, cy - 12, 28.5), (2.2, 1.3, 1.5), P["wood2"])
            height = _yurt(m, cx + 8, cy + 8, 15, 8, 14, P)
            _yurt(m, 16, H - 14, 8, 5, 8, P, door=False)
            anim.append(("smoke", (cx + 8, cy + 8, 26), 0, (120, 115, 110)))
            _banner(m, 6, cy + 4, 0, 30, P["wood2"])
        elif shape == "house":  # erődített hosszúház-jurta karópalánkkal
            height = _yurt(m, cx, cy, W / 2 - 7, 6, 10, P)
            for k in range(14):
                a = k / 14 * 2 * math.pi
                if math.sin(a) > 0.75:
                    continue  # bejárat
                x, y = cx + math.cos(a) * (W / 2 - 2), cy + math.sin(a) * (H / 2 - 2)
                m.capsule((x, y, 0), (x, y, 9), 1.1, P["wood2"], r1=0.4)
            anim.append(("smoke", (cx, cy, 18), 0, (120, 115, 110)))
        else:  # harcosok háza: két jurta + gyakorlótér
            height = _yurt(m, cx - 10, cy - 8, 12, 7, 12, P)
            _yurt(m, cx + 14, cy + 6, 9, 6, 9, P, door=False)
            for k in range(3):  # fegyverállvány
                m.capsule((W - 10 - k * 4, H - 6, 2), (W - 12 - k * 4, H - 6, 16), 0.7, P["wood2"])
                m.box((W - 12 - k * 4, H - 6, 15), (0.5, 2.2, 3.0), P["wood"], tex=0.2)
            m.capsule((10, H - 7, 0), (10, H - 7, 12), 1.2, P["wood2"])  # gyakorlóbáb
            m.ellipsoid((10, H - 7, 13), (2.4, 2.4, 3), P["hay"], tex=0.2)
            _banner(m, 4, H - 6, 2, 26, P["wood2"])
            anim.append(("smoke", (cx - 10, cy - 8, 24), 0, (120, 115, 110)))
        m.height = height + 6
        m.anim = anim
        return m
    if False:
        wall_h = {"hq": 14, "house": 9, "barracks": 12}[shape]
        rh = {"hq": 20, "house": 12, "barracks": 15}[shape]
        hx, hy = W / 2 - 4, H / 2 - (8 if shape != "house" else 6)
        m.box((cx, cy, 2), (hx + 2, hy + 2, 2), P["stone2"], mat="bricks")
        m.box((cx, cy, wall_h / 2 + 2), (hx, hy, wall_h / 2), P["wood"], tex=0.1, mat="planks")
        for k in range(int(hx * 2 / 6)):
            x = cx - hx + 3 + k * 6
            m.capsule((x, cy + hy + 0.5, 3), (x, cy + hy + 0.5, wall_h + 2), 0.9, P["wood2"])
        roof = P["turf"] if shape == "house" else P["roof"]
        m.roof((cx, cy, wall_h + 2), hx + 3, hy + 4, rh, roof, axis=0, tex=0.15, stripe=(1, 3))
        m.box((cx, cy, wall_h + rh + 1.2), (hx + 3, 1.0, 0.8), P["wood2"])
        m.box((cx, cy + hy + 0.6, 6), (3.4, 0.8, 5), P["wood2"], mat="planks")
        m.ellipsoid((cx, cy + hy + 1.4, 6), (0.6, 0.4, 0.6), P["metal"], spec=0.9)
        for sx in (-1, 1):  # ablakok
            m.box((cx + sx * hx * 0.55, cy + hy + 0.6, wall_h * 0.62), (1.6, 0.6, 1.4), (40, 30, 24))
            m.box((cx + sx * hx * 0.55, cy + hy + 0.9, wall_h * 0.62), (1.0, 0.4, 0.8), (255, 190, 90), glow=True)
        if shape != "house":
            for sx in (-1, 1):  # sárkányfej-oromdísz
                x = cx + sx * (hx + 3)
                m.capsule((x, cy, wall_h + rh), (x + sx * 6, cy, wall_h + rh + 7), 1.6, P["wood2"], r1=1.0)
                m.ellipsoid((x + sx * 7, cy, wall_h + rh + 7.5), (2.4, 1.4, 1.6), P["wood2"])
                m.ellipsoid((x + sx * 8.4, cy, wall_h + rh + 8), (0.4, 0.5, 0.4), (255, 80, 40), glow=True)
            n = 4 if shape == "hq" else 3
            for k in range(n):
                x = cx - hx + 10 + k * (2 * hx - 20) / max(1, n - 1)
                m.ellipsoid((x, cy + hy + 1.5, wall_h * 0.6), (3.4, 0.9, 3.4), (200, 200, 200), team=True)
                m.ellipsoid((x, cy + hy + 2.2, wall_h * 0.6), (1, 0.6, 1), P["metal"], spec=1.0)
                m.ellipsoid((x, cy + hy + 1.6, wall_h * 0.6), (3.6, 0.7, 0.5), P["metal"], spec=0.8)
        if shape == "barracks":
            for k in range(3):
                m.capsule((W - 10 - k * 4, H - 6, 2), (W - 12 - k * 4, H - 6, 16), 0.7, P["wood2"])
                m.ellipsoid((W - 12 - k * 4, H - 6, 16), (1.8, 0.5, 2.4), P["metal"], spec=1.0)
            m.capsule((10, H - 7, 0), (10, H - 7, 12), 1.2, P["wood2"])  # gyakorlóbáb
            m.ellipsoid((10, H - 7, 13), (2.4, 2.4, 3), P["hay"], tex=0.2)
            _banner(m, 4, H - 6, 2, 26, P["wood2"])
        anim.append(("smoke", (cx - hx * 0.3, cy, wall_h + rh), 0, (110, 110, 110)))
        height = wall_h + rh + 16
    elif shape == "dropoff":
        for x in (8, W - 8):
            for y in (8, H - 8):
                m.capsule((x, y, 0), (x, y, 16), 1.4, P["wood2"])
        m.roof((cx, cy, 16), W / 2 - 4, H / 2 - 4, 9, P["roof"], axis=0)
        for k in range(4):
            m.cylinder((10, H - 10 - k * 3.4, 2 + (k % 2) * 3), (W - 18, H - 10 - k * 3.4, 2 + (k % 2) * 3), 2,
                       P["wood"], stripe=(0, 3))
        for k in range(5):
            m.ellipsoid((W - 10 + (k % 2) * 3, 12 + k * 3, 3), (3, 2.6, 2.4), P["stone"], tex=0.15)
        m.box((14, 14, 3), (3, 2, 3), P["wood2"], mat="planks")
        m.capsule((12, 14, 7), (18, 18, 9), 0.6, P["wood2"])
        m.ellipsoid((18, 18, 9), (1.4, 0.4, 1.6), P["metal"], spec=1.0)
        _banner(m, 4, H - 4, 0, 20, P["wood2"])
        height = 30
    elif shape == "tech":
        m.box((cx, cy, 1), (W / 2 - 3, H / 2 - 3, 1), P["stone2"], mat="stones")
        _yurt(m, cx - 6, cy - 4, W / 2 - 15, 6, 11, P)
        m.cylinder((cx + W * 0.28, cy - H * 0.2, 0), (cx + W * 0.28, cy - H * 0.2, 34), 5.5, P["stone2"], mat="stones")
        m.ellipsoid((cx + W * 0.28, cy - H * 0.2, 35), (3, 3, 1.5), P["ember"], glow=True)
        m.box((cx + 4, H - 12, 6), (6, 4, 4), (60, 30, 20), mat="bricks")
        m.ellipsoid((cx + 4, H - 12, 11), (4.5, 3, 1.5), P["ember"], glow=True)
        m.box((cx + 18, H - 10, 5), (3, 1.6, 2), P["metal"], spec=1.0)
        m.cylinder((12, H - 10, 0), (12, H - 10, 16), 3, P["stone2"])
        m.ellipsoid((12, H - 7.2, 10), (1.6, 0.6, 3), (120, 200, 255), glow=True)
        anim.append(("smoke", (cx + W * 0.28, cy - H * 0.2, 36), 0, (70, 70, 70)))
        anim.append(("embers", (cx + 4, H - 12, 12), 4, P["ember"]))
        height = 54
    elif shape == "pen":
        n = 9
        for k in range(n):
            for (x0, y0, x1, y1) in ((4, 4, W - 4, 4), (4, H - 4, W - 4, H - 4), (4, 4, 4, H - 4), (W - 4, 4, W - 4, H - 4)):
                x = x0 + (x1 - x0) * k / (n - 1)
                y = y0 + (y1 - y0) * k / (n - 1)
                m.capsule((x, y, 0), (x, y, 9), 1.2, P["wood2"], r1=0.6)
        for (x0, y0, x1, y1) in ((4, 4, W - 4, 4), (4, H - 4, W * 0.4, H - 4), (W * 0.6, H - 4, W - 4, H - 4),
                                 (4, 4, 4, H - 4), (W - 4, 4, W - 4, H - 4)):
            m.capsule((x0, y0, 6), (x1, y1, 6), 0.8, P["wood"])
        _yurt(m, W * 0.3, H * 0.3, W * 0.16, 5, 8, P)
        for k in range(3):
            m.ellipsoid((W * 0.7, H * 0.4 + k * 6, 3), (5, 3, 3), P["hay"], tex=0.25)
        _banner(m, W - 8, H - 8, 0, 22, P["wood2"])
        height = 30
    elif shape == "aerie":
        for k, (r, z) in enumerate(((W * 0.4, 6), (W * 0.32, 16), (W * 0.24, 26), (W * 0.16, 34))):
            m.ellipsoid((cx + (k % 2) * 3, cy - k * 2, z), (r, r * 0.9, 7), P["stone"] if k % 2 else P["stone2"], tex=0.18)
        for i in range(14):
            a = i / 14 * 2 * math.pi
            m.capsule((cx + math.cos(a) * 6, cy - 6 + math.sin(a) * 6, 40), (cx + math.cos(a) * 12, cy - 6 + math.sin(a) * 12,
                      44), 1.6, P["wood"])
        m.ellipsoid((cx, cy - 6, 41), (6, 6, 2), (60, 50, 40))
        _banner(m, cx + W * 0.3, cy + H * 0.3, 4, 20, P["wood2"])
        height = 50
    elif shape == "tower":
        m.box((cx, cy, 6), (W / 2 - 6, H / 2 - 6, 6), P["stone2"], mat="bricks")
        for x in (12, W - 12):
            for y in (12, H - 12):
                m.capsule((x, y, 12), (x, y, 32), 1.5, P["wood2"])
        m.capsule((12, 12, 14), (W - 12, H - 12, 30), 0.7, P["wood2"])
        m.capsule((W - 12, 12, 14), (12, H - 12, 30), 0.7, P["wood2"])
        m.box((cx, cy, 34), (W / 2 - 8, H / 2 - 8, 3), P["wood"], mat="planks")
        m.box((cx, cy + H / 2 - 9, 39), (W / 2 - 8, 0.8, 3), (200, 200, 200), team=True)
        m.capsule((cx, cy, 37), (cx, cy, 52), W / 2 - 5, P["hide"], r1=0.5, tex=0.25)
        for k in range(4):
            a = k * math.pi / 2 + 0.6
            m.capsule((cx, cy, 51), (cx + math.cos(a) * 2.5, cy + math.sin(a) * 2.5, 56), 0.5, P["wood2"])
        height = 60
    elif shape == "wall":  # ursan cölöpfal: hegyes karók kötéllel
        for k in range(5):
            x = 4 + k * 6
            hh = 16 + (k % 2) * 3
            m.capsule((x, cy, 0), (x, cy, hh), 2.4, P["wood"], r1=2.2, stripe=(2, 2.5))
            m.capsule((x, cy, hh), (x, cy, hh + 4), 2.2, P["wood2"], r1=0.3)
        for z in (5, 12):
            m.capsule((2, cy + 2.2, z), (W - 2, cy + 2.2, z), 0.6, P["hide2"])
        height = 24
    else:
        return nordic("house", w, h)
    m.height = height
    m.anim = anim
    return m


def organic(shape, w, h):
    m = Model(shape)
    W, H = w * T, h * T
    cx, cy = W / 2, H / 2
    P = OR
    anim = []
    _patch(m, cx, cy, W / 2 - 1, H / 2 - 1, 2.5, P["shell2"], seed=w * 5 + h, tex=0.2, rough=0.22)
    for k in range(6):  # erek a nyálkán
        a = k / 6 * 2 * math.pi + 0.4
        m.capsule((cx, cy, 2), (cx + math.cos(a) * W * 0.46, cy + math.sin(a) * H * 0.46, 1), 0.7, (150, 60, 110))
    if shape == "hq":
        for k in range(7):
            a = k / 7 * 2 * math.pi
            m.ellipsoid((cx + math.cos(a) * W * 0.22, cy + math.sin(a) * H * 0.2, 12), (W * 0.2, H * 0.18, 13),
                        P["shell"] if k % 2 else P["shell2"], tex=0.12, spec=0.4, mat="chitin")
        m.ellipsoid((cx, cy - 4, 24), (W * 0.28, H * 0.26, 18), P["shell"], tex=0.1, spec=0.45, mat="scales")
        m.ellipsoid((cx, cy - 8, 36), (W * 0.18, H * 0.16, 8), (200, 200, 200), team=True)
        m.ellipsoid((cx, cy + H * 0.3, 12), (10, 3, 7), P["dark"], tex=0)
        for k in range(5):
            m.capsule((cx - 8 + k * 4, cy + H * 0.33, 18), (cx - 8 + k * 4, cy + H * 0.34, 14), 0.6, P["bone"], r1=0.1)
        for k in range(6):
            a = k / 6 * 2 * math.pi + 0.3
            m.capsule((cx + math.cos(a) * W * 0.3, cy + math.sin(a) * H * 0.28, 20),
                      (cx + math.cos(a) * W * 0.45, cy + math.sin(a) * H * 0.42, 36), 2.2, P["bone"], r1=0.3)
        anim.append(("pustules", (cx, cy, 24), 6, P["glow"]))
        height = 52
    elif shape == "house":
        m.capsule((cx, cy, 0), (cx, cy, 16), 5, P["flesh"], r1=4, stripe=(2, 2))
        m.ellipsoid((cx, cy, 20), (W / 2 - 3, H / 2 - 4, 7), P["shell"], tex=0.12, spec=0.4)
        m.ellipsoid((cx, cy - 2, 25), (W * 0.25, H * 0.22, 2.5), (200, 200, 200), team=True)
        anim.append(("pustules", (cx, cy, 20), 4, P["glow"]))
        height = 32
    elif shape == "dropoff":
        m.ellipsoid((cx, cy, 10), (W / 2 - 4, H / 2 - 5, 11), P["flesh"], tex=0.18, stripe=(0, 3))
        m.ellipsoid((cx, cy + H * 0.3, 10), (7, 2, 5), P["dark"], tex=0)
        m.ellipsoid((cx - 4, cy - 6, 19), (8, 6, 3), (200, 200, 200), team=True)
        for k in range(5):
            a = k / 5 * 2 * math.pi
            m.capsule((cx, cy, 18), (cx + math.cos(a) * W * 0.4, cy + math.sin(a) * H * 0.4, 2), 1.2, P["shell2"], r1=0.6)
        height = 26
    elif shape == "barracks":
        m.ellipsoid((cx, cy + 2, 2), (W * 0.32, H * 0.28, 2.5), P["slime"], glow=True, tex=0.2)
        for k in range(8):
            a = k / 8 * 2 * math.pi
            x, y = cx + math.cos(a) * W * 0.38, cy + 2 + math.sin(a) * H * 0.34
            m.capsule((x, y, 0), (x - math.cos(a) * 8, y - math.sin(a) * 8, 26), 2.6, P["bone"], r1=0.5)
        m.ellipsoid((cx, cy - H * 0.3, 10), (W * 0.2, H * 0.12, 10), P["shell"], spec=0.4)
        m.ellipsoid((cx, cy - H * 0.3, 18), (W * 0.12, H * 0.08, 4), (200, 200, 200), team=True)
        anim.append(("bubbles", (cx, cy + 2, 3), 8, P["glow"]))
        height = 34
    elif shape == "pen":
        m.ellipsoid((cx, cy, 6), (W / 2 - 3, H / 2 - 3, 8), P["shell"], tex=0.12, spec=0.4, mat="scales")
        m.ellipsoid((cx, cy, 10), (W * 0.3, H * 0.28, 5), P["dark"], tex=0)
        for k in range(10):
            a = k / 10 * 2 * math.pi
            m.capsule((cx + math.cos(a) * W * 0.34, cy + math.sin(a) * H * 0.32, 8),
                      (cx + math.cos(a) * W * 0.46, cy + math.sin(a) * H * 0.44, 26), 1.8, P["bone"], r1=0.2)
        m.ellipsoid((cx + W * 0.32, cy + H * 0.32, 12), (5, 5, 4), (200, 200, 200), team=True)
        height = 32
    elif shape == "tech":
        m.ellipsoid((cx, cy, 14), (W * 0.36, H * 0.34, 16), P["flesh"], tex=0.2)
        for k in range(5):
            m.ellipsoid((cx - 14 + k * 7, cy, 26), (2.6, H * 0.3, 2), (170, 90, 130))
        m.ellipsoid((cx, cy + H * 0.15, 10), (W * 0.3, H * 0.15, 6), (200, 200, 200), team=True)
        for k in range(6):
            a = k / 6 * 2 * math.pi
            m.capsule((cx + math.cos(a) * W * 0.3, cy + math.sin(a) * H * 0.3, 6),
                      (cx + math.cos(a) * W * 0.48, cy + math.sin(a) * H * 0.46, 1), 1.6, P["shell2"], r1=0.6)
        anim.append(("motes", (cx, cy, 36), 8, P["glow"]))
        height = 42
    elif shape == "aerie":
        pts = [(cx, cy, 0), (cx + 4, cy - 2, 16), (cx - 3, cy - 4, 32), (cx + 2, cy - 6, 48), (cx, cy - 7, 60)]
        for k, (a, b) in enumerate(zip(pts, pts[1:])):
            m.capsule(a, b, 9 - k * 2, P["bone"], r1=7 - k * 2, stripe=(2, 3))
        for k in range(4):
            z = 14 + k * 12
            m.capsule((cx, cy - k, z), (cx + (8 if k % 2 else -8), cy - k, z + 6), 1.6, P["bone"], r1=0.2)
        m.ellipsoid((cx, cy, 6), (12, 10, 6), P["shell"], spec=0.4)
        m.ellipsoid((cx, cy + 6, 7), (6, 3, 3), (200, 200, 200), team=True)
        anim.append(("motes", (cx, cy - 7, 58), 5, P["glow"]))
        height = 68
    elif shape == "tower":
        m.ellipsoid((cx, cy, 5), (W / 2 - 6, H / 2 - 6, 6), P["shell"], tex=0.12, spec=0.4)
        pts = [(cx, cy, 6), (cx + 3, cy, 18), (cx - 2, cy, 30), (cx + 1, cy, 40)]
        for k, (a, b) in enumerate(zip(pts, pts[1:])):
            m.capsule(a, b, 5 - k, P["shell2"], r1=4 - k, stripe=(2, 2.2), spec=0.4)
        for k in range(9):
            a = k * 2.4
            z = 10 + k * 3.4
            m.capsule((cx, cy, z), (cx + math.cos(a) * 9, cy + math.sin(a) * 9, z + 4), 1.2, P["bone"], r1=0.1)
        m.ellipsoid((cx + 1, cy, 42), (3, 3, 3), P["glow"], glow=True)
        m.ellipsoid((cx, cy + 6, 8), (6, 2, 3), (200, 200, 200), team=True)
        height = 50
    else:
        return organic("house", w, h)
    m.height = height
    m.anim = anim
    return m


# ---------------------------------------------------------------------------
def _tipi(m, x, y, r, hgt, P, bands=True, team=True):
    """Bőrtipi: enyhén homorú kúp varrással, festett sávokkal, felhajtott ajtólappal, kereszteződő rudakkal."""
    H_ = hgt * 0.86
    m.add(_tent(x, y, 0, r, 0.9, H_, 1.12), shade(P["hide"], 1.14), tex=0.1, mat="cloth")
    if bands:
        for k, (z, col) in enumerate(((hgt * 0.14, P["red"]), (hgt * 0.27, P["turq"]), (hgt * 0.56, P["black"]))):
            t0, t1 = z / H_, (z + 1.6) / H_
            r0 = 0.9 + (r - 0.9) * (1 - t0) ** 1.12 + 0.5
            r1 = 0.9 + (r - 0.9) * (1 - t1) ** 1.12 + 0.5
            m.capsule((x, y, z), (x, y, z + 1.6), r0, col, r1=r1, ends=False, team=(team and k == 0))
            if k == 0:  # cikcakk-minta a vörös sáv fölött
                for j in range(12):
                    a = j / 12 * 2 * math.pi
                    rr = r0 - 0.4
                    m.ellipsoid((x + math.cos(a) * rr, y + math.sin(a) * rr, z + 2.4 + (j % 2) * 0.9),
                                (0.7, 0.7, 0.5), P["black"])
    # varrásvonal (fapeckek) a front fölött
    for k in range(6):
        t = 0.45 + k * 0.08
        rr = 0.9 + (r - 0.9) * (1 - t) ** 1.12 + 0.2
        m.ellipsoid((x, y + rr, t * H_), (0.35, 0.3, 0.35), P["wood2"])
    for k in range(7):  # kilógó, csúcsban keresztező rudak
        a = k / 7 * 2 * math.pi + 0.3
        m.capsule((x + math.cos(a) * r * 0.16, y + math.sin(a) * r * 0.16, H_ - 3),
                  (x - math.cos(a) * 3.2, y - math.sin(a) * 3.2, hgt + 5), 0.45, P["wood2"])
    # bejárat felhajtott lappal
    dz = hgt * 0.34
    m.capsule((x, y + r * 0.86, 0.2), (x, y + r * 0.62, dz), r * 0.22, (52, 38, 30), r1=0.4)
    m.capsule((x + r * 0.18, y + r * 0.82, dz * 0.4), (x + r * 0.34, y + r * 0.6, dz * 0.95), 1.1, P["hide2"], r1=0.5)
    for k in range(3):
        a = 0.8 + k * 0.6
        rr = 0.9 + (r - 0.9) * (1 - 0.42) ** 1.12 + 0.2
        m.ellipsoid((x + math.cos(a) * rr, y + math.sin(a) * rr, H_ * 0.42), (1.0, 0.5, 1.0),
                    (240, 236, 220), tex=0)  # festett holdak


def _totem(m, x, y, hgt, P):
    cols = [P["red"], P["turq"], P["black"], P["wood"]]
    for k in range(int(hgt // 6)):
        z = k * 6
        m.box((x, y, z + 3), (2.6, 2.6, 3), cols[k % 4] if k % 2 else P["wood"], mat="planks")
        m.ellipsoid((x, y + 2.7, z + 4), (1.4, 0.4, 0.6), (240, 236, 220), tex=0)
        m.ellipsoid((x, y + 2.9, z + 4), (0.5, 0.3, 0.4), (20, 18, 16), tex=0)
        m.capsule((x, y + 2.6, z + 2.6), (x, y + 4.6, z + 2.0), 0.6, P["red"], r1=0.2)
    m.ellipsoid((x - 4, y, hgt + 1), (4, 1.2, 1.4), P["turq"])  # szárnyak a tetején
    m.ellipsoid((x + 4, y, hgt + 1), (4, 1.2, 1.4), P["turq"])
    m.ellipsoid((x, y, hgt + 2), (2.2, 2.2, 2.0), P["red"], team=True)


AU = dict(bark=(172, 142, 108), bark2=(132, 106, 80), bark3=(214, 186, 146), ochre=(190, 88, 46), ochre2=(214, 150, 72),
          white=(236, 230, 214), trunk=(240, 234, 222), trunk2=(206, 196, 180), leaf=(98, 124, 82),
          leaf2=(138, 152, 98), earth=(174, 110, 70), earth2=(146, 90, 58), rock=(176, 92, 56), rock2=(140, 70, 44),
          emu=(134, 114, 92), emu2=(90, 76, 64), skinb=(70, 110, 150), stone=(132, 126, 118), ember=(255, 150, 50),
          wood=(126, 88, 52), wood2=(90, 60, 36), black=(40, 34, 32), red=TR["red"], turq=TR["turq"])


def _slab(p0, p1, width, side, out=None):
    """Vékony, téglalap alakú kéreglap p0-tól p1-ig; 'side' a vízszintes szélességi irány (egységvektor)."""
    from .engine import _q
    p0, p1, sd = np.asarray(p0, float), np.asarray(p1, float), np.asarray(side, float)
    ax = p1 - p0
    L = np.linalg.norm(ax) + 1e-9
    st = 0.5 / _q()
    T_, S_ = np.meshgrid(np.arange(0, 1.0001, st / L), np.arange(-width / 2, width / 2 + 0.01, st), indexing="ij")
    pts = p0 + np.outer(T_.ravel(), ax) + np.outer(S_.ravel(), sd)
    n = np.cross(ax / L, sd)
    if (np.dot(n, out) if out is not None else n[2]) < 0:  # kifelé (vagy felfelé) nézzen
        n = -n
    return pts, np.repeat(n[None], len(pts), 0)


def _humpy(m, x, y, r, hgt, P, team=True, door=True, seed=0):
    """Kéregkunyhó (humpy): ágvázra fektetett, egymást átfedő eukaliptuszkéreg-lapok, alacsony kupola."""
    rng = np.random.default_rng(seed + int(r * 10))
    hgt = min(hgt, r * 0.78)  # lapos kupola: a meredek oldalt a felülről jövő fény alig éri
    m.add(_tent(x, y, 0, r, 2.0, hgt, 1.15), (236, 210, 168), tex=0.2, mat="shingles")
    n = max(4, int(r * 0.3))
    for k in range(n):  # a kérget leszorító hosszú ágak (a kupola felületén)
        a = k / n * 2 * math.pi + rng.uniform(-0.15, 0.15)
        prev = None
        for t in np.linspace(0.02, 0.9, 5):
            rr = 2.0 + (r - 2.0) * (1 - t) ** 1.15 + 0.7
            pt = (x + math.cos(a) * rr, y + math.sin(a) * rr, t * hgt + 0.7)
            if prev is not None:
                m.capsule(prev, pt, 0.5, P["bark3"])
            prev = pt
    for k in range(5):  # a csúcson kiálló ágak
        a = k / 5 * 2 * math.pi + 0.5
        m.capsule((x + math.cos(a) * 1.5, y + math.sin(a) * 1.5, hgt - 1),
                  (x + math.cos(a) * 4.5, y + math.sin(a) * 4.5, hgt + 3.5), 0.45, P["bark2"])
    if door:
        dy = y + r * 0.92
        m.capsule((x, dy, 0.2), (x, dy - 0.6, hgt * 0.42), r * 0.2, (46, 34, 28), r1=0.6)
        if team:  # csapatszínű festett pötty-sor az ajtó fölött
            for k in range(5):
                m.ellipsoid((x + (k - 2) * 1.6, dy - 0.4, hgt * 0.52 + abs(k - 2) * -0.6), (0.6, 0.4, 0.6),
                            (200, 200, 200), team=True, tex=0)
    return hgt


def _gumtree(m, x, y, hgt, P, seed=0):
    """Eukaliptusz: fehér, foltos, görbe törzs, laza, csomós lombkorona."""
    rng = np.random.default_rng(seed)
    pts = [(x, y, 0)]
    for k in range(3):
        px, py, pz = pts[-1]
        pts.append((px + rng.uniform(-2.5, 2.5), py + rng.uniform(-2.0, 2.0), pz + hgt * 0.22))
    for k in range(3):
        m.capsule(pts[k], pts[k + 1], 2.1 - k * 0.45, P["trunk"] if k % 2 == 0 else P["trunk2"], r1=1.8 - k * 0.45,
                  tex=0.15)
    top = pts[-1]
    for k in range(4):  # ágak és lombcsomók
        a = k / 4 * 2 * math.pi + rng.uniform(0, 1)
        e = (top[0] + math.cos(a) * hgt * 0.22, top[1] + math.sin(a) * hgt * 0.18, top[2] + hgt * 0.12)
        m.capsule(top, e, 0.9, P["trunk"], r1=0.5)
        for j in range(3):
            c = (e[0] + rng.uniform(-3, 3), e[1] + rng.uniform(-3, 3), e[2] + rng.uniform(0, 4))
            m.ellipsoid(c, (rng.uniform(3.5, 5.5), rng.uniform(3.5, 5.0), rng.uniform(2.2, 3.2)),
                        P["leaf"] if (j + k) % 2 else P["leaf2"], mat="fur", tex=0.2)
    return top[2] + hgt * 0.2


def _emu(m, x, y, z, s, P, facing=0.0, sit=False):
    """Kis emu díszletnek (karám, lovasállás)."""
    ca, sa = math.cos(facing), math.sin(facing)
    def R(dx, dy, dz):
        return (x + dx * ca - dy * sa, y + dx * sa + dy * ca, z + dz)
    lz = 0 if sit else 6 * s
    if not sit:
        for side in (-1, 1):
            m.capsule(R(0, side * 1.2 * s, lz), R(0.4 * s, side * 1.2 * s, 0), 0.5 * s, P["emu2"])
    m.ellipsoid(R(-0.5 * s, 0, lz + 3 * s), (4.2 * s, 3.0 * s, 3.0 * s), P["emu"], mat="fur")
    m.capsule(R(2.6 * s, 0, lz + 4 * s), R(3.6 * s, 0, lz + 10 * s), 0.9 * s, P["skinb"], r1=0.7 * s)
    m.ellipsoid(R(3.9 * s, 0, lz + 10.6 * s), (1.2 * s, 0.9 * s, 0.9 * s), P["emu2"])
    m.capsule(R(4.6 * s, 0, lz + 10.5 * s), R(5.8 * s, 0, lz + 10.2 * s), 0.35 * s, P["bark3"], r1=0.2 * s)


def _dotrock(m, x, y, r, hgt, P, seed=0):
    """Vörös homokkő szikla, rajta fehér-okker pöttyfestés körökben."""
    rng = np.random.default_rng(seed)
    m.ellipsoid((x, y, hgt * 0.45), (r, r * 0.8, hgt * 0.6), P["rock"], tex=0.2, mat="stones")
    for ring in range(3):
        rr = 1.5 + ring * 1.6
        n = 6 + ring * 5
        for k in range(n):
            a = k / n * 2 * math.pi
            m.ellipsoid((x + math.cos(a) * rr * 0.9, y + r * 0.8 * 0.55 + math.sin(a) * rr * 0.35,
                         hgt * 0.62 + math.sin(a) * rr * 0.6), (0.45, 0.3, 0.45),
                        P["white"] if ring != 1 else P["ochre2"], tex=0)


def tribal(shape, w, h):
    """Wullof (ausztrál karakter): vörös föld, kéregkunyhók, eukaliptuszok, emuk, pöttyfestés."""
    m = Model(shape)
    W, H = w * T, h * T
    cx, cy = W / 2, H / 2
    P = AU
    anim = []
    _patch(m, cx, cy, W / 2 - 2, H / 2 - 2, 1.2, P["earth"], seed=w * 7 + h, tex=0.25)  # vörös föld
    if shape == "hq":
        _humpy(m, cx - 6, cy - 6, 24, 26, P, seed=1)
        for k, (dx, dy) in enumerate(((-38, 28), (36, 30))):
            _humpy(m, cx + dx, cy + dy, 10, 13, P, team=False, seed=k + 3)
        _gumtree(m, cx + 38, cy - 30, 40, P, seed=5)
        for k in range(8):  # tűzrakóhely
            a = k / 8 * 2 * math.pi
            m.ellipsoid((cx + math.cos(a) * 5, cy + 32 + math.sin(a) * 4, 1), (1.6, 1.4, 1.2), P["stone"], tex=0.2)
        m.ellipsoid((cx, cy + 32, 1.5), (3, 2.6, 1.6), P["ember"], glow=True)
        for k in range(3):  # bumerángok és dárdák a falnak döntve
            m.capsule((cx - 34 + k * 3, cy - 8, 0), (cx - 33 + k * 3, cy - 10, 18), 0.5, P["wood"])
        _dotrock(m, cx - 38, cy - 30, 7, 9, P, seed=2)
        anim.append(("embers", (cx, cy + 32, 3), 4, P["ember"]))
        anim.append(("smoke", (cx - 6, cy - 6, 30), 0, (130, 125, 120)))
        height = 66
    elif shape == "house":
        _humpy(m, cx, cy, 14, 16, P, seed=2)
        m.capsule((cx + 18, cy + 14, 0), (cx + 17, cy + 13, 9), 0.5, P["wood2"])  # szárítórúd
        m.capsule((cx + 17, cy + 13, 9), (cx + 10, cy + 18, 8), 0.4, P["wood2"])
        anim.append(("smoke", (cx, cy, 20), 0, (130, 125, 120)))
        height = 26
    elif shape == "barracks":
        _humpy(m, cx - 18, cy - 12, 14, 16, P, seed=4)
        _gumtree(m, cx + 24, cy - 18, 36, P, seed=7)
        for k in range(8):
            a = k / 8 * 2 * math.pi
            m.ellipsoid((cx + 4 + math.cos(a) * 6, cy + 20 + math.sin(a) * 5, 1), (1.8, 1.6, 1.3), P["stone"], tex=0.2)
        m.ellipsoid((cx + 4, cy + 20, 2), (3.6, 3, 2), P["ember"], glow=True)
        for k in range(4):  # lándzsatartó
            m.capsule((cx + 26 + k * 2, cy + 20, 0), (cx + 24 + k * 2.5, cy + 18, 24), 0.55, P["wood"])
            m.capsule((cx + 24 + k * 2.5, cy + 18, 24), (cx + 23.6 + k * 2.5, cy + 17.6, 27), 0.9, P["stone"], r1=0.1)
        for k in range(3):  # bumerángok egy rúdon
            bx = cx - 30 + k * 4
            m.capsule((bx, cy + 22, 8), (bx + 1.6, cy + 22, 6), 0.5, P["wood"])
            m.capsule((bx + 1.6, cy + 22, 6), (bx + 3.0, cy + 22, 8), 0.5, P["ochre"])
        m.capsule((cx - 32, cy + 22, 0), (cx - 32, cy + 22, 9), 0.4, P["wood2"])
        m.capsule((cx - 20, cy + 22, 0), (cx - 20, cy + 22, 9), 0.4, P["wood2"])
        m.capsule((cx - 32, cy + 22, 8.5), (cx - 20, cy + 22, 8.5), 0.3, P["wood2"])
        anim.append(("embers", (cx + 4, cy + 20, 4), 4, P["ember"]))
        height = 50
    elif shape == "pen":
        n = 9
        for k in range(n):  # rönkkerítés
            for (x0, y0, x1, y1) in ((4, 4, W - 4, 4), (4, H - 4, W - 4, H - 4), (4, 4, 4, H - 4), (W - 4, 4, W - 4, H - 4)):
                x = x0 + (x1 - x0) * k / (n - 1)
                y = y0 + (y1 - y0) * k / (n - 1)
                m.capsule((x, y, 0), (x + 0.5, y, 9), 1.0, P["trunk2"], r1=0.7)
        for (x0, y0, x1, y1) in ((4, 4, W - 4, 4), (4, H - 4, W * 0.4, H - 4), (W * 0.6, H - 4, W - 4, H - 4),
                                 (4, 4, 4, H - 4), (W - 4, 4, W - 4, H - 4)):
            m.capsule((x0, y0, 6), (x1, y1, 6), 0.7, P["trunk"])
        _emu(m, W * 0.34, H * 0.4, 0, 1.4, P, facing=0.4)
        _emu(m, W * 0.6, H * 0.62, 0, 1.3, P, facing=2.6)
        _emu(m, W * 0.7, H * 0.3, 0, 1.2, P, sit=True, facing=1.2)
        for k in range(5):  # tojások a fészekben
            m.ellipsoid((W * 0.22 + (k % 3) * 2.4, H * 0.72 + (k // 3) * 2, 1.4), (1.3, 1.0, 1.2), (46, 70, 70), spec=0.4)
        _gumtree(m, W * 0.82, H * 0.82, 30, P, seed=9)
        height = 44
    elif shape == "bisonpen":  # bölénykarám: rönkkerítés, bölények, szénakazal, itatóvályú
        n = 8
        for k in range(n):
            for (x0, y0, x1, y1) in ((4, 4, W - 4, 4), (4, H - 4, W - 4, H - 4), (4, 4, 4, H - 4), (W - 4, 4, W - 4, H - 4)):
                x = x0 + (x1 - x0) * k / (n - 1)
                y = y0 + (y1 - y0) * k / (n - 1)
                m.capsule((x, y, 0), (x + 0.5, y, 11), 1.3, P["wood2"], r1=0.9)
        for (x0, y0, x1, y1) in ((4, 4, W - 4, 4), (4, H - 4, W * 0.4, H - 4), (W * 0.6, H - 4, W - 4, H - 4),
                                 (4, 4, 4, H - 4), (W - 4, 4, W - 4, H - 4)):
            for z in (4, 9):
                m.capsule((x0, y0, z), (x1, y1, z), 0.8, P["wood"])
        for k, (bx, by, f) in enumerate(((W * 0.36, H * 0.42, 0.2), (W * 0.62, H * 0.6, 2.8))):
            ca, sa = math.cos(f), math.sin(f)
            m.ellipsoid((bx, by, 6), (9 * abs(ca) + 5 * abs(sa), 9 * abs(sa) + 5 * abs(ca), 5.5), (96, 66, 44), mat="fur")
            m.ellipsoid((bx + ca * 8, by + sa * 8, 7), (4, 4, 4.4), (70, 48, 32), mat="fur")  # púp és fej
            for sd in (-1, 1):
                m.capsule((bx + ca * 9 - sa * sd * 2.5, by + sa * 9 + ca * sd * 2.5, 9),
                          (bx + ca * 9 - sa * sd * 4.5, by + sa * 9 + ca * sd * 4.5, 11), 0.6, (232, 224, 200), r1=0.2)
        m.ellipsoid((W * 0.78, H * 0.26, 4), (6, 5, 4.5), (206, 180, 96), mat="thatch")  # széna
        m.box((W * 0.2, H * 0.75, 1.6), (6, 2.2, 1.6), P["wood"], mat="planks")  # vályú
        m.box((W * 0.2, H * 0.75, 3.0), (5.2, 1.5, 0.2), (90, 140, 170), spec=0.6)
        _gumtree(m, W * 0.86, H * 0.84, 28, P, seed=15)
        height = 40
    elif shape == "groominest":  # Groomi-fészek: hatalmas ágfészek piros tojásokkal, csontokkal, vörös sziklán
        _dotrock(m, cx + W * 0.3, cy - H * 0.3, 8, 10, P, seed=8)
        rng = np.random.default_rng(31)
        for k in range(46):  # ágakból rakott fészekgyűrű
            a = k / 46 * 2 * math.pi
            r0 = 18 + rng.uniform(-2, 2)
            m.capsule((cx + math.cos(a) * r0, cy + math.sin(a) * r0 * 0.9, rng.uniform(1, 6)),
                      (cx + math.cos(a + 0.5) * (r0 + 4), cy + math.sin(a + 0.5) * (r0 + 4) * 0.9, rng.uniform(1, 7)),
                      rng.uniform(0.6, 1.0), (P["wood"], P["wood2"], P["bark"])[k % 3])
        m.ellipsoid((cx, cy, 2), (17, 15, 2.5), (110, 84, 60), mat="thatch")
        for k in range(5):  # piros, pettyes tojások
            a = k * 1.3
            ex, ey = cx + math.cos(a) * 6, cy + math.sin(a) * 5
            m.ellipsoid((ex, ey, 5), (2.6, 2.2, 3.2), (196, 60, 46), spec=0.3)
            m.ellipsoid((ex + 1.2, ey + 1.6, 6), (0.5, 0.4, 0.5), (250, 220, 120), tex=0)
        for k in range(3):  # csontok a fészek szélén
            a = 2.2 + k * 0.7
            m.capsule((cx + math.cos(a) * 22, cy + math.sin(a) * 20, 1), (cx + math.cos(a) * 26, cy + math.sin(a) * 23, 1.5),
                      0.8, (232, 224, 200))
        for k, (x_, y_) in enumerate(((W * 0.12, H * 0.2), (W * 0.84, H * 0.8))):
            m.capsule((x_, y_, 0), (x_, y_, 22), 0.8, P["wood2"])
            for j in range(3):
                m.capsule((x_, y_, 21 - j * 2.5), (x_ + 3, y_ + 1, 18 - j * 2.5), 0.9, (200, 52, 40) if j % 2 else
                          (250, 140, 40), r1=0.25)  # piros tollak
        anim.append(("motes", (cx, cy, 12), 3, (255, 140, 60)))
        height = 30
    elif shape == "tech":
        _humpy(m, cx - 4, cy - 6, 17, 19, P, seed=6)
        _dotrock(m, cx + 22, cy + 18, 8, 10, P, seed=3)
        m.capsule((cx - 26, cy + 18, 2), (cx - 12, cy + 24, 3), 1.3, P["ochre"], r1=1.0)  # didzseridu
        for k in range(4):
            m.capsule((cx - 26 + k * 3.5, cy + 18 + k * 1.5, 2.4), (cx - 26 + k * 3.5 + 0.6, cy + 18 + k * 1.5 + 0.3, 2.6),
                      1.45, P["white"] if k % 2 else P["black"], r1=1.3)
        anim.append(("motes", (cx, cy, 30), 6, (240, 220, 170)))
        height = 34
    elif shape == "aerie":  # vörös szikla viharkakadu-fészekkel
        rng = np.random.default_rng(21)
        for k, (dx, dy, r, hz) in enumerate(((0, -4, 20, 30), (-16, 8, 13, 16), (17, 6, 12, 18), (-6, 18, 10, 10),
                                              (10, -18, 11, 20), (-20, -12, 9, 12), (22, 20, 7, 7))):
            m.ellipsoid((cx + dx, cy + dy, hz * 0.5), (r * rng.uniform(0.85, 1.15), r * 0.8, hz * 0.62),
                        (P["rock"], P["rock2"], P["ochre"])[k % 3], tex=0.18, mat="blocks")
        for i in range(14):  # ágfészek
            a = i / 14 * 2 * math.pi
            m.capsule((cx + math.cos(a) * 5, cy - 4 + math.sin(a) * 5, 34), (cx + math.cos(a) * 10, cy - 4 + math.sin(a) * 10,
                      37), 1.2, P["wood"])
        m.ellipsoid((cx, cy - 4, 35), (5, 5, 1.6), (60, 50, 40))
        _gumtree(m, cx - W * 0.36, cy + H * 0.3, 30, P, seed=11)
        _dotrock(m, cx + W * 0.3, cy + H * 0.34, 5, 6, P, seed=4)
        anim.append(("motes", (cx, cy - 4, 42), 4, (255, 240, 140)))
        height = 58
    elif shape == "tower":  # eukaliptusz-őrfa kilátóval
        top = _gumtree(m, cx, cy, 62, P, seed=13)
        m.box((cx, cy, 22), (7, 7, 0.8), P["wood"], mat="planks")  # kilátó-dobogó
        for k in range(4):  # korlát
            a = k * math.pi / 2
            m.capsule((cx + math.cos(a) * 7, cy + math.sin(a) * 7, 23), (cx + math.cos(a + math.pi / 2) * 7,
                      cy + math.sin(a + math.pi / 2) * 7, 23), 0.35, P["wood2"])
        for x_ in (cx - 6, cx + 6):
            for y_ in (cy - 6, cy + 6):
                m.capsule((x_, y_, 0), (x_ * 0.9 + cx * 0.1, y_ * 0.9 + cy * 0.1, 22), 0.6, P["wood2"])
        for k in range(10):  # létrafokok
            m.capsule((cx - 1.5, cy + 6.5, 2 + k * 2), (cx + 1.5, cy + 6.5, 2 + k * 2), 0.3, P["wood2"])
        m.box((cx + 4.5, cy + 6, 25), (1.6, 0.3, 2.2), (200, 200, 200), team=True)
        height = int(top) + 8
    else:
        return tribal("house", w, h)
    m.height = height
    m.anim = anim
    return m


def _mushroom(m, x, y, stalk_h, cap_r, P, cap=None, glow=False, team=False):
    m.capsule((x, y, 0), (x, y, stalk_h), cap_r * 0.28, P["stalk"], r1=cap_r * 0.22, tex=0.08)
    m.ellipsoid((x, y, stalk_h), (cap_r * 0.95, cap_r * 0.95, cap_r * 0.25), P["gill"], stripe=(0, 0.9))
    col = cap or P["cap"]
    m.ellipsoid((x, y, stalk_h + cap_r * 0.25), (cap_r, cap_r, cap_r * 0.55), col, spec=0.3, glow=glow)
    for k in range(6):
        a = k * 2.4
        r = cap_r * (0.2 + (k % 3) * 0.25)
        z = stalk_h + cap_r * 0.25 + cap_r * 0.55 * math.sqrt(max(0.0, 1 - (r / cap_r) ** 2))
        m.ellipsoid((x + math.cos(a) * r, y + math.sin(a) * r, z), (cap_r * 0.13, cap_r * 0.13, cap_r * 0.06),
                    (200, 200, 200) if team and k == 0 else P["spot"], tex=0, team=team and k == 0)


def fungal(shape, w, h):
    m = Model(shape)
    W, H = w * T, h * T
    cx, cy = W / 2, H / 2
    P = FU
    anim = []
    _patch(m, cx, cy, W / 2 - 1, H / 2 - 1, 1.5, P["moss"], seed=w * 3 + h, tex=0.25, mat="fur")
    for k in range(5):  # gyökérfonalak
        a = k / 5 * 2 * math.pi + 0.2
        m.capsule((cx, cy, 1.4), (cx + math.cos(a) * W * 0.5, cy + math.sin(a) * H * 0.5, 0.6), 0.6, P["stalk2"])
    if shape == "hq":  # Szívgomba: kicsi, lüktető, lövő
        _mushroom(m, cx, cy, 20, 15, P, cap=(150, 70, 120))
        m.ellipsoid((cx, cy, 34), (6, 6, 3), (200, 200, 200), team=True)
        m.ellipsoid((cx + 9, cy + 6, 26), (2.6, 2.6, 2.6), P["glow"], glow=True)  # spóraszáj
        m.ellipsoid((cx - 9, cy + 6, 26), (2.0, 2.0, 2.0), P["glow"], glow=True)
        for k in range(4):
            a = k * 1.6 + 0.4
            _mushroom(m, cx + math.cos(a) * 20, cy + math.sin(a) * 18, 7, 5, P)
        anim.append(("pustules", (cx, cy, 24), 4, P["glow"]))
        height = 46
    elif shape == "node":
        _mushroom(m, cx, cy, 8, 6, P, cap=P["cap2"])
        m.ellipsoid((cx, cy, 13), (1.4, 1.4, 1.0), P["glow"], glow=True)
        height = 18
    elif shape == "barracks":  # Termőtest: nagy taplógomba-polcok
        m.capsule((cx, cy - 6, 0), (cx, cy - 6, 30), 9, P["root"], r1=6, tex=0.15, mat="fur")
        for k, (z, r) in enumerate(((10, 18), (20, 14), (29, 10))):
            m.ellipsoid((cx + 4, cy + 2, z), (r, r * 0.8, 3), P["cap"] if k % 2 == 0 else (196, 120, 70), spec=0.2,
                        stripe=(1, 1.4))
        m.ellipsoid((cx, cy - 6, 33), (6, 6, 2.4), (200, 200, 200), team=True)
        for k in range(3):
            _mushroom(m, cx - 26 + k * 6, cy + 24, 6, 4, P)
        anim.append(("bubbles", (cx + 4, cy + 12, 8), 5, P["glow"]))
        height = 40
    elif shape == "pen":  # Tüskegomba-telep
        for k in range(7):
            a = k / 7 * 2 * math.pi
            x, y = cx + math.cos(a) * 26, cy + math.sin(a) * 24
            m.capsule((x, y, 0), (x, y, 18 + (k % 3) * 4), 3, (120, 70, 150), r1=0.3, spec=0.3, stripe=(2, 2))
        _mushroom(m, cx, cy, 16, 12, P, cap=(120, 70, 150), team=True)
        anim.append(("motes", (cx, cy, 30), 5, (200, 140, 255)))
        height = 34
    elif shape == "tech":  # Gyökérbarlang
        m.ellipsoid((cx, cy, 6), (W * 0.42, H * 0.4, 18), P["root"], tex=0.2, mat="fur")
        m.ellipsoid((cx, cy + H * 0.34, 8), (9, 4, 8), (30, 22, 18), tex=0)
        for k in range(8):
            a = k / 8 * 2 * math.pi
            m.capsule((cx + math.cos(a) * W * 0.2, cy + math.sin(a) * H * 0.2, 20),
                      (cx + math.cos(a) * W * 0.48, cy + math.sin(a) * H * 0.46, 1), 2.2, P["root"], r1=0.8)
        for k in range(3):
            _mushroom(m, cx - 10 + k * 10, cy - 6, 22 + k * 2, 5, P, cap=P["blue"], glow=True)
        m.ellipsoid((cx, cy, 24), (5, 5, 2), (200, 200, 200), team=True)
        height = 40
    elif shape == "observatory":  # Álomkert
        for k in range(9):
            a = k * 2.3
            r = 6 + (k % 3) * 9
            _mushroom(m, cx + math.cos(a) * r, cy + math.sin(a) * r, 8 + (k % 4) * 3, 4 + (k % 3), P, cap=P["blue"],
                      glow=True)
        _mushroom(m, cx, cy, 22, 9, P, cap=(90, 160, 230), glow=True, team=True)
        anim.append(("motes", (cx, cy, 30), 9, (150, 210, 255)))
        height = 38
    elif shape == "aerie":  # Lebegőbölcső
        for k in range(4):
            a = k * 1.57 + 0.3
            x, y = cx + math.cos(a) * 18, cy + math.sin(a) * 16
            m.capsule((x, y, 0), (x, y, 24 + k * 3), 1.6, P["stalk2"])
            m.ellipsoid((x, y, 28 + k * 3), (6, 6, 5.5), P["cap2"], tex=0.12)
        m.ellipsoid((cx, cy, 12), (10, 10, 9), P["cap2"], tex=0.12)
        m.ellipsoid((cx, cy, 21), (4, 4, 2), (200, 200, 200), team=True)
        height = 44
    else:
        return fungal("node", w, h)
    m.height = height
    m.anim = anim
    return m


def _steps(m, cx, cy, hx, hy, n, sh, col, **kw):
    """Lépcsőzetes talapzat: n fok, mindegyik sh magas, befelé szűkülve."""
    for k in range(n):
        m.box((cx, cy, k * sh + sh / 2), (hx - k * 1.8, hy - k * 1.8, sh / 2), shade(col, 1 - 0.05 * (n - 1 - k)), **kw)


def _arch(cx, y, z, r, th):
    """Félköríves boltív (kapu fölé), a homlokzat síkjában (X–Z), y-ban kissé kiálló."""
    from .engine import _q
    pts, ns = [], []
    for a in np.linspace(0, math.pi, max(16, int(r * 6 * _q()))):
        for rr in np.arange(r - th, r + 0.01, 0.5 / _q()):
            pts.append((cx + math.cos(a) * rr, y, z + math.sin(a) * rr))
            ns.append((0, 1, 0))
        pts.append((cx + math.cos(a) * r, y - 0.5, z + math.sin(a) * r))
        ns.append((math.cos(a), 0, math.sin(a)))
    return np.array(pts, float), np.array(ns, float)


def _gable(cx, y, z, hw, h):
    """Háromszögű oromzatlap (X–Z síkban, +Y felé néz)."""
    from .engine import _q
    st = 0.5 / _q()
    X, Z = np.meshgrid(np.arange(-hw, hw + 0.01, st), np.arange(0, h + 0.01, st), indexing="ij")
    msk = Z <= h * (1 - np.abs(X) / hw)
    X, Z = X[msk], Z[msk]
    p = np.stack([cx + X, np.full_like(X, y), z + Z], 1)
    n = np.zeros_like(p)
    n[:, 1] = 1
    return p, n


def _column(m, x, y, z0, z1, P):
    m.box((x, y, z0 + 0.8), (2.6, 2.6, 0.8), P["marble2"])
    m.cylinder((x, y, z0 + 1.6), (x, y, z1 - 1.6), 1.9, P["marble"], stripe=(0, 0.9))
    m.box((x, y, z1 - 0.8), (2.6, 2.6, 0.8), P["marble2"])


def imperial(shape, w, h):
    m = Model(shape)
    W, H = w * T, h * T
    cx, cy = W / 2, H / 2
    P = IM
    anim = []
    if shape == "hq":  # Fórum: oromzatos templomhomlokzat oszlopcsarnokkal, kétoldalt műhelyszárnyak
        _steps(m, cx, cy, W / 2 - 2, H / 2 - 2, 3, 1.6, P["marble2"], mat="blocks")
        z0 = 4.8
        for sx in (-1, 1):  # oldalszárnyak: alacsonyabb, cseréptetős műhelyek
            wx = cx + sx * (W / 2 - 17)
            m.box((wx, cy - 6, z0 + 7), (13, H / 2 - 16, 7), P["marble"], mat="blocks")
            m.roof((wx, cy - 6, z0 + 14), 14.5, H / 2 - 14.5, 5, P["roof"], axis=0, mat="tiles")
            for k in range(3):  # lélekfényes ívablakok
                x = wx - 8 + k * 8
                m.box((x, cy - 6 + H / 2 - 16 + 0.2, z0 + 7), (1.8, 0.6, 2.8), (40, 34, 32))
                m.box((x, cy - 6 + H / 2 - 16 + 0.5, z0 + 7.6), (1.1, 0.5, 1.8), P["soul"], glow=True)
            m.cylinder((wx - sx * 4, cy - H * 0.3, z0 + 10), (wx - sx * 4, cy - H * 0.3, 54), 2.8, P["iron"],
                       spec=0.4, mat="rivets")
            m.cylinder((wx - sx * 4, cy - H * 0.3, 54), (wx - sx * 4, cy - H * 0.3, 56), 3.6, P["bronze"], spec=0.9)
            anim.append(("smoke", (wx - sx * 4, cy - H * 0.3, 57), 0, (110, 130, 120)))
        # cella: középső templomtest, a gerinc a néző felé fut, így az oromzat szemből látszik
        hw, hd = W * 0.2, H / 2 - 10
        m.box((cx, cy - 6, z0 + 11), (hw, hd - 6, 11), P["marble"], mat="blocks")
        zt = z0 + 24
        front = cy - 6 + hd - 6
        m.box((cx, cy - 2, zt - 1.2), (hw + 2, hd - 0.5, 1.2), P["marble2"], mat="blocks")  # párkány
        m.roof((cx, cy - 2, zt), hw + 2.5, hd, 9, P["roof"], axis=1, mat="tiles")
        # timpanon (márvány oromzatháromszög) és díszek
        m.add(_gable(cx, cy - 2 + hd + 0.3, zt, hw + 2.5, 9), P["marble"], tex=0.05)
        m.add(_gable(cx, cy - 2 + hd + 0.5, zt + 1.2, hw, 6.6), P["marble2"], tex=0.05)
        m.emblem((cx, cy - 2 + hd + 0.8, zt + 3.4), 3.6, 2.6, 1, "eagle", P["bronze"], spec=0.9)
        m.ellipsoid((cx, cy - 2 + hd, zt + 9.8), (1.6, 1.0, 2.0), P["bronze"], spec=1.0)
        for sx in (-1, 1):
            m.ellipsoid((cx + sx * (hw + 2), cy - 2 + hd, zt + 0.8), (1.4, 1.0, 1.6), P["bronze"], spec=1.0)
        # oszlopcsarnok
        for k in range(6):
            _column(m, cx - hw + 1 + k * (2 * hw - 2) / 5, front + 5, z0, zt - 2.4, P)
        m.box((cx, front + 0.4, z0 + 7), (4.5, 0.6, 7), (34, 30, 30))  # kapu
        m.box((cx, front + 0.7, z0 + 13.5), (5, 0.5, 0.8), P["bronze"], spec=0.9)
        m.box((cx, front + 0.8, z0 + 9), (3.4, 0.4, 3.0), P["soul"], glow=True)
        for sx in (-1, 1):
            _banner(m, cx + sx * (hw + 8), H - 6, 4, 26, P["iron"])
            _brazier(m, cx + sx * 7, H - 5, 3.2, P["soul"])
        anim.append(("embers", (cx - 7, H - 5, 7), 2, P["soul"]))
        height = 66
    elif shape == "house":  # Lélekreaktor
        m.box((cx, cy, 2), (W / 2 - 3, H / 2 - 3, 2), P["marble2"], mat="bricks")
        m.cylinder((cx, cy, 4), (cx, cy, 24), 11, P["metal"], spec=0.7, mat="rivets")
        for z in (8, 16, 23):
            m.cylinder((cx, cy, z), (cx, cy, z + 1.2), 11.6, P["bronze"], spec=0.9)
        m.box((cx, cy + 11, 14), (3, 0.6, 5), P["soul"], glow=True)
        m.ellipsoid((cx, cy, 26), (8, 8, 4), P["soul"], glow=True)
        for sx in (-1, 1):
            m.capsule((cx + sx * 11, cy, 8), (cx + sx * 15, cy + 8, 2), 1.4, P["iron"], spec=0.5)
        m.box((cx, cy - 11.5, 12), (4, 0.6, 3), (200, 200, 200), team=True)
        anim.append(("motes", (cx, cy, 30), 6, P["soul"]))
        height = 34
    elif shape in ("barracks", "pen"):  # Légió-gyár / Öntöde: bazilika-csarnok támpillérekkel, felülvilágítóval
        _steps(m, cx, cy, W / 2 - 2, H / 2 - 2, 2, 1.5, P["marble2"], mat="blocks")
        z0 = 3
        hx, hy = W / 2 - 7, H / 2 - 12
        wall = P["iron"] if shape == "pen" else P["metal"]
        m.box((cx, cy - 4, z0 + 11), (hx, hy, 11), wall, spec=0.35, mat="rivets")
        m.box((cx, cy - 4, z0 + 1.2), (hx + 0.6, hy + 0.6, 1.2), P["marble2"], mat="blocks")  # lábazat
        for k in range(5):  # márvány támpillérek
            x = cx - hx + k * hx / 2
            m.box((x, cy - 4 + hy + 1.2, z0 + 10), (1.8, 1.6, 10), P["marble"], mat="blocks")
            m.box((x, cy - 4 + hy + 1.6, z0 + 20.6), (2.2, 2.0, 0.8), P["marble2"])
        m.box((cx, cy - 4 + hy + 0.3, z0 + 21.4), (hx + 1, 0.8, 1.0), P["bronze"], spec=0.7)  # párkány
        m.roof((cx, cy - 4, z0 + 22), hx + 2, hy + 2, 5, P["roof"], axis=0, mat="tiles", ridge=False)
        # felülvilágító (clerestory): keskeny, magasabb középső sáv saját tetővel
        m.box((cx, cy - 4, z0 + 29), (hx - 6, hy * 0.35, 3), P["marble"], mat="blocks")
        for k in range(6):
            m.box((cx - hx + 9 + k * (2 * hx - 18) / 5, cy - 4 + hy * 0.35 + 0.2, z0 + 29), (1.4, 0.5, 1.6),
                  P["soul"], glow=True)
        m.roof((cx, cy - 4, z0 + 32), hx - 4.5, hy * 0.35 + 2.0, 3.5, P["bronze"], axis=0, mat="rivets", spec=0.6)
        # boltíves kapu
        gy = cy - 4 + hy + 0.3
        m.box((cx, gy, z0 + 6), (7, 0.6, 6), (34, 30, 28))
        m.add(_arch(cx, gy + 0.4, z0 + 12, 7.6, 3.2), P["marble"], tex=0.05)
        m.emblem((cx, gy + 1.3, z0 + 15.3), 2.0, 1.7, 1, "eagle", P["bronze"], spec=0.9)
        for k in range(4):  # kapuszárny-bordák
            m.box((cx - 5.25 + k * 3.5, gy + 0.5, z0 + 6), (0.3, 0.3, 6), P["bronze"], spec=0.8)
        m.box((cx, cy + H / 2 - 4, 0.6), (8, 3.5, 0.6), P["marble2"], mat="blocks")  # rámpa
        m.cylinder((cx + W * 0.3, cy - H * 0.28, z0 + 18), (cx + W * 0.3, cy - H * 0.28, 52), 3.2, P["iron"],
                   mat="rivets")
        m.cylinder((cx + W * 0.3, cy - H * 0.28, 52), (cx + W * 0.3, cy - H * 0.28, 54), 4.0, P["bronze"], spec=0.9)
        anim.append(("smoke", (cx + W * 0.3, cy - H * 0.28, 55), 0, (80, 80, 80)))
        _banner(m, cx - hx - 2, H - 8, 2, 28, P["iron"])
        if shape == "pen":
            m.cylinder((cx - W * 0.3, cy - H * 0.26, z0 + 18), (cx - W * 0.3, cy - H * 0.26, 46), 3.2, P["iron"],
                       mat="rivets")
            m.ellipsoid((cx + hx - 4, H - 7, 5), (5, 4.4, 4.4), (70, 60, 56), spec=0.4)  # olvasztótégely
            m.ellipsoid((cx + hx - 4, H - 7, 8.6), (3.6, 3.0, 1.2), P["ember"], glow=True)
            anim.append(("embers", (cx + hx - 4, H - 7, 9), 4, P["ember"]))
            anim.append(("smoke", (cx - W * 0.3, cy - H * 0.26, 47), 0, (80, 80, 80)))
        else:
            for k in range(2):  # félkész légiós a kapu előtt (váz)
                x = cx + 14 + k * 7
                m.capsule((x, H - 6, 1), (x, H - 6, 9), 1.4, P["iron"], spec=0.5)
                m.ellipsoid((x, H - 6, 10.5), (1.6, 1.6, 1.6), P["metal"], spec=0.6)
        height = 60
    elif shape == "tech":  # Ordan temploma
        m.box((cx, cy, 2.5), (W / 2 - 2, H / 2 - 2, 2.5), P["marble2"], mat="bricks")
        m.cylinder((cx, cy, 5), (cx, cy, 20), W * 0.32, P["marble"], mat="bricks")
        for k in range(8):
            a = k / 8 * 2 * math.pi
            _column(m, cx + math.cos(a) * W * 0.4, cy + math.sin(a) * H * 0.4, 5, 22, P)
        m.ellipsoid((cx, cy, 22), (W * 0.36, H * 0.36, 14), P["bronze"], spec=0.9, stripe=(0, 3))
        m.ellipsoid((cx, cy, 36), (3, 3, 3), P["soul"], glow=True)
        m.box((cx, cy + W * 0.32, 12), (4, 0.6, 6), P["soul"], glow=True)
        m.box((cx, cy - W * 0.3, 16), (5, 0.6, 2), (200, 200, 200), team=True)
        anim.append(("motes", (cx, cy, 38), 8, P["soul"]))
        height = 46
    elif shape == "aerie":  # Hangár
        m.box((cx, cy, 2), (W / 2 - 2, H / 2 - 2, 2), P["marble2"], mat="bricks")
        m.add(_half_cylinder(cx, cy, W / 2 - 4, H / 2 - 6, 20), P["metal"], spec=0.6, stripe=(0, 3))
        m.box((cx, cy + H / 2 - 6, 9), (10, 0.6, 8), (36, 32, 30))
        m.box((cx, cy + H / 2 - 5.6, 17.6), (10.4, 0.5, 0.8), P["bronze"], spec=0.9)
        m.box((cx, cy - H / 2 + 6, 14), (8, 0.6, 2.4), (200, 200, 200), team=True)
        m.capsule((cx + 20, cy + 20, 0), (cx + 20, cy + 20, 18), 0.8, P["iron"])  # jelzőárboc
        m.ellipsoid((cx + 20, cy + 20, 19), (1.4, 1.4, 1.4), P["soul"], glow=True)
        height = 40
    elif shape == "tower":  # Aquila-torony
        m.box((cx, cy, 4), (W / 2 - 5, H / 2 - 5, 4), P["marble2"], mat="bricks")
        _column(m, cx, cy, 8, 34, dict(P, marble=P["marble"]))
        m.box((cx, cy, 36), (7, 7, 2), P["bronze"], spec=0.9)
        m.ellipsoid((cx, cy, 41), (3.4, 2.4, 3), P["bronze"], spec=1.0)  # sas-szobor
        for sx in (-1, 1):
            m.ellipsoid((cx, cy + sx * 5, 42), (2.4, 4.4, 0.8), P["bronze"], spec=1.0)
        m.box((cx, cy + 6, 30), (2, 0.6, 3), (200, 200, 200), team=True)
        height = 48
    else:
        return imperial("house", w, h)
    m.height = height
    m.anim = anim
    return m


def _half_cylinder(cx, cy, hx, hy, r):
    """Félhenger (hangártető), tengelye az X mentén."""
    from .engine import SS
    step = 0.45 / SS
    xs = np.arange(-hx, hx + 0.01, step)
    angs = np.linspace(0, math.pi, max(12, int(math.pi * r / step)))
    X, A = np.meshgrid(xs, angs, indexing="ij")
    X, A = X.ravel(), A.ravel()
    ry = hy
    pts = np.stack([cx + X, cy + np.cos(A) * ry, np.sin(A) * r], 1)
    nrm = np.stack([np.zeros_like(X), np.cos(A) / ry, np.sin(A) / r], 1)
    nrm /= np.linalg.norm(nrm, axis=1, keepdims=True)
    return pts, nrm


def _clutter(m, style, w, h, shape):
    """Életszerű apróságok az épület elé: hordók, ládák, urnák, kosarak, gombák, tojások, fogaskerekek."""
    if w < 2 or shape in ("tower", "obelisk", "node"):
        return
    W, H = w * T, h * T
    spots = [(5, H - 4), (9, H - 3), (W - 6, H - 4)] if w >= 3 else [(4, H - 3), (W - 4, H - 3)]
    for i, (x, y) in enumerate(spots):
        k = (i + w + h) % 3
        if style == "nordic":
            if k == 0:  # hordó vasabroncsokkal
                m.cylinder((x, y, 0), (x, y, 5), 2.2, (120, 82, 46), stripe=(2, 1.2))
                m.cylinder((x, y, 1.2), (x, y, 1.7), 2.35, (90, 90, 96), spec=0.7)
                m.cylinder((x, y, 3.6), (x, y, 4.1), 2.35, (90, 90, 96), spec=0.7)
            elif k == 1:  # láda
                m.box((x, y, 2), (2.4, 2.0, 2), (150, 110, 64), mat="planks")
            else:  # zsákok
                m.ellipsoid((x, y, 1.6), (2.2, 1.8, 1.6), (196, 180, 140), tex=0.15)
                m.ellipsoid((x + 1.6, y - 0.6, 3.2), (1.6, 1.4, 1.2), (186, 170, 130), tex=0.15)
        elif style == "aztec":
            if k == 0:  # festett urna
                m.capsule((x, y, 0.5), (x, y, 4.4), 2.0, (196, 120, 70), r1=1.2)
                m.cylinder((x, y, 2.4), (x, y, 3.0), 2.05, (40, 160, 130))
            elif k == 1:  # kőszobor-talapzat jádegömbbel
                m.box((x, y, 1.5), (1.6, 1.6, 1.5), (176, 160, 120), mat="bricks")
                m.ellipsoid((x, y, 4), (1.2,) * 3, (40, 196, 156), spec=0.6)
            else:  # kukoricakosár
                m.cylinder((x, y, 0), (x, y, 2.6), 2.0, (170, 130, 70), stripe=(2, 0.7))
                m.ellipsoid((x, y, 2.9), (1.6, 1.6, 0.8), (240, 200, 60))
        elif style == "tribal":
            if k == 0:  # szárítóállvány bőrrel
                m.capsule((x - 2, y, 0), (x - 2, y, 7), 0.4, (110, 76, 44))
                m.capsule((x + 2, y, 0), (x + 2, y, 7), 0.4, (110, 76, 44))
                m.box((x, y, 4.6), (2.2, 0.3, 2.2), (214, 190, 150))
            elif k == 1:  # fonott kosár
                m.cylinder((x, y, 0), (x, y, 2.6), 2.0, (176, 136, 76), stripe=(2, 0.6))
            else:  # fahasábok
                for j in range(3):
                    m.cylinder((x - 3, y - j * 1.4, 0.9 + (j % 2)), (x + 3, y - j * 1.4, 0.9 + (j % 2)), 0.9, (120, 84, 50))
        elif style == "fungal":
            for j in range(3):
                xx, yy = x + (j - 1) * 2.2, y - (j % 2) * 1.4
                m.capsule((xx, yy, 0), (xx, yy, 2 + j), 0.6, (226, 214, 186))
                m.ellipsoid((xx, yy, 2.2 + j), (1.4, 1.4, 0.7), [(178, 74, 60), (90, 160, 230), (220, 200, 120)][(j + k) % 3],
                            glow=(j + k) % 3 == 1)
        elif style == "imperial":
            if k == 0:  # fogaskerék a földön
                m.cylinder((x, y, 0), (x, y, 0.8), 2.6, (150, 150, 150), spec=0.8)
                m.cylinder((x, y, 0.8), (x, y, 1.0), 1.0, (90, 90, 96))
            elif k == 1:  # ládák egymáson
                m.box((x, y, 1.8), (2.0, 1.8, 1.8), (130, 100, 70), mat="planks")
                m.box((x + 0.6, y - 0.4, 4.6), (1.5, 1.4, 1.0), (130, 100, 70), mat="planks")
            else:  # lélekfény-kanna
                m.cylinder((x, y, 0), (x, y, 4), 1.4, (90, 90, 96), spec=0.6)
                m.ellipsoid((x, y, 4.4), (1.0, 1.0, 0.6), (120, 255, 190), glow=True)
        else:  # organic: tojások és csontok
            if k == 2:
                m.capsule((x - 2, y, 0.6), (x + 2, y - 1, 0.6), 0.6, (226, 214, 186))
            else:
                for j in range(3):
                    m.ellipsoid((x + (j - 1) * 1.8, y - (j % 2), 1.4), (1.1, 1.0, 1.5), (220, 200, 160), spec=0.3)


def build_building(defn, style):
    w, h = defn["size"]
    shape = defn.get("shape", "barracks")
    fn = {"aztec": aztec, "nordic": nordic, "organic": organic, "tribal": tribal, "fungal": fungal,
          "imperial": imperial}.get(style, nordic)
    _CUR["emblem"] = EMBLEM.get(style, "paw")
    if shape in ("altar", "farm"):
        m = special(shape, style, w, h)
    else:
        m = fn(shape, w, h)
    _clutter(m, style, w, h, shape)
    return m


def shade(c, f):
    return tuple(max(0, min(255, int(v * f))) for v in c)


def special(shape, style, w, h):
    """Hősidéző oltárok (fajonként saját anyagból) és az ursan tanya."""
    P = {"aztec": AZ, "nordic": NO, "organic": OR, "tribal": TR, "fungal": FU, "imperial": IM}[style]
    m = Model(shape)
    cx, cy = w * T / 2, h * T / 2
    if shape == "farm":  # ursan tanya: árpaföld kerítéssel, kis jurta, szénakazal
        W, H = w * T, h * T
        fx0, fx1, fy0, fy1 = W * 0.38, W - 3, H * 0.3, H - 3
        m.box(((fx0 + fx1) / 2, (fy0 + fy1) / 2, 0.5), ((fx1 - fx0) / 2, (fy1 - fy0) / 2, 0.5), (104, 74, 46),
              stripe=(1, 3.0), tex=0.2)
        rng = np.random.default_rng(7)
        for k in range(int((fy1 - fy0) / 4.2)):  # árpasorok
            y = fy0 + 2.5 + k * 4.2
            for x in np.arange(fx0 + 2.5, fx1 - 1, 3.2):
                hgt = 4.5 + rng.uniform(0, 1.6)
                m.capsule((x, y, 0.8), (x + rng.uniform(-0.6, 0.6), y, hgt), 0.9, (214, 184, 84), r1=0.5, tex=0.25)
                m.ellipsoid((x, y, hgt), (0.9, 0.7, 1.3), (232, 204, 104), tex=0.2)
        for t in np.linspace(0, 1, 9):  # kerítés
            for (x0, y0, x1, y1) in ((fx0, fy0, fx1, fy0), (fx0, fy0, fx0, fy1)):
                x, y = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
                m.capsule((x, y, 0), (x, y, 6), 0.7, P["wood2"], r1=0.45)
        m.capsule((fx0, fy0, 4.5), (fx1, fy0, 4.5), 0.45, P["wood"])
        m.capsule((fx0, fy0, 4.5), (fx0, fy1, 4.5), 0.45, P["wood"])
        _yurt(m, W * 0.2, H * 0.28, W * 0.15, 4, 8, P)
        m.ellipsoid((W * 0.17, H * 0.8, 5), (6, 5, 6), (200, 168, 80), tex=0.35)  # szénakazal
        m.capsule((W * 0.17, H * 0.8, 9), (W * 0.17, H * 0.8, 13), 0.5, P["wood2"])
        _banner(m, 4, H * 0.55, 0, 22, P["wood2"])
        m.height = 24
        m.anim = [("smoke", (W * 0.2, H * 0.28, 16), 0, (120, 115, 110))]
        return m
    base = P.get("stone", P.get("sand", P.get("marble", (150, 140, 130))))
    glow = {"aztec": (255, 210, 90), "nordic": (150, 220, 255), "organic": (150, 255, 120), "tribal": (200, 230, 255),
            "fungal": (150, 255, 210), "imperial": (120, 255, 190)}[style]
    for k in range(3):  # lépcsős talapzat
        r = (w * T / 2 - 2) - k * 6
        m.box((cx, cy, 1.5 + k * 3), (r, r, 1.5), shade(base, 1 - k * 0.08), mat="bricks")
    for k in range(4):  # oszlopok / totemek / tüskék a sarkokban
        a = math.pi / 4 + k * math.pi / 2
        x, y = cx + math.cos(a) * (w * T / 2 - 9), cy + math.sin(a) * (h * T / 2 - 9)
        if style == "organic":
            m.capsule((x, y, 0), (x * 0.95 + cx * 0.05, y * 0.95 + cy * 0.05, 26), 2.6, OR["shell"], r1=0.4, spec=0.4)
        elif style == "fungal":
            m.capsule((x, y, 0), (x, y, 16), 2.2, FU["stalk"])
            m.ellipsoid((x, y, 17), (5, 5, 2.4), FU["cap"], spec=0.3)
        else:
            m.cylinder((x, y, 0), (x, y, 22), 2.4, P.get("wood", base) if style in ("nordic", "tribal") else base,
                       spec=0.3)
            m.ellipsoid((x, y, 23), (2.6, 2.6, 2.0), glow, glow=True)
    m.cylinder((cx, cy, 10), (cx, cy, 15), 6, shade(base, 0.8), spec=0.3)  # oltárkő
    m.ellipsoid((cx, cy, 20), (4.5, 4.5, 4.5), glow, glow=True)          # a hős-láng
    _banner(m, cx - 14, cy + h * T / 2 - 5, 0, 26, P.get("wood2", (90, 70, 50)))
    m.height = 28
    m.anim = [("motes", (cx, cy, 22), 4, glow)]
    return m


def anim_extra(spec_list, frame, n, rng_seed=0):
    """Animált részletek (füst, szikra, lebegő fényrészecskék) egy képkockára."""
    pts, nrms, cols, glows = [], [], [], []
    for kind, (x, y, z), cnt, col in spec_list:
        if kind == "smoke":
            for k in range(4):
                ph = (frame / n + k / 4) % 1.0
                r = 1.5 + ph * 3
                c = (x + ph * 6 + k * 0.5, y - ph * 3, z + ph * 18)
                p, nn = ellipsoid(c, (r, r, r))
                pts.append(p)
                nrms.append(nn)
                cols.append(np.repeat(np.asarray(col, float)[None] * (1.1 - ph * 0.4), len(p), 0))
                glows.append(np.zeros(len(p), bool))
        elif kind in ("motes", "embers", "bubbles"):
            for k in range(cnt):
                base_a = k / cnt * 2 * math.pi
                a = base_a + frame / n * 2 * math.pi * (0.5 if kind == "motes" else 0.2)
                rad = 6 + (k % 3) * 3 if kind == "motes" else 4
                zz = z + (math.sin(a * 2 + k) * 3 if kind == "motes" else ((frame / n + k / cnt) % 1.0) * 10)
                c = (x + math.cos(a) * rad, y + math.sin(a) * rad * 0.8, zz)
                rr = 1.1 if kind != "bubbles" else 1.4
                p, nn = ellipsoid(c, (rr, rr, rr))
                pts.append(p)
                nrms.append(nn)
                cols.append(np.repeat(np.asarray(col, float)[None], len(p), 0))
                glows.append(np.ones(len(p), bool))
        elif kind == "pustules":
            for k in range(cnt):
                a = k / cnt * 2 * math.pi
                pulse = 1.6 + 0.9 * math.sin(frame / n * 2 * math.pi + k)
                c = (x + math.cos(a) * 14, y + math.sin(a) * 12, z - 6 + (k % 2) * 8)
                p, nn = ellipsoid(c, (pulse, pulse, pulse))
                pts.append(p)
                nrms.append(nn)
                cols.append(np.repeat(np.asarray(col, float)[None], len(p), 0))
                glows.append(np.ones(len(p), bool))
    if not pts:
        return None
    return np.concatenate(pts), np.concatenate(nrms), np.concatenate(cols), np.concatenate(glows)


def scaffold(w, h, height, frac, wood=(140, 100, 60)):
    """Építési állvány oszlopai és gerendái."""
    W, H = w * T, h * T
    pts, nrms = [], []
    top = max(8, height * min(1.0, frac + 0.15))
    for x in (3, W - 3):
        for y in (3, H - 3):
            p, nn = capsule((x, y, 0), (x, y, top), 1.0)
            pts.append(p)
            nrms.append(nn)
    for z in np.arange(8, top, 10):
        for (x0, y0, x1, y1) in ((3, 3, W - 3, 3), (3, H - 3, W - 3, H - 3), (3, 3, 3, H - 3), (W - 3, 3, W - 3, H - 3)):
            p, nn = capsule((x0, y0, z), (x1, y1, z), 0.7)
            pts.append(p)
            nrms.append(nn)
    P = np.concatenate(pts)
    return P, np.concatenate(nrms), np.repeat(np.asarray(wood, float)[None], len(P), 0), np.zeros(len(P), bool)
