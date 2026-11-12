"""Sprite-ellenőrző: méretek, átlátszóság, képkockák, metaadat-konzisztencia, lefedettség.

    python tools/check_sprites.py            ellenőrzés + összefoglaló
    python tools/check_sprites.py --preview  előnézeti képek: assets/sprites/_preview/

Kilépési kód: 0 = minden rendben, 1 = hiba.
"""
import argparse
import json
import os
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
OUT = os.path.join(ROOT, "assets", "sprites")


def frame_boxes(meta):
    """(oszlop, sor) párok minden képkockára."""
    fw, fh = meta.get("frame_size", meta.get("tile_size"))
    W, H = meta["sheet_size"]
    cols, rows = W // fw, H // fh
    return fw, fh, cols, rows


def check_sheet(rel, errors, warns, stats):
    path = os.path.join(OUT, rel)
    if not os.path.isfile(path):
        errors.append(f"hiányzó metaadat: {rel}")
        return None
    with open(path, encoding="utf-8") as f:
        meta = json.load(f)
    base = os.path.dirname(path)
    img_path = os.path.join(base, meta["image"])
    if not os.path.isfile(img_path):
        errors.append(f"hiányzó kép: {meta['image']}")
        return None
    img = Image.open(img_path)
    name = meta["id"]
    if img.mode != "RGBA":
        errors.append(f"{name}: nem RGBA ({img.mode})")
        img = img.convert("RGBA")
    if list(img.size) != list(meta["sheet_size"]):
        errors.append(f"{name}: méret {img.size} != metaadat {meta['sheet_size']}")
    fw, fh, cols, rows = frame_boxes(meta)
    if img.width % fw or img.height % fh:
        errors.append(f"{name}: a sheet nem osztható a képkockamérettel")
    alpha = img.getchannel("A")
    hist = alpha.histogram()
    transparent = hist[0] / (img.width * img.height)
    if meta["type"] != "tileset":
        if transparent < 0.3:
            errors.append(f"{name}: túl kevés átlátszó pixel ({transparent:.0%})")
        if alpha.getpixel((0, 0)) != 0:
            errors.append(f"{name}: a bal felső sarok nem átlátszó")
    anims = meta.get("animations")
    if anims:
        total = max(a["start"] + a["frames"] for a in anims.values())
        if total != cols:
            errors.append(f"{name}: animációk képkockái ({total}) != oszlopok ({cols})")
        ndirs = len(meta.get("directions", [1]))
        if meta["type"] in ("unit", "projectile") and rows != ndirs:
            errors.append(f"{name}: {rows} sor, de {ndirs} irány")
    elif meta.get("frames") and meta["frames"] != cols:
        errors.append(f"{name}: frames={meta['frames']} != oszlopok {cols}")
    empty = clipped = 0
    for r in range(rows):
        for c in range(cols):
            fr = alpha.crop((c * fw, r * fh, (c + 1) * fw, (r + 1) * fh))
            bb = fr.getbbox()
            stats["frames"] += 1
            if not bb:
                empty += 1
                continue
            if meta["type"] in ("unit", "building") and (bb[0] == 0 or bb[1] == 0 or bb[2] == fw or bb[3] == fh):
                clipped += 1
    if meta["type"] == "tileset":  # a rövidebb sorok végén szándékosan üres cellák vannak
        expected = sum(r.get("variants", cols) for r in meta.get("rows", {}).values()) if "order" not in meta             else rows * cols
        empty = max(0, rows * cols - (rows * cols - empty) - (rows * cols - expected))
    if empty:
        errors.append(f"{name}: {empty} üres képkocka")
    if clipped:
        warns.append(f"{name}: {clipped} képkocka a keret széléig ér (levágás gyanú)")
    tm = meta.get("team_mask")
    if tm:
        t = Image.open(os.path.join(base, tm))
        if t.size != img.size:
            errors.append(f"{name}: csapatmaszk mérete eltér")
    stats["sheets"] += 1
    stats["bytes"] += os.path.getsize(img_path)
    return meta, img


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview", action="store_true")
    args = ap.parse_args()
    man = os.path.join(OUT, "manifest.json")
    if not os.path.isfile(man):
        print("Nincs manifest – futtasd: python tools/make_sprites.py")
        return 1
    with open(man, encoding="utf-8") as f:
        manifest = json.load(f)
    errors, warns = [], []
    stats = {"sheets": 0, "frames": 0, "bytes": 0}
    loaded = []
    for cat in ("units", "buildings", "resources", "projectiles", "effects", "tiles", "props"):
        for key, entry in manifest.get(cat, {}).items():
            rel = entry["meta"] if isinstance(entry, dict) else entry
            res = check_sheet(rel, errors, warns, stats)
            if res:
                loaded.append((cat, key, *res))
            if isinstance(entry, dict) and entry.get("icon"):
                ip = os.path.join(OUT, entry["icon"])
                if not os.path.isfile(ip):
                    errors.append(f"{key}: hiányzó ikon")
                else:
                    ic = Image.open(ip)
                    if ic.mode != "RGBA" or not ic.getbbox():
                        errors.append(f"{key}: hibás ikon")
    from rts import data
    d = data.load(use_mods=False)
    for did, defn in d.defs.items():
        cat = "units" if defn["kind"] == "unit" else "buildings"
        if did not in manifest.get(cat, {}) and not defn.get("sprite"):
            errors.append(f"nincs sprite: {did}")
    for kind, resource in d.game["resources"].items():
        if resource.get("node_amount") and kind not in manifest.get("resources", {}):
            errors.append(f"nincs nyersanyag-sprite: {kind}")
    for f in d.faction_list():
        for u in f["units"].values():
            pk = (u.get("attack") or {}).get("projectile")
            if pk and pk not in manifest.get("projectiles", {}):
                errors.append(f"nincs lövedék-sprite: {pk}")
    print(f"Sheet-ek: {stats['sheets']}, képkockák: {stats['frames']}, méret: {stats['bytes'] / 1e6:.1f} MB")
    for w in warns:
        print("  figyelmeztetés:", w)
    for e in errors:
        print("  HIBA:", e)
    if args.preview:
        pdir = os.path.join(OUT, "_preview")
        os.makedirs(pdir, exist_ok=True)
        for cat, key, meta, img in loaded:
            bg = Image.new("RGBA", img.size, (64, 100, 52, 255))
            bg.alpha_composite(img)
            sc = 2 if img.width < 2400 else 1
            bg.resize((img.width * sc, img.height * sc), Image.NEAREST).save(os.path.join(pdir, f"{cat}_{key}.png"))
        print("Előnézetek:", pdir)
    print("RENDBEN" if not errors else f"{len(errors)} hiba")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
