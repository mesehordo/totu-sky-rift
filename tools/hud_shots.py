"""Képernyőképek a kezelőfelületről (docs/hud_kepek/): fajonként hős, egység, épület, többes kijelölés;
valamint 1280×720, 1920×1080 és 2560×1440 felbontás.

    python tools/hud_shots.py [faj,faj,...]
"""
import os
import sys

os.environ["SDL_VIDEODRIVER"] = "dummy"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

import pygame  # noqa: E402

from rts import data, heroes as H  # noqa: E402
from rts.entities import Unit  # noqa: E402
from rts.game import Game  # noqa: E402
from rts.ui import UI  # noqa: E402

OUT = os.path.join(ROOT, "docs", "hud_kepek")


def scene(d, fac, size):
    import main as M
    screen = pygame.display.set_mode(size)
    g = Game(d, [(fac, False, "Játékos"), ("croftian" if fac != "croftian" else "ursan", True, "Gép")], seed=5)
    p = g.players[0]
    ui = UI(g, screen, p, M.make_fonts(pygame))
    ui.settings["edge_scroll"] = False  # a képkészítéskor az egér a sarokban áll
    p.bonus_supply = 30
    p.res.update({"meteor": 185, "wood": 320, "stone": 560})
    hq = [b for b in g.buildings if b.owner is p and b.defn["id"] == p.faction["hq"]][0]
    army = []
    ids = [uid for uid in p.faction["units"] if not d.defs[uid].get("hero") and not d.defs[uid].get("worker")
           and not d.defs[uid].get("path") and not d.defs[uid].get("livestock")][:4]
    for k, uid in enumerate(ids):
        for j in range(5 - k):
            u = Unit(g, p, d.defs[uid], hq.x + 110 + j * 22, hq.y + 70 + k * 26)
            u.hp = u.max_hp * (1 - 0.15 * ((j + k) % 4))
            army.append(u)
    g.units += army
    hero = None
    hid = next((uid for uid in p.faction["units"] if d.defs[uid].get("hero")), None)
    if hid:
        hero = Unit(g, p, d.defs[hid], hq.x + 80, hq.y + 120)
        g.units.append(hero)
        H.add_xp(g, hero, 900)
        its = list(d.items)
        hero.inventory[0], hero.inventory[1], hero.inventory[3] = its[0], its[1], its[2]
        H.recalc(g, hero)
    ui.center_on(hq.x + 120, hq.y + 80)
    return g, ui, hq, army, hero


def frame(g, ui, n=1):
    for _ in range(n):
        g.update(1 / 30)
        ui.update(1 / 30)
        ui.draw(60)


def shoot(g, ui, name):
    frame(g, ui, 2)
    pygame.image.save(ui.screen, os.path.join(OUT, name + ".png"))
    print("  ", name)


def main():
    pygame.init()
    d = data.load()
    os.makedirs(OUT, exist_ok=True)
    facs = sys.argv[1].split(",") if len(sys.argv) > 1 else ["wullof", "ursan", "elves", "croftian"]
    for fac in facs:
        g, ui, hq, army, hero = scene(d, fac, (1920, 1080))
        frame(g, ui, 20)
        if hero is not None:
            ui.sel = [hero]
            shoot(g, ui, f"{fac}_1_hos")
        ui.sel = army[:1]
        shoot(g, ui, f"{fac}_2_egyseg")
        ui.sel = [hq]
        for b in [hq]:
            for uid in list(b.defn.get("produces", ()))[:2]:
                g.cmd_train(g.players[0], uid, b)
        shoot(g, ui, f"{fac}_3_epulet")
        ui.sel = ([hero] if hero else []) + army
        shoot(g, ui, f"{fac}_4_tobbes")
        ui.sel = [u for u in g.units if u.owner is g.players[0] and u.is_worker][:1]
        ui.cmd_page = "build"
        shoot(g, ui, f"{fac}_5_epites")
    for size in ((1280, 720), (2560, 1440)):
        g, ui, hq, army, hero = scene(d, facs[0], size)
        frame(g, ui, 20)
        ui.sel = ([hero] if hero else []) + army
        shoot(g, ui, f"felbontas_{size[0]}x{size[1]}")


if __name__ == "__main__":
    main()
