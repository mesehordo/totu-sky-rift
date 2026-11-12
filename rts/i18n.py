"""Nyelvek: magyar (alap) és angol.

A felület szövegei magyarul szerepelnek a kódban; ``tr("Magyar szöveg")`` angol nyelvnél a
``data/lang/en.json`` "ui" szótárából adja a fordítást (ha nincs, a magyar marad).
A játékadatok (egységek, képességek, fajok, tárgyak) neveit és leírásait a "data" rész adja
azonosító szerint; a pályák neve/leírása a pályafájlban két nyelven szerepel.
"""
import json
import os

from .data import ROOT

LANG = "hu"
_TABLES = {}


def _table(lang):
    if lang not in _TABLES:
        p = os.path.join(ROOT, "data", "lang", lang + ".json")
        t = {"ui": {}, "data": {}}
        if os.path.isfile(p):
            with open(p, encoding="utf-8") as f:
                t.update(json.load(f))
        _TABLES[lang] = t
    return _TABLES[lang]


def set_lang(lang):
    global LANG
    LANG = lang if lang in ("hu", "en") else "hu"
    reset()


def tr(text, **kw):
    """Felületi szöveg fordítása; a {mezők} a kw-ből töltődnek ki."""
    if LANG != "hu" and text:
        text = _table(LANG)["ui"].get(text, text)
    return text.format(**kw) if kw else text


def name(defn, field="name"):
    """Játékadat (egység, épület, képesség, faj, tárgy, fejlesztés) neve/leírása az aktuális nyelven."""
    if defn is None:
        return ""
    if LANG != "hu":
        did = defn.get("id")
        loc = _table(LANG)["data"].get(did) if did else None
        if loc and field in loc:
            return loc[field]
    return defn.get(field, "")


def pick(v):
    """{"hu": ..., "en": ...} mezőből az aktuális nyelvű; sima szövegnél önmaga."""
    if isinstance(v, dict):
        return v.get(LANG) or v.get("hu") or next(iter(v.values()), "")
    return v or ""


# ---------------------------------------------------------------------------
# fordítás a kiírás pontján: minden képernyőre kerülő szöveg átmegy rajta
_cache = {}
_pats = None
MISSES = set() if os.environ.get("RTS_I18N_LOG") else None  # fejlesztéshez: le nem fordított szövegek
_SEPS = ("  •  ", " • ", "   ", "  ", ", ", ": ")


def _patterns():
    """(konkrét minták, általános minták) – az általánosban a kitöltésen kívül nincs betű."""
    global _pats
    if _pats is None:
        import re
        spec, gen = [], []
        for rx, tpl in _table(LANG).get("patterns", []):
            lit = re.sub(r"\(\.\+\?\)", "", rx)
            (spec if any(c.isalpha() for c in lit.replace("\\", "")) else gen).append((re.compile(rx), tpl))
        _pats = (spec, gen)
    return _pats


def T(text):
    """A kiírt szöveg lefordítása (angolul): pontos egyezés, minta (a kitöltött részek is fordulnak),
    végül elválasztók menti darabolás. Magyarul változatlan."""
    if LANG == "hu" or not text or text.isascii() and not any(c.isalpha() for c in text):
        return text
    hit = _cache.get(text)
    if hit is not None:
        return hit
    res = _translate(text, 0)
    if MISSES is not None and res == text and any(c in "áéíóöőúüűÁÉÍÓÖŐÚÜŰ" for c in text):
        MISSES.add(text)
    if len(_cache) > 20000:
        _cache.clear()
    _cache[text] = res
    return res


def _translate(text, depth):
    ui = _table(LANG)["ui"]
    if text in ui:
        return ui[text]
    st = text.strip()
    if st != text and st in ui:
        return text.replace(st, ui[st])
    if depth > 3:
        return text
    spec, gen = _patterns()
    for rx, tpl in spec:
        m = rx.match(text)
        if m:
            return tpl.format(*[_translate(g, depth + 1) for g in m.groups()])
    for sep in _SEPS:
        if sep in text:
            parts = text.split(sep)
            out = [_translate(p, depth + 1) for p in parts]
            if out != parts:
                return sep.join(out)
    for rx, tpl in gen:
        m = rx.match(text)
        if m:
            res = tpl.format(*[_translate(g, depth + 1) for g in m.groups()])
            if res != text:
                return res
    return text


class TFont:
    """pygame-betűkészlet burkolója: a render/size előtt fordít."""

    def __init__(self, font):
        self._f = font

    def render(self, text, *a, **kw):
        return self._f.render(T(text), *a, **kw)

    def size(self, text):
        return self._f.size(T(text))

    def __getattr__(self, k):
        return getattr(self._f, k)


def reset():
    """Nyelvváltás után a gyorsítótárak ürítése."""
    global _pats
    _cache.clear()
    _pats = None
