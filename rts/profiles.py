"""Játékosprofilok (Armies of Exigo módra): név, címer-faj, beállítások,
statisztikák és profilonkénti kampány-haladás. Helye: saves/profiles/<név>/."""
import hashlib
import json
import os
import re
import shutil
import time

from .data import ROOT
from .savegame import _replace_file

SAVES = os.path.join(ROOT, "saves")
PROFILES = os.path.join(SAVES, "profiles")
LAST = os.path.join(SAVES, "last_profile.txt")

DEFAULT_SETTINGS = {
    "game_speed": 1.0,      # alapértelmezett játéksebesség
    "scroll_speed": 900,    # kamera-görgetés px/mp
    "edge_scroll": True,
    "show_fps": True,
    "fps_limit": 60,        # képkocka-korlát (30 / 60 / 120 / 0 = korlát nélkül)
    "view_scale": 0.75,     # kamera-távolság (1 = közel … 0,55 = messze); játék közben az egérgörgővel
    "show_clock": True,     # jobb fent: játékidő
    "show_speed": True,     # jobb fent: sebesség −/+
    "show_help_hint": True,  # jobb fent: „F1: súgó”
    "shadows": True,        # vetett árnyékok (egységek, épületek, fák, kövek)
    "status_pulse": True,   # az állapotok (ég, mérgezett, lassult…) pulzáló színe a figurán
    "status_fx": True,      # az állapotok animációja a figurán (láng, buborék, jégkristály, Z Z Z…)
    "ambient": True,        # környezeti részecskék (szentjánosbogár, hó, por, pára)
    "water_anim": True,     # mozgó víz, part menti hab
    "fullscreen": False,
    "resolution": "1280x760",
    "difficulty": "normal",  # easy | normal | hard – a gépi ellenfél agressziója
}


def _safe(name):
    safe = re.sub(r"[^\w\- ]", "_", name.strip())[:24].rstrip() or "Névtelen"
    digest = hashlib.sha256(name.encode("utf-8")).hexdigest()[:24]
    return f"{safe}-{digest}"


class Profile:
    def __init__(self, name, data=None):
        self.name = name
        self.dir = os.path.join(PROFILES, _safe(name))
        self.data = data or {
            "name": name, "created": time.time(), "emblem": "ursan",
            "settings": dict(DEFAULT_SETTINGS),
            "stats": {"games": 0, "wins": 0, "losses": 0, "play_time": 0.0, "by_faction": {}, "kills": 0},
            "campaign": {},
        }
        for k, v in DEFAULT_SETTINGS.items():
            self.data.setdefault("settings", {}).setdefault(k, v)

    @property
    def settings(self):
        return self.data["settings"]

    @property
    def saves_dir(self):
        return os.path.join(self.dir, "saves")

    def save(self):
        content = json.dumps(self.data, ensure_ascii=False, indent=1).encode("utf-8")
        os.makedirs(self.dir, exist_ok=True)
        _replace_file(os.path.join(self.dir, "profile.json"), content)
        os.makedirs(SAVES, exist_ok=True)
        _replace_file(LAST, self.name.encode("utf-8"))


    def record_game(self, faction, result, seconds, kills=0):
        st = self.data["stats"]
        st["games"] += 1
        st["play_time"] += seconds
        st["kills"] = st.get("kills", 0) + kills
        bf = st["by_faction"].setdefault(faction, {"games": 0, "wins": 0})
        bf["games"] += 1
        if result == "win":
            st["wins"] += 1
            bf["wins"] += 1
        elif result == "lose":
            st["losses"] += 1
        self.save()

    # kampány-haladás (a campaign modul ezt használja)
    def progress(self):
        return self.data.setdefault("campaign", {})

    def flags(self):
        """A kampányban hozott döntések (jelzők) – a későbbi küldetések ezekhez igazodnak."""
        return dict(self.data.get("flags", {}))

    def set_flags(self, new):
        self.data.setdefault("flags", {}).update(new)

    def heroes(self):
        """Hadjárati hősök fejlődése (szint, tapasztalat, képességek, tárgyak) hős-azonosítónként."""
        return dict(self.data.get("heroes", {}))

    def set_heroes(self, states):
        self.data.setdefault("heroes", {}).update(states)
        self.save()

    def complete_mission(self, campaign_id, mission_id, hero_states=None):
        if hero_states is not None:
            self.data.setdefault("heroes", {}).update(hero_states)
        c = self.progress().setdefault(campaign_id, {"completed": []})
        if mission_id not in c["completed"]:
            c["completed"].append(mission_id)
        self.save()

    def novellas_done(self):
        """A novella-pályák saját haladása (független a kampánytól)."""
        return self.data.setdefault("novellak", [])

    def complete_novella(self, nid):
        done = self.novellas_done()
        if nid not in done:
            done.append(nid)
        self.save()


