"""Biztonságos, verziózott JSON-állapotmentés (a pickle helyett).

A teljes szimuláció objektumgráfját menti (egységek, épületek, játékosok, térkép, gépi ellenfelek, kampány-rendező),
a megosztott hivatkozásokkal együtt, hogy betöltés után a játék bitre ugyanúgy folytatódjon.

Biztonság: betöltéskor csak a szimulációs modulok (``SIM_MODULES``) saját osztályai jöhetnek létre, ``__new__``-val,
konstruktor- vagy egyéb kódfuttatás nélkül; minden más típus beépített (dict, list, tuple, set, bytes …). Idegen
mentés így nem futtathat kódot.

A játékadatok (GameData) nem kerülnek a mentésbe: az adatfájlokból származó dict/list objektumokra útvonallal
hivatkozunk (``{"$data": [...]}``), és betöltéskor a futó adatkészlet ugyanazon objektumára kötjük vissza – pontosan
úgy, ahogy megszakítás nélküli futásban is meg vannak osztva.

Formátum (``schema`` 1):
    {"format": "totu-snapshot", "schema": 1, "root": <érték>, "objects": [{"c": "modul.Osztály", "f": {...}}, ...]}
Érték-kódolás: JSON-primitív | lista | dict (csupa nem-$ szöveges kulccsal) | jelölő-objektum:
    $o  objektum (index az objects táblában)      $t  tuple       $s  set        $fs  frozenset
    $d  dict nem-szöveges kulcsokkal [[k, v],...]  $b  bytes (b64) $ba bytearray $q   deque (+ "m": maxlen)
    $i/$v  megosztott tároló első előfordulása     $c  hivatkozás egy korábban megadott megosztott tárolóra
    $data  útvonal a játékadatokban                 $rng  random.Random állapota   $gamedata  a futó GameData
"""
import base64
import collections
import importlib
import inspect
import random

FORMAT = "totu-snapshot"
SCHEMA = 1
SIM_MODULES = ("rts.game", "rts.entities", "rts.ai", "rts.heroes", "rts.campaign", "rts.mission", "rts.world",
               "rts.points")

_CONTAINERS = (dict, list, set, bytearray, collections.deque)
_DATA_SKIP = ("scripts", "warnings", "mods", "mod_ids")


class SnapshotError(ValueError):
    pass


def _allowed_classes():
    out = {}
    for name in SIM_MODULES:
        mod = importlib.import_module(name)
        for _, cls in inspect.getmembers(mod, inspect.isclass):
            if cls.__module__ == name:
                out[f"{name}.{cls.__qualname__}"] = cls
    return out


def _data_paths(data):
    """id(objektum) → útvonal minden dict/list tárolóra a játékadatokban (determinisztikus bejárás)."""
    paths = {}
    if data is None:
        return paths
    stack = []
    for attr in sorted(vars(data)):
        if attr in _DATA_SKIP or attr.startswith("_"):
            continue
        stack.append(((attr,), getattr(data, attr)))
    stack.reverse()
    while stack:
        path, obj = stack.pop()
        if not isinstance(obj, (dict, list)) or id(obj) in paths:
            continue
        paths[id(obj)] = path
        items = obj.items() if isinstance(obj, dict) else enumerate(obj)
        children = [(path + (k,), v) for k, v in items if isinstance(v, (dict, list)) and isinstance(k, (str, int))]
        stack.extend(reversed(children))
    return paths


def _resolve_data(data, path):
    if (data is None or not isinstance(path, list) or not path or not isinstance(path[0], str)
            or path[0].startswith("_") or path[0] in _DATA_SKIP or path[0] not in vars(data)):
        raise SnapshotError("Sérült mentés: érvénytelen adathivatkozás.")
    try:
        obj = vars(data)[path[0]]
        for k in path[1:]:
            obj = obj[k]
    except (KeyError, IndexError, TypeError):
        raise SnapshotError("A mentés olyan játékadatra hivatkozik, ami nincs meg: " + "/".join(map(str, path)))
    if not isinstance(obj, (dict, list)):
        raise SnapshotError("Sérült mentés: az adathivatkozás nem adattárolóra mutat.")
    return obj


# ----------------------------------------------------------------------------------------------------------------
# mentés

