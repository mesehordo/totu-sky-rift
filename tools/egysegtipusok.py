"""Egységtípusok és erősségek (Ötletdoboz Ö24, ParaWorld-szerű): javaslat-lista átnézésre.

    python tools/egysegtipusok.py

Kimenet:
  docs/EGYSEGTIPUSOK.md     – a típusok, az erősségi táblázat, és mind az egységek: típus + javasolt kivétel
  docs/egysegtipusok.json   – ugyanez géppel olvashatóan (a beépítéskor ebből kerül az adatokba)
A besorolás a meglévő jellemzőkből (alak, címkék, leírás) indul; a kivételeket a leírás és a mostani
sebzés-bónuszok alapján javasolja. A `KEZI` szótárban felülírható egységenként.
"""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

TYPES = {
    "kozelharcos": ("⚔", "Közelharcos", "kard, buzogány, balta"),
    "landzsas": ("🛡", "Lándzsás / pajzsos", "lándzsa, dárda, pajzsfal"),
    "lovesz": ("🏹", "Lövész", "íj, parittya, dobófegyver"),
    "lovas": ("🐎", "Lovas / hátas", "gyors hátas harcos"),
    "orias": ("🗿", "Óriás / szörny", "nagy testű lény vagy gép"),
    "varazslo": ("✨", "Varázsló", "mágiahasználó, támogató"),
    "repulo": ("🪶", "Repülő", "légi egység"),
    "ostrom": ("💥", "Ostromgép", "távoli, épületromboló"),
    "munkas": ("⛏", "Munkás", "gyűjt, épít"),
}
# támadó típus -> {cél típus vagy "epulet": szorzó}
MATRIX = {
    "kozelharcos": {"landzsas": 1.25, "varazslo": 1.25},
    "landzsas": {"lovas": 1.5, "orias": 1.25},
    "lovesz": {"kozelharcos": 1.25, "repulo": 1.5},
    "lovas": {"lovesz": 1.5, "varazslo": 1.5, "ostrom": 1.5},
    "orias": {"kozelharcos": 1.25, "lovas": 1.25, "epulet": 1.5},
    "varazslo": {"orias": 1.5, "landzsas": 1.25},
    "repulo": {"ostrom": 1.5, "varazslo": 1.25},
    "ostrom": {"epulet": 2.0, "landzsas": 1.25},
    "munkas": {},
}
NAMES = {k: v[1] for k, v in TYPES.items()}
NAMES["epulet"] = "Épületek"
# kézi felülírások: egység -> {"type": ..., "extra": {cél: szorzó}, "note": ...}
KEZI = {
    "gho_koharapo": {"extra": {"epulet": 1.5}, "note": "a köveket is megrágja"},
    "gho_meszbomba": {"extra": {"epulet": 2.0}, "note": "öngyilkos, épületromboló"},
    "oon_kapudongeto": {"type": "ostrom", "extra": {"epulet": 2.0}, "note": "faltörő kos – közelről"},
    "pin_jegdardas": {"extra": {"lovas": 1.25}, "note": "a jéghegy lelassítja a lovasokat"},
    "zha_csirkepasztor": {"note": "olcsó tömegegység – nincs külön erőssége", "nostrength": True},
    "cro_robbanoatka": {"extra": {"epulet": 1.25}, "note": "öngyilkos tömegegység"},
    "huk_ebihal": {"note": "idézett, rövid életű"},
}


def classify(u):
    t = set(u.get("tags", []))
    sh = u.get("shape", "")
    n = u["name"].lower()
    if u.get("worker"):
        return "munkas"
    if u.get("flying") or "flyer" in t:
        return "repulo"
    if "siege" in t or sh == "artillery":
        return "ostrom"
    if "caster" in t:
        return "varazslo"
    if sh == "rider" or "cavalry" in t:
        return "lovas"
    if ("large" in t and not u.get("hero")) or sh in ("golem", "horror", "theropod") or \
            (sh == "beast" and u.get("radius", 0) >= 0.55):
        return "orias"
    if sh == "shieldbearer" or any(w in n for w in ("lándzs", "dárd", "pajzs", "szigony")):
        return "landzsas"
    if "ranged" in t:
        return "lovesz"
    return "kozelharcos"


def suggest_extra(u):
    out, notes = {}, []
    b = (u.get("attack") or {}).get("bonus", {})
    if "building" in b:
        out["epulet"] = 1.5
        notes.append("épületek ellen bónusza van")
    dsc = u.get("desc", "").lower()
    if "épület" in dsc and ("ellen" in dsc or "rombol" in dsc) and "epulet" not in out:
        out["epulet"] = 1.5
        notes.append("leírás: épületek ellen")
    if ("légi" in dsc or "repülők ellen" in dsc) and "repulo" not in out:
        out["repulo"] = 1.5
        notes.append("leírás: légvédelem")
    return out, notes


def fmt(m):
    return " · ".join(f"{TYPES[k][0] if k in TYPES else '🏛'} {NAMES[k]} ×{v:g}".replace(".", ",") for k, v in m.items())


