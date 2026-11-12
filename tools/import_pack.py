"""Külső, kézzel festett sprite-csomag importálása a játék formátumába.

    python tools/import_pack.py --base meteor_rts_sprite_alapcsomag_v01.zip --anim meteor_rts_animaciok_v02.zip

A bemenet lehet zip vagy kicsomagolt mappa (``meteor-rts-atlas/1`` alapcsomag és
``meteor-rts-animation/1`` animációs csomag). A megfeleltetés: ``tools/pack_map.json``.

Kimenet: ``assets/pack/`` – saját manifest.json-nal; a játék ezeket a sprite-okat a
generáltak ELŐTT használja (felülírás), ami nincs a csomagban, az marad generált.

A csomag egy nézőirányú (DK); a nyugati irányok tükrözéssel készülnek, a többi
irány a DK képet használja. Ahol a csomagban nincs animáció, az importáló
egyszerű mozgást készít (lépés-ringás, előredőlő támadás, eldőlő halál) – ezek
nem kézzel rajzolt animációk. Ursan kék posztóból csapatszín-maszk készül.
"""
import argparse
import colorsys
import io
import json
import math
import os
import sys
import zipfile

import numpy as np
from PIL import Image, ImageFilter, ImageOps

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
OUT = os.path.join(ROOT, "assets", "pack")
DIRS = ["E", "SE", "S", "SW", "W", "NW", "N", "NE"]
MIRRORED = {"SW", "W", "NW"}
TILE = 32
WORK = 224                 # munkavászon a forgatásokhoz
PIV = (112, 160)           # a talppont a munkavásznon
ALPHA_CUT = 110


class Source:
    """Zip vagy mappa; a gyökér az, ahol a megadott jelzőfájl van."""

    def __init__(self, path, marker):
        self.zip = zipfile.ZipFile(path) if zipfile.is_zipfile(path) else None
        names = self.zip.namelist() if self.zip else [
            os.path.relpath(os.path.join(dp, f), path).replace("\\", "/") for dp, _, fs in os.walk(path) for f in fs]
        hit = [n for n in names if n.endswith(marker)]
        if not hit:
            raise SystemExit(f"Nem találom: {marker} ebben: {path}")
        self.prefix = hit[0][: -len(marker)]
        self.path = path

    def read(self, rel):
        rel = self.prefix + rel
        if self.zip:
            return self.zip.read(rel)
        with open(os.path.join(self.path, rel), "rb") as f:
            return f.read()

    def image(self, rel):
        return Image.open(io.BytesIO(self.read(rel))).convert("RGBA")

    def json_js(self, rel):
        s = self.read(rel).decode("utf-8")
        return json.loads(s[s.index("{"): s.rindex("}") + 1])


# ---------------------------------------------------------------------------
def to_work(img, pivot):
    """128/256-os képkocka a munkavászonra, a talpponttal PIV-re igazítva."""
    w = Image.new("RGBA", (max(WORK, img.width + 64), max(WORK, img.height + 64)), (0, 0, 0, 0))
    px, py = PIV if w.width == WORK else (w.width // 2, w.height - 64)
    w.alpha_composite(img, (px - pivot[0], py - pivot[1]))
    return w, (px, py)


def rot(img, ang, piv, dx=0, dy=0):
    out = img.rotate(ang, resample=Image.BICUBIC, center=piv, translate=(dx, dy))
    return out


def downscale(img, s):
    """Prémultiplikált (sötét perem nélküli) kicsinyítés, majd kemény alfa és enyhe élesítés."""
    w, h = max(1, round(img.width * s)), max(1, round(img.height * s))
    small = img.convert("RGBa").resize((w, h), Image.LANCZOS).convert("RGBA")
    a = np.asarray(small).copy()
    keep = a[..., 3] >= ALPHA_CUT
    a[..., 3] = np.where(keep, 255, 0)
    rgb = Image.fromarray(a[..., :3]).filter(ImageFilter.UnsharpMask(radius=1, percent=70, threshold=2))
    a[..., :3] = np.asarray(rgb)
    a[~keep] = 0
    return Image.fromarray(a, "RGBA")


def team_mask(img, hue_rng):
    """Csapatszín-maszk: a megadott színtartományú (pl. ursan kék) képpontok világossága."""
    a = np.asarray(img).astype(np.float32) / 255
    r, g, b, al = a[..., 0], a[..., 1], a[..., 2], a[..., 3]
    mx, mn = a[..., :3].max(-1), a[..., :3].min(-1)
    sat = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-6), 0)
    d = mx - mn + 1e-6
    hue = np.where(mx == r, ((g - b) / d) % 6, np.where(mx == g, (b - r) / d + 2, (r - g) / d + 4)) * 60
    m = (al > 0.5) & (sat > 0.28) & (mx > 0.16) & (hue >= hue_rng[0]) & (hue <= hue_rng[1])
    out = np.zeros(a.shape, np.uint8)
    lum = np.clip((0.35 + mx * 0.75) * 0.62 * 255, 0, 255)
    out[..., 0] = out[..., 1] = out[..., 2] = lum.astype(np.uint8)
    out[..., 3] = np.where(m, 255, 0)
    return Image.fromarray(out, "RGBA")


