"""Játékadatok betöltése, modok összefésülése és validálás.

Az alapadatok a ``data/`` mappában vannak. Minden ``mods/<név>/`` mappa
ugyanazokkal a relatív útvonalakkal tartalmazhat JSON fájlokat, ezek mélyen
összefésülődnek az alapadatokkal (``null`` = kulcs törlése). Egy mod új fájlt
is hozhat (pl. ``factions/uj_faj.json``), és opcionálisan ``script.py``-t
hookokkal: ``on_load(data)``, ``on_game_start(game)``, ``on_tick(game, dt)``.
"""
import copy
import importlib.util
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA_DIR = os.path.join(ROOT, "data")
MODS_DIR = os.path.join(ROOT, "mods")


def deep_merge(base, over):
    for k, v in over.items():
        if v is None:
            base.pop(k, None)
        elif isinstance(v, dict) and isinstance(base.get(k), dict):
            deep_merge(base[k], v)
        else:
            base[k] = copy.deepcopy(v)
    return base


def _read_json(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def _collect(root):
    """{relatív útvonal: json} minden .json fájlra (a mod.json kivételével)."""
    out = {}
    for dirpath, _, files in os.walk(root):
        for fn in sorted(files):
            if fn.endswith(".json") and fn != "mod.json":
                full = os.path.join(dirpath, fn)
                rel = os.path.relpath(full, root).replace("\\", "/")
                out[rel] = _read_json(full)
    return out


class GameData:
    def __init__(self):
        self.files = {}
        self.mods = []        # (név, leírás) a betöltött modokról
        self.mod_ids = []     # a betöltött modok mappanevei, betöltési sorrendben
        self.scripts = []     # betöltött script modulok
        self.warnings = []
        self.defs = {}        # id -> egység/épület definíció
        self.researches = {}  # id -> fejlesztés
        self.factions = {}

    @property
    def game(self):
        return self.files["game.json"]

    @property
    def abilities(self):
        return self.files.get("abilities.json", {})

    def faction_list(self, include_neutral=False):
        """Játszható fajok sorrendben (a semleges szörny-"faj" csak kérésre). A szövetséges népek és a rejtett
        (csak hadjáratbeli) fajok nem játszhatók; include_neutral=True mellett ezek is benne vannak (sprite-gyártás)."""
        return sorted((f for f in self.factions.values()
                       if include_neutral or not (f.get("neutral") or f.get("ally") or f.get("hidden"))),
                      key=lambda f: f.get("order", 99))

    def allies(self):
        """A szövetséges népek (Diplomácia csarnoka), sorrendben."""
        return sorted((f for f in self.factions.values() if f.get("ally")), key=lambda f: f.get("order", 99))

    @property
    def items(self):
        return self.files.get("items.json", {})

    def neutral_faction(self):
        for f in self.factions.values():
            if f.get("neutral"):
                return f
        return None

    def hook(self, name, *args):
        for mod in self.scripts:
            fn = getattr(mod, name, None)
            if fn:
                try:
                    fn(*args)
                except Exception as e:  # egy hibás mod ne döntse le a játékot
                    print(f"[mod] {mod.__name__}.{name} hiba: {e}")

    def _apply_elements(self):
        """Elemi sebzés (data/elements.json): a fegyverek és a képességek "element" mezője, a hatások beállítása."""
        el = self.files.get("elements.json") or {}
        self.element_fx = el.get("effects", {})
        for aid, e in el.get("abilities", {}).items():
            if aid in self.abilities and e:
                self.abilities[aid]["element"] = e
        for uid, e in el.get("units", {}).items():
            a = (self.defs.get(uid) or {}).get("attack")
            if isinstance(a, dict) and e:
                a["element"] = e

    def _index(self):
        for key in ("abilities.json", "items.json"):  # azonosító a fordításokhoz
            for k, v in self.files.get(key, {}).items():
                if isinstance(v, dict):
                    v["id"] = k
        self.factions = {}
        for rel, d in self.files.items():
            if rel.startswith("factions/") and isinstance(d, dict) and "id" in d:
                self.factions[d["id"]] = d
        self.defs, self.researches = {}, {}
        summon_research = self.game.get("summon_research", {})
        for rid, research in summon_research.items():
            self.researches[rid] = {**research, "id": rid, "kind": "research"}
        for f in self.factions.values():
            hq = f.get("buildings", {}).get(f.get("hq"))
            if hq is not None and f["id"] != "croftian" and not (f.get("ally") or f.get("neutral")):
                hq["researches"] = list(dict.fromkeys([*hq.get("researches", ()), *summon_research]))
            for kind, key in (("unit", "units"), ("building", "buildings")):
                for did, d in f.get(key, {}).items():
                    d["id"] = did
                    d["kind"] = kind
                    d["faction"] = f["id"]
                    self.defs[did] = d
            for rid, r in f.get("researches", {}).items():
                r["id"] = rid
                r["kind"] = "research"
                r["faction"] = f["id"]
                self.researches[rid] = r

    def _validate(self):
        w = self.warnings
        for f in self.factions.values():
            for key in (() if f.get("ally") else ("hq", "worker", "supply_building")):
                if f.get(key) not in self.defs:
                    w.append(f"{f['id']}: ismeretlen '{key}': {f.get(key)}")
            for did, d in list(f.get("units", {}).items()) + list(f.get("buildings", {}).items()):
                for ref in d.get("produces", []) + d.get("requires", []):
                    if ref not in self.defs:
                        w.append(f"{did}: ismeretlen hivatkozás: {ref}")
                for ref in d.get("researches", []):
                    if ref not in self.researches:
                        w.append(f"{did}: ismeretlen fejlesztés: {ref}")
                for ab in d.get("abilities", []) + d.get("hero_abilities", []):
                    if ab not in self.abilities:
                        w.append(f"{did}: ismeretlen képesség: {ab}")
            for definitions in ("units", "buildings", "researches"):
                for did, definition in f.get(definitions, {}).items():
                    for ref in definition.get("requires_research", []):
                        if ref not in self.researches:
                            w.append(f"{did}: ismeretlen fejlesztési előfeltétel: {ref}")
            for uid in f.get("ai", {}).get("build_order", []) + list(f.get("ai", {}).get("army", {})):
                if uid not in self.defs:
                    w.append(f"{f['id']} gépi ellenfél: ismeretlen azonosító: {uid}")


def load(enabled_mods=None, use_mods=True):
    """Betölti az alapadatokat és a modokat.

    enabled_mods: ha meg van adva, csak ezek a modok töltődnek be, ebben a sorrendben (a mod.json
    'enabled' mezőjétől függetlenül; a menü mod-ablaka így adja át a saves/mods.json listáját).
    """
    gd = GameData()
    gd.files = _collect(DATA_DIR)
    if use_mods:
        mods = []
        requested = None if enabled_mods is None else list(dict.fromkeys(enabled_mods))
        folders = sorted(os.listdir(MODS_DIR)) if os.path.isdir(MODS_DIR) else []
        names = folders if requested is None else requested
        for i, name in enumerate(names):
            mdir = os.path.join(MODS_DIR, name)
            meta_path = os.path.join(mdir, "mod.json")
            if name not in folders or not os.path.isfile(meta_path):
                if requested is not None:
                    gd.warnings.append(f"{name}: a kért mod nincs telepítve")
                continue
            try:
                meta = _read_json(meta_path)
                if not isinstance(meta, dict):
                    raise ValueError("a manifest nem objektum")
                if requested is None and meta.get("enabled") is False:
                    continue
                if type(meta.get("enabled", True)) is not bool or type(meta.get("priority", 0)) is not int:
                    raise ValueError("hibás enabled vagy priority mező")
                if not all(isinstance(meta.get(key, ""), str) for key in ("name", "description")):
                    raise ValueError("hibás name vagy description mező")
            except (OSError, ValueError) as exc:
                gd.warnings.append(f"{name}: hibás mod.json: {exc}")
                continue
            order = i if requested is not None else meta.get("priority", 0)
            mods.append((order, name, mdir, meta))
        for _, name, mdir, meta in sorted(mods):
            try:
                updates = _collect(mdir)
                for rel, content in updates.items():
                    if rel in gd.files:
                        updates[rel] = deep_merge(copy.deepcopy(gd.files[rel]), content)
                script = os.path.join(mdir, "script.py")
                if os.path.isfile(script):
                    spec = importlib.util.spec_from_file_location(f"mod_{name}", script)
                    mod = importlib.util.module_from_spec(spec)
                    spec.loader.exec_module(mod)
                    gd.scripts.append(mod)
            except (Exception, SystemExit) as exc:
                gd.warnings.append(f"{name}: a mod betöltése sikertelen: {exc}")
                continue
            gd.files.update(updates)
            gd.mods.append((meta.get("name", name), meta.get("description", "")))
            gd.mod_ids.append(name)
    gd._index()
    gd.hook("on_load", gd)
    gd._index()
    gd._apply_elements()
    gd._validate()
    return gd
