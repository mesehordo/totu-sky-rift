"""Mentés és betöltés.

A teljes szimuláció-állapotot (térkép, játékosok, egységek parancsokkal,
épületek, gépi ellenfelek, kampány-rendező) verziózott JSON-ba menti (``rts/snapshot.py``, gzip);
a játékadatokat (GameData) nem – betöltéskor a futó adatkészlet kapcsolódik vissza. Mentési hely:
saves/profiles/<profil>/saves/<hely>.tsav + .json (fejléc) + .png (bélyegkép).

A betöltés nem futtat kódot (csak engedélyezett osztályok jönnek létre). A régi, pickle-alapú ``.sav`` mentéseket
a játék nem tölti be; ezeket egyszer át kell alakítani: ``python tools/convert_saves.py`` (csak saját mentéssel!).
"""
import gzip
import hashlib
import io
import json
import os
import random
import re
import sys
import tempfile
import time

SAVE_VERSION = 2          # 2: JSON-állapot (.tsav); 1: pickle (.sav, csak átalakítással)
SAVE_EXT = ".tsav"
LEGACY_EXT = ".sav"


def _slot_file(saves_dir, slot):
    safe = re.sub(r"[^\w\- ]", "_", slot)[:40].strip() or "mentes"
    digest = hashlib.sha256(slot.encode("utf-8")).hexdigest()[:24]
    return os.path.join(saves_dir, f"{safe}-{digest}")


def _save_base(saves_dir, slot):
    legacy = os.path.join(saves_dir, re.sub(r"[^\w\- ]", "_", slot)[:40] or "mentes")
    if os.path.isfile(legacy + ".json"):
        try:
            with open(legacy + ".json", encoding="utf-8") as f:
                meta = json.load(f)
            if isinstance(meta, dict) and meta.get("slot") == slot:
                return legacy
        except (OSError, ValueError):
            pass
    return _slot_file(saves_dir, slot)


def _replace_file(path, content):
    tmp = None
    try:
        with tempfile.NamedTemporaryFile(dir=os.path.dirname(path) or ".", delete=False) as f:
            tmp = f.name
            f.write(content)
            f.flush()
            os.fsync(f.fileno())
        os.replace(tmp, path)
    finally:
        if tmp is not None and os.path.isfile(tmp):
            os.unlink(tmp)


def save_game(game, saves_dir, slot, cam=(0, 0), seen=(), thumb_surface=None, extra=None):
    import pygame
    os.makedirs(saves_dir, exist_ok=True)
    base = _save_base(saves_dir, slot)
    write_state(base + SAVE_EXT, game, cam, seen, extra)
    if os.path.isfile(base + LEGACY_EXT):  # az azonos nevű régi mentést az új váltja
        os.remove(base + LEGACY_EXT)
    me = game.players[0]
    meta = {
        "slot": slot, "version": SAVE_VERSION, "format": "tsav", "date": time.strftime("%Y-%m-%d %H:%M"),
        "game_time": round(game.time), "player": me.faction["name"],
        "factions": [p.faction["name"] for p in game.players],
        "mission": game.director.m.get("title") if game.director is not None else None,
        "campaign": (extra or {}).get("campaign"), "mission_id": (extra or {}).get("mission_id"),
    }
    _replace_file(base + ".json", json.dumps(meta, ensure_ascii=False, indent=1).encode("utf-8"))
    if thumb_surface is not None:
        preview = io.BytesIO()
        pygame.image.save(pygame.transform.smoothscale(thumb_surface, (192, 114)), preview, "preview.png")
        _replace_file(base + ".png", preview.getvalue())
    return base


def write_state(path, game, cam=(0, 0), seen=(), extra=None):
    """A játékállapot JSON-mentése (gzip) – a mentés, az átalakító és a tesztek közös útja."""
    from . import snapshot
    data = game.data
    doc = snapshot.dump({"version": SAVE_VERSION, "game": game, "cam": tuple(cam), "seen": set(seen),
                         "extra": extra or {}, "rng": random.getstate(), "mod_ids": tuple(data.mod_ids)}, data)
    text = json.dumps(doc, ensure_ascii=False, separators=(",", ":"))
    _replace_file(path, gzip.compress(text.encode("utf-8"), compresslevel=6))


def read_state(path, data):
    from . import snapshot
    try:
        with open(path, "rb") as f:
            doc = json.loads(gzip.decompress(f.read()).decode("utf-8"))
    except (OSError, EOFError, ValueError) as e:
        raise ValueError(f"Sérült mentésfájl: {e}")
    try:
        return snapshot.load(doc, data)
    except snapshot.SnapshotError:
        raise
    except Exception as e:  # sérült vagy kézzel szerkesztett tartalom
        raise ValueError(f"Sérült mentés: {type(e).__name__}: {e}")


def list_saves(saves_dir):
    out = []
    if not os.path.isdir(saves_dir):
        return out
    for fn in os.listdir(saves_dir):
        if fn.endswith(".json"):
            base = os.path.join(saves_dir, fn[:-5])
            legacy = not os.path.isfile(base + SAVE_EXT) and os.path.isfile(base + LEGACY_EXT)
            if os.path.isfile(base + SAVE_EXT) or legacy:
                try:
                    with open(base + ".json", encoding="utf-8") as f:
                        meta = json.load(f)
                except (OSError, ValueError):
                    continue
                if not isinstance(meta, dict) or not isinstance(meta.get("slot"), str):
                    continue
                factions = meta.setdefault("factions", [])
                game_time = meta.setdefault("game_time", 0)
                if (not isinstance(factions, list) or not all(isinstance(f, str) for f in factions)
                        or type(game_time) is not int or game_time < 0
                        or not isinstance(meta.setdefault("date", ""), str)
                        or (meta.get("mission") is not None and not isinstance(meta["mission"], str))):
                    continue
                try:
                    meta["mtime"] = os.path.getmtime(base + (LEGACY_EXT if legacy else SAVE_EXT))
                except OSError:
                    continue
                meta["path"] = base
                meta["legacy"] = legacy
                out.append(meta)
    out.sort(key=lambda m: -m["mtime"])
    return out


