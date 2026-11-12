"""Automatikus vetett árnyékok: minden figura (egység, épület, fa, kő, bánya) körvonalából számolt, lapított és
ferdített, puha szélű árnyék a talajon. A fény a festett képeken bal felülről jön, ezért az árnyék jobbra lefelé
dől. Képkockánként egyszer készül, utána gyorsítótárból jön.
"""
from collections import OrderedDict

import numpy as np
import pygame

from . import canvas as gdraw

FLAT = 0.44     # az árnyék magassága a figura magasságához képest
SHEAR = 0.62    # jobbra dőlés: ennyi képpont oldalra minden magasság-képpontra (lapítás előtt)
ALPHA = 150     # legsötétebb árnyék (0–255)
_CACHE = OrderedDict()
_MAX = 5000


BLD_LEN = 0.72  # az épületek árnyéka rövidebb (a magas, festett homlokzat különben túl hosszú árnyékot vet)


def shadow_of(fr, foot=None, mode=None):
    """(árnyék-kép, eltolás x, eltolás y) a képkocka bal felső sarkához képest, a kép saját képpontjaiban;
    None, ha üres a kép. foot: a talpvonal (a figura a talajon itt áll) a képkockán belül; az árnyék innen
    vetül, és csak a talpvonal FELETTI rész vet árnyékot (a festett talp-folt és az alapzat eleje nem)."""
    k = (id(fr), foot, mode)
    v = _CACHE.get(k)
    if v is not None and v[0] is fr:
        _CACHE.move_to_end(k)
        return v[1]
    if mode == "col":
        out = _make_columns(fr)
    else:
        # A fák festett talajfoltja áttetsző. A törzs talpát a fa képe adja.
        out = _make(fr, foot, opacity=160 if mode == "tree" else 61)
    _CACHE[k] = (fr, out)
    if len(_CACHE) > _MAX:
        _CACHE.popitem(last=False)
    return out


