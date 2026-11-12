"""Pixel-art sprite-ok generálása a játékadatokból.

    python tools/make_sprites.py            minden sprite (párhuzamosan)
    python tools/make_sprites.py --only elf_tollas_harcos,urs_mammut
    python tools/make_sprites.py --mods pelda_mod   modok egységeivel együtt

Kimenet: assets/sprites/
    manifest.json                 minden elem listája (a játék ezt tölti be)
    units/<id>.png, _team.png     sprite sheet: sorok = 8 irány, oszlopok = animációk képkockái
    units/<id>.json               metaadat (képkockaméret, horgony, animációk, irányok)
    buildings/, resources/, projectiles/, effects/, tiles/, icons/

Igény: numpy, Pillow (csak a generáláshoz – a játék futásához nem kell).
"""
import argparse
import json
import math
import os
import sys
from concurrent.futures import ProcessPoolExecutor

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

from tools.spritegen import buildings as B  # noqa: E402
from tools.spritegen import misc as M  # noqa: E402
from tools.spritegen import units as U  # noqa: E402
from tools.spritegen.engine import CAM_G, DIRS, K_OBL, OUT_SCALE, ellipsoid, palettize, render, render_res  # noqa: E402

HD = OUT_SCALE   # TOTU_HD=2: HD edition – kétszeres felbontás, paletta nélkül, külön mappába
OUT = os.path.join(ROOT, "assets", "sprites")   # HD (2×) – a retró változatot a játék kicsinyítéssel állítja elő
GEN = "tools/make_sprites.py v4 (festett stílus: 2× render, LANCZOS-lekicsinyítés, 3/4-es kamera)"


def _pal(img, *a, **kw):
    """Retró paletta (HD-ben nincs: ott a festett, folyamatos színek maradnak)."""
    return img if HD > 1 else palettize(img, *a, **kw)


def _up(img):
    """1×-ben renderelt kép (lövedék, effekt) HD-méretre."""
    return img if HD == 1 else img.resize((img.width * HD, img.height * HD), Image.LANCZOS)


def _r8(v):
    return int(math.ceil(v / 8.0) * 8)


def _save(img, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path, optimize=True)


def _write_json(path, obj):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(obj, f, ensure_ascii=False, indent=1)


def _blood_extra(color, frame, n, extent):
    """Szabálytalan, növekvő vér-/nedvfolt a tetem alatt a halál-animáció végén."""
    k = frame - (n - 4)
    if k < 0:
        return None
    rng = np.random.default_rng(int(extent * 10))
    pts, nrms = [], []
    for j in range(4):
        r = extent * (0.12 + 0.05 * k) * rng.uniform(0.6, 1.1)
        c = (-extent * 0.25 + rng.uniform(-1, 1) * extent * 0.3, rng.uniform(-1, 1) * extent * 0.25, 0.2)
        p, nn = ellipsoid(c, (r, r * 0.8, 0.3))
        pts.append(p)
        nrms.append(nn)
    P = np.concatenate(pts)
    return P, np.concatenate(nrms), np.repeat(np.asarray(color, float)[None], len(P), 0), np.zeros(len(P), bool)


def _crop_frames(frames, anchor, pad=2):
    """Nagy vásznon renderelt képkockák közös befoglaló téglalapra vágása.
    frames: [(oszlop, sor, kép, csapatmaszk)] – visszatér: (sheet, maszk, (W, H), új horgony)."""
    x0 = y0 = 10 ** 9
    x1 = y1 = -1
    for _, _, im, _ in frames:
        bb = im.getbbox()
        if bb:
            x0, y0 = min(x0, bb[0]), min(y0, bb[1])
            x1, y1 = max(x1, bb[2]), max(y1, bb[3])
    x0, y0 = max(0, x0 - pad), max(0, y0 - pad)
    x1 += pad
    y1 += pad
    W, H = x1 - x0, y1 - y0
    W += W % 2
    H += H % 2
    cols = max(c for c, _, _, _ in frames) + 1
    rows = max(r for _, r, _, _ in frames) + 1
    sheet = Image.new("RGBA", (W * cols, H * rows), (0, 0, 0, 0))
    team = Image.new("RGBA", (W * cols, H * rows), (0, 0, 0, 0))
    for c, r, im, tm in frames:
        sheet.alpha_composite(im.crop((x0, y0, x0 + W, y0 + H)), (c * W, r * H))
        team.alpha_composite(tm.crop((x0, y0, x0 + W, y0 + H)), (c * W, r * H))
    return sheet, team, (W, H), (anchor[0] - x0, anchor[1] - y0)


