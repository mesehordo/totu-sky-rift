"""A mai játékadatok átalakítása az új motor sablonformátumára (ADR-005, M2 3. lépés).

    python tools/konverter_motor.py            # → engine/content/base/ (sablonok, fordítások, jelentés)
    python tools/konverter_motor.py --check    # csak ellenőriz: a kimenet naprakész-e (CI)

Bemenet: data/factions/*.json, data/abilities.json, data/items.json, data/elements.json, data/game.json,
data/lang/en.json. Kimenet (determinisztikus, a repóba kerül, hogy a motor tesztjei a valódi tartalommal fussanak):
  engine/content/base/mod.json
  engine/content/base/templates/common.json          alapsablonok (template_unit, template_hero, template_building)
  engine/content/base/templates/factions.json
  engine/content/base/templates/units/<faj>.json, buildings/<faj>.json, researches/<faj>.json
  engine/content/base/templates/abilities.json, items.json, statuses.json
  engine/content/base/locale/hu.po, en.po            a nevek és leírások (msgctxt = kulcs)
  engine/content/base/KONVERZIO.md                    mi hová került, mi maradt `Legacy`-ben

Szabály: semmi nem veszhet el. Az ismert kulcsok komponensekbe kerülnek (a leképezés lent, táblázatban); ami még
nincs leképezve, a `Legacy` komponensbe – ezt az M6/M7 dolgozza fel, amikor a hozzá tartozó rendszer elkészül.
A nevek és leírások helyén fordítási kulcs áll.
"""
import argparse
import collections
import glob
import json
import os
import sys

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "engine", "content", "base")

# --- leképezés: forráskulcs → (komponens, mező) ------------------------------------------------------------------
UNIT_MAP = {
    "tags": ("Identity", "tags"), "utype": ("Identity", "class"), "shape": ("Identity", "shape"),
    "sprite": ("Identity", "sprite"), "hidden": ("Identity", "hidden"), "summoned": ("Identity", "summoned"),
    "worker": ("Identity", "worker"), "hero": ("Identity", "hero"),
    "cost": ("Cost", "resources"), "time": ("Cost", "time"), "supply": ("Cost", "supply"), "count": ("Cost", "count"),
    "hp": ("Health", "max"), "armor": ("Health", "armor"), "shield": ("Health", "shield"),
    "shield_slot": ("Health", "shield_slot"), "shield_delay": ("Health", "shield_delay"),
    "shield_zone": ("Health", "shield_zone"),
    "speed": ("Movement", "speed"), "radius": ("Movement", "radius"), "flying": ("Movement", "flying"),
    "sight": ("Vision", "sight"), "always_hidden": ("Vision", "always_hidden"), "mine_stealth": ("Vision", "mine_stealth"),
    "attack": ("Attack", None), "type_bonus": ("Attack", "type_bonus"), "type_nostrength": ("Attack", "type_nostrength"),
    "death_burst": ("Attack", "death_burst"),
    "energy": ("Energy", "max"),
    "abilities": ("Abilities", "list"), "ability_research": ("Abilities", "research"), "ability_path": ("Abilities", "path"),
    "hero_abilities": ("Hero", "abilities"), "growth": ("Hero", "growth"), "xp": ("Hero", "xp"),
    "gathers": ("Worker", "gathers"), "carry": ("Worker", "carry"), "builds": ("Worker", "builds"),
    "tap_wood": ("Worker", "tap_wood"), "find_items": ("Worker", "find_items"),
    "unit_supply": ("Supply", "provides"),
    "lifetime": ("Lifetime", "seconds"), "no_corpse": ("Lifetime", "no_corpse"),
}
BUILDING_MAP = {
    "shape": ("Identity", "shape"), "sprite": ("Identity", "sprite"), "no_sprite": ("Identity", "no_sprite"),
    "hidden": ("Identity", "hidden"), "wall": ("Identity", "wall"),
    "cost": ("Cost", "resources"), "time": ("Cost", "time"),
    "hp": ("Health", "max"), "armor": ("Health", "armor"), "shield": ("Health", "shield"),
    "size": ("Footprint", "size"),
    "sight": ("Vision", "sight"), "detect": ("Vision", "detect"),
    "produces": ("Producer", "units"), "parallel": ("Producer", "parallel"), "larva": ("Producer", "larva"),
    "larva_time": ("Producer", "larva_time"),
    "researches": ("Researcher", "list"), "powers": ("Powers", "list"),
    "supply": ("Supply", "provides"), "dropoff": ("Dropoff", "resources"),
    "attack": ("Attack", None), "garrison": ("Garrison", "slots"),
    "aura": ("Aura", None), "regen_aura": ("Aura", "regen"),
    "no_creep": ("Placement", "no_creep"), "no_power": ("Placement", "no_power"), "no_pond": ("Placement", "no_pond"),
    "no_light": ("Placement", "no_light"), "on_resource": ("Placement", "on_resource"),
    "foundation_free": ("Placement", "foundation_free"), "packable": ("Placement", "packable"),
    "creep_radius": ("Mechanics", "creep_radius"), "link_radius": ("Mechanics", "link_radius"),
    "field_radius": ("Mechanics", "field_radius"), "core": ("Mechanics", "network_core"),
    "power_radius": ("Mechanics", "power_radius"), "light_radius": ("Mechanics", "light_radius"),
    "light_heal": ("Mechanics", "light_heal"), "lantern": ("Mechanics", "lantern"),
    "rift_source": ("Mechanics", "rift_source"), "shield_reactor": ("Mechanics", "shield_reactor"),
    "torii": ("Mechanics", "torii"), "diplomacy": ("Mechanics", "diplomacy"), "altar": ("Mechanics", "altar"),
    "pray_slots": ("Mechanics", "pray_slots"), "mine": ("Mechanics", "mine"),
}
# kutatási stat → módosító-útvonal (D24); minden hatás összeadó, a mai viselkedés szerint
STAT_PATH = {
    "damage": "Attack.damage", "armor": "Health.armor", "hp": "Health.max", "speed_pct": "Movement.speed_pct",
    "regen": "Health.regen", "range": "Attack.range", "sight": "Vision.sight", "shield": "Health.shield",
    "energy": "Energy.max", "carry_pct": "Worker.carry_pct",
}
REQ_KEYS = ("requires", "requires_research", "requires_ascended", "path", "ally")
TEXT_KEYS = ("name", "desc", "tagline")