def _make(fr, foot=None, opacity=61):
    r = fr.get_bounding_rect(min_alpha=opacity if opacity > 61 else 40)
    if r.w < 2 or r.h < 2:
        return None
    if foot is None:
        foot = r.bottom
    foot = int(max(r.y + 2, min(r.bottom, foot)))
    a = pygame.surfarray.array_alpha(fr.subsurface(r)).astype(np.float32)  # (w, h)
    W = a.shape[0]
    H = foot - r.y            # a talpvonal feletti magasság: csak ez vet árnyékot
    a = a[:, :H]
    mask = (a >= opacity).astype(np.float32)
    sh_h = max(2, int(H * FLAT) + 1)
    sh_w = W + int(H * SHEAR) + 2
    dst = np.zeros((sh_w, sh_h), np.float32)
    for y in range(H):  # a talpvonaltól felfelé haladva: minden sor lejjebb és jobbra kerül
        d = H - 1 - y
        dy = int(d * FLAT)
        dx = int(d * SHEAR)
        col = mask[:, y]
        seg = dst[dx:dx + W, dy]
        np.maximum(seg, col, out=seg)
    pad = max(3, int(min(W, H) * 0.04))
    big = np.zeros((sh_w + 2 * pad, sh_h + 2 * pad), np.float32)
    big[pad:pad + sh_w, pad:pad + sh_h] = dst
    # távolabb halványabb: a talpnál sötétebb, a végén világosabb
    fade = np.linspace(1.0, 0.55, big.shape[1])[None, :]
    big *= fade
    s = pygame.Surface(big.shape, pygame.SRCALPHA)
    s.fill((8, 10, 14))
    pa = pygame.surfarray.pixels_alpha(s)
    pa[...] = np.clip(big * ALPHA, 0, 255).astype(np.uint8)
    del pa
    # puha szél: kicsinyítés és visszanagyítás
    w2, h2 = s.get_size()
    f = 3
    s = pygame.transform.smoothscale(pygame.transform.smoothscale(s, (max(1, w2 // f), max(1, h2 // f))), (w2, h2))
    s.set_alpha(255, pygame.RLEACCEL)
    if gdraw.is_hd(fr):
        gdraw.mark_hd(s)
    return s, r.x - pad, foot - 1 - pad


def _make_columns(fr):
    """Épület-árnyék oszloponkénti talppal: minden képoszlop a saját legalsó pontján „áll” a földön, és a
    képpont magassága ehhez mérődik. Így a lapos részek (lépcső, talapzat, udvar, víz) alig vetnek árnyékot, a
    tornyok árnyéka pedig a tövüknél kezdődik, nem válik el az épülettől."""
    r = fr.get_bounding_rect(min_alpha=40)
    if r.w < 2 or r.h < 2:
        return None
    a = pygame.surfarray.array_alpha(fr.subsurface(r))          # (w, h)
    W, H = a.shape
    mask = a > 60
    has = mask.any(axis=1)
    if not has.any():
        return None
    base = H - 1 - np.argmax(mask[:, ::-1], axis=1)              # oszloponként a legalsó képpont sora
    xs, ys = np.nonzero(mask)
    h = (base[xs] - ys).astype(np.float32)                       # a képpont magassága a saját talpa fölött
    sx = (xs + h * SHEAR * BLD_LEN).astype(np.int32)
    sy = (base[xs] + h * FLAT * BLD_LEN).astype(np.int32)
    hmax = max(1.0, float(h.max()))
    val = 1.0 - 0.45 * h / hmax                                  # távolabb halványabb
    pad = max(3, int(min(W, H) * 0.04))
    sw, sh = int(sx.max()) + 2 * pad + 2, int(sy.max()) + 2 * pad + 2
    dst = np.zeros((sw, sh), np.float32)
    np.maximum.at(dst, (sx + pad, sy + pad), val)
    s = pygame.Surface((sw, sh), pygame.SRCALPHA)
    s.fill((8, 10, 14))
    pa = pygame.surfarray.pixels_alpha(s)
    pa[...] = np.clip(dst * ALPHA, 0, 255).astype(np.uint8)
    del pa
    f = 3
    s = pygame.transform.smoothscale(pygame.transform.smoothscale(s, (max(1, sw // f), max(1, sh // f))), (sw, sh))
    s.set_alpha(255, pygame.RLEACCEL)
    if gdraw.is_hd(fr):
        gdraw.mark_hd(s)
    return s, r.x - pad, r.y - pad


def draw(screen, items, cx, cy):
    """Az előző képkockában kirajzolt figurák árnyékai (világ-koordinátában rögzítve) a talajra, a figurák alá."""
    for it in items:
        fr, wx, wy = it[0], it[1], it[2]
        sh = shadow_of(fr, it[3] if len(it) > 3 else None, it[4] if len(it) > 4 else None)
        if sh is None:
            continue
        img, ox, oy = sh
        k = screen.k if isinstance(screen, gdraw.WorldCanvas) and gdraw.is_hd(fr) else 1
        screen.blit(img, (wx - cx + ox / k, wy - cy + oy / k))


# ---------------------------------------------------------------- belső árnyék
_INNER = OrderedDict()
INNER_ALPHA = 190   # a belső árnyék legsötétebb pontja


def inner_of(fr):
    """Belső árnyék: a figura fénytől elforduló (jobb alsó) belső szélei mentén sötétedés, a talp felé erősebb –
    ettől a figurák tömörebbek, jobban „ülnek” a talajon. (kép, eltolás x, eltolás y) a képkockához képest."""
    k = id(fr)
    v = _INNER.get(k)
    if v is not None and v[0] is fr:
        _INNER.move_to_end(k)
        return v[1]
    out = _make_inner(fr)
    _INNER[k] = (fr, out)
    if len(_INNER) > _MAX:
        _INNER.popitem(last=False)
    return out


def _make_inner(fr):
    r = fr.get_bounding_rect(min_alpha=40)
    if r.w < 4 or r.h < 4:
        return None
    a = pygame.surfarray.array_alpha(fr.subsurface(r)).astype(np.float32) / 255.0  # (w, h)
    W, H = a.shape
    off = max(4, int(min(W, H) * 0.22))  # messzebbről (bal-fentebbről) indul
    sh = np.zeros_like(a)  # a maszk bal-fel tolva: ahol a jobb alsó szomszéd üres, ott árnyékos a szél
    sh[:-off, :-off] = a[off:, off:]
    sh = _soft(sh, 7)   # erősen elmosva: fokozatos átmenet, nem hirtelen sötét sáv
    edge = np.clip(a - sh, 0, 1)
    edge = _soft(edge, 4) ** 1.4
    ground = np.linspace(0.45, 1.0, H)[None, :]  # a talp felé kicsit erősebb
    base = np.clip(np.linspace(-0.5, 1.0, H), 0, 1)[None, :] ** 1.6 * 0.4  # az alsó rész lágy sötétedése
    al = np.clip((edge * ground * 0.85 + base) * a, 0, 1)
    s = pygame.Surface((W, H), pygame.SRCALPHA)
    s.fill((10, 8, 14))
    pa = pygame.surfarray.pixels_alpha(s)
    pa[...] = (al * INNER_ALPHA).astype(np.uint8)
    del pa
    s.set_alpha(255, pygame.RLEACCEL)
    if gdraw.is_hd(fr):
        gdraw.mark_hd(s)
    return s, r.x, r.y


def _soft(arr, f):
    """Erős, gyors elmosás: kicsinyítés f-szeresére, majd vissza (kétlineárisan)."""
    w, h = arr.shape
    sw, shh = max(1, w // f), max(1, h // f)
    img = pygame.Surface((w, h), pygame.SRCALPHA)
    img.fill((255, 255, 255))
    pa = pygame.surfarray.pixels_alpha(img)
    pa[...] = (np.clip(arr, 0, 1) * 255).astype(np.uint8)
    del pa
    img = pygame.transform.smoothscale(pygame.transform.smoothscale(img, (sw, shh)), (w, h))
    return pygame.surfarray.array_alpha(img).astype(np.float32) / 255.0


def blit_inner(screen, fr, x, y):
    """A belső árnyék a már kirajzolt figurára (x, y: a képkocka bal felső sarka, rajzolási koordinátában)."""
    v = inner_of(fr)
    if v is None:
        return
    img, ox, oy = v
    k = screen.k if isinstance(screen, gdraw.WorldCanvas) and gdraw.is_hd(fr) else 1
    screen.blit(img, (x + ox / k, y + oy / k))