def crop_union(frames, anchor, pad=2):
    """frames: [(oszlop, sor, kép, maszk)] azonos méretben -> sheet, maszk, keretméret, horgony."""
    x0 = y0 = 10 ** 9
    x1 = y1 = -1
    for _, _, im, _ in frames:
        bb = im.getbbox()
        if bb:
            x0, y0, x1, y1 = min(x0, bb[0]), min(y0, bb[1]), max(x1, bb[2]), max(y1, bb[3])
    x0, y0 = max(0, x0 - pad), max(0, y0 - pad)
    W, H = x1 + pad - x0, y1 + pad - y0
    cols = max(c for c, *_ in frames) + 1
    rows = max(r for _, r, *_ in frames) + 1
    sheet = Image.new("RGBA", (W * cols, H * rows), (0, 0, 0, 0))
    team = Image.new("RGBA", sheet.size, (0, 0, 0, 0))
    for c, r, im, tm in frames:
        sheet.alpha_composite(im.crop((x0, y0, x0 + W, y0 + H)), (c * W, r * H))
        if tm is not None:
            team.alpha_composite(tm.crop((x0, y0, x0 + W, y0 + H)), (c * W, r * H))
    return sheet, team, (W, H), (anchor[0] - x0, anchor[1] - y0)


def save(img, rel):
    p = os.path.join(OUT, rel)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    img.save(p, optimize=True)


def save_json(rel, obj):
    p = os.path.join(OUT, rel)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w", encoding="utf-8") as f:
        json.dump(obj, f, ensure_ascii=False, indent=1)


# ---------------------------------------------------------------------------
def synth_clips(img, piv, flying):
    """Animáció hiányában egyszerű mozgás a statikus képből (a munkavásznon)."""
    clips = {"idle": [img]}
    walk = []
    for k in range(4):
        ang = (1.6 if k == 1 else -1.6 if k == 3 else 0.0)
        walk.append(rot(img, ang, piv, 0, -3 if k % 2 else 0))
    clips["walk"] = walk
    clips["attack"] = [rot(img, a, piv, dx, 0) for a, dx in ((0, 0), (-5, 3), (-11, 7), (-4, 2))]
    death = []
    for k in range(6):
        f = (k + 1) / 6
        im = rot(img, -84 * min(1, f * 1.25), piv, -6 * f, (16 if flying else 4) * f)
        if k >= 4:
            arr = np.asarray(im).copy()
            arr[..., :3] = (arr[..., :3] * (0.82 if k == 4 else 0.68)).astype(np.uint8)
            im = Image.fromarray(arr, "RGBA")
        death.append(im)
    clips["death"] = death
    return clips


ANIM_INFO = {"idle": (5, True), "walk": (9, True), "attack": (11, False), "death": (8, False)}


def import_unit(uid, pid, base, anim, defn, hue):
    a = base["assets"][pid]
    static = base_src.image(a["file"])
    work, piv = to_work(static, a["pivot"])
    ua = anim.get("units", {}).get(pid) if anim else None
    if ua:  # kézzel rajzolt képkockák
        clips = {}
        for clip in ("idle", "walk", "attack", "death"):
            c = ua["clips"].get(clip)
            if not c:
                continue
            fr = c["directions"]["SE"]["frames"]
            clips[clip] = [to_work(anim_src.image(f["file"]), ua["clips"][clip].get("pivot", c.get("pivot", [64, 112])))[0]
                           for f in fr]
        clips.setdefault("idle", [work])
        syn = synth_clips(work, piv, defn.get("flying"))
        for k in ("walk", "attack", "death"):
            clips.setdefault(k, syn[k])
        kind = "kézzel rajzolt (DK) + tükrözés"
    else:
        clips = synth_clips(work, piv, defn.get("flying"))
        kind = "statikus kép (DK) + tükrözés + generált mozgás"
    bx0, by0, bx1, by1 = a["pixel_bounds"]
    bw, bh = bx1 - bx0, by1 - by0
    s = defn.get("radius", 0.4) * TILE * 2.7 / max(bh, bw * 0.85)
    s = max(0.28, min(0.85, s))
    frames, meta_anims, col = [], {}, 0
    for clip, ims in clips.items():
        fps, loop = ANIM_INFO[clip]
        if ua and clip in ua["clips"]:
            fps = ua["clips"][clip].get("fps", fps)
        meta_anims[clip] = {"start": col, "frames": len(ims), "fps": fps, "loop": loop}
        for i, im in enumerate(ims):
            small = downscale(im, s)
            mir = ImageOps.mirror(small)
            tm = team_mask(small, hue) if hue else None
            tmm = ImageOps.mirror(tm) if tm is not None else None
            for d, name in enumerate(DIRS):
                m = name in MIRRORED
                frames.append((col + i, d, mir if m else small, (tmm if m else tm)))
        col += len(ims)
    anchor = (round(piv[0] * s), round(piv[1] * s))
    sheet, team, (W, H), anchor = crop_union(frames, anchor)
    save(sheet, f"units/{uid}.png")
    meta = {"id": uid, "type": "unit", "image": uid + ".png", "frame_size": [W, H], "anchor": list(anchor),
            "directions": DIRS, "animations": meta_anims, "flying": bool(defn.get("flying")), "pack": pid,
            "pack_kind": kind, "sheet_size": list(sheet.size)}
    if hue:
        save(team, f"units/{uid}_team.png")
        meta["team_mask"] = uid + "_team.png"
    save_json(f"units/{uid}.json", meta)
    icon_rel = import_icon(uid, a)
    return {"meta": f"units/{uid}.json", "icon": icon_rel}


