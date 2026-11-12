"""Betűkészletek: a játék betűtípusa (Rováskönyv) a megadott méretekben; a kiírt szövegek a választott nyelven.

A menük betűi a képernyővel együtt nőnek (MENU × méretarány), a játékon belüliek (GAME) állandók.
"""
import os

from . import data as gamedata

GAME = {"tiny": 11, "small": 13, "med": 15, "big": 24, "huge": 60}
MENU = {"tiny": 14, "small": 16, "med": 18, "big": 26, "huge": 54}   # 1280×720-on
BOLD = ("med", "big", "huge")


def load_font(pygame, size, *, display=False, bold=False):
    """Rováskönyv Text feliratokhoz, Rováskönyv a nagyobb címekhez."""
    directory = os.path.join(gamedata.ROOT, "assets", "fonts")
    suffixes = ("rovaskonyv", "rovaskonyv_text") if display else ("rovaskonyv_text", "rovaskonyv")
    for suffix in suffixes:
        path = os.path.join(directory, f"egszakadas_{suffix}.ttf")
        try:
            return pygame.font.Font(path, size)
        except (OSError, pygame.error):
            pass
    return pygame.font.SysFont("segoeui,arial,dejavusans", size, bold=bold)


def make_fonts(pygame, sizes=None, scale=1.0):
    from .i18n import TFont
    return {
        key: TFont(load_font(pygame, max(8, round(size * scale)),
                             display=key in ("big", "huge"), bold=key in BOLD))
        for key, size in (sizes or GAME).items()
    }


INGAME = {"tiny": 14, "small": 15, "med": 17, "big": 24, "huge": 56}  # játékon belüli feliratok (súgó, üzenetek), 720p-n
_CACHE = {}


def scaled_fonts(pygame, sizes, u):
    """Méretarányhoz igazított betűkészlet (gyorsítótárazva)."""
    key = (id(sizes), round(u, 2))
    if key not in _CACHE:
        _CACHE[key] = make_fonts(pygame, sizes, round(u, 2))
    return _CACHE[key]