# ---------------------------------------------------------------------------
def gen_unit(defn):
    m = U.build_unit(defn).compile()
    anims = U.anims_for(m, defn)
    P = m.P
    R = float(np.max(np.hypot(P[:, 0], P[:, 1]))) * 1.1
    Hm = float(P[:, 2].max())
    R2 = max(R, Hm)
    # bőséges vászon, utána közös befoglaló téglalapra vágunk
    W = int(2 * (R2 + Hm) + 40)
    H = int(2 * (R2 + Hm) + 40)
    ax, ay = W // 2, int(K_OBL * Hm + R2 + 20)
    frames = []
    blood = {"croftian": (80, 190, 60)}.get(defn.get("faction"), (140, 18, 18))
    meta_anims = {}
    col = 0
    for name, (n, fps, loop) in anims.items():
        meta_anims[name] = {"start": col, "frames": n, "fps": fps, "loop": loop}
        for i in range(n):
            pz = U.pose(m, name, i, n)
            extra = _blood_extra(blood, i, n, R * 0.8) if name == "death" else None
            for d in range(8):
                im, tm = render_res(m, pz, d * 45, (W, H), (ax, ay), extra=extra, shadow="blob")
                frames.append((col + i, d, im, tm))
        col += n
    sheet, team, (W, H), (ax, ay) = _crop_frames(frames, (ax * HD, ay * HD))
    sheet = _pal(sheet, 48)  # korlátozott, „kézzel pixelezett" paletta
    uid = defn["id"]
    _save(sheet, os.path.join(OUT, "units", uid + ".png"))
    _save(team, os.path.join(OUT, "units", uid + "_team.png"))
    meta = {
        "id": uid, "type": "unit", "image": uid + ".png", "team_mask": uid + "_team.png",
        "frame_size": [W, H], "anchor": [ax, ay], "directions": DIRS,
        "layout": "sorok = irányok (DIRS sorrendben), oszlopok = animációk képkockái egymás után",
        "animations": meta_anims, "flying": bool(defn.get("flying")), "rig": m.rig,
        "sheet_size": [sheet.width, sheet.height], "scale": HD, "generator": GEN,
    }
    _write_json(os.path.join(OUT, "units", uid + ".json"), meta)
    # ikon / portré: nagyított, délkelet felé néző alapkép
    f = min(40.0 / max(1, Hm + 4), 30.0 / max(1, R))
    pz = U.pose(m, "idle", 0, 4)
    pz["_scale"] = (f, f, f)
    ic, it = render_res(m, pz, 45, (160, 160), (80, 115), shadow=False)
    ic = _fit_icon(ic, it)
    _save(ic[0], os.path.join(OUT, "icons", uid + ".png"))
    _save(ic[1], os.path.join(OUT, "icons", uid + "_team.png"))
    return "units", uid, {"meta": f"units/{uid}.json", "icon": f"icons/{uid}.png",
                          "icon_team": f"icons/{uid}_team.png"}