def import_icon(did, a, size=(72, 52)):
    try:
        ic = base_src.image(a["icon"])
    except (KeyError, OSError):
        return None
    h = size[1] - 2
    ic = ic.convert("RGBa").resize((h, h), Image.LANCZOS).convert("RGBA")
    out = Image.new("RGBA", size, (0, 0, 0, 0))
    out.alpha_composite(ic, ((size[0] - h) // 2, 1))
    save(out, f"icons/{did}.png")
    return f"icons/{did}.png"


def import_building(bid, pid, base, defn, found_id, hue):
    a = base["assets"][pid]
    img = base_src.image(a["file"])
    w, h = defn["size"]
    bx0, _, bx1, _ = a["pixel_bounds"]
    s = w * TILE * (1.18 if w >= 3 else 1.3) / (bx1 - bx0)
    px, py = a["pivot"]
    full = downscale(img, s)
    stages = []
    if found_id:
        fa = base["assets"][found_id]
        fimg = downscale(base_src.image(fa["file"]), s)
        for frac in (0.0, 0.3, 0.6, 0.85):
            st = fimg.copy()
            if frac:
                bb = full.getbbox()
                top = bb[1] + int((bb[3] - bb[1]) * (1 - frac))
                part = Image.new("RGBA", full.size, (0, 0, 0, 0))
                part.paste(full.crop((0, top, full.width, full.height)), (0, top))
                st.alpha_composite(part)
            stages.append(st)
    else:  # szerves épület: a talppont körül „kinő”
        for f in (0.35, 0.55, 0.75, 0.9):
            sm = downscale(img, s * f)
            st = Image.new("RGBA", full.size, (0, 0, 0, 0))
            st.alpha_composite(sm, (round(px * s - px * s * f), round(py * s - py * s * f)))
            stages.append(st)
    ox = px * s - w * TILE / 2
    oy = py * s - (h * TILE + 4)
    frames = [(0, 0, full, team_mask(full, hue) if hue else None)]
    for i, st in enumerate(stages):
        frames.append((1 + i, 0, st, team_mask(st, hue) if hue else None))
    sheet, team, (W, H), (ox, oy) = crop_union(frames, (round(ox), round(oy)))
    save(sheet, f"buildings/{bid}.png")
    meta = {"id": bid, "type": "building", "image": bid + ".png", "frame_size": [W, H], "footprint": [w, h],
            "footprint_offset": [ox, oy], "directions": ["S"], "pack": pid,
            "animations": {"idle": {"start": 0, "frames": 1, "fps": 0, "loop": True},
                           "build": {"start": 1, "frames": 4, "fps": 0, "loop": False}},
            "sheet_size": list(sheet.size)}
    if hue:
        save(team, f"buildings/{bid}_team.png")
        meta["team_mask"] = bid + "_team.png"
    save_json(f"buildings/{bid}.json", meta)
    return {"meta": f"buildings/{bid}.json", "icon": import_icon(bid, a)}


def recolor(img, fn):
    a = np.asarray(img).astype(np.float32)
    rgb = a[..., :3] / 255
    mx, mn = rgb.max(-1), rgb.min(-1)
    a[..., :3] = fn(rgb, mx, mn) * 255
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8), "RGBA")


