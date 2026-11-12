"""Képernyőképek a menükről (docs/menu_kepek/): minden menüképernyő 1280×720-ban és 1920×1080-ban.

    python tools/menu_shots.py [felbontás,...]
"""
import os
import sys
import types

os.environ["SDL_VIDEODRIVER"] = "dummy"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

import pygame  # noqa: E402

OUT = os.path.join(ROOT, "docs", "menu_kepek")


def frame(app, scr, hover=None):
    """Egy képkocka a képernyő ciklusából (háttér, tartalom, gombok, rátét)."""
    scr.layout()
    s = pygame.display.get_surface()
    app.bg.draw(s, getattr(scr, "BG", None))
    scr.draw()
    mouse = hover or (-10, -10)
    for b in scr.buttons:
        b.draw(s, scr.fonts, mouse, scr.is_selected(b))
    scr.overlay()
    if hasattr(scr, "after"):
        scr.after(mouse)
    return s


def main():
    import main as M
    from rts import data as D
    from rts import menu as MN
    from rts import setup_screen as SS
    sizes = sys.argv[1].split(",") if len(sys.argv) > 1 else ["1280x720", "1920x1080"]
    os.makedirs(OUT, exist_ok=True)
    pygame.init()
    d = D.load()
    for size in sizes:
        args = types.SimpleNamespace(size=size, fullscreen=False)
        app = M.App(pygame, d, args)
        app.profile.settings["fullscreen"] = False
        screens = [("1_fomenu", MN.MainMenu(app)), ("2_csata", SS.SetupScreen(app, "skirmish")),
                   ("3_kampany", MN.CampaignMenu(app)), ("4_betoltes", MN.LoadMenu(app)),
                   ("5_profil", MN.ProfileMenu(app)), ("6_beallitasok", MN.SettingsMenu(app)),
                   ("7_keszitok", MN.CreditsMenu(app)), ("8_lan", SS.LanMenu(app))]
        mm = MN.MainMenu(app)
        mm.open, mm.open_t = 0, 0
        screens.insert(1, ("1b_fomenu_almenu", mm))
        snap = pygame.display.get_surface().copy()
        screens.append(("9_szunet", MN.PauseMenu(app, snap)))
        for name, scr in screens:
            try:
                hover = None
                if name == "1b_fomenu_almenu":
                    scr.layout()
                    hover = scr.buttons[-2].rect.center
                for _ in range(2):
                    s = frame(app, scr, hover)
                pygame.image.save(s, os.path.join(OUT, f"{size}_{name}.png"))
                print("  ", size, name)
            except Exception as e:  # noqa: BLE001
                import traceback
                traceback.print_exc()
                print("HIBA", size, name, e)


if __name__ == "__main__":
    main()
