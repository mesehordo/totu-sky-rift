"""Képlap-eszköz a grafikák elkészítéséhez: szétbontás kockákra, összerakás, háttér- és csapatszín-kulcs.

    python tools/sheet_frames.py bont   <kepek.png> <kepek.json> <mappa>
        a képlapot kockákra bontja: <mappa>/<irány>_<animáció>_<n>.png (pl. S_walk_3.png) – mintának, a rajzhoz

    python tools/sheet_frames.py rak    <mappa> <kepek.json> <kepek.png> [--tukroz] [--hatter auto|#RRGGBB|nincs]
        a (megrajzolt) kockákból újra képlapot rak; a kocka lehet nagyobb is (pl. 4×): arányosan lekicsinyíti.
        A hiányzó nyugati irányokat (SW, W, NW) --tukroz esetén a keletiek tükrözésével pótolja.
        Mellé elkészíti a csapatszín-maszkot (kepek_csapatszin.png) a magenta (#FF00FF) részekből.

    python tools/sheet_frames.py kep    <be.png> <ki.png> <SZxMA> [--hatter …]
        egyetlen kép (ikon, arckép, címer): háttér-kulcs, csapatszín-kulcs, átméretezés (pl. 72x52).

Kulcsszínek: a háttér legyen egyszínű (a sarok színe, vagy amit megadsz – javasolt: #00FF00 zöld, gombáknál
#0000FF kék); a csapatszínű részek tiszta magentával (#FF00FF) festve. A magenta helyére a fő képen világosszürke
kerül (ezt színezi a játék), a maszkra fehér.
"""
import json
import os
import sys

import numpy as np
from PIL import Image

MAGENTA = np.array([255, 0, 255], float)


