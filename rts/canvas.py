"""HD edition: a világ rajzolása kétszeres képpontsűrűséggel.

A játéktér kódja „világ-képpontban” rajzol (1 csempe = 32). Retró módban ez egy félfelbontású felületre megy,
amit a játék a végén 2×-re nagyít. HD módban a WorldCanvas veszi át a felület szerepét: a koordinátákat
k-szorosra váltja, és közvetlenül a képernyőre rajzol.
  * a HD-ként megjelölt képeket (HD sprite-ok, HD terep) élesen, nagyítás nélkül rakja ki;
  * minden más képet (felirat, köd, régi sprite) k-szorosra nagyít, így azok pontosan úgy néznek ki, mint eddig.
A pygame.draw hívások helyett a modul azonos nevű függvényei kellenek (line, rect, circle …): valódi
felületen változatlanul továbbadják a hívást, WorldCanvas-on átszámolják a koordinátákat.
"""
import weakref

import pygame

_HD = weakref.WeakSet()   # a már HD-felbontású (k-szoros) képek


_TRIM = {}


def trimmed(surf):
    """A kép átlátszó szegélye nélkül, RLE-gyorsítással (sokkal gyorsabb blit), és a levágott eltolás.
    Gyorsítótárazva; az eredeti képre is hivatkozik, így az azonosító nem kerülhet újra elő."""
    k = id(surf)
    v = _TRIM.get(k)
    if v is not None and v[0] is surf:
        return v[1], v[2], v[3]
    r = surf.get_bounding_rect()
    if r.w <= 0 or r.h <= 0:
        r = pygame.Rect(0, 0, 1, 1)
    c = surf.subsurface(r).copy()
    if surf.get_flags() & pygame.SRCALPHA:
        c.set_alpha(surf.get_alpha(), pygame.RLEACCEL)
    if surf in _HD:
        _HD.add(c)
    if len(_TRIM) > 6000:
        _TRIM.clear()
    _TRIM[k] = (surf, c, r.x, r.y)
    return c, r.x, r.y


def blit_trim(s, surf, x, y, special_flags=0):
    """Mint s.blit(surf, (x, y)), de a levágott, gyorsított képpel (HD vásznon a levágás HD-képpontban)."""
    c, ox, oy = trimmed(surf)
    k = s.k if isinstance(s, WorldCanvas) and surf in _HD else 1
    return s.blit(c, (x + ox / k, y + oy / k), None, special_flags)


def mark_hd(surf):
    """A kép HD-felbontású: a vászon nagyítás nélkül rakja ki."""
    if surf is not None:
        _HD.add(surf)
    return surf


def is_hd(surf):
    return surf in _HD


class WorldCanvas:
    """Világ-képpontos rajzfelület, amely k-szoros felbontásban a valódi képernyőre rajzol."""

    def __init__(self, target, k, size):
        self.t = target
        self.k = k
        self.size = (int(size[0]), int(size[1]))

    # --- méret, vágás
    def get_size(self):
        return self.size

    def get_width(self):
        return self.size[0]

    def get_height(self):
        return self.size[1]

    def get_rect(self, **kw):
        return pygame.Rect((0, 0), self.size)

    def _r(self, r):
        r = pygame.Rect(r)
        k = self.k
        return pygame.Rect(r.x * k, r.y * k, r.w * k, r.h * k)

    def set_clip(self, r=None):
        self.t.set_clip(None if r is None else self._r(r))

    def get_clip(self):
        c = self.t.get_clip()
        k = self.k
        return pygame.Rect(c.x // k, c.y // k, c.w // k, c.h // k)

    def fill(self, color, rect=None, special_flags=0):
        return self.t.fill(color, None if rect is None else self._r(rect), special_flags)

    # --- képek
    def blit(self, src, dest, area=None, special_flags=0):
        k = self.k
        x, y = (dest[0], dest[1]) if not isinstance(dest, pygame.Rect) else (dest.x, dest.y)
        pos = (round(x * k), round(y * k))
        if src in _HD:
            return self.t.blit(src, pos, None if area is None else self._r(area), special_flags)
        if area is not None:
            a = pygame.Rect(area).clip(src.get_rect())
            if a.w <= 0 or a.h <= 0:
                return pygame.Rect(pos, (0, 0))
            src = src.subsurface(a)
        w, h = src.get_size()
        if w == 0 or h == 0:
            return pygame.Rect(pos, (0, 0))
        return self.t.blit(pygame.transform.scale(src, (w * k, h * k)), pos, None, special_flags)

    def blits(self, seq, doreturn=True):
        out = [self.blit(*item) for item in seq]
        return out if doreturn else None


def _k(s):
    return s.k if isinstance(s, WorldCanvas) else 0


def _p(p, k):
    return (p[0] * k, p[1] * k)


def _w(width, k):
    return 0 if width == 0 else max(1, int(round(width * k)))


# --- a pygame.draw megfelelői -------------------------------------------------------
def line(s, color, a, b, width=1):
    k = _k(s)
    if not k:
        return pygame.draw.line(s, color, a, b, width)
    return pygame.draw.line(s.t, color, _p(a, k), _p(b, k), _w(width, k))


def lines(s, color, closed, points, width=1):
    k = _k(s)
    if not k:
        return pygame.draw.lines(s, color, closed, points, width)
    return pygame.draw.lines(s.t, color, closed, [_p(p, k) for p in points], _w(width, k))


def aaline(s, color, a, b, *args):
    k = _k(s)
    if not k:
        return pygame.draw.aaline(s, color, a, b, *args)
    return pygame.draw.aaline(s.t, color, _p(a, k), _p(b, k))


def rect(s, color, r, width=0, *args, **kw):
    k = _k(s)
    if not k:
        return pygame.draw.rect(s, color, r, width, *args, **kw)
    kw = {key: (v * k if isinstance(v, (int, float)) and v > 0 else v) for key, v in kw.items()}
    return pygame.draw.rect(s.t, color, s._r(r), _w(width, k), *[a * k for a in args], **kw)


def circle(s, color, center, radius, width=0, *args):
    k = _k(s)
    if not k:
        return pygame.draw.circle(s, color, center, radius, width, *args)
    return pygame.draw.circle(s.t, color, _p(center, k), radius * k, _w(width, k), *args)


def ellipse(s, color, r, width=0):
    k = _k(s)
    if not k:
        return pygame.draw.ellipse(s, color, r, width)
    return pygame.draw.ellipse(s.t, color, s._r(r), _w(width, k))


def arc(s, color, r, start, stop, width=1):
    k = _k(s)
    if not k:
        return pygame.draw.arc(s, color, r, start, stop, width)
    return pygame.draw.arc(s.t, color, s._r(r), start, stop, _w(width, k))


def polygon(s, color, points, width=0):
    k = _k(s)
    if not k:
        return pygame.draw.polygon(s, color, points, width)
    return pygame.draw.polygon(s.t, color, [_p(p, k) for p in points], _w(width, k))
