"""A régi játék csatapályái (data/maps/*.json, leíró formában) kész csempés pályaként az új motorba.

A régi `rts/mapgen.py` `generate(...)` függvényét hívja (a régi kódot nem módosítja), pályánként rögzített
maggal (a pálya azonosítójából) és a pálya alapértelmezett játékosszámával, majd az új motor pályaformátumába
írja: `engine/content/base/maps/<id>.json`. A pályák neve és leírása a .po fájlokba kerül
(`map.<id>.name`, `map.<id>.desc`; az `engine/content/base/locale/{hu,en}.po` „map.” bejegyzéseit cseréli).
Újrafuttatva bájtra azonos kimenetet ad.

Használat:  python tools/palya_export.py [--regi <a régi játék gyökere>]
A régi játék gyökere alapértelmezésben az első olyan szülőmappa, amelyben `rts/mapgen.py` van.
"""
import argparse
import json
import os
import sys
import zlib

ITT = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(ITT)
CEL = os.path.join(REPO, "engine", "content", "base", "maps")
LOCALE = os.path.join(REPO, "engine", "content", "base", "locale")

# a régi téma → az új motor éghajlata (D35) és terepfajtái (a régi GRASS, DIRT, WATER, ROCK sorrendjében)
EGHAJLAT = {"forest": "temperate", "winter": "snowy", "wasteland": "desert", "swamp": "swampy"}
TEREP = {
    "forest": ("grass", "dirt", "deep_water", "rock"),
    "winter": ("snow", "gravel", "deep_water", "rock"),
    "wasteland": ("grass_dry", "gravel", "deep_water", "rock"),
    "swamp": ("grass_lush", "dirt", "deep_water", "rock"),
}
NYERSANYAG = {"stone": "gold", "meteor": "meteor"}   # a régi „stone” az arany


def regi_gyoker(megadott):
    if megadott:
        return os.path.abspath(megadott)
    d = REPO
    while True:
        if os.path.isfile(os.path.join(d, "rts", "mapgen.py")):
            return d
        fel = os.path.dirname(d)
        if fel == d:
            sys.exit("nem találom a régi játékot (rts/mapgen.py): add meg a --regi kapcsolóval")
        d = fel


def terep_jelek():
    with open(os.path.join(REPO, "engine", "content", "base", "templates", "terrain.json"), encoding="utf-8") as f:
        t = json.load(f)
    return {k: v["Terrain"]["char"] for k, v in t.items() if isinstance(v, dict) and "char" in v.get("Terrain", {})}


def mag(map_id):
    return zlib.crc32(map_id.encode("utf-8")) & 0x3FFFFFFF


def export(data, md, jelek, mapgen, en_szotar):
    seed = mag(md["id"])
    m = mapgen.generate(data, md, seed, md["players"])
    W, H = m.w, m.h
    tema = md.get("theme", "forest")
    fajtak = TEREP.get(tema, TEREP["forest"])
    sorok = ["".join(jelek[fajtak[m.terrain[y * W + x]]] for x in range(W)) for y in range(H)]
    fak = [["."] * W for _ in range(H)]
    lelohelyek = []
    for r in m.resources:
        if not r.alive:
            continue
        if r.kind == "wood":
            fak[r.ty][r.tx] = "T"
        else:
            lelohelyek.append({"kind": NYERSANYAG[r.kind], "x": r.tx, "y": r.ty, "size": r.w, "amount": r.amount})
    nev = md.get("name", {})
    leiras = md.get("desc", {})
    doc = {
        "_leiras": leiras.get("hu", ""),
        "_forras": "tools/palya_export.py: a régi data/maps/%s.json, mag %d" % (md["id"], seed),
        "id": md["id"],
        "name": {"hu": nev.get("hu", md["id"]), "en": nev.get("en") or en_szotar.get(nev.get("hu", ""), md["id"])},
        "desc": {"hu": leiras.get("hu", ""), "en": leiras.get("en") or en_szotar.get(leiras.get("hu", ""), "")},
        "players": md["players"],
        "order": md.get("order", 99),
        "width": W,
        "height": H,
        "climate": EGHAJLAT.get(tema, "temperate"),
        "terrain": {"rows": sorok},
    }
    if getattr(m, "teams_hint", None):
        doc["teams"] = list(m.teams_hint)
    if md.get("camps"):
        doc["camps"] = {k: md["camps"][k] for k in sorted(md["camps"])}
    if getattr(m, "layout_decor", None):   # a díszletépületek (a motor még nem helyezi el őket; adatként megmarad)
        doc["_decor"] = m.layout_decor
    doc["objects"] = {
        "starts": [[int(x), int(y)] for x, y in m.starts],
        "trees": {"rows": ["".join(s) for s in fak]},
        "resources": lelohelyek,
    }
    return doc


def po_esc(s):
    return s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")


def po_frissit(path, bejegyzesek, angol):
    """A „map.” kulcsú bejegyzéseket a fejléc utánra írja (a régieket törli)."""
    with open(path, encoding="utf-8", newline="") as f:
        text = f.read().replace("\r\n", "\n")
    vege = text[len(text.rstrip("\n")):] or "\n"   # az eredeti fájlvég megmarad
    blokkok = text.rstrip("\n").split("\n\n")
    marad = [b for b in blokkok[1:] if not b.startswith('msgctxt "map.')]
    uj = []
    for kulcs, hu, en in bejegyzesek:
        uj.append('msgctxt "%s"\nmsgid "%s"\nmsgstr "%s"' % (kulcs, po_esc(hu), po_esc(en) if angol else ""))
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n\n".join([blokkok[0]] + uj + marad) + vege)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--regi", help="a régi játék gyökere (benne rts/ és data/)")
    a = ap.parse_args()
    gy = regi_gyoker(a.regi)
    sys.path.insert(0, gy)
    from rts import data as regi_data, mapgen   # noqa: E402  (a régi kód, változatlanul)
    data = regi_data.load(use_mods=False)
    en_szotar = {}
    en_path = os.path.join(gy, "data", "lang", "en.json")
    if os.path.isfile(en_path):
        with open(en_path, encoding="utf-8") as f:
            for v in json.load(f).values():   # csoportonként (ui, …) magyar → angol
                if isinstance(v, dict):
                    en_szotar.update(v)
    jelek = terep_jelek()
    os.makedirs(CEL, exist_ok=True)
    po = []
    for md in sorted(mapgen.list_maps(), key=lambda d: d["id"]):
        doc = export(data, md, jelek, mapgen, en_szotar)
        with open(os.path.join(CEL, md["id"] + ".json"), "w", encoding="utf-8", newline="\n") as f:
            json.dump(doc, f, ensure_ascii=False, indent=1)
            f.write("\n")
        po.append(("map.%s.name" % md["id"], doc["name"]["hu"], doc["name"]["en"]))
        po.append(("map.%s.desc" % md["id"], doc["desc"]["hu"], doc["desc"]["en"]))
        print("%-22s %d játékos, %dx%d, %d lelőhely" % (md["id"], md["players"], doc["width"], doc["height"],
                                                     len(doc["objects"]["resources"])))
    po_frissit(os.path.join(LOCALE, "hu.po"), po, False)
    po_frissit(os.path.join(LOCALE, "en.po"), po, True)


if __name__ == "__main__":
    main()
