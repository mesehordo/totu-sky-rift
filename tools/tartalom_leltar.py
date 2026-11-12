"""A játék teljes tartalmi és szabályleltára az új motorhoz (UJ_MOTOR_TERV.md, M0).

    python tools/tartalom_leltar.py            # docs/alap/TARTALOM_LELTAR.md + docs/alap/tartalom_leltar.json

Az adatfájlokból (data/, mods/ nélkül) összegyűjti a fajokat, egységeket, épületeket, kutatásokat, képességeket,
tárgyakat, és ami a legfontosabb: a **szabályszókincset** – minden adatkulcsot, képességtípust, statnevet,
módosítót és fajmechanikát, amelyet az új motornak meg kell valósítania. Mindegyikhez megadja, hol hivatkozik
rá a mostani Python-kód (ez a viselkedés referenciája). Ami az adatban van, de a kód nem hivatkozik rá szó
szerint, külön listába kerül (lehet halott adat, vagy dinamikusan képzett kulcs – kézzel ellenőrizendő).
"""
import collections
import glob
import json
import os
import re
import sys

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SIM = {"game", "entities", "ai", "heroes", "campaign", "mission", "world", "points", "pathfinding", "dmath"}


def load(p):
    with open(os.path.join(ROOT, p), encoding="utf-8") as f:
        return json.load(f)


# --- kódhivatkozások ------------------------------------------------------------------------------------------

CODE = {}
for path in sorted(glob.glob(os.path.join(ROOT, "rts", "*.py"))):
    with open(path, encoding="utf-8") as f:
        CODE[os.path.splitext(os.path.basename(path))[0]] = f.read().split("\n")


def refs(token):
    """{modul: [sorszámok]} – a token szövegliterálként ("x" vagy 'x') vagy attribútumként (.x) fordul elő."""
    pat = re.compile(r"""(["'])%s\1|\.%s\b""" % (re.escape(token), re.escape(token)))
    out = {}
    for mod, lines in CODE.items():
        hits = [i + 1 for i, l in enumerate(lines) if pat.search(l)]
        if hits:
            out[mod] = hits
    return out


def fmt_refs(r, limit=4):
    if not r:
        return "**nincs**"
    parts = []
    for mod in sorted(r, key=lambda m: (m not in SIM, m)):
        ln = r[mod]
        s = f"`{mod}` {','.join(map(str, ln[:3]))}{'…' if len(ln) > 3 else ''}"
        parts.append(s)
    more = len(parts) - limit
    return "; ".join(parts[:limit]) + (f"; +{more} modul" if more > 0 else "")


# --- adatok ---------------------------------------------------------------------------------------------------

factions = {}
for p in sorted(glob.glob(os.path.join(ROOT, "data", "factions", "*.json"))):
    d = load(os.path.relpath(p, ROOT))
    factions[d["id"]] = d
abilities = load("data/abilities.json")
items = load("data/items.json")
elements = load("data/elements.json")
camps = load("data/camps.json")
game = load("data/game.json")


def kind(f):
    if f.get("neutral"):
        return "semleges"
    if f.get("ally"):
        return "szövetséges kisfaj"
    if f.get("hidden"):
        return "rejtett főfaj"
    return "főfaj"


def walk_keys(obj, counter, where, prefix=""):
    if isinstance(obj, dict):
        for k, v in obj.items():
            counter[k].add(where)


# szókincs-gyűjtők: kulcs -> előfordulási helyek (faj-azonosítók)
voc = {name: collections.defaultdict(set) for name in
       ("faction", "unit", "building", "research", "ability", "effect_stat", "modifier", "rule", "path", "item")}
ab_types = collections.defaultdict(lambda: {"count": 0, "keys": collections.Counter(), "factions": set()})
mechanics = collections.defaultdict(list)
used_abilities = collections.defaultdict(set)


def collect_effects(effs, fid):
    for e in effs or []:
        if isinstance(e, dict):
            if "stat" in e:
                voc["effect_stat"][e["stat"]].add(fid)
            for k in e:
                voc["effect_stat"]["<effect-kulcs> " + k].add(fid)


def collect_rules(rules, fid):
    for k in (rules or {}):
        voc["rule"][k].add(fid)


