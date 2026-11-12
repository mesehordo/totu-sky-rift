"""Közös játékpaletta (a 90-es évek 256 színű játékai szerint) a meglévő sprite-okból és csempékből.

    python tools/make_palette.py        -> assets/sprites/palette.json (224 szín)

A sprite-generátor (engine.palettize) ezt használja rendezett (Bayer) ditherrel, így minden
egység, épület és csempe ugyanabból a színkészletből áll – egységes, retró hatás.
"""
import glob
import json
import os
import random

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets", "sprites", "palette.json")


def main(n=224):
    files = [f for f in glob.glob(os.path.join(ROOT, "assets", "sprites", "*", "*.png")) if "_team" not in f]
    random.Random(1).shuffle(files)
    px = []
    for f in files[:400]:
        a = np.asarray(Image.open(f).convert("RGBA")).reshape(-1, 4)
        a = a[a[:, 3] > 200][:, :3]
        if len(a):
            px.append(a[np.random.default_rng(len(a)).choice(len(a), min(len(a), 4000), replace=False)])
    allpx = np.concatenate(px)
    # szürke rámpa a csapatszín-maszkok alá és a kőnek, a többi színt a képekből
    greys = [[int(v), int(v), int(v)] for v in np.linspace(8, 248, 24)]
    img = Image.fromarray(allpx.reshape(1, -1, 3).astype(np.uint8), "RGB")
    q = img.quantize(colors=n - len(greys), method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = np.asarray(q.getpalette()[: (n - len(greys)) * 3]).reshape(-1, 3).tolist()
    pal = pal + greys
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump({"colors": pal, "note": "közös játékpaletta (tools/make_palette.py)"}, f)
    sw = Image.new("RGB", (16 * 12, ((len(pal) + 15) // 16) * 12))
    for i, c in enumerate(pal):
        sw.paste(tuple(c), ((i % 16) * 12, (i // 16) * 12, (i % 16) * 12 + 12, (i // 16) * 12 + 12))
    sw.save(OUT.replace(".json", ".png"))
    print(len(pal), "szín ->", OUT)


if __name__ == "__main__":
    main()
