"""Mini "előrenderelő" motor pixel-art sprite-okhoz (numpy + Pillow) – 2. változat.

A StarCraft 1 / Warcraft 2 sprite-jai előre renderelt 3D modellekből készültek;
ez a modul ugyanezt teszi: primitívekből (ellipszoid, kapszula, doboz, tető)
álló, csontokra fűzött modelleket mintavételez pontfelhővé, beállítja a pózt,
elforgatja a 8 irány valamelyikébe, ferde vetítéssel z-pufferrel rendereli.

A részletességhez:
* SS-szeres szuperszampling, majd élsimítás nélküli lekicsinyítés (többségi
  alfa + színátlag) – így a vékony részletek (fegyverek, tollak) is megmaradnak,
* képtér-alapú környezeti árnyékolás (mélyedések, takarások sötétítése),
* Lambert + Blinn-csillanás (fém, kitin), perem-fény, anyagtextúrák
  (zaj, sávok, tégla, deszka),
* sötét, színezett körvonal és vetett árnyék; a végén sprite-onkénti paletta.

Koordináták: X kelet, Y dél (a térkép síkja), Z fel. 1 egység = 1 képpont (SS előtt).
A modell nyugalmi helyzetben +X (kelet) felé néz.
"""
import math
import os

import numpy as np
from PIL import Image

# v4 (festett stílus, a kézzel festett csomaghoz igazítva): alacsonyabb, 3/4-es kamera –
# képernyő_y = CAM_G*Y - K_OBL*Z: a talaj rövidül, a figurák elölnézete nagyobb.
K_OBL = 0.92
CAM_G = 0.72
LIGHT = np.array([-0.62, -0.5, 0.6])
LIGHT = LIGHT / np.linalg.norm(LIGHT)
VIEW = np.array([0.0, K_OBL, CAM_G])
VIEW = VIEW / np.linalg.norm(VIEW)
HALF = (LIGHT + VIEW) / np.linalg.norm(LIGHT + VIEW)
DIRS = ["E", "SE", "S", "SW", "W", "NW", "N", "NE"]
SHADOW = (0, 0, 0, 100)
SS = 1     # szuperszampling-szorzó a render()-en belül
# HD edition: TOTU_HD=2 -> kétszeres kimeneti felbontás (assets/sprites_hd); a render ennek is a duplája
OUT_SCALE = max(1, int(os.environ.get("TOTU_HD", "2") or 2))   # a játék grafikája HD-ben készül
RES = 2 * OUT_SCALE    # v4: ennyiszeres felbontásban renderelünk, majd festett lekicsinyítés (render_res)
PAINTED = True
OUTLINE = (30, 20, 14)


def _hash(p):
    """Determinisztikus 'textúrazaj' a nyugalmi pozícióból."""
    h = np.sin(p[:, 0] * 12.9898 + p[:, 1] * 78.233 + p[:, 2] * 37.719) * 43758.5453
    return h - np.floor(h)


def _dens():
    return (SS * RES) ** 2


def _q():
    return SS * RES


# ---------------------------------------------------------------------------
# primitívek -> (pontok, normálok)
def _fib_sphere(n):
    i = np.arange(n) + 0.5
    phi = np.arccos(1 - 2 * i / n)
    th = math.pi * (1 + 5 ** 0.5) * i
    return np.stack([np.cos(th) * np.sin(phi), np.sin(th) * np.sin(phi), np.cos(phi)], 1)


def ellipsoid(c, r):
    r = np.maximum(np.asarray(r, float), 0.05)
    area = 4 * math.pi * ((r[0] * r[1]) ** 1.6 + (r[0] * r[2]) ** 1.6 + (r[1] * r[2]) ** 1.6) ** (1 / 1.6) / 3 * 3
    n = int(max(40, area * 2.5 * _dens()))
    d = _fib_sphere(n)
    pts = np.asarray(c, float) + d * r
    nrm = d / (r * r)
    nrm /= np.linalg.norm(nrm, axis=1, keepdims=True)
    return pts, nrm


def capsule(p0, p1, r0, r1=None, ends=True):
    r1 = r0 if r1 is None else r1
    p0, p1 = np.asarray(p0, float), np.asarray(p1, float)
    ax = p1 - p0
    L = np.linalg.norm(ax) + 1e-9
    a = ax / L
    tmp = np.array([0, 0, 1.0]) if abs(a[2]) < 0.9 else np.array([1.0, 0, 0])
    u = np.cross(a, tmp)
    u /= np.linalg.norm(u)
    v = np.cross(a, u)
    rm = max(r0, r1, 0.3)
    nl = max(2, int(L * 1.9 * _q()))
    nc = max(8, int(2 * math.pi * rm * 1.9 * _q()))
    t = np.linspace(0, 1, nl)
    ang = np.linspace(0, 2 * math.pi, nc, endpoint=False)
    T, A = np.meshgrid(t, ang, indexing="ij")
    T, A = T.ravel(), A.ravel()
    R = r0 + (r1 - r0) * T
    dirs = np.outer(np.cos(A), u) + np.outer(np.sin(A), v)
    pts = p0 + np.outer(T * L, a) + dirs * R[:, None]
    slope = (r0 - r1) / L
    nrm = dirs + a * slope
    nrm /= np.linalg.norm(nrm, axis=1, keepdims=True)
    out_p, out_n = [pts], [nrm]
    for c, r in (((p0, r0), (p1, r1)) if ends else ()):
        if r > 0.35:
            sp, sn = ellipsoid(c, (r, r, r))
            out_p.append(sp)
            out_n.append(sn)
    return np.concatenate(out_p), np.concatenate(out_n)


