"""Épület-portrék (1:1) az épület képlapjából – a gyártósorban, a kijelölésben és a karakterlapon ezek látszanak.

    python tools/epulet_portrek.py            (csak a hiányzókat és a korábban generáltakat írja)
    python tools/epulet_portrek.py --mind     (a generáltakat újraírja; a kézzel rajzolt portréhoz nem nyúl)

A képlap első álló képkockáját (és a csapatszín-maszkját) kivágja, faji színű, sötét átmenetes háttérre teszi, mögé
finom fényt rajzol, és 256×256-os portrét ment: assets/portraits/<épület>.png + <épület>_csapatszin.png.
A generált fájlok listája: assets/portraits/_generalt_epulet_portrek.json – ezeket a program újraírhatja; ha egy
portrét kézzel lecserélsz (és kiveszed a listából), azt nem írja felül.
"""
import json
import os
import sys

os.environ.setdefault("SDL_VIDEODRIVER", "dummy")
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

import pygame  # noqa: E402

SIZE = 256
BLD = os.path.join(ROOT, "assets", "sprites", "buildings")
OUT = os.path.join(ROOT, "assets", "portraits")
LIST = os.path.join(OUT, "_generalt_epulet_portrek.json")


def background(col):
    bg = pygame.Surface((SIZE, SIZE))
    top = tuple(int(c * 0.42) for c in col)
    for y in range(SIZE):  # felül a faj színe sötéten, alul szinte fekete (mint az egység-portrékon)
        t = y / (SIZE - 1)
        pygame.draw.line(bg, tuple(int(top[k] * (1 - t) + 10 * t) for k in range(3)), (0, y), (SIZE, y))
    glow = pygame.Surface((SIZE, SIZE), pygame.SRCALPHA)
    for r in range(SIZE // 2, 0, -4):  # fény az épület mögött
        a = int(70 * (1 - r / (SIZE / 2)) ** 1.6)
        pygame.draw.ellipse(glow, (255, 230, 170, a), (SIZE / 2 - r, SIZE * 0.52 - r * 0.8, 2 * r, 1.6 * r))
    bg.blit(glow, (0, 0))
    return bg


def vignette(surf):
    v = pygame.Surface((SIZE, SIZE), pygame.SRCALPHA)
    for i in range(18):
        pygame.draw.rect(v, (0, 0, 0, 10), (i, i, SIZE - 2 * i, SIZE - 2 * i), 1)
    surf.blit(v, (0, 0))


def make(bid, col):
    meta = json.load(open(os.path.join(BLD, bid + ".json"), encoding="utf-8"))
    img = pygame.image.load(os.path.join(BLD, meta["image"])).convert_alpha()
    fw, fh = meta["frame_size"]
    fr = img.subsurface((0, 0, fw, fh))
    team = None
    if meta.get("team_mask") and os.path.isfile(os.path.join(BLD, meta["team_mask"])):
        team = pygame.image.load(os.path.join(BLD, meta["team_mask"])).convert_alpha().subsurface((0, 0, fw, fh))
    box = fr.get_bounding_rect(min_alpha=20)
    if box.w < 4 or box.h < 4:
        return None
    pad = 14
    k = min((SIZE - 2 * pad) / box.w, (SIZE - 2 * pad) / box.h)
    w, h = max(1, int(box.w * k)), max(1, int(box.h * k))
    x, y = (SIZE - w) // 2, SIZE - pad - h + 4
    main = pygame.transform.smoothscale(fr.subsurface(box), (w, h))
    out = background(col)
    shadow = pygame.Surface((SIZE, SIZE), pygame.SRCALPHA)
    pygame.draw.ellipse(shadow, (0, 0, 0, 90), (x + w * 0.1, y + h - h * 0.12, w * 0.8, h * 0.16))
    out.blit(shadow, (0, 0))
    out.blit(main, (x, y))
    vignette(out)
    mask = pygame.Surface((SIZE, SIZE), pygame.SRCALPHA)
    if team is not None:
        mask.blit(pygame.transform.smoothscale(team.subsurface(box), (w, h)), (x, y))
    return out, mask


def main():
    allmode = "--mind" in sys.argv
    pygame.init()
    pygame.display.set_mode((1, 1))
    from rts import data
    d = data.load()
    done = set(json.load(open(LIST, encoding="utf-8"))) if os.path.isfile(LIST) else set()
    made, kept = [], 0
    for fn in sorted(os.listdir(BLD)):
        if not fn.endswith(".json"):
            continue
        bid = fn[:-5]
        dst = os.path.join(OUT, bid + ".png")
        if os.path.isfile(dst) and bid not in done:
            kept += 1  # kézzel rajzolt portré: nem írjuk felül
            continue
        if os.path.isfile(dst) and not allmode:
            made.append(bid)
            continue
        df = d.defs.get(bid, {})
        f = d.factions.get(df.get("faction"), {})
        res = make(bid, tuple(f.get("color", (90, 90, 100))))
        if res is None:
            continue
        out, mask = res
        pygame.image.save(out, dst)
        pygame.image.save(mask, os.path.join(OUT, bid + "_csapatszin.png"))
        made.append(bid)
    json.dump(sorted(set(made)), open(LIST, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    print(f"épület-portrék: {len(made)} generált, {kept} kézzel rajzolt megtartva")


if __name__ == "__main__":
    main()