def _validate_profile(data):
    if not isinstance(data, dict):
        raise ValueError("A profil gyökere nem objektum.")
    if "name" in data and not isinstance(data["name"], str):
        raise ValueError("Hibás profilnév.")
    for key in ("settings", "stats", "campaign", "flags", "heroes"):
        if key in data and not isinstance(data[key], dict):
            raise ValueError(f"Hibás profilmező: {key}")
    settings = data.get("settings", {})
    for key, default in DEFAULT_SETTINGS.items():
        if key in settings and not isinstance(settings[key], type(default)):
            if not (type(default) in (int, float) and type(settings[key]) in (int, float)):
                raise ValueError(f"Hibás beállítás: {key}")
    stats = data.get("stats", {})
    for key in ("games", "wins", "losses", "play_time", "kills"):
        if key in stats and type(stats[key]) not in (int, float):
            raise ValueError(f"Hibás statisztika: {key}")
    if not isinstance(stats.get("by_faction", {}), dict):
        raise ValueError("Hibás fajstatisztika.")
    for value in stats.get("by_faction", {}).values():
        if not isinstance(value, dict) or any(type(value.get(k, 0)) not in (int, float) for k in ("games", "wins")):
            raise ValueError("Hibás fajstatisztika.")
    for value in data.get("campaign", {}).values():
        if not isinstance(value, dict) or not isinstance(value.get("completed", []), list):
            raise ValueError("Hibás kampányhaladás.")
    if not isinstance(data.get("novellak", []), list):
        raise ValueError("Hibás novella-haladás.")


def list_profiles():
    out = []
    if os.path.isdir(PROFILES):
        for d in sorted(os.listdir(PROFILES)):
            p = os.path.join(PROFILES, d, "profile.json")
            if os.path.isfile(p):
                try:
                    with open(p, encoding="utf-8") as f:
                        data = json.load(f)
                    _validate_profile(data)
                    profile = Profile(data.get("name", d), data)
                    profile.dir = os.path.dirname(p)
                    out.append(profile)
                except (OSError, ValueError) as e:
                    print(f"[profil] {p}: {e}")
    return out


def create(name, emblem="ursan"):
    p = Profile(name)
    p.data["emblem"] = emblem
    os.makedirs(PROFILES, exist_ok=True)
    os.mkdir(p.dir)
    p.save()
    return p


def delete(profile):
    if os.path.isdir(profile.dir):
        shutil.rmtree(profile.dir)
    if os.path.isfile(LAST):
        with open(LAST, encoding="utf-8") as f:
            if f.read().strip() == profile.name:
                os.remove(LAST)


def last_profile():
    try:
        with open(LAST, encoding="utf-8") as f:
            name = f.read().strip()
    except OSError:
        return None
    for p in list_profiles():
        if p.name == name:
            return p
    return None


def ensure_profile():
    """Az utoljára használt profil, vagy egy alapértelmezett, ha még nincs egy sem."""
    p = last_profile()
    if p:
        return p
    ps = list_profiles()
    if ps:
        return ps[0]
    return create("Hadvezér")