def box(c, half):
    c, h = np.asarray(c, float), np.asarray(half, float)
    step = 0.6 / _q()
    ps, ns = [], []
    for axis in range(3):
        o = [i for i in range(3) if i != axis]
        g0 = np.arange(-h[o[0]], h[o[0]] + 0.01, step)
        g1 = np.arange(-h[o[1]], h[o[1]] + 0.01, step)
        G0, G1 = np.meshgrid(g0, g1, indexing="ij")
        for sgn in (-1, 1):
            p = np.zeros((G0.size, 3))
            p[:, o[0]] = G0.ravel()
            p[:, o[1]] = G1.ravel()
            p[:, axis] = sgn * h[axis]
            n = np.zeros_like(p)
            n[:, axis] = sgn
            ps.append(p + c)
            ns.append(n)
    return np.concatenate(ps), np.concatenate(ns)


def roof(c, half_x, half_y, height, ridge_axis=0):
    """Nyeregtető: két ferde sík (gerinc az X vagy Y tengely mentén) + oromzatok."""
    c = np.asarray(c, float)
    step = 0.6 / _q()
    ps, ns = [], []
    along, across = (half_x, half_y) if ridge_axis == 0 else (half_y, half_x)
    g_a = np.arange(-along, along + 0.01, step)
    g_c = np.arange(0, across + 0.01, step)
    A, C = np.meshgrid(g_a, g_c, indexing="ij")
    A, C = A.ravel(), C.ravel()
    Z = height * (1 - C / across)
    slope = height / across
    for sgn in (-1, 1):
        p = np.zeros((A.size, 3))
        n = np.zeros((A.size, 3))
        if ridge_axis == 0:
            p[:, 0], p[:, 1] = A, sgn * C
            n[:, 1], n[:, 2] = sgn * slope, 1
        else:
            p[:, 1], p[:, 0] = A, sgn * C
            n[:, 0], n[:, 2] = sgn * slope, 1
        p[:, 2] = Z
        ps.append(p + c)
        ns.append(n / np.linalg.norm(n, axis=1, keepdims=True))
    for sgn in (-1, 1):
        gz = np.arange(0, height + 0.01, step)
        gc = np.arange(-across, across + 0.01, step)
        CC, ZZ = np.meshgrid(gc, gz, indexing="ij")
        m = ZZ <= height * (1 - np.abs(CC) / across)
        CC, ZZ = CC[m], ZZ[m]
        p = np.zeros((CC.size, 3))
        n = np.zeros((CC.size, 3))
        if ridge_axis == 0:
            p[:, 0], p[:, 1] = sgn * along, CC
            n[:, 0] = sgn
        else:
            p[:, 1], p[:, 0] = sgn * along, CC
            n[:, 1] = sgn
        p[:, 2] = ZZ
        ps.append(p + c)
        ns.append(n)
    return np.concatenate(ps), np.concatenate(ns)


def cone(base, tip, r):
    return capsule(base, tip, r, 0.0)


def cylinder(c0, c1, r):
    """Zárt henger: palást + lapos véglapok (gömbvégek nélkül)."""
    p, n = capsule(c0, c1, r, r, ends=False)
    c0, c1 = np.asarray(c0, float), np.asarray(c1, float)
    ax = (c1 - c0) / (np.linalg.norm(c1 - c0) + 1e-9)
    caps_p, caps_n = [], []
    rr = np.arange(0, r + 0.01, 0.55 / _q())
    for cc, sgn in ((c0, -1), (c1, 1)):
        tmp = np.array([0, 0, 1.0]) if abs(ax[2]) < 0.9 else np.array([1.0, 0, 0])
        u = np.cross(ax, tmp)
        u /= np.linalg.norm(u)
        v = np.cross(ax, u)
        pts = []
        for rad in rr:
            k = max(6, int(2 * math.pi * rad * 2.2 * _q()))
            a = np.linspace(0, 2 * math.pi, k, endpoint=False)
            pts.append(cc + np.outer(np.cos(a) * rad, u) + np.outer(np.sin(a) * rad, v))
        pts = np.concatenate(pts)
        caps_p.append(pts)
        caps_n.append(np.repeat((ax * sgn)[None], len(pts), 0))
    return np.concatenate([p] + caps_p), np.concatenate([n] + caps_n)