rows = []
for fid, f in sorted(factions.items(), key=lambda kv: kv[1].get("order", 0)):
    mechanics[f.get("mechanic", "standard")].append(fid)
    for k in f:
        voc["faction"][k].add(fid)
    for uid, u in f.get("units", {}).items():
        for k in u:
            voc["unit"][k].add(fid)
        for a in u.get("abilities", []):
            used_abilities[a].add(fid)
        for k in (u.get("attack") or {}):
            voc["unit"]["attack." + k].add(fid)
    for bid, b in f.get("buildings", {}).items():
        for k in b:
            voc["building"][k].add(fid)
        for a in b.get("abilities", []) + b.get("powers", []):
            used_abilities[a].add(fid)
        for k in (b.get("attack") or {}):
            voc["building"]["attack." + k].add(fid)
    for rid, r in f.get("researches", {}).items():
        for k in r:
            voc["research"][k].add(fid)
        collect_effects(r.get("effects"), fid)
        collect_rules(r.get("rules"), fid)
    for pid, pth in (f.get("paths") or {}).items():
        for k in pth:
            voc["path"][k].add(fid)
        collect_effects(pth.get("effects"), fid)
        collect_rules(pth.get("rules"), fid)
    rows.append({
        "id": fid, "name": f.get("name"), "kind": kind(f), "mechanic": f.get("mechanic", "standard"),
        "units": len(f.get("units", {})), "buildings": len(f.get("buildings", {})),
        "researches": len(f.get("researches", {})), "paths": len(f.get("paths") or {}),
        "abilities": len({a for u in f.get("units", {}).values() for a in u.get("abilities", [])}
                         | {a for b in f.get("buildings", {}).values() for a in b.get("abilities", []) + b.get("powers", [])}),
    })

for aid, a in abilities.items():
    t = a.get("type", "?")
    ab_types[t]["count"] += 1
    ab_types[t]["keys"].update(a.keys())
    ab_types[t]["factions"].update(used_abilities.get(aid, ()))
    for k in a:
        voc["ability"][k].add(t)
    for k in (a.get("modifiers") or {}):
        voc["modifier"][k].add(aid)
    collect_effects(a.get("effects"), aid)
for iid, it in items.items():
    for k in it:
        voc["item"][k].add(iid)
    for k in (it.get("modifiers") or {}):
        voc["modifier"][k].add("tárgy:" + iid)

unused_abilities = sorted(set(abilities) - set(used_abilities))

# --- kimenet --------------------------------------------------------------------------------------------------

L = []
w = L.append
w("# Tartalmi és szabályleltár (generált)")
w("")
w("Készítette: `python tools/tartalom_leltar.py` az alapadatokból (`data/`, modok nélkül). **Kézzel ne szerkeszd** –"
  " a magyarázó részek az [UJ_MOTOR_TERV.md](UJ_MOTOR_TERV.md)-ben vannak.")
w("A „kód” oszlop a mostani Python-játék hivatkozásait mutatja (`modul` sorszámok); ez a viselkedés referenciája az új"
  " motorhoz. **nincs** = a kulcs szó szerint nem szerepel a kódban (halott adat, vagy dinamikusan képzett kulcs).")
w("")
tot = {k: sum(r[k] for r in rows) for k in ("units", "buildings", "researches")}
w(f"**Összesen:** {len(rows)} faj, {tot['units']} egység, {tot['buildings']} épület, {tot['researches']} kutatás, "
  f"{len(abilities)} képesség ({len(ab_types)} típus), {len(items)} tárgy, {len(mechanics)} fajmechanika.")
w("")
w("## 1. Fajok")
w("")
w("| Faj | Név | Típus | Mechanika | Egység | Épület | Kutatás | Ág | Képesség |")
w("|---|---|---|---|---|---|---|---|---|")
for r in rows:
    w(f"| `{r['id']}` | {r['name']} | {r['kind']} | `{r['mechanic']}` | {r['units']} | {r['buildings']} | "
      f"{r['researches']} | {r['paths']} | {r['abilities']} |")
w("")
w("## 2. Fajmechanikák (`mechanic`)")
w("")
w("| Mechanika | Fajok | Kód |")
w("|---|---|---|")
for m, fs in sorted(mechanics.items()):
    w(f"| `{m}` | {', '.join(fs)} | {fmt_refs(refs(m))} |")
w("")
w("## 3. Képességtípusok")
w("")
w("| Típus | Db | Használó fajok | Adatkulcsok | Kód |")
w("|---|---|---|---|---|")
for t, info in sorted(ab_types.items(), key=lambda kv: -kv[1]["count"]):
    keys = ", ".join(k for k, _ in info["keys"].most_common() if k not in ("name", "desc", "type", "icon", "hotkey"))
    w(f"| `{t}` | {info['count']} | {', '.join(sorted(info['factions'])) or '–'} | {keys} | {fmt_refs(refs(t))} |")
w("")
if unused_abilities:
    w(f"Egy egység vagy épület sem hivatkozik rájuk közvetlenül ({len(unused_abilities)}; tárgy, elem, tábor, "
      f"küldetés vagy idézés adhatja): " + ", ".join(f"`{a}`" for a in unused_abilities))
    w("")

TITLES = {
    "faction": "4. Faj-szintű kulcsok",
    "unit": "5. Egységkulcsok",
    "building": "6. Épületkulcsok",
    "research": "7. Kutatáskulcsok",
    "path": "8. Fejlődési ágak (`paths`) kulcsai",
    "rule": "9. Szabálymódosítók (`rules`, ágakból és kutatásokból)",
    "effect_stat": "10. Hatások: statnevek és hatáskulcsok (`effects`)",
    "ability": "11. Képességkulcsok (melyik típusban)",
    "modifier": "12. Módosítók (`modifiers`: képességek és tárgyak)",
    "item": "13. Tárgykulcsok",
}
missing = []
for name, title in TITLES.items():
    w(f"## {title}")
    w("")
    w("| Kulcs | Előfordul | Kód |")
    w("|---|---|---|")
    for k, where in sorted(voc[name].items(), key=lambda kv: (-len(kv[1]), kv[0])):
        tok = k.split(" ", 1)[-1].split(".")[-1]
        r = refs(tok)
        if not r:
            missing.append((name, k))
        ws = sorted(where)
        shown = ", ".join(ws[:6]) + (f" (+{len(ws) - 6})" if len(ws) > 6 else "")
        w(f"| `{k}` | {len(ws)}: {shown} | {fmt_refs(r)} |")
    w("")
