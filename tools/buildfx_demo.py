"""Bemutató: a tavacska és az alapozás építési animációinak változatai (docs/proba/epites_*.png)."""
import json
import os
import sys

os.environ["SDL_VIDEODRIVER"] = "dummy"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
os.chdir(ROOT)
import pygame  # noqa: E402

pygame.init()
pygame.display.set_mode((8, 8))
from rts import buildfx as BF  # noqa: E402

A = "assets/sprites/"


def sheet(rel):
    m = json.load(open(A + rel + ".json", encoding="utf-8"))
    im = pygame.image.load(A + rel + ".png").convert_alpha()
    return m, im


T = 64
tm, ti = sheet("tiles/tileset")


def row(name):
    r = tm["rows"][name]
    return [ti.subsurface((v * 64, r["row"] * 64, 64, 64)).copy() for v in range(r["variants"])]


dm, di = sheet("effects/dust")
rm, ri = sheet("props/rock")
rfw, rfh = rm["frame_size"]
rocks = []
for i in range(rm["frames"]):
    f = ri.subsurface((i * rfw, 0, rfw, rfh)).copy()
    rocks.append(f.subsurface(f.get_bounding_rect()).copy())
sm, si = sheet("resources/stone")
fw = dm["frame_size"][0]
marble = None
p = "assets/ui/foundation.png"
if os.path.isfile(p):
    marble = pygame.image.load(p).convert_alpha()
else:  # ugyanaz a kódból rajzolt márvány, mint a játékban
    import random
    rng = random.Random(7)
    marble = pygame.Surface((32, 32))
    marble.fill((206, 200, 188))
    for _ in range(40):
        x, y = rng.randrange(32), rng.randrange(32)
        pygame.draw.line(marble, (182, 176, 168), (x, y), (x + rng.randrange(-6, 7), y + rng.randrange(2, 6)))
    pygame.draw.rect(marble, (150, 144, 136), (0, 0, 32, 32), 1)
    pygame.draw.line(marble, (232, 228, 220), (1, 1), (30, 1))
    pygame.draw.line(marble, (232, 228, 220), (1, 1), (1, 30))
    marble = pygame.transform.scale(marble, (64, 64))
tex = {"grass": row("grass"), "dirt": row("dirt"), "sand": row("sand"),
       "pond": [pygame.image.load(f"assets/ui/pond_{i}.png").convert_alpha() for i in range(4)],
       "marble": marble, "rocks": rocks, "stone": si.subsurface(si.get_bounding_rect()).copy(),
       "dust": [di.subsurface((i * fw, 0, fw, dm["frame_size"][1])).copy() for i in range(5)]}
F = pygame.font.SysFont(None, 24)
STEPS = [0.1, 0.25, 0.4, 0.55, 0.7, 0.85, 1.0]


def board(kind, variants, n, out):
    cw = (n + 1) * T + 16
    S = pygame.Surface((160 + cw * len(STEPS), (n + 1) * T * len(variants) + 40 + 16 * len(variants)))
    S.fill((28, 28, 30))
    for i, pp in enumerate(STEPS):
        S.blit(F.render(f"{int(pp * 100)}%", True, (240, 230, 190)), (160 + i * cw + cw // 2 - 14, 10))
    for r, (vid, label) in enumerate(variants):
        y0 = 40 + r * ((n + 1) * T + 16)
        S.blit(F.render(label, True, (255, 240, 180)), (8, y0 + (n + 1) * T // 2 - 8))
        for i, pp in enumerate(STEPS):
            x0 = 160 + i * cw
            bg = BF._tiled_var(tex["grass"], (n + 1) * T, (n + 1) * T, T, r)
            S.blit(bg, (x0, y0))
            img, M = (BF.draw_pond if kind == "pond" else BF.draw_foundation)(tex, n, T, pp, vid, 0, *(() if kind == "pond" else (pp * 3,)))
            S.blit(img, (x0 + (n + 1) * T // 2 - img.get_width() // 2, y0 + (n + 1) * T // 2 - img.get_height() // 2))
    pygame.image.save(S, out)
    print(out, S.get_size())


board("pond", [("asas", "A) Kiásás + víz"), ("csempe", "B) Csempénként"), ("iszap", "C) Part + iszap")], 2,
      "docs/proba/epites_tavacska.png")
board("found", [("lapok", "A) Lapról lapra"), ("kituzes", "B) Kitűzés+kavics"), ("kohalom", "C) Kőhalom")], 2,
      "docs/proba/epites_alapozas.png")