# ---------------------------------------------------------------------------
class Model:
    """Csontokra fűzött primitívek."""

    def __init__(self, name=""):
        self.name = name
        self.bones = {"root": (None, np.zeros(3))}
        self.order = ["root"]
        self.parts = []

    def bone(self, name, parent, pivot):
        self.bones[name] = (parent, np.asarray(pivot, float))
        self.order.append(name)
        return name

    def add(self, geom, color, bone="root", team=False, glow=False, tex=0.07, stripe=None, spec=0.0, mat=None):
        """stripe=(tengely, periódus): sávos textúra; mat: 'bricks' | 'planks' | 'fur' | 'scales' | 'rivets';
        spec: fémes/kitines csillanás erőssége."""
        pts, nrm = geom
        self.parts.append((pts, nrm, np.asarray(color, float), bone, team, glow, tex, stripe, spec, mat))

    def emblem(self, c, hu, hv, axis, kind, color, bone="root", sign=1, **kw):
        """Lapos címer-matrica (pl. mancs a pajzson): a sík normálisa az 'axis' tengely (0=X, 1=Y);
        a minta az emblem_mask(kind) szerint."""
        q = _q()
        u = np.arange(-hu, hu + 0.01, 0.5 / q)
        v = np.arange(-hv, hv + 0.01, 0.5 / q)
        U, V = np.meshgrid(u, v, indexing="ij")
        m = emblem_mask(kind, U / max(hu, 1e-6), V / max(hv, 1e-6))
        U, V = U[m], V[m]
        if not len(U):
            return
        p = np.zeros((len(U), 3))
        n = np.zeros((len(U), 3))
        o = 1 if axis == 0 else 0
        p[:, o], p[:, 2], p[:, axis] = U, V, 0
        n[:, axis] = sign
        self.add((p + np.asarray(c, float), n), color, bone, **kw)

    def ellipsoid(self, c, r, color, bone="root", **kw):
        self.add(ellipsoid(c, r), color, bone, **kw)

    def capsule(self, p0, p1, r0, color, bone="root", r1=None, ends=True, **kw):
        self.add(capsule(p0, p1, r0, r1, ends), color, bone, **kw)

    def cylinder(self, p0, p1, r, color, bone="root", **kw):
        self.add(cylinder(p0, p1, r), color, bone, **kw)

    def box(self, c, half, color, bone="root", **kw):
        self.add(box(c, half), color, bone, **kw)

    def roof(self, c, hx, hy, h, color, bone="root", axis=0, ridge=True, **kw):
        """Nyeregtető; anyag nélkül zsindelyes, gerincléccel."""
        kw.setdefault("mat", "shingles")
        if kw["mat"] in ("shingles", "tiles", "thatch"):
            kw.pop("stripe", None)
        self.add(roof(c, hx, hy, h, axis), color, bone, **kw)
        if ridge:
            c = np.asarray(c, float)
            along = hx if axis == 0 else hy
            d = np.array([1.0, 0, 0]) if axis == 0 else np.array([0, 1.0, 0])
            top = c + np.array([0, 0, h + 0.3])
            dark = tuple(np.asarray(color, float) * 0.62)
            self.add(capsule(top - d * (along + 0.4), top + d * (along + 0.4), 0.9), dark, bone, tex=0.1)

    def compile(self):
        P, N, C, B, T, G, X, SV, SP = [], [], [], [], [], [], [], [], []
        bidx = {b: i for i, b in enumerate(self.order)}
        for pts, nrm, col, bone, team, glow, tex, stripe, spec, mat in self.parts:
            n = len(pts)
            sv = np.ones(n)
            if stripe:
                ax, per = stripe
                sv *= np.where((np.floor(pts[:, ax] / per) % 2) == 0, 1.0, 0.84)
            disp = None   # felületi domborítás a normális mentén (kövek, zsindely, rönk, bunda…)
            x, y, z = pts[:, 0], pts[:, 1], pts[:, 2]
            anx, any_ = np.abs(nrm[:, 0]), np.abs(nrm[:, 1])
            uu = (x * any_ + y * anx) / (anx + any_ + 1e-6)  # vízszintes érintő menti koordináta
            if mat in ("bricks", "stones"):  # rakott, szabálytalan kövek kiálló felülettel, mélyülő fugákkal
                rh = 2.8
                row = np.floor(z / rh)
                fz = z / rh - row
                u = (uu + row * 1.9) / 3.6
                col_ = np.floor(u)
                fu = u - col_
                hs = _hash(np.stack([row, col_, row * 0.37 + col_ * 0.11], 1))
                edge = (fz < 0.14) | (fu < 0.1) | (fz > 0.95)
                disp = np.where(edge, -0.35, 0.12 + 0.45 * hs)
                sv *= np.where(edge, 0.6, 0.84 + 0.28 * hs)
            elif mat == "blocks":  # faragott kőtömbök (templomok, római falak)
                rh = 3.0
                row = np.floor(z / rh)
                fz = z / rh - row
                u = (uu + row * 2.5) / 5.0
                col_ = np.floor(u)
                fu = u - col_
                hs = _hash(np.stack([row, col_, row * 0.5], 1))
                edge = (fz < 0.1) | (fu < 0.06)
                bevel = (fz < 0.22) | (fu < 0.13) | (fz > 0.9)
                disp = np.where(edge, -0.3, 0.18 + 0.1 * hs)
                sv *= np.where(edge, 0.66, np.where(bevel, 0.9, 1.0) * (0.92 + 0.14 * hs))
            elif mat == "shingles":  # fazsindely: minden sor kilóg az alatta lévő fölé
                rh = 1.7
                row = np.floor(z / rh)
                fz = z / rh - row
                u = (uu + row * 1.3) / 2.4
                col_ = np.floor(u)
                fu = u - col_
                hs = _hash(np.stack([row, col_, 3.0 + row], 1))
                disp = (1 - fz) * 0.6 - 0.1 + 0.08 * hs
                sv *= (0.7 + 0.36 * fz) * (0.88 + 0.22 * hs) * np.where(fu < 0.09, 0.7, 1.0)
            elif mat == "tiles":  # római félköríves cserép
                rh = 2.0
                row = np.floor(z / rh)
                fz = z / rh - row
                fu = (uu / 2.2) % 1.0
                hs = _hash(np.stack([row, np.floor(uu / 2.2), 5.0 + row], 1))
                arc = np.sin(np.pi * fu)
                disp = 0.5 * arc + (1 - fz) * 0.25
                sv *= (0.68 + 0.4 * arc) * (0.84 + 0.2 * fz) * (0.92 + 0.12 * hs)
            elif mat == "thatch":  # nád / szalma / gyep: szálas, rétegzett
                rh = 1.4
                row = np.floor(z / rh)
                fz = z / rh - row
                st = _hash(np.stack([np.floor(uu * 1.6), row, 9.0 + row], 1))
                disp = (1 - fz) * 0.45 + 0.3 * st
                sv *= (0.72 + 0.32 * st) * (0.8 + 0.25 * fz)
            elif mat == "logs":  # egymásra rakott, kerek rönkök kéreggel
                rh = 2.6
                row = np.floor(z / rh)
                fz = z / rh - row
                bark = _hash(np.stack([row, np.floor(uu * 1.3), 2.0 + row], 1))
                arc = np.sin(np.pi * fz)
                disp = 0.75 * arc - 0.25
                sv *= (0.58 + 0.48 * arc) * (0.86 + 0.22 * bark)
            elif mat == "planks":  # deszkák réssel és erezettel
                col_ = np.floor(uu / 2.4)
                fu = uu / 2.4 - col_
                hs = _hash(np.stack([col_, np.floor(z / 9.0), 4.0 + col_], 1))
                gap = fu < 0.1
                disp = np.where(gap, -0.3, 0.12 * hs)
                sv *= np.where(gap, 0.6, (0.86 + 0.22 * hs) * (0.95 + 0.07 * np.sin(z * 2.3 + col_ * 1.7)))
            elif mat == "fur":  # bozontos bunda: szőrcsomók a körvonalon is
                hs = _hash(np.round(pts * 1.4))
                fine = _hash(np.round(pts * np.array([3.1, 3.1, 1.2])) + 7)  # függőleges szőrtincsek
                disp = 0.12 + 0.6 * fine ** 2
                sv *= 0.86 + 0.18 * hs + 0.22 * fine ** 2  # a kiálló tincsvégek világosabbak
            elif mat == "chitin":  # croftian páncélszelvények: fényes, kiálló lemezek
                seg = z / 2.2 + x * 0.12
                f = seg - np.floor(seg)
                disp = 0.45 * (1 - f) - 0.1
                sv *= 0.66 + 0.46 * (1 - f)
            elif mat == "cloth":  # ruharedők
                fold = np.sin(uu * 1.3 + z * 0.25)
                disp = 0.28 * fold
                sv *= 0.86 + 0.14 * fold
            elif mat == "scales":
                sv *= np.where(((x * 0.9 + np.floor(z * 0.8) * 0.5) % 1.0) < 0.22, 0.78, 1.0)
                disp = np.where(((x * 0.9 + np.floor(z * 0.8) * 0.5) % 1.0) < 0.22, -0.15, 0.12)
            elif mat == "rivets":  # fémlemezek: hornyok és szegecsek
                u = uu / 5.0
                v = z / 4.0
                fu, fz = u - np.floor(u), v - np.floor(v)
                seam = (fu < 0.06) | (fz < 0.06)
                rivet = (fu < 0.2) & (fz < 0.2) & ~seam
                hs = _hash(np.stack([np.floor(u), np.floor(v), 1.0 + np.floor(u)], 1))
                disp = np.where(seam, -0.25, np.where(rivet, 0.35, 0.04 * hs))
                sv *= np.where(seam, 0.64, np.where(rivet, 1.32, 0.92 + 0.14 * hs))
            if mat in ("bricks", "stones", "blocks", "planks", "rivets"):
                # vízszintes felületek (padló, talapzat teteje): kőlapok / padlódeszka / lemezek x–y rácson
                top = np.abs(nrm[:, 2]) > 0.7
                if top.any():
                    tx, ty = x[top], y[top]
                    if mat == "planks":
                        r_ = np.floor(ty / 2.4)
                        f1 = ty / 2.4 - r_
                        f2 = ((tx + r_ * 7.3) / 11.0) % 1.0
                        hs = _hash(np.stack([r_, np.floor((tx + r_ * 7.3) / 11.0), r_ + 2], 1))
                        e = (f1 < 0.1) | (f2 < 0.03)
                    else:
                        sz = 3.4 if mat != "blocks" else 5.0
                        r_ = np.floor(ty / sz)
                        f1 = ty / sz - r_
                        u2 = (tx + r_ * sz * 0.53) / (sz * 1.3)
                        f2 = u2 - np.floor(u2)
                        hs = _hash(np.stack([r_, np.floor(u2), r_ * 0.3 + 1], 1))
                        e = (f1 < 0.09) | (f2 < 0.07)
                    if disp is None:
                        disp = np.zeros(n)
                    # a függőleges mintához számolt szorzót a vízszintes lapon kicseréljük
                    sv[top] = np.where(e, 0.66, 0.86 + 0.24 * hs)
                    disp[top] = np.where(e, -0.25, 0.1 * hs)
            if disp is not None:
                pts = pts + nrm * disp[:, None]
            SV.append(sv)
            P.append(pts)
            N.append(nrm)
            C.append(np.repeat(col[None, :], n, 0))
            B.append(np.full(n, bidx[bone]))
            T.append(np.full(n, team))
            G.append(np.full(n, glow))
            X.append(np.full(n, tex))
            SP.append(np.full(n, spec))
        self.P = np.concatenate(P)
        self.N = np.concatenate(N)
        self.C = np.concatenate(C)
        self.B = np.concatenate(B)
        self.T = np.concatenate(T)
        self.G = np.concatenate(G)
        self.X = np.concatenate(X)
        self.SV = np.concatenate(SV)
        self.SP = np.concatenate(SP)
        self.noise = _hash(self.P)
        return self