def main():
    from rts import data
    d = data.load()
    weak = {k: [a for a, m in MATRIX.items() if k in m] for k in TYPES}
    rows, out = [], {"types": {k: {"icon": v[0], "name": v[1], "desc": v[2]} for k, v in TYPES.items()},
                     "matrix": MATRIX, "units": {}}
    facs = [f for f in d.factions.values() if not f.get("neutral") and not f.get("hidden")]
    facs.sort(key=lambda f: (bool(f.get("ally")), f.get("order", 99), f["name"]))
    md = ["# Egységtípusok és erősségek – átnézésre", "",
          "*© Mesehordo – TOTU: Sky Rift. Ötletdoboz Ö24. A `python tools/egysegtipusok.py` állítja elő; a besorolás "
          "kézzel felülírható (a script `KEZI` szótára), vagy írd meg, melyik egység kerüljön máshova.*", "",
          "## Típusok és szorzók", "",
          "| Típus | Mi ez | Erős ez ellen | Gyenge ez ellen |", "|---|---|---|---|"]
    for k, (ic, nm, ds) in TYPES.items():
        wk = ", ".join(f"{TYPES[a][0]} {NAMES[a]}" for a in weak[k]) or "–"
        md.append(f"| {ic} **{nm}** | {ds} | {fmt(MATRIX[k]) or '–'} | {wk} |")
    md += ["", "A szorzó a támadó sebzésére vonatkozik (a páncél előtt). A karakterlapon a portré mellett a típus "
           "ikonja, alatta „Erős:” és „Gyenge:” ikonsor; egérrel fölé állva a súgó megmondja a szorzót.", ""]
    for f in facs:
        units = [(uid, u) for uid, u in f["units"].items()]
        if not units:
            continue
        tag = " (szövetséges nép)" if f.get("ally") else ""
        md += [f"## {f['name']}{tag}", "", "| Egység | Típus | Erős ez ellen | Javasolt kivétel | Megjegyzés |",
               "|---|---|---|---|---|"]
        for uid, u in units:
            k = KEZI.get(uid, {})
            typ = k.get("type") or classify(u)
            extra, notes = suggest_extra(u)
            extra.update(k.get("extra", {}))
            if k.get("note"):
                notes.append(k["note"])
            if not u.get("attack"):
                notes.append("nem harcol")
            if u.get("summoned"):
                notes.append("idézett")
            if u.get("hero"):
                notes.append("hős")
            strong = {} if k.get("nostrength") else dict(MATRIX[typ])
            out["units"][uid] = {"type": typ, "extra": extra, "nostrength": bool(k.get("nostrength"))}
            md.append(f"| {u['name']} | {TYPES[typ][0]} {NAMES[typ]} | {fmt(strong) or '–'} | {fmt(extra) or '–'} | "
                      f"{'; '.join(notes)} |")
            rows.append(typ)
        md.append("")
    from collections import Counter
    c = Counter(rows)
    md[5:5] = [f"**{len(rows)} egység:** " + ", ".join(f"{TYPES[k][0]} {NAMES[k]} {v}" for k, v in c.most_common()), ""]
    open(os.path.join(ROOT, "docs", "EGYSEGTIPUSOK.md"), "w", encoding="utf-8").write("\n".join(md) + "\n")
    json.dump(out, open(os.path.join(ROOT, "docs", "egysegtipusok.json"), "w", encoding="utf-8"), ensure_ascii=False,
              indent=1)
    print(f"docs/EGYSEGTIPUSOK.md + docs/egysegtipusok.json: {len(rows)} egység")


def apply():
    """Az adatláncban (tools/factions_v3.py): minden egység megkapja a típusát (utype), a kivételeit
    (type_bonus), a game.json pedig a típusokat és a szorzó-táblázatot."""
    fdir = os.path.join(ROOT, "data", "factions")
    for fn in sorted(os.listdir(fdir)):
        if not fn.endswith(".json"):
            continue
        p = os.path.join(fdir, fn)
        d = json.load(open(p, encoding="utf-8"))
        for uid, u in d.get("units", {}).items():
            k = KEZI.get(uid, {})
            for key in ("utype", "type_bonus", "type_nostrength"):
                u.pop(key, None)
            u["utype"] = k.get("type") or classify(u)
            extra, _ = suggest_extra(u)
            extra.update(k.get("extra", {}))
            if extra:
                u["type_bonus"] = extra
            if k.get("nostrength"):
                u["type_nostrength"] = True
        for uid, u in d.get("units", {}).items():  # falka: a tagjai típusát kapja
            sg = u.get("spawn_group")
            if sg and sg.get("unit") in d.get("units", {}):
                u["utype"] = d["units"][sg["unit"]]["utype"]
        with open(p, "w", encoding="utf-8") as f:
            f.write(json.dumps(d, ensure_ascii=False, indent=2) + chr(10))
    gp = os.path.join(ROOT, "data", "game.json")
    g = json.load(open(gp, encoding="utf-8"))
    g["unit_types"] = {"types": {k: {"icon": v[0], "name": v[1], "desc": v[2]} for k, v in TYPES.items()},
                       "building": "Épületek", "matrix": MATRIX}
    with open(gp, "w", encoding="utf-8") as f:
        f.write(json.dumps(g, ensure_ascii=False, indent=2) + chr(10))
    print("egységtípusok: minden egység besorolva (utype, type_bonus)")


if __name__ == "__main__":
    main()
elif __name__ == "__apply__":
    apply()