def import_resource(kind, pids, base):
    size = {"wood": 1}.get(kind, 2)
    imgs = []
    for pid in pids:
        a = base["assets"][pid]
        imgs.append((base_src.image(a["file"]), a))
    a0 = imgs[0][1]
    bx0, _, bx1, _ = a0["pixel_bounds"]
    s = (44 if kind == "wood" else size * TILE * 1.12) / (bx1 - bx0)
    variants = []
    for k, (im, a) in enumerate(imgs):
        if kind == "wood" and k >= 2:
            im = ImageOps.mirror(im)
        variants.append(im)
    meaning = "változatok"
    fps = 0
    if kind == "food":  # tele / fél / üres bokor: a piros bogyók elfogynak
        def half(rgb, mx, mn):
            red = (rgb[..., 0] > rgb[..., 1] * 1.5) & (rgb[..., 0] > 0.3)
            out = rgb.copy()
            out[red] = out[red] * np.array([0.55, 0.9, 0.6])
            return out

        def empty(rgb, mx, mn):
            red = (rgb[..., 0] > rgb[..., 1] * 1.3) & (rgb[..., 0] > 0.25)
            out = rgb.copy()
            out[red] = np.stack([rgb[red][:, 1] * 0.6, rgb[red][:, 0] * 0.55, rgb[red][:, 2] * 0.4], -1)
            return out
        variants = [variants[0], recolor(variants[0], half), recolor(variants[0], empty)]
        meaning = "telítettség: tele / fél / üres"
    if kind == "meteor":  # izzás-animáció: a lila kristályok fénylése lüktet
        def glow(f):
            def fn(rgb, mx, mn):
                pur = (rgb[..., 2] > rgb[..., 1] * 1.3) & (rgb[..., 0] > rgb[..., 1])
                out = rgb.copy()
                out[pur] = np.clip(out[pur] * f, 0, 1)
                return out
            return fn
        variants = [recolor(variants[0], glow(f)) for f in (1.0, 1.15, 1.3, 1.15)]
        meaning, fps = "izzás-animáció", 4
    px, py = a0["pivot"]
    frames = []
    for i, v in enumerate(variants):
        frames.append((i, 0, downscale(v, s), None))
    ox = px * s - size * TILE / 2
    oy = py * s - (size * TILE + (2 if kind == "wood" else 4))
    sheet, _, (W, H), (ox, oy) = crop_union(frames, (round(ox), round(oy)))
    save(sheet, f"resources/{kind}.png")
    save_json(f"resources/{kind}.json", {
        "id": kind, "type": "resource", "image": kind + ".png", "frame_size": [W, H], "footprint": [size, size],
        "footprint_offset": [ox, oy], "frames": len(variants), "frames_meaning": meaning, "fps": fps,
        "pack": pids, "sheet_size": list(sheet.size)})
    return f"resources/{kind}.json"


def main():
    global base_src, anim_src
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", required=True, help="alapcsomag (zip vagy mappa)")
    ap.add_argument("--anim", default=None, help="animációs csomag (zip vagy mappa)")
    ap.add_argument("--map", default=os.path.join(ROOT, "tools", "pack_map.json"))
    args = ap.parse_args()
    base_src = Source(args.base, "data/manifest.js")
    base = base_src.json_js("data/manifest.js")
    anim = None
    anim_src = None
    if args.anim:
        anim_src = Source(args.anim, "animation_manifest.json")
        anim = json.loads(anim_src.read("animation_manifest.json").decode("utf-8"))
    mp = json.load(open(args.map, encoding="utf-8"))
    from rts import data
    d = data.load(use_mods=False)
    man = {"version": 1, "source": {"base": os.path.basename(args.base), "anim": os.path.basename(args.anim or "")},
           "note": "Külső sprite-csomag importja (tools/import_pack.py); a generált sprite-ok elé kerül.",
           "units": {}, "buildings": {}, "resources": {}}
    for uid, pid in mp["units"].items():
        if uid not in d.defs or pid not in base["assets"]:
            print("  kihagyva:", uid, pid)
            continue
        hue = mp.get("team_hue", {}).get(d.defs[uid]["faction"])
        man["units"][uid] = import_unit(uid, pid, base, anim, d.defs[uid], hue)
        print("  egység  ", uid, "<-", pid, "(animált)" if anim and pid in anim.get("units", {}) else "")
    for bid, pid in mp["buildings"].items():
        if bid not in d.defs or pid not in base["assets"]:
            print("  kihagyva:", bid, pid)
            continue
        sheet = pid.split(".")[0]
        hue = mp.get("team_hue", {}).get(d.defs[bid]["faction"])
        man["buildings"][bid] = import_building(bid, pid, base, d.defs[bid], mp.get("foundations", {}).get(sheet), hue)
        print("  épület  ", bid, "<-", pid)
    for kind, pids in mp.get("resources", {}).items():
        man["resources"][kind] = import_resource(kind, pids, base)
        print("  nyersanyag", kind)
    save_json("manifest.json", man)
    print("kész:", os.path.join(OUT, "manifest.json"))


if __name__ == "__main__":
    main()