def emblem_mask(kind, u, v):
    """Fajcímerek [-1,1]² koordinátákban (v felfelé)."""
    r = np.hypot(u, v)
    if kind == "paw":  # ursan medvemancs
        pad = ((u / 0.42) ** 2 + ((v + 0.28) / 0.32) ** 2) <= 1
        toes = np.zeros_like(pad)
        for tx, ty in ((-0.5, 0.2), (-0.18, 0.46), (0.18, 0.46), (0.5, 0.2)):
            toes |= np.hypot(u - tx, v - ty) <= 0.17
        return pad | toes
    if kind == "sun":  # szürke elfek: nap
        a = np.arctan2(v, u)
        rays = (r > 0.42) & (r < 0.82) & (np.cos(a * 8) > 0.55)
        return (r <= 0.34) | rays
    if kind == "eye":  # croftian: Kórus-szem
        almond = np.abs(v) <= 0.42 * np.clip(1 - u * u, 0, 1)
        return almond & ~((r < 0.24) & (r > 0.1)) & (np.abs(u) < 0.9)
    if kind == "moon":  # wullof: holdsarló
        return (r <= 0.68) & (np.hypot(u - 0.3, v - 0.16) > 0.58)
    if kind == "spore":  # gombanép: spóra
        out = r <= 0.24
        for k in range(6):
            a = k * math.pi / 3
            out |= np.hypot(u - 0.58 * math.cos(a), v - 0.58 * math.sin(a)) <= 0.14
        return out
    if kind == "eagle":  # Vasbirodalom: sas
        wing = (np.abs(v - (0.45 * np.abs(u) - 0.05)) < 0.14) & (np.abs(u) < 0.85)
        return wing | (np.hypot(u, v - 0.3) < 0.16) | ((np.abs(u) < 0.1) & (v < 0.1) & (v > -0.6))
    return r <= 0.5


