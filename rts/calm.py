"""„Nyugodt + belső átlag” – a játéktér figuráinak (egység, épület, nyersanyag, tereptárgy) képfeldolgozása futás
közben. A képfájlok nem változnak: a szűrő a képkockán fut, amikor a játék először megjeleníti, a tényleges
megjelenítési méretben (a vonalvastagság így mindig 1 képernyő-képpont), és az eredmény gyorsítótárba kerül.
A HUD-ra és a terep alapjára (csempék, talajmatricák) nem hat.

Recept (docs/NYUGODT_BELSO_ATLAG.md):
  1. felület: a belső részen (5×5-ös alfa-minimum > 220) 3×3-as medián, élvédelemmel keverve;
  2. belső tinta: a simított világosság helyi maximumánál sötétebb pontok halk, lilásfekete tintát kapnak;
  3. külső körvonal: 1 képpontos, a sziluett belsejének helyi színéből (0,40 × átlag + (8, 6, 8)), a kép alá.
"""
import numpy as np
import pygame

import time

ENABLED = True
WORLD_DIRS = ("units", "buildings", "resources", "props")   # a játéktér figurái; a csempék és a HUD nem
BUDGET_MS = 6.0      # képkockánként legfeljebb ennyi időt tölt új képkockák feldolgozásával (nincs akadás)
_spent = 0.0


def new_frame():
    """A UI hívja minden megjelenített képkocka elején: újraindul a feldolgozási keret."""
    global _spent
    _spent = 0.0


def applies(meta_path):
    import os
    folder = os.path.basename(os.path.dirname(meta_path))
    return folder in WORLD_DIRS and not os.path.basename(meta_path).startswith("decals")


def get(cache, key, raw):
    """A feldolgozott képkocka a gyorsítótárból; ha még nincs, és belefér a keretbe, most készül el.
    Ha nem fér bele, ebben a képkockában az eredeti látszik (a következőkben pótlódik)."""
    global _spent
    c = cache.get(key)
    if c is not None:
        return c
    if _spent > BUDGET_MS:
        return raw
    t = time.perf_counter()
    c = process(raw)
    _spent += (time.perf_counter() - t) * 1000.0
    cache[key] = c
    return c
INK = np.array([34, 23, 36], np.float32)
_LUM = np.array([0.299, 0.587, 0.114], np.float32)
# a körvonal színmintáinak eltolásai és alapsúlyai (|dx|,|dy| ≤ 3, dx² + dy² ≤ 10)
_OFFS = [(dx, dy, 1.0 / (1 + dx * dx + dy * dy)) for dx in range(-3, 4) for dy in range(-3, 4)
         if dx * dx + dy * dy <= 10]


def _shifts(a, r, pad_value=0):
    """Az a tömb minden (dx, dy) eltolása |dx|,|dy| ≤ r tartományban (a széleken pad_value)."""
    w, h = a.shape[:2]
    p = np.pad(a, ((r, r), (r, r)) + ((0, 0),) * (a.ndim - 2), constant_values=pad_value)
    for dx in range(-r, r + 1):
        for dy in range(-r, r + 1):
            yield dx, dy, p[r + dx:r + dx + w, r + dy:r + dy + h]


def _maxf(a, r=1, pad=0):
    out = None
    for _, _, s in _shifts(a, r, pad):
        out = s.copy() if out is None else np.maximum(out, s, out=out)
    return out


def _minf(a, r=1, pad=0):
    out = None
    for _, _, s in _shifts(a, r, pad):
        out = s.copy() if out is None else np.minimum(out, s, out=out)
    return out


def _gauss065(a):
    """0,65 képpontos Gauss-elmosás (3×3-as, szétválasztható mag)."""
    k = np.array([0.306, 1.0, 0.306], np.float32)
    k /= k.sum()
    p = np.pad(a, 1, mode="edge")
    t = p[:-2, :] * k[0] + p[1:-1, :] * k[1] + p[2:, :] * k[2]
    return t[:, :-2] * k[0] + t[:, 1:-1] * k[1] + t[:, 2:] * k[2]


def process(src):
    """Új (feldolgozott) felület a képkockából; a bemenet nem változik."""
    W, H = src.get_size()
    r = src.get_bounding_rect(min_alpha=1)
    if r.w < 2 or r.h < 2:
        return src
    r = r.inflate(8, 8).clip(pygame.Rect(0, 0, W, H))   # munkaperem a körvonalnak és a szűrőknek
    sub = src.subsurface(r)
    rgb = pygame.surfarray.array3d(sub).astype(np.float32)          # (w, h, 3)
    a = pygame.surfarray.array_alpha(sub).astype(np.float32)        # (w, h)
    w, h = a.shape
    inner = (_minf(a, 2) > 220).astype(np.float32)
    # 1) felület: medián az élek kivételével
    L = rgb @ _LUM
    edge = np.clip((_maxf(L) - _minf(L) - 8.0) / 26.0, 0, 1)
    stack = np.stack([s for _, _, s in _shifts(rgb.astype(np.uint8), 1)], 0)
    med = np.partition(stack, 4, axis=0)[4].astype(np.float32)
    wgt = (0.90 * (1 - edge) * inner)[..., None]
    smooth = rgb * (1 - wgt) + med * wgt
    # 2) belső tinta
    Lg = _gauss065(smooth @ _LUM)
    ink = (0.35 * np.clip((_maxf(Lg) - Lg - 8.0) / 24.0, 0, 0.76) * inner)[..., None]
    out = smooth * (1 - ink) + INK * ink
    # 3) külső körvonal a sziluett köré, a helyi belső színből
    sil = a >= 128
    ring = _maxf(sil.astype(np.uint8)).astype(bool) & ~sil
    if ring.any():
        solid = a > 190
        core = _minf(a) > 190
        ix, iy = np.nonzero(ring)                 # csak a körvonal pontjain számolunk
        p_rgb = np.pad(smooth, ((3, 3), (3, 3), (0, 0)))
        wmap = np.pad(np.where(core, 3.0, 1.0).astype(np.float32) * solid, 3)
        acc = np.zeros((ix.size, 3), np.float32)
        wsum = np.zeros(ix.size, np.float32)
        for dx, dy, base in _OFFS:
            ww = wmap[ix + 3 + dx, iy + 3 + dy] * base
            acc += p_rgb[ix + 3 + dx, iy + 3 + dy] * ww[:, None]
            wsum += ww
        ok = wsum > 0
        ix, iy = ix[ok], iy[ok]
        line = np.clip(0.40 * acc[ok] / wsum[ok, None] + np.array([8, 6, 8], np.float32), 0, 255)
        af = (a[ix, iy] / 255.0)[:, None]
        out[ix, iy] = out[ix, iy] * af + line * (1 - af)
        a[ix, iy] = 255.0
    piece = pygame.Surface((w, h), pygame.SRCALPHA)
    pygame.surfarray.blit_array(piece, np.clip(out, 0, 255).astype(np.uint8))
    pa = pygame.surfarray.pixels_alpha(piece)
    pa[...] = a.astype(np.uint8)
    del pa
    res = pygame.Surface((W, H), pygame.SRCALPHA)
    res.blit(piece, r.topleft)
    return res