class Ctx:
    def __init__(self, en):
        self.po = collections.OrderedDict()   # kulcs → magyar szöveg
        self.en = en
        self.legacy = collections.Counter()
        self.mapped = collections.Counter()

    def text(self, key, hu):
        self.po[key] = hu
        return key


def requirements(src):
    """A mai előfeltétel-kulcsokból egységes kifejezés (ADR-005): {"all": [...]}."""
    terms = []
    for b in src.get("requires", []) or []:
        terms.append({"building": b})
    for r in src.get("requires_research", []) or []:
        terms.append({"research": r})
    if src.get("requires_ascended"):
        terms.append({"ascended": True})
    if src.get("path"):
        terms.append({"path": src["path"]})
    if src.get("ally"):
        terms.append({"ally": src["ally"]})
    return {"all": terms} if terms else None


def convert_entity(ctx, kind, eid, src, faction, mapping):
    out = collections.OrderedDict()
    out["inherits"] = "template_hero" if src.get("hero") else f"template_{kind}"
    ident = collections.OrderedDict([("kind", kind), ("faction", faction)])
    out["Identity"] = ident
    for k in TEXT_KEYS:
        if k in src:
            ident[k] = ctx.text(f"{kind}.{eid}.{k}", src[k])
    legacy = collections.OrderedDict()
    for k, v in src.items():
        if k in TEXT_KEYS or k in REQ_KEYS:
            continue
        if k in mapping:
            comp, field = mapping[k]
            c = out.setdefault(comp, collections.OrderedDict())
            if field is None and isinstance(v, dict):
                for kk, vv in v.items():
                    c[kk] = vv
            else:
                c[field or k] = v
            ctx.mapped[f"{kind}:{k}"] += 1
        else:
            legacy[k] = v
            ctx.legacy[f"{kind}:{k}"] += 1
    req = requirements(src)
    if req:
        out["Requirements"] = req
    if legacy:
        out["Legacy"] = legacy
    return out