def _rot(rx, ry, rz):
    rx, ry, rz = (math.radians(a) for a in (rx, ry, rz))
    cx, sx, cy, sy, cz, sz = math.cos(rx), math.sin(rx), math.cos(ry), math.sin(ry), math.cos(rz), math.sin(rz)
    Rx = np.array([[1, 0, 0], [0, cx, -sx], [0, sx, cx]])
    Ry = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]])
    Rz = np.array([[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]])
    return Rz @ Ry @ Rx


def _mat(R, t):
    M = np.eye(4)
    M[:3, :3] = R
    M[:3, 3] = t
    return M


def pose_points(model, pose):
    """pose: {csont: (rx, ry, rz)} fokban; '_offset': (x,y,z); '_scale': (sx,sy,sz);
    '_hide': {csontnevek}; '_zcut': magasság-vágás (építési fázisok)."""
    mats = {}
    for name in model.order:
        parent, pivot = model.bones[name]
        r = pose.get(name, (0, 0, 0))
        local = _mat(np.eye(3), pivot) @ _mat(_rot(*r), np.zeros(3)) @ _mat(np.eye(3), -pivot)
        if parent is None:
            off = np.asarray(pose.get("_offset", (0, 0, 0)), float)
            mats[name] = _mat(np.eye(3), off) @ local
        else:
            mats[name] = mats[parent] @ local
    P = np.empty_like(model.P)
    N = np.empty_like(model.N)
    keep = np.ones(len(P), bool)
    hide = pose.get("_hide", ())
    for i, name in enumerate(model.order):
        m = model.B == i
        if not m.any():
            continue
        if name in hide:
            keep &= ~m
        M = mats[name]
        P[m] = model.P[m] @ M[:3, :3].T + M[:3, 3]
        N[m] = model.N[m] @ M[:3, :3].T
    sc = pose.get("_scale")
    if sc is not None:
        P = P * np.asarray(sc, float)
    zcut = pose.get("_zcut")
    if zcut is not None:
        keep &= P[:, 2] <= zcut
    return P, N, keep


def _shift(a, dy, dx, fill):
    out = np.full_like(a, fill)
    H, W = a.shape[:2]
    ys0, ys1 = max(0, dy), min(H, H + dy)
    xs0, xs1 = max(0, dx), min(W, W + dx)
    out[ys0:ys1, xs0:xs1] = a[ys0 - dy:ys1 - dy, xs0 - dx:xs1 - dx]
    return out


