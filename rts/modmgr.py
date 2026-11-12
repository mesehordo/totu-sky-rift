"""Mod-kezelő: a telepített modok listája, az engedélyezett modok és sorrendjük, függőség-ellenőrzés.

A modok a ``mods/<mappa>/`` alatt vannak, mindegyikben egy ``mod.json``:

    {"name": "Példa mod", "label": "Jégkorszak", "version": "1.0", "author": "…",
     "description": "…", "dependencies": ["masik_mod"], "incompatible": ["harmadik_mod"]}

Az engedélyezett modok listája és sorrendje a ``saves/mods.json``-ban van (a játékos beállítása, mint a 0 A.D.
mod-választójában). A sorrend a betöltési sorrend: a később betöltött mod felülírja a korábbit.
Ha a fájl még nincs, a ``mod.json`` régi ``enabled`` / ``priority`` mezői adják a kiinduló állapotot.
"""
import json
import os

from .data import MODS_DIR, ROOT

CONFIG = os.path.join(ROOT, "saves", "mods.json")


def _count(path, sub):
    p = os.path.join(path, sub)
    n = 0
    if os.path.isdir(p):
        for _, _, files in os.walk(p):
            n += len(files)
    return n


def list_mods():
    """Minden telepített mod adatai (mappa szerinti ábécérendben)."""
    out = []
    if not os.path.isdir(MODS_DIR):
        return out
    for folder in sorted(os.listdir(MODS_DIR)):
        path = os.path.join(MODS_DIR, folder)
        mp = os.path.join(path, "mod.json")
        if not os.path.isfile(mp):
            continue
        m = {"id": folder, "path": path, "name": folder, "label": "", "version": "", "author": "",
             "description": "", "dependencies": [], "incompatible": [], "error": None,
             "enabled_default": False, "priority": 0}
        try:
            with open(mp, encoding="utf-8") as f:
                meta = json.load(f)
            if not isinstance(meta, dict):
                raise ValueError("A manifest gyökere nem objektum.")
            for key in ("name", "label", "version", "author", "description"):
                if key in meta and not isinstance(meta[key], str):
                    raise ValueError(f"Hibás szövegmező: {key}")
            for key in ("dependencies", "incompatible"):
                value = meta.get(key, [])
                if not isinstance(value, list) or not all(isinstance(v, str) for v in value):
                    raise ValueError(f"Hibás modlista: {key}")
            if type(meta.get("priority", 0)) is not int:
                raise ValueError("Hibás prioritás.")
            if type(meta.get("enabled", False)) is not bool:
                raise ValueError("Hibás enabled jelző.")
            m.update({k: meta.get(k, m[k]) for k in ("name", "label", "version", "author", "description")})
            m["dependencies"] = list(meta.get("dependencies") or [])
            m["incompatible"] = list(meta.get("incompatible") or [])
            m["enabled_default"] = bool(meta.get("enabled", False))
            m["priority"] = meta.get("priority", 0)
        except (OSError, ValueError) as e:
            m["error"] = f"hibás mod.json: {e}"
        parts = []
        for sub, nm in (("factions", "nép-fájl"), ("campaigns", "kampány-fájl"), ("assets", "kép")):
            c = _count(path, sub)
            if c:
                parts.append(f"{c} {nm}")
        loose = [f for f in os.listdir(path) if f.endswith(".json") and f != "mod.json"]
        if loose:
            parts.append(f"{len(loose)} adatfájl")
        if os.path.isfile(os.path.join(path, "script.py")):
            parts.append("Python-szkript")
        m["contents"] = ", ".join(parts) or "üres"
        m["has_script"] = os.path.isfile(os.path.join(path, "script.py"))
        out.append(m)
    return out


def enabled_list():
    """Az engedélyezett modok mappanevei, betöltési sorrendben (csak a ténylegesen telepítettek)."""
    mods = {m["id"]: m for m in list_mods()}
    if os.path.isfile(CONFIG):
        try:
            with open(CONFIG, encoding="utf-8") as f:
                config = json.load(f)
            ids = config.get("enabled", []) if isinstance(config, dict) else None
            if not isinstance(ids, list) or not all(isinstance(i, str) for i in ids):
                raise ValueError("hibás modlista")
            return [i for i in ids if i in mods and not mods[i]["error"]]
        except (OSError, ValueError):
            pass
    return [m["id"] for m in sorted(mods.values(), key=lambda m: (m["priority"], m["id"]))
            if m["enabled_default"] and not m["error"]]


def save_enabled(ids):
    os.makedirs(os.path.dirname(CONFIG), exist_ok=True)
    with open(CONFIG, "w", encoding="utf-8") as f:
        json.dump({"enabled": list(ids)}, f, ensure_ascii=False, indent=1)


def problems(ids, mods=None):
    """A megadott (sorrendes) lista hibái: hiányzó vagy később betöltött függőség, összeférhetetlenség."""
    mods = mods or {m["id"]: m for m in list_mods()}
    out = []
    pos = {i: k for k, i in enumerate(ids)}
    for i in ids:
        m = mods.get(i)
        if m is None:
            out.append(f"{i}: nincs telepítve")
            continue
        if m["error"]:
            out.append(f"{m['name']}: {m['error']}")
        for dep in m["dependencies"]:
            if dep not in mods:
                out.append(f"{m['name']}: hiányzó függőség: {dep}")
            elif dep not in pos:
                out.append(f"{m['name']}: engedélyezni kell ezt is: {mods[dep]['name']}")
            elif pos[dep] > pos[i]:
                out.append(f"{m['name']}: a(z) {mods[dep]['name']} modnak előtte kell betöltődnie")
        for bad in m["incompatible"]:
            if bad in pos:
                out.append(f"{m['name']}: nem fér össze ezzel: {mods.get(bad, {}).get('name', bad)}")
    return out


def auto_order(ids, mods=None):
    """A lista olyan sorrendben, hogy minden függőség előbb töltődjön be (a meglévő sorrendet megtartva)."""
    mods = mods or {m["id"]: m for m in list_mods()}
    out, seen = [], set()

    def add(i, stack=()):
        if i in seen or i in stack or i not in ids:
            return
        for dep in mods.get(i, {}).get("dependencies", []):
            add(dep, stack + (i,))
        seen.add(i)
        out.append(i)
    for i in ids:
        add(i)
    return out