def convert_research(ctx, rid, r, faction):
    out = collections.OrderedDict([("Identity", collections.OrderedDict([("kind", "research"), ("faction", faction)]))])
    for k in TEXT_KEYS:
        if k in r:
            out["Identity"][k] = ctx.text(f"research.{rid}.{k}", r[k])
    if "icon" in r:
        out["Identity"]["icon"] = r["icon"]
    out["Cost"] = collections.OrderedDict([("resources", r.get("cost", {})), ("time", r.get("time", 0))])
    mods = []
    for e in r.get("effects", []) or []:
        m = collections.OrderedDict([("path", STAT_PATH.get(e["stat"], e["stat"])), ("op", "add"),
                                     ("value", e.get("add", 0))])
        tags = [t for t in e.get("tags", ["all"]) if t != "all"]
        if tags:
            m["filter"] = {"tags": tags}
        mods.append(m)
    if mods:
        out["Modifiers"] = mods
    if r.get("rules"):
        out["Rules"] = r["rules"]          # D24: az M7-ben módosítókká / passzív képességekké alakul
    if r.get("exclusive"):
        out["Exclusive"] = r["exclusive"]
    req = requirements(r)
    if req:
        out["Requirements"] = req
    extra = {k: v for k, v in r.items() if k not in TEXT_KEYS + REQ_KEYS + ("icon", "cost", "time", "effects", "rules",
                                                                              "exclusive")}
    if extra:
        out["Legacy"] = extra
        for k in extra:
            ctx.legacy[f"research:{k}"] += 1
    return out


def generic(ctx, kind, oid, src, keep_component):
    out = collections.OrderedDict([("Identity", collections.OrderedDict([("kind", kind)]))])
    for k in TEXT_KEYS:
        if k in src:
            out["Identity"][k] = ctx.text(f"{kind}.{oid}.{k}", src[k])
    body = collections.OrderedDict((k, v) for k, v in src.items() if k not in TEXT_KEYS)
    if body:
        out[keep_component] = body
    return out


def po_file(entries, translate):
    lines = ['msgid ""', 'msgstr ""', '"Content-Type: text/plain; charset=UTF-8\\n"', ""]

    def esc(s):
        return s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")
    for key, hu in entries.items():
        tr = translate(hu) if translate else ""
        lines += [f'msgctxt "{esc(key)}"', f'msgid "{esc(hu)}"', f'msgstr "{esc(tr)}"', ""]
    return "\n".join(lines)


def dump(obj):
    return json.dumps(obj, ensure_ascii=False, indent=1) + "\n"