def render(model, pose, yaw_deg, size, anchor, extra=None, shadow=True, outline=True, light_boost=1.0, ao=True):
    """Egy képkocka. Visszatér: (RGBA kép, csapatszín-maszk kép) a kért (1×) méretben."""
    W, H = size
    ax, ay = anchor
    P, N, keep = pose_points(model, pose)
    C = model.C * model.SV[:, None]
    T, G, X, noise, SP = model.T, model.G, model.X, model.noise, model.SP
    if extra is not None:  # pózfüggő extra pontok (vértócsa, füst, varázsfény)
        n = len(extra[0])
        P = np.concatenate([P, extra[0]])
        N = np.concatenate([N, extra[1]])
        C = np.concatenate([C, extra[2]])
        T = np.concatenate([T, np.zeros(n, bool)])
        G = np.concatenate([G, extra[3] if len(extra) > 3 else np.zeros(n, bool)])
        X = np.concatenate([X, np.zeros(n)])
        SP = np.concatenate([SP, np.zeros(n)])
        noise = np.concatenate([noise, np.zeros(n)])
        keep = np.concatenate([keep, np.ones(n, bool)])
    P, N, C, T, G, X, noise, SP = P[keep], N[keep], C[keep], T[keep], G[keep], X[keep], noise[keep], SP[keep]
    R = _rot(0, 0, yaw_deg)
    P = P @ R.T
    N = N @ R.T
    if render_res.active and RES != 1:  # nagyfelbontású render: a modell RES-szeresére nagyítva
        P = P * RES
    s = SS
    Ws, Hs = W * s, H * s
    # --- árnyalás
    lam = np.clip(N @ LIGHT, 0, 1)
    nv = np.clip(N @ VIEW, 0, 1)
    if PAINTED:  # festett: folyamatos, mély árnyékú fény, ecsetvonás-szerű zaj, perem-csúcsfény
        inten = 0.34 + 0.86 * lam ** 0.9 + 0.18 * (1 - nv) ** 2 * (N[:, 2] > -0.2) + 0.08 * np.clip(N[:, 2], 0, 1)
        stroke = np.sin(noise * 31.0 + P[:, 2] * 0.35) * 0.5 + 0.5
        inten = inten * (1 + X * (noise - 0.5) * 2.2) * (0.93 + 0.14 * stroke)
    else:
        inten = 0.38 + 0.84 * lam + 0.12 * (1 - nv) * (N[:, 2] > -0.2)
        inten = inten * (1 + X * (noise - 0.5) * 2)
        # festett (WC2-szerű) tónuslépcsők: 6 sáv, árnyékban hideg, fényben meleg színeltolással
        inten = np.floor(inten * 6.0) / 6.0 + 1 / 12
    tt = np.clip(inten - 0.78, -0.5, 0.5)[:, None]
    warm = np.array([0.16, 0.06, -0.14])
    cool = np.array([-0.16, -0.05, 0.18])
    tint = np.where(tt > 0, 1 + tt * warm, 1 + (-tt) * cool)
    spec = SP * np.clip(N @ HALF, 0, 1) ** (14 if PAINTED else 18) * (290 if PAINTED else 255)
    base = C * tint
    grey = base.mean(axis=1, keepdims=True)
    base = grey + (base - grey) * (1.26 if PAINTED else 1.18)  # telítettebb, "festett" színek
    col = base * inten[:, None] + spec[:, None]
    col = np.where(G[:, None], np.minimum(255, C * (1.05 + 0.2 * lam[:, None])), col)
    col = np.clip(col, 0, 255)
    px = np.rint((ax + P[:, 0]) * s).astype(int)
    py = np.rint((ay + CAM_G * P[:, 1] - K_OBL * P[:, 2]) * s).astype(int)
    depth = K_OBL * P[:, 1] + CAM_G * P[:, 2]
    m = (px >= 0) & (px < Ws) & (py >= 0) & (py < Hs)
    LU, CB = inten[m], C[m]
    px, py, depth, col, T, G = px[m], py[m], depth[m], col[m], T[m], G[m]
    o = np.argsort(depth, kind="stable")
    px, py, col, T, depth, G = px[o], py[o], col[o], T[o], depth[o], G[o]
    LU, CB = LU[o], CB[o]
    solid = np.zeros((Hs, Ws), bool)
    rgb = np.zeros((Hs, Ws, 3), np.float32)
    tm = np.zeros((Hs, Ws), bool)
    dep = np.full((Hs, Ws), -1e6, np.float32)
    glw = np.zeros((Hs, Ws), bool)
    lum = np.ones((Hs, Ws), np.float32)
    cbuf = np.zeros((Hs, Ws, 3), np.float32)
    lum[py, px] = LU
    cbuf[py, px] = CB
    rgb[py, px] = col
    tm[py, px] = T
    solid[py, px] = True
    dep[py, px] = depth
    glw[py, px] = G
    # lyukak kitöltése
    for _ in range(3):
        nb = np.zeros((Hs, Ws), int)
        for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            nb += _shift(solid, dy, dx, False)
        hole = (~solid) & (nb >= 3)
        if not hole.any():
            break
        ys, xs = np.nonzero(hole)
        for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            yy, xx = np.clip(ys + dy, 0, Hs - 1), np.clip(xs + dx, 0, Ws - 1)
            ok = solid[yy, xx] & ~solid[ys, xs]
            rgb[ys[ok], xs[ok]] = rgb[yy[ok], xx[ok]]
            tm[ys[ok], xs[ok]] = tm[yy[ok], xx[ok]]
            dep[ys[ok], xs[ok]] = dep[yy[ok], xx[ok]]
            glw[ys[ok], xs[ok]] = glw[yy[ok], xx[ok]]
            lum[ys[ok], xs[ok]] = lum[yy[ok], xx[ok]]
            cbuf[ys[ok], xs[ok]] = cbuf[yy[ok], xx[ok]]
            solid[ys[ok], xs[ok]] = True
    # --- képtér-alapú környezeti árnyékolás: a pixelt takaró, "magasabban" lévő szomszédok sötétítenek
    if ao:
        occ = np.zeros((Hs, Ws), np.float32)
        cnt = 0
        for r in (2 * s * OUT_SCALE, 4 * s * OUT_SCALE):
            for dy, dx in ((-r, 0), (r, 0), (0, -r), (0, r), (-r, -r), (-r, r), (r, -r), (r, r)):
                nd = _shift(dep, dy, dx, -1e6)
                occ += np.clip((nd - dep - 0.8) / 4.0, 0, 1)
                cnt += 1
        aof = 1.0 - (0.55 if PAINTED else 0.42) * (occ / cnt)
        aof = np.where(glw, 1.0, aof)
        rgb *= aof[:, :, None]
    if PAINTED:  # kézzel rajzolt hatás (a nagy felbontású képen)
        yy_, xx_ = np.mgrid[0:Hs, 0:Ws]
        flat = solid & ~glw
        # 1) tónuslépcsők kézi pixelezésre emlékeztető pontozással (rendezett dither a sávhatárokon)
        bayer = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], np.float32) / 16 - 0.47
        step = 0.2
        lq = np.floor((lum + bayer[yy_ % 4, xx_ % 4] * step * 0.9) / step) * step + step * 0.55
        ratio = np.clip(lq / np.maximum(lum, 0.05), 0.62, 1.45)
        rgb[flat] *= ratio[flat][:, None]
        # 2) vonalkázás az árnyékos részeken (átlós tollvonások)
        hatch = flat & (lum < 0.62) & (((xx_ - yy_) % 6) == 0)
        hatch |= flat & (lum < 0.42) & (((xx_ + yy_) % 6) == 0)
        rgb[hatch] *= 0.74
        # 3) tintavonal az anyaghatárokon (pl. bunda és bőr, fa és festés között)
        ink = np.zeros((Hs, Ws), bool)
        for dy, dx in ((1, 0), (0, 1)):
            nb = _shift(cbuf, dy, dx, 0)
            ns = _shift(solid, dy, dx, False)
            ink |= (np.abs(nb - cbuf).sum(axis=2) > 95) & ns
        ink &= flat
        rgb[ink] = rgb[ink] * 0.42 + np.array(OUTLINE, np.float32) * 0.58
    # --- lekicsinyítés: többségi alfa, színátlag a szilárd alpixelekből
    sol4 = solid.reshape(H, s, W, s).astype(np.float32)
    cover = sol4.sum(axis=(1, 3))
    rgbsum = (rgb.reshape(H, s, W, s, 3) * sol4[..., None]).sum(axis=(1, 3))
    tsum = (tm.reshape(H, s, W, s) & solid.reshape(H, s, W, s)).sum(axis=(1, 3))
    small = cover >= (s * s) / 2.0
    out_rgb = np.where(cover[..., None] > 0, rgbsum / np.maximum(cover, 1)[..., None], 0)
    tsmall = small & (tsum * 2 >= np.maximum(cover, 1))
    img = np.zeros((H, W, 4), np.uint8)
    team = np.zeros((H, W, 4), np.uint8)
    if shadow == "blob":  # festett stílus: kis, puha árnyékfolt a talp alatt
        low = P[:, 2] < 3.0 * (RES if render_res.active else 1)
        if low.any():
            rx = max(3.0, float(np.percentile(np.abs(P[low, 0]), 90)) * 1.15)
            ry = max(2.0, float(np.percentile(np.abs(P[low, 1]), 90)) * 1.15 * CAM_G)
            yy, xx = np.mgrid[0:H, 0:W]
            e = ((xx - ax) / rx) ** 2 + ((yy - ay) / ry) ** 2
            img[e <= 1] = (0, 0, 0, 90)
    elif shadow:
        # vetett árnyék: a pontokat a fény irányában a talajra vetítjük (1× felbontás)
        sx = np.rint(ax + P[:, 0] + P[:, 2] * 0.55).astype(int)
        sy = np.rint(ay + CAM_G * (P[:, 1] + P[:, 2] * 0.3)).astype(int)
        mm = (sx >= 0) & (sx < W) & (sy >= 0) & (sy < H)
        img[sy[mm], sx[mm]] = SHADOW
    img[small, :3] = np.clip(out_rgb[small], 0, 255).astype(np.uint8)
    img[small, 3] = 255
    # peremfény: a fény felőli (bal-felső) szélen világosabb kontúr – kiemeli a sziluettet
    rim = small & ~_shift(small, 1, 1, False)
    rim &= ~(_shift(small, 1, 0, False) & _shift(small, 0, 1, False))
    img[rim, :3] = np.clip(img[rim, :3].astype(np.float32) * 1.22 + np.array([10, 8, 0]), 0, 255).astype(np.uint8)
    if outline and PAINTED:  # sötét, meleg barna, 2 képpontos kontúr (a lekicsinyítés után ~1)
        grown = small.copy()
        wob = (np.sin(np.arange(H)[:, None] * 0.9 + np.arange(W)[None, :] * 0.37) * 43758.5) % 1.0
        for it in range(3 if RES > 1 else 1):  # egyenetlen, „tollal húzott" vastagság
            nb = np.zeros((H, W), bool)
            for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                nb |= _shift(grown, dy, dx, False)
            grown = grown | (nb if it < 2 else nb & (wob > 0.55))
        edge = grown & ~small
        img[edge, :3] = OUTLINE
        img[edge, 3] = 255
        # belső kontúr: a mélységugrásoknál (pl. kar a törzs előtt) sötétebb vonal
        dsm = dep.reshape(H, s, W, s).max(axis=(1, 3))
        jump = np.zeros((H, W), bool)
        for dy, dx in ((1, 0), (0, 1), (-1, 0), (0, -1)):
            jump |= (_shift(dsm, dy, dx, -1e6) - dsm > 3.5 * RES) & _shift(small, dy, dx, False)
        jump &= small
        img[jump, :3] = (img[jump, :3].astype(np.float32) * 0.45 + np.array(OUTLINE) * 0.55).astype(np.uint8)
    elif outline:
        nb = np.zeros((H, W), bool)
        for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            nb |= _shift(small, dy, dx, False)
        edge = nb & ~small
        # színezett körvonal: a szomszédos szín sötét változata
        acc = np.zeros((H, W, 3), np.float32)
        k = np.zeros((H, W), np.float32)
        for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            sm = _shift(small, dy, dx, False)
            acc += _shift(img[:, :, :3].astype(np.float32), dy, dx, 0) * sm[..., None]
            k += sm
        oc = acc / np.maximum(k, 1)[..., None] * 0.28 + 8
        img[edge, :3] = oc[edge].astype(np.uint8)
        img[edge, 3] = 255
    if tsmall.any():
        grey = img[tsmall, :3].astype(np.float32).mean(axis=1)
        g3 = np.stack([grey] * 3, 1).astype(np.uint8)
        img[tsmall, :3] = g3
        team[tsmall, :3] = g3
        team[tsmall, 3] = 255
    return Image.fromarray(img, "RGBA"), Image.fromarray(team, "RGBA")