def dump(root, data):
    """A ``root`` gráfot JSON-képes dict-té alakítja. ``data`` a futó GameData (nem kerül a mentésbe)."""
    allowed = {cls: name for name, cls in _allowed_classes().items()}
    dpaths = _data_paths(data)

    # 1. menet: mely tárolókra mutat egynél több hivatkozás
    refs = collections.Counter()
    seen_obj = set()
    stack = [root]
    while stack:
        v = stack.pop()
        if isinstance(v, (str, int, float, bool, type(None), bytes)):
            continue
        if isinstance(v, _CONTAINERS):
            if id(v) in dpaths:
                continue
            refs[id(v)] += 1
            if refs[id(v)] > 1:
                continue
            if isinstance(v, dict):
                stack.extend(v.keys())
                stack.extend(v.values())
            elif not isinstance(v, bytearray):
                stack.extend(v)
        elif isinstance(v, (tuple, frozenset)):
            stack.extend(v)
        elif type(v) in allowed:
            if id(v) not in seen_obj:
                seen_obj.add(id(v))
                stack.extend(vars(v).values())
        elif isinstance(v, random.Random):
            pass
        elif data is not None and v is data:
            pass
        else:
            raise SnapshotError(f"Nem menthető típus a játékállapotban: {type(v).__module__}.{type(v).__qualname__}")

    shared = {i for i, n in refs.items() if n > 1}
    shared_ids = {}
    obj_index = {}
    objects = []
    queue = collections.deque()

    def enc(v):
        if v is None or isinstance(v, (bool, int, str)):
            return v
        if isinstance(v, float):
            return v
        vid = id(v)
        if isinstance(v, _CONTAINERS) and vid in dpaths:
            return {"$data": list(dpaths[vid])}
        if vid in shared:
            if vid in shared_ids:
                return {"$c": shared_ids[vid]}
            shared_ids[vid] = len(shared_ids)
            return {"$i": shared_ids[vid], "$v": enc_inner(v)}
        return enc_inner(v)

    def enc_inner(v):
        if isinstance(v, list):
            return [enc(x) for x in v]
        if isinstance(v, dict):
            if all(isinstance(k, str) and not k.startswith("$") for k in v):
                return {k: enc(x) for k, x in v.items()}
            return {"$d": [[enc(k), enc(x)] for k, x in v.items()]}
        if isinstance(v, tuple):
            return {"$t": [enc(x) for x in v]}
        if isinstance(v, set):
            return {"$s": [enc(x) for x in v]}
        if isinstance(v, frozenset):
            return {"$fs": [enc(x) for x in v]}
        if isinstance(v, bytearray):
            return {"$ba": base64.b64encode(bytes(v)).decode("ascii")}
        if isinstance(v, bytes):
            return {"$b": base64.b64encode(v).decode("ascii")}
        if isinstance(v, collections.deque):
            return {"$q": [enc(x) for x in v], "m": v.maxlen}
        if isinstance(v, random.Random):
            return {"$rng": enc(v.getstate())}
        if data is not None and v is data:
            return {"$gamedata": 1}
        cls = type(v)
        if cls in allowed:
            n = obj_index.get(id(v))
            if n is None:
                n = obj_index[id(v)] = len(objects)
                objects.append(None)
                queue.append((n, v))
            return {"$o": n}
        raise SnapshotError(f"Nem menthető típus: {cls.__module__}.{cls.__qualname__}")

    out_root = enc(root)
    while queue:
        n, v = queue.popleft()
        fields = {}
        for k, x in vars(v).items():
            fields[k] = enc(x)
        objects[n] = {"c": allowed[type(v)], "f": fields}
    return {"format": FORMAT, "schema": SCHEMA, "root": out_root, "objects": objects}


# ----------------------------------------------------------------------------------------------------------------
# betöltés

def load(doc, data):
    """A ``dump`` kimenetéből visszaépíti a gráfot; csak engedélyezett osztályokat hoz létre."""
    if not isinstance(doc, dict) or doc.get("format") != FORMAT:
        raise SnapshotError("Ez nem TOTU-mentés.")
    if doc.get("schema") != SCHEMA:
        raise SnapshotError(f"Ismeretlen mentésséma: {doc.get('schema')} (ez a játék: {SCHEMA}).")
    allowed = _allowed_classes()
    table = doc.get("objects")
    if not isinstance(table, list):
        raise SnapshotError("Sérült mentés: hiányzik az objektumtábla.")
    objs = []
    for entry in table:
        cls = allowed.get(entry.get("c")) if isinstance(entry, dict) else None
        if cls is None:
            raise SnapshotError(f"Nem engedélyezett osztály a mentésben: {entry.get('c') if isinstance(entry, dict) else entry!r}")
        objs.append(cls.__new__(cls))
    shared = {}

    def dec(v):
        if v is None or isinstance(v, (bool, int, float, str)):
            return v
        if isinstance(v, list):
            return [dec(x) for x in v]
        if not isinstance(v, dict):
            raise SnapshotError("Sérült mentés: ismeretlen érték.")
        if len(v) and next(iter(v)).startswith("$"):
            if "$o" in v:
                return objs[v["$o"]]
            if "$c" in v:
                return shared[v["$c"]]
            if "$i" in v:
                return dec_shared(v["$i"], v["$v"])
            if "$data" in v:
                return _resolve_data(data, v["$data"])
            if "$t" in v:
                return tuple(dec(x) for x in v["$t"])
            if "$s" in v:
                return {dec(x) for x in v["$s"]}
            if "$fs" in v:
                return frozenset(dec(x) for x in v["$fs"])
            if "$d" in v:
                return {dec(k): dec(x) for k, x in v["$d"]}
            if "$ba" in v:
                return bytearray(base64.b64decode(v["$ba"]))
            if "$b" in v:
                return base64.b64decode(v["$b"])
            if "$q" in v:
                return collections.deque((dec(x) for x in v["$q"]), v.get("m"))
            if "$rng" in v:
                r = random.Random()
                r.setstate(dec(v["$rng"]))
                return r
            if "$gamedata" in v:
                return data
            raise SnapshotError("Sérült mentés: ismeretlen jelölő " + next(iter(v)))
        return {k: dec(x) for k, x in v.items()}

    def dec_shared(i, inner):
        # a tárolót előbb regisztráljuk, hogy a saját belsejéből jövő hivatkozás is feloldható legyen
        if isinstance(inner, list):
            box = shared[i] = []
            box.extend(dec(x) for x in inner)
            return box
        if isinstance(inner, dict) and not (len(inner) and next(iter(inner)).startswith("$")):
            box = shared[i] = {}
            for k, x in inner.items():
                box[k] = dec(x)
            return box
        if isinstance(inner, dict) and "$d" in inner:
            box = shared[i] = {}
            for k, x in inner["$d"]:
                box[dec(k)] = dec(x)
            return box
        val = shared[i] = dec(inner)
        return val

    root = dec(doc.get("root"))
    for obj, entry in zip(objs, table):
        fields = entry.get("f")
        if not isinstance(fields, dict):
            raise SnapshotError("Sérült mentés: hibás objektummezők.")
        obj.__dict__.update({k: dec(x) for k, x in fields.items()})
    return root
