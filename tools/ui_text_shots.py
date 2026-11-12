"""Képernyőképek a játékon belüli szöveges elemekről (docs/hud_kepek/szoveg_*): súgóablak (tooltip) a parancsrács
fölött, F1-súgó, irányválasztó, szünet – 1280×720-ban és 2560×1440-ben. A szövegek olvashatóságának ellenőrzéséhez.

    python tools/ui_text_shots.py [faj]
"""
import os
import sys

os.environ["SDL_VIDEODRIVER"] = "dummy"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
sys.path.insert(0, os.path.join(ROOT, "tools"))

import pygame  # noqa: E402

import hud_shots as HS  # noqa: E402
from rts import data  # noqa: E402

OUT = os.path.join(ROOT, "docs", "hud_kepek")
MOUSE = [(-50, -50)]
pygame.mouse.get_pos = lambda: MOUSE[0]


def main():
    pygame.init()
    d = data.load()
    fac = sys.argv[1] if len(sys.argv) > 1 else "ooni"
    sizes = [tuple(int(v) for v in t.split("x")) for t in sys.argv[2].split(",")] if len(sys.argv) > 2         else ((1280, 720), (2560, 1440))
    for size in sizes:
        tag = f"{size[0]}x{size[1]}"
        g, ui, hq, army, hero = HS.scene(d, fac, size)
        HS.frame(g, ui, 10)
        ui.sel = [hero] if hero is not None else army[:1]
        HS.frame(g, ui, 2)
        cmd = ui.L.get("cmd_cells") or []
        cells = [r for r, *_ in ui.regions if any(r.colliderect(c) for c in cmd)] if cmd else []
        target = cells[1] if len(cells) > 1 else (ui.regions[-1][0] if ui.regions else None)
        if target is not None:
            MOUSE[0] = target.center
        HS.shoot(g, ui, f"szoveg_{tag}_1_sugo")
        MOUSE[0] = (-50, -50)
        ui.sel = [hq]
        HS.frame(g, ui, 2)
        cells = [r for r, *_ in ui.regions if any(r.colliderect(c) for c in cmd)] if cmd else []
        if cells:
            MOUSE[0] = cells[0].center
        HS.shoot(g, ui, f"szoveg_{tag}_2_kepzes")
        MOUSE[0] = (-50, -50)
        ui.show_help = True
        HS.shoot(g, ui, f"szoveg_{tag}_3_f1")
        ui.show_help = False
        ui.p.ascended = True
        ui.p.path = None
        HS.shoot(g, ui, f"szoveg_{tag}_4_irany")
        ui.p.ascended = False


if __name__ == "__main__":
    main()