def paint_downscale(img, factor, cut=110):
    """Festett lekicsinyítés (a kézzel festett csomag importjával azonos): prémultiplikált LANCZOS,
    kemény alfa, enyhe élesítés."""
    from PIL import ImageFilter
    if factor == 1:
        return img
    w, h = max(1, round(img.width / factor)), max(1, round(img.height / factor))
    small = img.convert("RGBa").resize((w, h), Image.LANCZOS).convert("RGBA")
    a = np.asarray(small).copy()
    keep = a[..., 3] >= cut
    rgb = Image.fromarray(a[..., :3]).filter(ImageFilter.UnsharpMask(radius=1, percent=60, threshold=2))
    a[..., :3] = np.asarray(rgb)
    a[..., 3] = np.where(keep, 255, 0)
    a[~keep] = 0
    return Image.fromarray(a, "RGBA")


def hand_finish(img):
    """Kézzel pixelezett befejezés 1× méreten: éles 1 px-es sötét kontúr, fény felőli peremcsík,
    tintavonal az erős belső éleken."""
    a = np.asarray(img).astype(np.float32)
    solid = a[..., 3] > 0
    rgb = a[..., :3]
    lum = rgb @ np.array([0.3, 0.55, 0.15], np.float32)
    out_edge = np.zeros(solid.shape, bool)
    for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
        out_edge |= ~_shift(solid, dy, dx, False)
    ring = solid & out_edge
    # belső tintavonal: a sötétebbik oldal kap vonalat, ha nagy a világosságugrás
    ink = np.zeros(solid.shape, bool)
    for dy, dx in ((1, 0), (0, 1), (-1, 0), (0, -1)):
        nl = _shift(lum, dy, dx, 0)
        ink |= (nl - lum > 52) & _shift(solid, dy, dx, False)
    ink &= solid & ~ring
    rgb[ink] = rgb[ink] * 0.55 + np.array(OUTLINE, np.float32) * 0.45
    # peremfény: a bal-felső (fény felőli) belső szélen világosabb pixelek
    lit = solid & ~ring & (~_shift(solid, 2, 2, False) | ~_shift(solid, 2, 0, False)) & ~ink
    rgb[lit] = np.minimum(255, rgb[lit] * 1.18 + 14)
    rgb[ring] = rgb[ring] * 0.22 + np.array(OUTLINE, np.float32) * 0.78
    a[..., :3] = rgb
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8), "RGBA")