w("## 14. Egyéb adatállományok")
w("")
w(f"- `data/elements.json`: kulcsai {', '.join(f'`{k}`' for k in elements)}")
w(f"- `data/camps.json`: kulcsai {', '.join(f'`{k}`' for k in camps)}")
w(f"- `data/game.json`: {', '.join(f'`{k}`' for k in game)}")
w("")
w("## 15. Kódhivatkozás nélküli adatkulcsok (kézzel ellenőrizendő)")
w("")
w(f"{len(missing)} kulcs: " + ", ".join(f"`{n}:{k}`" for n, k in missing))
w("")

# --- a kódba égetett tartalomazonosítók: ezek „rejtett szabályok”, az új motorban szkript vagy általános szabály lesz
ids = {}
for fid, f in factions.items():
    ids[fid] = "faj"
    for k in ("units", "buildings", "researches"):
        for x in f.get(k, {}):
            ids[x] = {"units": "egység", "buildings": "épület", "researches": "kutatás"}[k]
for a in abilities:
    ids.setdefault(a, "képesség")
for i in items:
    ids.setdefault(i, "tárgy")
lit = re.compile(r"""["']([a-z][a-z0-9_]{2,})["']""")
hard = collections.defaultdict(lambda: collections.defaultdict(list))
for mod, lines in CODE.items():
    for n, line in enumerate(lines, 1):
        if line.lstrip().startswith("#"):
            continue
        for m in lit.finditer(line):
            tok = m.group(1)
            if tok in ids and ids[tok] != "faj" and "_" in tok:
                hard[tok][mod].append(n)
w("## 16. A kódba égetett tartalomazonosítók („rejtett szabályok”)")
w("")
w("Ezekre az egységekre, épületekre, kutatásokra, képességekre vagy tárgyakra a Python-kód **név szerint** ágazik el."
  " Az új motorban mindegyikből általános, adatból vezérelt szabály vagy a tartalomhoz tartozó Lua-szkript lesz.")
w("")
w("| Azonosító | Fajta | Kód |")
w("|---|---|---|")
for tok in sorted(hard, key=lambda t: (ids[t], t)):
    w(f"| `{tok}` | {ids[tok]} | {fmt_refs(dict(hard[tok]))} |")
w("")

# --- D22: varázserő-szabály – a nem hős, nem `caster` egység képessége nem fizethet energiával, ha a faj
# varázserő-forrása nem az energia (a hős és a varázsló energiát használ)
violations = []
for fid, f in factions.items():
    res = f.get("spell_resource")
    if res in (None, "energy"):
        continue
    for uid, u in f.get("units", {}).items():
        if u.get("hero") or "caster" in u.get("tags", []):
            continue
        for a in u.get("abilities", []):
            ab = abilities.get(a, {})
            if ab.get("energy") or any(lv.get("energy") for lv in ab.get("levels") or []):
                violations.append(f"`{fid}`/`{uid}`: `{a}`")
w("## 17. Varázserő-szabály (D22)")
w("")
w("A nem hős, nem `caster` címkés egység képessége csak a faj varázserő-forrásából fizethet (vagy csak újratöltés), "
  "ha az nem az energia. Sértések: " + (", ".join(violations) if violations else "**nincs**") + ".")
w("")

out_md = os.path.join(ROOT, "docs", "alap", "TARTALOM_LELTAR.md")
with open(out_md, "w", encoding="utf-8") as f:
    f.write("\n".join(L))
summary = {
    "factions": rows, "mechanics": dict(mechanics),
    "ability_types": {t: {"count": i["count"], "factions": sorted(i["factions"]), "keys": sorted(i["keys"])}
                      for t, i in ab_types.items()},
    "vocabulary": {n: sorted(v) for n, v in voc.items()},
    "unreferenced": [f"{n}:{k}" for n, k in missing],
}
with open(os.path.join(ROOT, "docs", "alap", "tartalom_leltar.json"), "w", encoding="utf-8") as f:
    json.dump(summary, f, ensure_ascii=False, indent=1)
print(f"{len(rows)} faj, {tot['units']} egység, {tot['buildings']} épület, {tot['researches']} kutatás, "
      f"{len(abilities)} képesség / {len(ab_types)} típus, szókincs: "
      + ", ".join(f"{n} {len(v)}" for n, v in voc.items()) + f"; kód nélkül: {len(missing)} → {out_md}")