def load_meta(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def frames_of(meta):
    """(irány, sor) és (animáció, kocka-sorszám, oszlop) listák a leíró alapján."""
    dirs = meta.get("directions") or ["S"]
    anims = meta.get("animations") or {"frame": {"start": 0, "frames": meta.get("frames", 1)}}
    cols = []
    for an, a in anims.items():
        for k in range(a["frames"]):
            cols.append((an, k + 1, a["start"] + k))
    return list(enumerate(dirs)), cols


def cmd_split(sheet, meta_p, out):
    meta = load_meta(meta_p)
    im = Image.open(sheet).convert("RGBA")
    fw, fh = meta["frame_size"]
    os.makedirs(out, exist_ok=True)
    rows, cols = frames_of(meta)
    n = 0
    for r, dname in rows:
        for an, k, c in cols:
            im.crop((c * fw, r * fh, c * fw + fw, r * fh + fh)).save(os.path.join(out, f"{dname}_{an}_{k}.png"))
            n += 1
    print(f"{n} kocka -> {out}")


def keyed(im, bg="auto", tol=60):
    """Háttér eltávolítása kulcsszín alapján + csapatszín (magenta) kinyerése. Vissza: (kép, maszk)."""
    a = np.asarray(im.convert("RGBA")).astype(float)
    rgb, al = a[..., :3], a[..., 3]
    if bg != "nincs":
        if bg == "auto":
            corners = np.array([rgb[0, 0], rgb[0, -1], rgb[-1, 0], rgb[-1, -1]])
            key = np.median(corners, axis=0)
            if al[0, 0] < 10 and al[-1, -1] < 10:  # már átlátszó
                key = None
        else:
            h = bg.lstrip("#")
            key = np.array([int(h[i:i + 2], 16) for i in (0, 2, 4)], float)
        if key is not None:
            dist = np.linalg.norm(rgb - key, axis=2)
            al = np.where(dist < tol, 0, np.where(dist < tol * 1.6, al * (dist - tol) / (tol * 0.6), al))
    dm = np.linalg.norm(rgb - MAGENTA, axis=2)
    team = (dm < 110) & (al > 20)
    light = rgb.mean(axis=2)
    grey = np.clip(150 + (light - 128) * 0.6, 90, 235)
    rgb = np.where(team[..., None], grey[..., None].repeat(3, 2), rgb)
    out = np.dstack([rgb, al]).clip(0, 255).astype(np.uint8)
    mask = np.zeros_like(out)
    mask[team] = [255, 255, 255, 255]
    return Image.fromarray(out, "RGBA"), Image.fromarray(mask, "RGBA")


def fit_frame(im, fw, fh):
    if im.size == (fw, fh):
        return im
    return im.resize((fw, fh), Image.LANCZOS)


def cmd_join(folder, meta_p, out, mirror=False, bg="auto"):
    meta = load_meta(meta_p)
    fw, fh = meta["frame_size"]
    rows, cols = frames_of(meta)
    W, H = meta.get("sheet_size", (fw * len(cols), fh * len(rows)))
    sheet = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    team = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    mirror_of = {"SW": "SE", "W": "E", "NW": "NE"}
    names = dict((d, r) for r, d in rows)
    missing = []
    for r, dname in rows:
        for an, k, c in cols:
            p = os.path.join(folder, f"{dname}_{an}_{k}.png")
            flip = False
            if not os.path.isfile(p) and mirror and dname in mirror_of:
                p = os.path.join(folder, f"{mirror_of[dname]}_{an}_{k}.png")
                flip = True
            if not os.path.isfile(p):
                missing.append(f"{dname}_{an}_{k}.png")
                continue
            im, mk = keyed(Image.open(p), bg)
            im, mk = fit_frame(im, fw, fh), fit_frame(mk, fw, fh)
            if flip:
                im, mk = im.transpose(Image.FLIP_LEFT_RIGHT), mk.transpose(Image.FLIP_LEFT_RIGHT)
                ax = meta.get("anchor", (fw // 2, 0))[0]  # a talppont maradjon a helyén
                shift = 2 * ax - fw
                if shift:
                    im2 = Image.new("RGBA", (fw, fh), (0, 0, 0, 0))
                    mk2 = Image.new("RGBA", (fw, fh), (0, 0, 0, 0))
                    im2.alpha_composite(im, (shift, 0) if shift > 0 else (0, 0), (0, 0) if shift > 0 else (-shift, 0))
                    mk2.alpha_composite(mk, (shift, 0) if shift > 0 else (0, 0), (0, 0) if shift > 0 else (-shift, 0))
                    im, mk = im2, mk2
            sheet.paste(im, (c * fw, r * fh))
            team.paste(mk, (c * fw, r * fh))
    sheet.save(out)
    tp = out[:-4] + "_csapatszin.png"
    team.save(tp)
    print(f"képlap: {out}, csapatszín-maszk: {tp}" + (f"\nHIÁNYZÓ kockák ({len(missing)}): " + ", ".join(missing[:20])
                                                         + (" …" if len(missing) > 20 else "") if missing else ""))
    _ = names
    return 0 if not missing else 2


def cmd_one(src, dst, size, bg="auto"):
    im, mk = keyed(Image.open(src), bg)
    if size:
        w, h = (int(v) for v in size.lower().split("x"))
        im, mk = im.resize((w, h), Image.LANCZOS), mk.resize((w, h), Image.LANCZOS)
    im.save(dst)
    if np.asarray(mk)[..., 3].any():
        mk.save(dst[:-4] + "_csapatszin.png")
        print("csapatszín-maszk:", dst[:-4] + "_csapatszin.png")
    print("kész:", dst)


def main():
    a = sys.argv[1:]
    if not a:
        print(__doc__)
        return 1
    bg = "auto"
    if "--hatter" in a:
        i = a.index("--hatter")
        bg = a[i + 1]
        del a[i:i + 2]
    mirror = "--tukroz" in a
    a = [x for x in a if x != "--tukroz"]
    if a[0] == "bont" and len(a) == 4:
        return cmd_split(a[1], a[2], a[3])
    if a[0] == "rak" and len(a) == 4:
        return cmd_join(a[1], a[2], a[3], mirror, bg)
    if a[0] == "kep" and len(a) in (3, 4):
        return cmd_one(a[1], a[2], a[3] if len(a) == 4 else None, bg)
    print(__doc__)
    return 1


if __name__ == "__main__":
    sys.exit(main() or 0)