def _upgrade(g):
    """Régebbi mentések: a később bevezetett tulajdonságok pótlása alapértékkel (irányok, méreg, sebesség…)."""
    if not hasattr(g, "speed"):
        g.speed = 1.0
    for key in ("found", "spikes", "pond"):
        if not hasattr(g, key):
            setattr(g, key, bytearray(g.map.w * g.map.h))
    if not hasattr(g, "spike_targets"):
        g.spike_targets = {
            i: g.make_spike(g.players[owner - 1], i % g.map.w, i // g.map.w)
            for i, owner in enumerate(g.spikes) if owner
        }
    if not hasattr(g, "points"):
        g.points = []
    rescue_research = "imp_lelekhorgony_mentes"
    rescue_ability = "lelekhorgony_mentes"
    new_research = {rescue_research, *g.data.game.get("summon_research", {})}
    for b in g.buildings:
        current = g.data.defs.get(b.defn["id"], {})
        existing = b.defn.get("researches", ())
        added = [rid for rid in current.get("researches", ()) if rid in new_research and rid not in existing]
        if added:
            b.defn = {**b.defn, "researches": [*existing, *added]}
        for k, v in (("builder_in", None), ("res", None), ("miners", []), ("mine_n", 0), ("mine_acc", 0.0),
                     ("partner", None), ("gate_q", []), ("gate_t", 0), ("gate_cd", 0)):
            if not hasattr(b, k):
                setattr(b, k, list(v) if isinstance(v, list) else v)
    for p in g.players:
        if not hasattr(p, "stats"):
            p.stats = {"units_killed": 0, "buildings_killed": 0, "units_lost": 0, "buildings_lost": 0,
                       "buildings_built": 0, "killed_value": 0, "lost_value": 0}
        if not hasattr(p, "spent"):
            p.spent = {}
        if not hasattr(p, "timeline"):
            p.timeline = []
        for k, v in (("path", None), ("ascended", False), ("rules", {}), ("forced_path", None), ("ally", None),
                     ("forced_ally", None)):
            if not hasattr(p, k):
                setattr(p, k, v if not isinstance(v, dict) else dict(v))
    for u in g.units:
        current = g.data.defs.get(u.defn["id"], {})
        if (current.get("ability_research", {}).get(rescue_ability) == rescue_research
                and rescue_ability not in u.defn.get("abilities", ())):
            u.defn = {**u.defn, "abilities": [*u.defn.get("abilities", ()), rescue_ability],
                      "ability_research": {**u.defn.get("ability_research", {}), rescue_ability: rescue_research}}
        for k in ("poison", "rider_id", "burn", "fear_from", "marked_by"):
            if not hasattr(u, k):
                setattr(u, k, None)
        for k in ("frozen_until", "shock_cd", "wet_until", "rooted_until", "asleep_until", "feared_until",
                  "blind_until", "silenced_until", "marked_until", "sunder_until"):
            if not hasattr(u, k):
                setattr(u, k, 0.0)


def load_game(base, data):
    if not os.path.isfile(base + SAVE_EXT):
        if os.path.isfile(base + LEGACY_EXT):
            raise ValueError("Régi formátumú mentés. Alakítsd át: python tools/convert_saves.py")
        raise ValueError("A mentésfájl nem található.")
    obj = read_state(base + SAVE_EXT, data)
    return _finish_load(obj, data)


def load_legacy_pickle(base, data):
    """CSAK az átalakítónak: régi pickle-mentés beolvasása. Tetszőleges kódot futtathat – idegen fájlra tilos."""
    import pickle
    old = sys.getrecursionlimit()
    sys.setrecursionlimit(max(old, 20000))
    try:
        with open(base + LEGACY_EXT, "rb") as f:
            obj = pickle.load(f)
    finally:
        sys.setrecursionlimit(old)
    if obj.get("version") != 1:
        raise ValueError("Ismeretlen régi mentésverzió.")
    obj["version"] = SAVE_VERSION
    return _finish_load(obj, data)


def _finish_load(obj, data):
    if not isinstance(obj, dict) or obj.get("version") != SAVE_VERSION:
        raise ValueError("Ez a mentés a játék egy másik verziójával készült.")
    saved_mods = obj.get("mod_ids")
    if saved_mods is not None and tuple(saved_mods) != tuple(data.mod_ids):
        expected = ", ".join(saved_mods) or "alapjáték"
        current = ", ".join(data.mod_ids) or "alapjáték"
        raise ValueError(f"Eltér a mentés modlistája. Mentés: {expected}. Aktív: {current}.")
    g = obj["game"]
    missing = set()
    for b in g.buildings:
        for item in b.queue:
            definitions = data.researches if item["kind"] == "research" else data.defs
            if item["id"] not in definitions:
                missing.add(item["id"])
    for u in g.units:
        if getattr(u, "morph", None) and u.morph[0] not in data.defs:
            missing.add(u.morph[0])
    if missing:
        raise ValueError("A mentéshez hiányzó játékadatok: " + ", ".join(sorted(missing)))
    g.data = data
    _upgrade(g)
    if obj.get("rng"):
        random.setstate(obj["rng"])
    return g, obj.get("cam", (0, 0)), obj.get("seen", set()), obj.get("extra", {})


def delete_save(base):
    for ext in (SAVE_EXT, LEGACY_EXT, ".json", ".png"):
        if os.path.isfile(base + ext):
            os.remove(base + ext)
