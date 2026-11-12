"""HD edition – a kért grafikák listája (docs/HD_GRAFIKA_LISTA.csv és .md).

    python tools/hd_lista.py

Minden képhez: kategória, fájl, mostani (1×) méret, kért HD méret, képkocka és talppont HD-ben, megjegyzés.
A HD méret mindenhol a mostani kétszerese (a háttérképeknél 2560×1440).
"""
import csv
import json
import os
import struct

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
A = os.path.join(ROOT, "assets")
DOCS = os.path.join(ROOT, "docs")

NOTE = {
    "units": "képlap: sorok = 8 irány (E, SE, S, SW, W, NW, N, NE), oszlopok = animációk képkockái, mint most",
    "buildings": "képlap: 4 álló + 4 építési fázis egymás mellett, mint most",
    "resources": "változatok / képkockák egymás mellett, mint most",
    "props": "változatok egymás mellett, mint most",
    "tiles": "csempe 64×64, hézagmentesen illeszkedő; ugyanaz a rács, mint most",
    "effects": "képkockák egymás mellett",
    "projectiles": "sorok = 8 irány, oszlopok = képkockák",
    "icons": "egység-/épületikon",
    "portraits": "arckép, négyzet",
    "abilities": "képesség-ikon",
    "research": "kutatás-ikon",
    "items": "tárgyikon",
    "emblems": "faj-címer",
    "paths": "irány-címer",
    "ui": "kezelőfelület",
}


def png_size(p):
    with open(p, "rb") as f:
        head = f.read(24)
    if head[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    return struct.unpack(">II", head[16:24])


def rows():
    out = []
    sp = os.path.join(A, "sprites")
    for cat in ("units", "buildings", "resources", "props", "tiles", "effects", "projectiles"):
        d = os.path.join(sp, cat)
        if not os.path.isdir(d):
            continue
        for fn in sorted(os.listdir(d)):
            if not fn.endswith(".json"):
                continue
            meta = json.load(open(os.path.join(d, fn), encoding="utf-8"))
            img = meta.get("image")
            if not img or not os.path.isfile(os.path.join(d, img)):
                continue
            w, h = png_size(os.path.join(d, img))
            fs = meta.get("frame_size") or meta.get("tile_size")
            anc = meta.get("anchor") or meta.get("footprint_offset")
            rel = f"sprites/{cat}/{img}"
            team = meta.get("team_mask")
            out.append({
                "kategoria": cat, "fajl": rel + (f" + {team}" if team else ""),
                "most": f"{w}×{h}", "hd": f"{w * 2}×{h * 2}",
                "kepkocka_hd": f"{fs[0] * 2}×{fs[1] * 2}" if fs else "",
                "talppont_hd": f"{round(anc[0] * 2)},{round(anc[1] * 2)}" if anc else "",
                "megjegyzes": NOTE[cat] + ("; csapatszín-maszk ugyanakkora" if team else ""),
            })
    simple = [("icons", "sprites/icons"), ("portraits", "portraits"), ("abilities", "abilities"), ("research", "research"),
              ("items", "items"), ("emblems", "emblems"), ("paths", "paths"), ("ui", "ui")]
    for cat, sub in simple:
        base = os.path.join(A, sub)
        for dp, _, fns in os.walk(base):
            for fn in sorted(fns):
                if not fn.lower().endswith(".png") or fn.endswith("_old.png"):
                    continue
                p = os.path.join(dp, fn)
                sz = png_size(p)
                if not sz:
                    continue
                w, h = sz
                rel = os.path.relpath(p, A).replace("\\", "/")
                if cat == "ui" and "/bg/" in "/" + rel or (cat == "ui" and w >= 1200):
                    hd = "2560×1440"
                    note = "háttérkép 16:9"
                else:
                    hd = f"{w * 2}×{h * 2}"
                    note = NOTE[cat]
                if cat == "ui" and "cursors" in rel:
                    note = "kurzor; a kattintási pont (hotspot) is a kétszeresére kerül"
                if fn.endswith("_team.png"):
                    note = "csapatszín-maszk (fehér a csapatszínű részeken)"
                out.append({"kategoria": cat, "fajl": rel, "most": f"{w}×{h}", "hd": hd, "kepkocka_hd": "",
                            "talppont_hd": "", "megjegyzes": note})
    return out


def main():
    rs = rows()
    os.makedirs(DOCS, exist_ok=True)
    keys = ["kategoria", "fajl", "most", "hd", "kepkocka_hd", "talppont_hd", "megjegyzes"]
    with open(os.path.join(DOCS, "HD_GRAFIKA_LISTA.csv"), "w", encoding="utf-8-sig", newline="") as f:
        w = csv.DictWriter(f, keys, delimiter=";")
        w.writeheader()
        w.writerows(rs)
    by = {}
    for r in rs:
        by.setdefault(r["kategoria"], []).append(r)
    lines = ["# HD edition – kért grafikák (automatikusan generált lista)", "",
             ""
             "A teljes lista táblázatban: `HD_GRAFIKA_LISTA.csv`.", "",
             "| Kategória | Darab | Példa: most → HD |", "|---|---|---|"]
    for cat, items in by.items():
        ex = items[0]
        lines.append(f"| {cat} | {len(items)} | `{ex['fajl'].split(' + ')[0]}`: {ex['most']} → {ex['hd']} |")
    lines.append("")
    for cat, items in by.items():
        lines += [f"## {cat} ({len(items)})", "", "| Fájl | Most | HD | Képkocka (HD) | Talppont (HD) |",
                  "|---|---|---|---|---|"]
        for r in items:
            lines.append(f"| `{r['fajl']}` | {r['most']} | **{r['hd']}** | {r['kepkocka_hd']} | {r['talppont_hd']} |")
        lines.append("")
    open(os.path.join(DOCS, "HD_GRAFIKA_LISTA.md"), "w", encoding="utf-8").write("\n".join(lines))
    print(f"HD-lista: {len(rs)} kép -> docs/HD_GRAFIKA_LISTA.csv / .md")


if __name__ == "__main__":
    main()
