"""A megrajzolt grafikai csomag visszahozása a játékba.

    python tools/import_drawn.py <EGSZAKADAS_grafika mappa vagy .zip>  [--proba]

A csomag.json alapján csak a megváltozott vagy új képeket másolja a helyükre. A pontosan kétszeres méretű rajzot
lekicsinyíti; más méretnél hibát jelez (a képlap rácsa nem csúszhat el). Az eredeti fájlokról biztonsági másolat
készül: assets/_mentes/<időbélyeg>/. --proba: csak kiírja, mit tenne.
"""
import hashlib
import json
import os
import shutil
import sys
import tempfile
import time
import zipfile

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def md5(p):
    with open(p, "rb") as f:
        return hashlib.md5(f.read()).hexdigest()


def fit(src, expected):
    """A rajz a várt méretre: azonos méret → változatlan; kétszeres → lekicsinyítés; más → hiba (None)."""
    im = Image.open(src).convert("RGBA")
    if expected is None or list(im.size) == list(expected):
        return im, None
    ew, eh = expected
    if im.size == (ew * 2, eh * 2):
        return im.resize((ew, eh), Image.LANCZOS), "2× rajz, lekicsinyítve"
    return None, f"rossz méret: {im.size[0]}×{im.size[1]}, várt {ew}×{eh} (vagy pontosan {ew * 2}×{eh * 2})"


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    dry = "--proba" in sys.argv
    if not args:
        print(__doc__)
        return 1
    src = args[0]
    tmp = None
    if src.lower().endswith(".zip"):
        tmp = tempfile.mkdtemp()
        with zipfile.ZipFile(src) as z:
            z.extractall(tmp)
        inner = [x for x in os.listdir(tmp) if os.path.isfile(os.path.join(tmp, x, "csomag.json"))]
        src = os.path.join(tmp, inner[0]) if inner else tmp
    cj = os.path.join(src, "csomag.json")
    if not os.path.isfile(cj):
        print("Nincs csomag.json ebben a mappában:", src)
        return 1
    entries = json.load(open(cj, encoding="utf-8"))["entries"]
    backup = os.path.join(ROOT, "assets", "_mentes", time.strftime("%Y%m%d-%H%M%S"))
    done, errors = [], []
    skipped = 0

    def place(img_src, game_rel, expected, note=""):
        dst = os.path.join(ROOT, game_rel)
        im, msg = fit(img_src, expected)
        if im is None:
            errors.append(f"{game_rel}: {msg}")
            return
        if not dry:
            if os.path.isfile(dst):
                b = os.path.join(backup, game_rel)
                os.makedirs(os.path.dirname(b), exist_ok=True)
                shutil.copyfile(dst, b)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            im.save(dst)
        done.append(game_rel + (f" ({msg})" if msg else "") + note)

    for e in entries:
        if "pack" in e:
            p = os.path.join(src, e["pack"])
            if not os.path.isfile(p):
                continue
            if e.get("md5") and md5(p) == e["md5"]:
                skipped += 1  # nem változott
                continue
            place(p, e["game"], e.get("size"))
        elif "pack_dir" in e:  # új képek gyűjtőmappája
            base = os.path.join(src, e["pack_dir"])
            for dp, _, files in os.walk(base):
                for fn in files:
                    if fn.lower().endswith(".png"):
                        rel = os.path.relpath(os.path.join(dp, fn), base).replace("\\", "/")
                        game = os.path.join(ROOT, e["game_dir"], rel)
                        if os.path.isfile(game) and md5(game) == md5(os.path.join(dp, fn)):
                            skipped += 1  # ugyanaz, mint ami már a játékban van
                            continue
                        place(os.path.join(dp, fn), f"{e['game_dir']}/{rel}", None)
    print(("PRÓBA – nem másoltam semmit.\n" if dry else "") + f"Bekerült: {len(done)}, változatlan: {skipped}, "
          f"hibás: {len(errors)}")
    for x in done:
        print("  +", x)
    for x in errors:
        print("  !", x)
    if done and not dry:
        print(f"Az eredetiek mentése: {backup}\nIndítsd újra a játékot.")
    if tmp:
        shutil.rmtree(tmp, ignore_errors=True)
    return 0 if not errors else 2


if __name__ == "__main__":
    sys.exit(main())
