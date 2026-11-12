"""Rajzolási próba: textúra-atlasz a valódi TOTU egység-sprite-okból.

    python tools/ab/render/build_atlas.py --types 24 --out %TEMP%/totu_atlas

A kiválasztott egységek walk és attack animációinak minden irányú képkockáját az átlátszó szél levágásával egyetlen
(legfeljebb 4096×4096-os) lapra csomagolja. Kimenet: atlas.png + atlas.json (képkockánként: lapon belüli téglalap,
eltolás a talpponthoz képest). Az atlasz a játék grafikájából készül, ezért NEM kerül a repóba (lásd LICENSE.md).
"""
import argparse
import glob
import json
import os

import sys

from PIL import Image

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
ANIMS = ("walk", "attack")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--types", type=int, default=24)
    ap.add_argument("--size", type=int, default=4096)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    metas = sorted(glob.glob(os.path.join(ROOT, "assets", "sprites", "units", "*.json")))
    metas = [m for m in metas if "_old" not in m]
    pick = metas[:: max(1, len(metas) // a.types)][: a.types]   # egyenletesen a fajok között
    frames = []
    for mp in pick:
        with open(mp, encoding="utf-8") as f:
            meta = json.load(f)
        if meta.get("flying") or not all(k in meta["animations"] for k in ANIMS):
            continue
        sheet = Image.open(os.path.join(os.path.dirname(mp), meta["image"])).convert("RGBA")
        fw, fh = meta["frame_size"]
        ax, ay = meta["anchor"]
        for d in range(len(meta["directions"])):
            for anim in ANIMS:
                an = meta["animations"][anim]
                for k in range(an["frames"]):
                    col = an["start"] + k
                    fr = sheet.crop((col * fw, d * fh, (col + 1) * fw, (d + 1) * fh))
                    bb = fr.getbbox()
                    if bb is None:
                        bb = (0, 0, 1, 1)
                    frames.append({"unit": meta["id"], "dir": d, "anim": anim, "k": k, "img": fr.crop(bb),
                                   "ox": bb[0] - ax, "oy": bb[1] - ay, "fps": an["fps"]})
    # polcos csomagolás magasság szerint csökkenő sorrendben
    order = sorted(range(len(frames)), key=lambda i: -frames[i]["img"].height)
    atlas = Image.new("RGBA", (a.size, a.size), (0, 0, 0, 0))
    x = y = shelf = 0
    for i in order:
        im = frames[i]["img"]
        if x + im.width + 1 > a.size:
            x, y, shelf = 0, y + shelf + 1, 0
        if y + im.height > a.size:
            raise SystemExit(f"Nem fér el {a.size}×{a.size}-es lapon – kevesebb típust válassz (--types).")
        atlas.paste(im, (x, y))
        frames[i]["rect"] = [x, y, im.width, im.height]
        x += im.width + 1
        shelf = max(shelf, im.height)
    os.makedirs(a.out, exist_ok=True)
    atlas.save(os.path.join(a.out, "atlas.png"))
    units = {}
    for fr in frames:
        u = units.setdefault(fr["unit"], {"walk": [[] for _ in range(8)], "attack": [[] for _ in range(8)],
                                          "walk_fps": 10, "attack_fps": 12})
        u[fr["anim"]][fr["dir"]].append({"r": fr["rect"], "o": [fr["ox"], fr["oy"]]})
        u[fr["anim"] + "_fps"] = fr["fps"]
    with open(os.path.join(a.out, "atlas.json"), "w", encoding="utf-8") as f:
        json.dump({"size": a.size, "used_height": y + shelf, "units": units}, f)
    print(f"{len(units)} egységtípus, {len(frames)} képkocka, kihasznált magasság {y + shelf}/{a.size} px → {a.out}")


if __name__ == "__main__":
    main()