def _fit_icon(img, team, size=(72, 52)):
    """Ikon középre igazítása a látható tartalom alapján."""
    size = (size[0] * HD, size[1] * HD)
    bb = img.getbbox()
    if not bb:
        return img, team
    out = Image.new("RGBA", size, (0, 0, 0, 0))
    out_t = Image.new("RGBA", size, (0, 0, 0, 0))
    crop, crop_t = img.crop(bb), team.crop(bb)
    if crop.width > size[0] - 2 or crop.height > size[1] - 2:
        sc = min((size[0] - 2) / crop.width, (size[1] - 2) / crop.height)
        ns = (max(1, int(crop.width * sc)), max(1, int(crop.height * sc)))
        crop, crop_t = crop.resize(ns, Image.NEAREST), crop_t.resize(ns, Image.NEAREST)
    pos = ((size[0] - crop.width) // 2, (size[1] - crop.height) // 2)
    out.alpha_composite(crop, pos)
    out_t.alpha_composite(crop_t, pos)
    return out, out_t


def gen_building(args):
    defn, style = args
    m = B.build_building(defn, style).compile()
    w, h = defn["size"]
    Hm = m.height
    margin = 40
    W = int(w * 32 + 2 * margin + 0.55 * Hm)
    H = int(h * 32 + 2 * margin + K_OBL * Hm + 0.3 * Hm)
    ox, oy = margin, int(margin + K_OBL * Hm)
    frames = []
    for i in range(4):
        extra = B.anim_extra(m.anim, i, 4)
        im, tm = render_res(m, {}, 0, (W, H), (ox, oy), extra=extra)
        frames.append((i, 0, im, tm))
    for i in range(4):
        frac = (i + 1) / 5
        if style == "organic":
            sxy = 0.45 + 0.5 * frac
            pz = {"_scale": (sxy, sxy, 0.25 + 0.7 * frac)}
            # az alapterület közepe felé zsugorodik
            pz["_offset"] = (w * 16 * (1 / sxy - 1), h * 16 * (1 / sxy - 1), 0)
            extra = None
        else:
            pz = {"_zcut": Hm * frac}
            extra = B.scaffold(w, h, Hm, frac)
        im, tm = render_res(m, pz, 0, (W, H), (ox, oy), extra=extra)
        frames.append((4 + i, 0, im, tm))
    sheet, team, (W, H), (ox, oy) = _crop_frames(frames, (ox * HD, oy * HD))
    sheet = _pal(sheet, 72)
    oy -= round((1 - CAM_G) * h * 32 * HD)  # a 3/4-es kamera a talpat rövidíti: az alja a telek aljára kerül
    bid = defn["id"]
    _save(sheet, os.path.join(OUT, "buildings", bid + ".png"))
    _save(team, os.path.join(OUT, "buildings", bid + "_team.png"))
    meta = {
        "id": bid, "type": "building", "image": bid + ".png", "team_mask": bid + "_team.png", "style": style,
        "frame_size": [W, H], "footprint": [w, h], "footprint_offset": [ox, oy], "directions": ["S"],
        "animations": {"idle": {"start": 0, "frames": 4, "fps": 5, "loop": True},
                       "build": {"start": 4, "frames": 4, "fps": 0, "loop": False,
                                 "note": "építési fázisok: a haladás aránya választja ki"}},
        "sheet_size": [sheet.width, sheet.height], "scale": HD, "generator": GEN,
    }
    _write_json(os.path.join(OUT, "buildings", bid + ".json"), meta)
    full = sheet.crop((0, 0, W, H))
    fullt = team.crop((0, 0, W, H))
    ic = _fit_icon(full, fullt)
    _save(ic[0], os.path.join(OUT, "icons", bid + ".png"))
    _save(ic[1], os.path.join(OUT, "icons", bid + "_team.png"))
    return "buildings", bid, {"meta": f"buildings/{bid}.json", "icon": f"icons/{bid}.png",
                              "icon_team": f"icons/{bid}_team.png"}


def gen_resources():
    out = {}
    specs = {
        "wood": ([M.tree(v).compile() for v in range(M.TREE_VARIANTS)], 1, None),
        "stone": ([M.stone_pile().compile()], 2, None),
        "food": ([M.berry_bush(f).compile() for f in (1.0, 0.5, 0.0)], 2, None),
        "meteor": ([M.crater().compile()] * 4, 2, M.crater_crystals),
    }
    for kind, (models, size, extra_fn) in specs.items():
        Hm = max(float(m.P[:, 2].max()) for m in models) + 14
        margin = 20 if kind == "wood" else 4  # a fák koronája túlnyúlik a csempén (egymásra lógnak)
        W = _r8(size * 32 + 2 * margin + 0.55 * Hm)
        H = _r8(size * 32 + 2 * margin + K_OBL * Hm + 0.3 * Hm)
        ox, oy = margin, int(margin + K_OBL * Hm)
        sheet = Image.new("RGBA", (W * HD * len(models), H * HD), (0, 0, 0, 0))
        for i, m in enumerate(models):
            extra = extra_fn(i, len(models)) if extra_fn else None
            im, _ = render_res(m, {}, 0, (W, H), (ox, oy), extra=extra)
            sheet.alpha_composite(im, (i * W * HD, 0))
        oy -= round((1 - CAM_G) * size * 32)
        W, H, ox, oy = W * HD, H * HD, ox * HD, oy * HD
        _save(sheet, os.path.join(OUT, "resources", kind + ".png"))
        frames_note = {"wood": "változatok", "stone": "egy kép", "food": "telítettség: tele / fél / üres",
                       "meteor": "izzás-animáció"}[kind]
        meta = {"id": kind, "type": "resource", "image": kind + ".png", "frame_size": [W, H], "footprint": [size, size],
                "footprint_offset": [ox, oy], "frames": len(models), "frames_meaning": frames_note,
                "fps": 4 if kind == "meteor" else 0, "sheet_size": [sheet.width, sheet.height], "scale": HD,
                "generator": GEN}
        _write_json(os.path.join(OUT, "resources", kind + ".json"), meta)
        out[kind] = f"resources/{kind}.json"
    return out


def gen_props():
    """Díszletek: a járhatatlan sziklacsempék kőtömbjei."""
    models = [M.rock_prop(v).compile() for v in range(4)]
    Hm = 22
    W = _r8(32 + 8 + 0.55 * Hm)
    H = _r8(32 + 8 + K_OBL * Hm + 0.3 * Hm)
    ox, oy = 4, int(4 + K_OBL * Hm)
    sheet = Image.new("RGBA", (W * HD * len(models), H * HD), (0, 0, 0, 0))
    for i, m in enumerate(models):
        im, _ = render_res(m, {}, 0, (W, H), (ox, oy))
        sheet.alpha_composite(im, (i * W * HD, 0))
    oy -= round((1 - CAM_G) * 32)
    W, H, ox, oy = W * HD, H * HD, ox * HD, oy * HD
    _save(sheet, os.path.join(OUT, "props", "rock.png"))
    meta = {"id": "rock", "type": "prop", "image": "rock.png", "frame_size": [W, H], "footprint": [1, 1],
            "footprint_offset": [ox, oy], "frames": len(models), "frames_meaning": "változatok",
            "sheet_size": [sheet.width, sheet.height], "scale": HD, "generator": GEN}
    _write_json(os.path.join(OUT, "props", "rock.json"), meta)
    doodads = gen_doodads()
    return {"rock": "props/rock.json", "doodads": doodads}


def gen_doodads():
    """Terep-díszletek (virágok, kövek, csontok, tuskók…) – egy sor, változatonként egy képkocka."""
    models = [M.doodad(k).compile() for k in M.DOODADS]
    Hm = 14
    W, H = 40, 40
    ox, oy = 4, int(4 + K_OBL * Hm)
    sheet = Image.new("RGBA", (W * HD * len(models), H * HD), (0, 0, 0, 0))
    for i, m in enumerate(models):
        im, _ = render_res(m, {}, 0, (W, H), (ox, oy), shadow=False)
        sheet.alpha_composite(im, (i * W * HD, 0))
    sheet = _pal(sheet, 48)
    W, H, ox, oy = W * HD, H * HD, ox * HD, oy * HD
    _save(sheet, os.path.join(OUT, "props", "doodads.png"))
    _write_json(os.path.join(OUT, "props", "doodads.json"), {
        "id": "doodads", "type": "prop", "image": "doodads.png", "frame_size": [W, H], "footprint": [1, 1],
        "footprint_offset": [ox, oy], "frames": len(models), "names": M.DOODADS,
        "frames_meaning": "terep-díszletek (nem akadályoznak)", "sheet_size": [sheet.width, sheet.height],
        "scale": HD, "generator": GEN})
    return "props/doodads.json"


def gen_item_icons(data):
    """Leltár-ikonok: assets/items/<tárgy>.png (48 px, ritkaság szerinti keret)."""
    from tools.spritegen.items import render_icon
    out_dir = os.path.join(ROOT, "assets", "items")
    for iid, it in data.items.items():
        _save(render_icon(it.get("icon", iid), it.get("rarity", "common")), os.path.join(out_dir, iid + ".png"))
    print("  tárgyikonok:", len(data.items))


def gen_projectiles():
    out = {}
    frames = {"arrow": 1, "spear": 1, "boulder": 4, "acid": 2, "magic": 4}
    for kind, n in frames.items():
        m = M.projectile(kind).compile()
        S = 24
        sheet = Image.new("RGBA", (S * n, S * 8), (0, 0, 0, 0))
        for i in range(n):
            pz = {}
            if kind == "boulder":
                pz = {"root": (0, i * 90, 0)}
            elif kind in ("acid", "magic"):
                f = 1 + 0.18 * math.sin(i / n * 2 * math.pi)
                pz = {"_scale": (f, f, f)}
            for d in range(8):
                im, _ = render(m, pz, d * 45, (S, S), (S // 2, S // 2), shadow=False)
                sheet.alpha_composite(im, (i * S, d * S))
        sheet = _up(sheet)
        _save(sheet, os.path.join(OUT, "projectiles", kind + ".png"))
        meta = {"id": kind, "type": "projectile", "image": kind + ".png", "frame_size": [S * HD, S * HD],
                "anchor": [S // 2 * HD, S // 2 * HD],
                "directions": DIRS, "animations": {"fly": {"start": 0, "frames": n, "fps": 12, "loop": True}},
                "sheet_size": [sheet.width, sheet.height], "scale": HD, "generator": GEN}
        _write_json(os.path.join(OUT, "projectiles", kind + ".json"), meta)
        out[kind] = f"projectiles/{kind}.json"
    return out


def gen_effects():
    from tools.spritegen.engine import Model
    out = {}
    for kind, (n, fps, S, loop) in M.EFFECTS.items():
        sheet = Image.new("RGBA", (S * n, S), (0, 0, 0, 0))
        empty = Model(kind)
        empty.add(ellipsoid((0, 0, -500), (0.1, 0.1, 0.1)), (0, 0, 0))
        empty.compile()
        ay = int(S * 0.62)
        for i in range(n):
            extra = M.effect_frame(kind, i, n)
            im, _ = render(empty, {}, 0, (S, S), (S // 2, ay), extra=extra, shadow=kind in ("dust",),
                           outline=kind in ("blood", "dust"))
            sheet.alpha_composite(im, (i * S, 0))
        sheet = _up(sheet)
        _save(sheet, os.path.join(OUT, "effects", kind + ".png"))
        meta = {"id": kind, "type": "effect", "image": kind + ".png", "frame_size": [S * HD, S * HD],
                "anchor": [S // 2 * HD, ay * HD], "animations": {"play": {"start": 0, "frames": n, "fps": fps, "loop": loop}},
                "sheet_size": [sheet.width, sheet.height], "scale": HD, "generator": GEN}
        _write_json(os.path.join(OUT, "effects", kind + ".json"), meta)
        out[kind] = f"effects/{kind}.json"
    return out


def gen_tiles():
    TS = 32 * HD
    kinds = list(M.TILE_KINDS.items())
    cols = max(n for _, n in kinds)
    sheet = Image.new("RGBA", (TS * cols, TS * len(kinds)), (0, 0, 0, 0))
    rows = {}
    for r, (kind, n) in enumerate(kinds):
        for v in range(n):
            sheet.paste(M.make_tile(kind, v), (v * TS, r * TS))
        rows[kind] = {"row": r, "variants": n}
    if HD > 1:  # HD: enyhén elsimított, festett terep
        from PIL import ImageFilter
        sheet = sheet.filter(ImageFilter.GaussianBlur(0.55))
    sheet = _pal(sheet, 96, dither=10.0)  # a terep is a közös palettán
    _save(sheet, os.path.join(OUT, "tiles", "tileset.png"))
    order = ["N", "E", "S", "W", "NE", "SE", "SW", "NW"]
    widths = {"narrow": 7, "wide": 11}
    msheet = Image.new("RGBA", (TS * len(order), TS * len(widths)), (0, 0, 0, 0))
    mrows = {}
    for r, (nm, wd) in enumerate(widths.items()):
        masks = M.edge_masks(wd * HD)
        for c, k in enumerate(order):
            tile = Image.new("RGBA", (TS, TS), (255, 255, 255, 0))
            tile.putalpha(masks[k])
            msheet.paste(tile, (c * TS, r * TS))
        mrows[nm] = {"row": r, "width": wd}
    _save(msheet, os.path.join(OUT, "tiles", "masks.png"))
    _write_json(os.path.join(OUT, "tiles", "masks.json"), {
        "id": "masks", "type": "tileset", "image": "masks.png", "tile_size": [TS, TS], "rows": mrows, "order": order,
        "note": "átmeneti alfa-maszkok: oldalak (N,E,S,W) és belső sarkok; a szomszéd terep textúráját vágják ki",
        "sheet_size": [msheet.width, msheet.height], "scale": HD, "generator": GEN})
    meta = {"id": "tileset", "type": "tileset", "image": "tileset.png", "tile_size": [TS, TS], "rows": rows,
            "note": "a víz 4 változata animációs képkockaként is használható; a creep a croftian mélység",
            "sheet_size": [sheet.width, sheet.height], "scale": HD, "generator": GEN}
    _write_json(os.path.join(OUT, "tiles", "tileset.json"), meta)
    return {"tileset": "tiles/tileset.json", "masks": "tiles/masks.json"}


def gen_emblems(data):
    """Faj-címerek: assets/emblems/<faj>.png (256 px) + .json."""
    from tools.spritegen.emblems import DESIGNS, render_emblem
    out_dir = os.path.join(ROOT, "assets", "emblems")
    for fid in data.factions:
        if fid not in DESIGNS:
            continue
        img = render_emblem(fid, 256)
        _save(img, os.path.join(out_dir, fid + ".png"))
        _write_json(os.path.join(out_dir, fid + ".json"), {
            "id": fid, "type": "emblem", "image": fid + ".png", "size": [256, 256],
            "faction": data.factions[fid]["name"], "generator": "tools/spritegen/emblems.py (domborított, fémes)"})
        print("  címer   ", fid)


def _job(job):
    kind, payload = job
    if kind == "unit":
        return gen_unit(payload)
    return gen_building(payload)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", default=None)
    ap.add_argument("--mods", default=None)
    ap.add_argument("--workers", type=int, default=max(1, (os.cpu_count() or 2)))
    ap.add_argument("--emblems", action="store_true", help="csak a faj-címereket generálja")
    ap.add_argument("--misc", action="store_true", help="csak díszletek, effektek, tárgyikonok")
    ap.add_argument("--resources", action="store_true", help="csak a nyersanyag-lelőhelyek")
    ap.add_argument("--world", action="store_true", help="a pálya elemei: csempék, nyersanyagok, díszletek, "
                                                            "lövedékek, effektek (egységek és épületek nélkül)")
    args = ap.parse_args()
    from rts import data
    d = data.load(enabled_mods=args.mods.split(",") if args.mods else None, use_mods=bool(args.mods))
    if args.emblems:
        gen_emblems(d)
        return
    man_path = os.path.join(OUT, "manifest.json")
    if args.world:
        manifest = {"version": 1, "tile_size": 32, "directions": DIRS, "units": {}, "buildings": {}}
        if os.path.isfile(man_path):
            with open(man_path, encoding="utf-8") as fh:
                manifest = json.load(fh)
        manifest["tiles"] = gen_tiles()
        manifest["resources"] = gen_resources()
        manifest["props"] = gen_props()
        manifest["projectiles"] = gen_projectiles()
        manifest["effects"] = gen_effects()
        _write_json(man_path, manifest)
        print("  pálya elemei kész")
        return
    if args.resources:
        manifest = {}
        if os.path.isfile(man_path):
            with open(man_path, encoding="utf-8") as fh:
                manifest = json.load(fh)
        manifest["resources"] = gen_resources()
        _write_json(man_path, manifest)
        return
    if args.misc:
        with open(man_path, encoding="utf-8") as fh:
            manifest = json.load(fh)
        manifest["props"] = gen_props()
        manifest["effects"] = gen_effects()
        if HD == 1:
            gen_item_icons(d)
        _write_json(man_path, manifest)
        return
    only = set(args.only.split(",")) if args.only else None
    jobs = []
    for f in d.faction_list(include_neutral=True):
        for uid in f["units"]:
            if (only is None or uid in only) and not d.defs[uid].get("sprite"):  # hősök: meglévő sprite álnévvel
                jobs.append(("unit", d.defs[uid]))
        for bid in f["buildings"]:
            if d.defs[bid].get("no_sprite") or d.defs[bid].get("sprite"):
                continue  # alapozás-lap; álneves (más sprite-ját használó) épület
            if only is None or bid in only:
                jobs.append(("building", (d.defs[bid], f.get("style", "nordic"))))
    manifest = {"version": 1, "tile_size": 32, "directions": DIRS, "units": {}, "buildings": {}}
    if os.path.isfile(man_path):
        with open(man_path, encoding="utf-8") as fh:
            old = json.load(fh)
        manifest.update({k: v for k, v in old.items() if k not in ("version",)})
    with ProcessPoolExecutor(args.workers) as ex:
        for cat, key, entry in ex.map(_job, jobs):
            manifest[cat][key] = entry
            print(f"  {cat[:-1]:9s} {key}")
    if only is None:
        manifest["resources"] = gen_resources()
        manifest["projectiles"] = gen_projectiles()
        manifest["effects"] = gen_effects()
        manifest["tiles"] = gen_tiles()
        manifest["props"] = gen_props()
        if HD == 1:  # a címerek és tárgyikonok közösek (nem függnek a HD-től)
            gen_emblems(d)
            gen_item_icons(d)
        print("  nyersanyagok, lövedékek, effektek, csempék kész")
    _write_json(man_path, manifest)
    print("manifest:", man_path)


if __name__ == "__main__":
    main()