def build():
    data = os.path.join(ROOT, "data")
    load = lambda p: json.load(open(os.path.join(data, p), encoding="utf-8"))  # noqa: E731
    en = load("lang/en.json") if os.path.exists(os.path.join(data, "lang", "en.json")) else {}
    ctx = Ctx(en)
    files = collections.OrderedDict()
    files["mod.json"] = dump({"id": "totu", "version": "0.1.0", "_leiras": "Az alaptartalom (generált: "
                              "tools/konverter_motor.py)."})
    files["templates/common.json"] = dump(collections.OrderedDict([
        ("_leiras", "Alapsablonok – a konkrét egységek, hősök és épületek ezekből örökölnek."),
        ("template_unit", {"Identity": {"kind": "unit"}, "Movement": {"flying": False}, "Health": {"armor": 0}}),
        ("template_hero", {"inherits": "template_unit", "Identity": {"hero": True}}),
        ("template_building", {"Identity": {"kind": "building"}, "Health": {"armor": 0}}),
    ]))
    factions = collections.OrderedDict()
    for p in sorted(glob.glob(os.path.join(data, "factions", "*.json"))):
        f = json.load(open(p, encoding="utf-8"))
        fid = f["id"]
        units = collections.OrderedDict((uid, convert_entity(ctx, "unit", uid, u, fid, UNIT_MAP))
                                        for uid, u in f.get("units", {}).items())
        blds = collections.OrderedDict((bid, convert_entity(ctx, "building", bid, b, fid, BUILDING_MAP))
                                       for bid, b in f.get("buildings", {}).items())
        res = collections.OrderedDict((rid, convert_research(ctx, rid, r, fid))
                                      for rid, r in f.get("researches", {}).items())
        if units:
            files[f"templates/units/{fid}.json"] = dump(units)
        if blds:
            files[f"templates/buildings/{fid}.json"] = dump(blds)
        if res:
            files[f"templates/researches/{fid}.json"] = dump(res)
        fac = collections.OrderedDict([("Identity", collections.OrderedDict([("kind", "faction")]))])
        for k in TEXT_KEYS:
            if k in f:
                fac["Identity"][k] = ctx.text(f"faction.{fid}.{k}", f[k])
        fac["Roster"] = collections.OrderedDict([("units", list(f.get("units", {}))),
                                                 ("buildings", list(f.get("buildings", {}))),
                                                 ("researches", list(f.get("researches", {})))])
        paths = collections.OrderedDict()
        for pid, pd in (f.get("paths") or {}).items():
            pc = collections.OrderedDict()
            for k in TEXT_KEYS:
                if k in pd:
                    pc[k] = ctx.text(f"path.{fid}.{pid}.{k}", pd[k])
            for k, v in pd.items():
                if k not in TEXT_KEYS:
                    pc[k] = v
            paths[pid] = pc
        if paths:
            fac["Paths"] = paths
        rest = collections.OrderedDict((k, v) for k, v in f.items()
                                       if k not in TEXT_KEYS + ("id", "units", "buildings", "researches", "paths"))
        fac["Faction"] = rest      # faji szabályok és paraméterek (spec/FAJMECHANIKAK); az M7 bontja tovább
        factions[f"faction_{fid}"] = fac
    files["templates/factions.json"] = dump(factions)
    abil = collections.OrderedDict((aid, generic(ctx, "ability", aid, a, "Spec"))
                                   for aid, a in load("abilities.json").items())
    files["templates/abilities.json"] = dump(abil)     # D22-modell: az M7-ben
    items = collections.OrderedDict()
    for iid, it in load("items.json").items():
        o = generic(ctx, "item", iid, it, "Item")
        bonus = o.get("Item", {}).pop("bonus", None)
        if bonus:
            o["Modifiers"] = [collections.OrderedDict([("path", STAT_PATH.get(k, k)), ("op", "add"), ("value", v)])
                              for k, v in bonus.items()]
        items[iid] = o
    files["templates/items.json"] = dump(items)
    el = load("elements.json")
    statuses = collections.OrderedDict()
    for eid, e in el.get("effects", {}).items():
        o = collections.OrderedDict([("Identity", collections.OrderedDict([("kind", "status"),
                                                                           ("name", ctx.text(f"status.{eid}.name", e["name"])),
                                                                           ("status", ctx.text(f"status.{eid}.status", e["status"]))]))])
        o["Status"] = collections.OrderedDict((k, v) for k, v in e.items() if k not in ("name", "status"))
        statuses[f"status_{eid}"] = o
    statuses["_hozzarendeles"] = {"abilities": el.get("abilities", {}), "units": el.get("units", {}),
                                  "kezi": el.get("kezi", {})}
    files["templates/statuses.json"] = dump(statuses)
    g = load("game.json")
    files["game.json"] = dump(collections.OrderedDict((k, v) for k, v in g.items()
                                                      if k not in ("title", "subtitle", "creator", "copyright")))
    files["locale/hu.po"] = po_file(ctx.po, None) + "\n"
    files["locale/en.po"] = po_file(ctx.po, lambda hu: ctx.en.get(hu, "")) + "\n"
    rep = ["# Konverziós jelentés (generált)", "",
           "`tools/konverter_motor.py` – a mai adatok leképezése az ADR-005 komponenseire.", "",
           f"Fordítási kulcsok: {len(ctx.po)}; angol fordítás: {sum(1 for hu in ctx.po.values() if hu in ctx.en)}.", "",
           "## Komponensbe leképezett kulcsok", "", "| Kulcs | Db |", "|---|---|"]
    rep += [f"| `{k}` | {n} |" for k, n in sorted(ctx.mapped.items())]
    rep += ["", "## `Legacy`-ben maradt kulcsok (az M6/M7 dolgozza fel)", "", "| Kulcs | Db |", "|---|---|"]
    rep += [f"| `{k}` | {n} |" for k, n in sorted(ctx.legacy.items())] or ["| – | – |"]
    rep += ["", "A képességek (`Spec`), a faji paraméterek (`Faction`) és a kutatások szabályai (`Rules`) a mai "
            "formájukban kerültek át; a D22/D24 modellre az M7 alakítja őket."]
    files["KONVERZIO.md"] = "\n".join(rep) + "\n"
    return files, ctx


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    files, ctx = build()
    stale = []
    for rel, text in files.items():
        path = os.path.join(OUT, rel)
        old = open(path, encoding="utf-8").read() if os.path.exists(path) else None
        if old != text:
            stale.append(rel)
            if not a.check:
                os.makedirs(os.path.dirname(path), exist_ok=True)
                with open(path, "w", encoding="utf-8", newline="\n") as f:
                    f.write(text)
    if a.check:
        if stale:
            print("A konvertált tartalom elavult – futtasd: python tools/konverter_motor.py\n  " + "\n  ".join(stale))
            raise SystemExit(1)
        print(f"rendben: {len(files)} fájl naprakész")
        return
    print(f"{len(files)} fájl ({len(stale)} változott) → {OUT}; fordítási kulcs: {len(ctx.po)}; "
          f"komponensbe: {sum(ctx.mapped.values())}, Legacy: {sum(ctx.legacy.values())} ({len(ctx.legacy)} kulcs)")


if __name__ == "__main__":
    main()