def render_res(model, pose, yaw_deg, size, anchor, **kw):
    """RES-szeres felbontású render + festett lekicsinyítés. size/anchor 1× képpontban; a kimenet
    OUT_SCALE-szeres (HD: 2×). Az árnyék (félig átlátszó) külön, keményítés nélkül kerül vissza."""
    W, H = size
    render_res.active = True
    try:
        im, tm = render(model, pose, yaw_deg, (W * RES, H * RES), (anchor[0] * RES, anchor[1] * RES), **kw)
    finally:
        render_res.active = False
    if RES == 1:
        return im, tm
    a = np.asarray(im)
    solid = a[..., 3] == 255
    body = a.copy()
    body[~solid] = 0
    shadow = a.copy()
    shadow[solid] = 0
    f = RES // OUT_SCALE
    out = hand_finish(paint_downscale(Image.fromarray(body, "RGBA"), f))
    sh = Image.fromarray(shadow, "RGBA").resize((W * OUT_SCALE, H * OUT_SCALE), Image.BOX)
    sh.alpha_composite(out)
    tmask = paint_downscale(tm, f, cut=128)
    return sh, tmask


render_res.active = False


_PAL = None


def game_palette():
    """A közös játékpaletta (assets/sprites/palette.json), ha van."""
    global _PAL
    if _PAL is None:
        import json
        import os
        p = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
                         "assets", "sprites", "palette.json")
        _PAL = np.asarray(json.load(open(p, encoding="utf-8"))["colors"], np.float32) if os.path.isfile(p) else False
    return _PAL


_BAYER = (np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], np.float32) / 16.0 - 0.47)


def palettize(img, colors=72, dither=14.0):
    """Retró paletta: a közös játékpalettára képez rendezett (Bayer) ditherrel, mint a 90-es évek
    256 színű játékai; ha nincs közös paletta, sprite-onkénti korlátozott paletta."""
    pal = game_palette()
    if pal is False:
        a = img.getchannel("A")
        q = img.convert("RGB").quantize(colors=colors, method=Image.Quantize.MEDIANCUT,
                                        dither=Image.Dither.NONE).convert("RGBA")
        q.putalpha(a)
        return q
    arr = np.asarray(img.convert("RGBA")).copy()
    H, W = arr.shape[:2]
    rgb = arr[..., :3].astype(np.float32)
    yy, xx = np.mgrid[0:H, 0:W]
    rgb += _BAYER[yy % 4, xx % 4][..., None] * dither
    flat = rgb.reshape(-1, 3)
    mask = arr[..., 3].reshape(-1) > 0
    idx = np.zeros(len(flat), np.int32)
    sel = np.nonzero(mask)[0]
    pn = (pal ** 2).sum(1)
    for k in range(0, len(sel), 65536):
        ch = flat[sel[k:k + 65536]]
        d = pn[None, :] - 2 * ch @ pal.T
        idx[sel[k:k + 65536]] = d.argmin(1)
    out = pal[idx].reshape(H, W, 3)
    arr[..., :3] = np.where(arr[..., 3:4] > 0, out, arr[..., :3]).astype(np.uint8)
    return Image.fromarray(arr, "RGBA")

def particles(n, center, spread, rng, color, rad=(0.6, 1.4), glow=True):
    """Részecske-felhő ellenőrzött véletlennel (effektekhez)."""
    pts, nrms, cols, glows = [], [], [], []
    for _ in range(n):
        c = np.asarray(center, float) + rng.normal(0, 1, 3) * spread
        r = rng.uniform(*rad)
        p, nn = ellipsoid(c, (r, r, r))
        pts.append(p)
        nrms.append(nn)
        cols.append(np.repeat(np.asarray(color, float)[None], len(p), 0))
        glows.append(np.full(len(p), glow))
    return np.concatenate(pts), np.concatenate(nrms), np.concatenate(cols), np.concatenate(glows)
