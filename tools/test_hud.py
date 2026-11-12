"""A kezelőfelület (HUD) automata tesztjei – a TOTU-specifikáció elfogadási pontjai szerint.

    python tools/test_hud.py

Képernyő nélkül fut (SDL dummy). Ellenőrzi: panelsorrend és rácsméretek 3 felbontáson, 1:1 portré,
billentyű = kattintás paritás (ugyanaz a parancs), ismétlés-védelem, modális tiltás, kijelölés-rács,
hős/nem hős középső panel, felső erőforrássáv sorrendje.
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
from rts.hud import KEY_SLOT, RES_ORDER, SLOT_KEYS, hud_layout  # noqa: E402
from rts.ui import UI  # noqa: E402

FAILS = []
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")


def check(cond, msg):
    print(("  OK   " if cond else "  HIBA ") + msg)
    if not cond:
        FAILS.append(msg)


def key(ui, k, up=True):
    ui.handle(pygame.event.Event(pygame.KEYDOWN, key=k, mod=0, unicode=""))
    if up:
        ui.handle(pygame.event.Event(pygame.KEYUP, key=k, mod=0))


def click(ui, pos, button=1):
    ui.handle(pygame.event.Event(pygame.MOUSEBUTTONDOWN, pos=pos, button=button))
    ui.handle(pygame.event.Event(pygame.MOUSEBUTTONUP, pos=pos, button=button))


def test_layout():
    print("Elrendezés")
    for sw, sh in ((1280, 720), (1920, 1080), (2560, 1440)):
        L = hud_layout(sw, sh)
        order = [L[n] for n in ("minimap", "banner1", "char", "inv", "sel", "banner2", "cmd")]
        check(all(a.right <= b.left for a, b in zip(order, order[1:])),
              f"{sw}×{sh}: minimap → banner → karakterlap → tárgyak → kijelölés → banner → parancsrács")
        check(L["hud"].left >= 0 and L["hud"].right <= sw and L["hud"].bottom == sh, f"{sw}×{sh}: a HUD a képernyőn belül, alul")
        check(L["portrait"].w == L["portrait"].h, f"{sw}×{sh}: portré 1:1 ({L['portrait'].w}×{L['portrait'].h})")

        def shape(cells):
            return len({c.x for c in cells}), len({c.y for c in cells})
        check(shape(L["inv_cells"]) == (2, 3), f"{sw}×{sh}: tárgyak 2×3")
        check(shape(L["sel_cells"]) == (4, 3), f"{sw}×{sh}: kijelölés 4×3")
        check(shape(L["cmd_cells"]) == (4, 3), f"{sw}×{sh}: parancsrács 4×3")
        check(L["cmd_cells"][0].w >= 40, f"{sw}×{sh}: parancsikon legalább 40 px ({L['cmd_cells'][0].w})")
        check(abs(L["topbar"].centerx - sw // 2) <= 1, f"{sw}×{sh}: felső sáv középen")
        check(L["allies"].right <= L["resbar"].left and L["resbar"].right <= L["gear"].left,
              f"{sw}×{sh}: [szövetségesek][erőforrások][fogaskerék]")
    check("".join(SLOT_KEYS) == "WERTSDFGXCVB", "billentyűk: W E R T / S D F G / X C V B")
    check([KEY_SLOT[getattr(pygame, "K_" + c.lower())] for c in "WERTSDFGXCVB"] == list(range(12)),
          "billentyű → hely sorrend")
    check(RES_ORDER == ("meteor", "wood", "stone"), "erőforrás-sorrend: meteorit, fa, arany (+ létszám)")


def make(d, fac, size=(1920, 1080)):
    screen = pygame.display.set_mode(size)
    import main as M
    g = Game(d, [(fac, False, "Játékos"), ("croftian", True, "Gép")], seed=3)
    ui = UI(g, screen, g.players[0], M.make_fonts(pygame))
    return g, ui


def frame(g, ui, n=1):
    for _ in range(n):
        g.update(1 / 30)
        ui.update(1 / 30)
        ui.draw(60)


def record(ui, execute=True):
    """A közös parancs-API (UI.cmd) hívásainak naplózása; execute=False: csak rögzít, nem hajt végre."""
    calls = []
    real = ui.cmd

    def spy(name, *a, **kw):
        calls.append((name, tuple(getattr(x, "id", x) if not isinstance(x, list) else tuple(e.id for e in x) for x in a),
                      tuple(sorted(kw.items()))))
        return real(name, *a, **kw) if execute else True
    ui.cmd = spy
    return calls


def test_parity(d):
    print("Billentyű = kattintás")
    for fac in ("wullof", "ursan", "elves", "croftian"):
        g, ui = make(d, fac)
        p = g.players[0]
        frame(g, ui, 2)
        workers = [u for u in g.units if u.owner is p and u.is_worker]
        hq = [b for b in g.buildings if b.owner is p and b.defn["id"] == p.faction["hq"]][0]
        for label, sel in (("munkás", workers[:1]), ("főépület", [hq])):
            for i in range(12):
                for via in ("key", "click"):
                    ui.sel = list(sel)
                    ui.mode, ui.cmd_page, ui.page_no = None, None, 0
                    frame(g, ui)
                    calls = record(ui, execute=False)
                    if via == "key":
                        key(ui, list(KEY_SLOT)[i])
                    else:
                        click(ui, ui.L["cmd_cells"][i].center)
                    res = (tuple(calls), ui.mode and ui.mode[0], ui.cmd_page)
                    ui.cmd = UI.cmd.__get__(ui)
                    if via == "key":
                        r_key = res
                    else:
                        r_click = res
                if r_key != r_click:
                    check(False, f"{fac} {label} {SLOT_KEYS[i]}: billentyű {r_key} ≠ kattintás {r_click}")
                    break
            else:
                check(True, f"{fac} {label}: mind a 12 hely billentyűre és kattintásra ugyanazt teszi")
        # munkás: W = mozgás mód, S = Építés oldal, B = vissza
        ui.sel = workers[:1]
        ui.mode, ui.cmd_page = None, None
        key(ui, pygame.K_w)
        check(ui.mode == ("move",), f"{fac}: W → mozgás-célzás")
        key(ui, pygame.K_ESCAPE)
        key(ui, pygame.K_s)
        check(ui.cmd_page == "build", f"{fac}: munkás S → Építés oldal")
        card = ui.command_card()
        check(card[11] is not None and card[11]["id"] == "back", f"{fac}: Építés oldalon B = Vissza")
        key(ui, pygame.K_b)
        check(ui.cmd_page is None, f"{fac}: B → vissza a fő parancsokhoz")
        # épület: S = első kiképezhető egység → sorba kerül
        ui.sel = [hq]
        frame(g, ui)
        card = ui.command_card()
        n0 = len(hq.queue)
        if card[0] is not None and card[0]["kind"] == "unit":
            p.res.update({k: 9999 for k in p.res})
            key(ui, pygame.K_w)
            check(len(hq.queue) == n0 + 1, f"{fac}: főépület W → {card[0]['label']} kiképzése")


def test_guards(d):
    print("Bemenet-védelem")
    g, ui = make(d, "ursan")
    p = g.players[0]
    frame(g, ui, 2)
    w = [u for u in g.units if u.owner is p and u.is_worker][:1]
    ui.sel = w
    calls = record(ui)
    ui.handle(pygame.event.Event(pygame.KEYDOWN, key=pygame.K_e, mod=0, unicode="e"))
    ui.handle(pygame.event.Event(pygame.KEYDOWN, key=pygame.K_e, mod=0, unicode="e"))  # ismétlés, nincs KEYUP
    ui.handle(pygame.event.Event(pygame.KEYUP, key=pygame.K_e, mod=0))
    check(len([c for c in calls if c[0] == "cmd_stop"]) == 1, "ismétlés: egy lenyomás = egy parancs")
    calls.clear()
    ui.text_focus = True
    key(ui, pygame.K_e)
    ui.text_focus = False
    check(not calls, "szövegbevitel alatt nincs parancs")
    ui.paused = True
    key(ui, pygame.K_e)
    check(not calls, "szünetben nincs parancs")
    ui.paused = False
    p.ascended = True
    p.path = None
    ui.path_hidden = False
    key(ui, pygame.K_e)
    click(ui, ui.L["cmd_cells"][1].center)
    check(not calls, "irányválasztó ablak alatt nincs parancs")
    p.ascended = False
    ui.mode = ("move",)
    key(ui, pygame.K_ESCAPE)
    check(ui.mode is None, "Esc: célzás megszakítása")


def test_selection(d):
    print("Kijelölés és karakterlap")
    g, ui = make(d, "elves")
    p = g.players[0]
    frame(g, ui, 2)
    hq = [b for b in g.buildings if b.owner is p][0]
    ids = [uid for uid, u in p.faction["units"].items() if not d.defs[uid].get("hero") and not d.defs[uid].get("worker")][:3]
    army = []
    for k, uid in enumerate(ids):
        for j in range(4 + k):
            army.append(Unit(g, p, d.defs[uid], hq.x + 120 + j * 14, hq.y + 80 + k * 20))
    g.units += army
    ui.sel = list(army)
    cells = ui.sel_cells()
    check(len(cells) == 3 and [len(c["ents"]) for c in cells] == [4, 5, 6], "vegyes sereg: típusonként összevonva, darabszámmal")
    order1 = [c["ents"][0].defn["id"] for c in cells]
    army[0].hp = army[0].max_hp * 0.2
    order2 = [c["ents"][0].defn["id"] for c in ui.sel_cells()]
    check(order1 == order2, "stabil sorrend sérüléskor")
    army[1].alive = False
    ui.sel = [e for e in ui.sel if e.alive]
    check(len(ui.sel_cells()[0]["ents"]) == 3, "halál után a darabszám frissül")
    ui.sel = [u for u in army if u.defn["id"] == ids[2]]
    check(all(c["kind"] == "one" for c in ui.sel_cells()), "egyféle típus: egyenként (felbontva)")
    frame(g, ui)
    check(ui.selection_kind() == "multi", "többes kijelölés")
    hid = next((uid for uid in p.faction["units"] if d.defs[uid].get("hero")), None)
    if hid:
        h = Unit(g, p, d.defs[hid], hq.x + 100, hq.y + 100)
        g.units.append(h)
        H.recalc(g, h)
        ui.sel = [h] + army[:3]
        check(ui.primary() is h, "hős a kijelölésben: ő az elsődleges (karakterlap, tárgyak)")
        frame(g, ui)
        inv = [r for r, *_ in ui.regions if any(r == c for c in ui.L["inv_cells"])]
        h.inventory[0] = next(iter(d.items))
        frame(g, ui)
        inv = [r for r, l, rr, t in ui.regions if t and t[0] == "item"]
        check(len(inv) == 1, "hősnél a 2×3-as rácsban a tárgyak kattinthatók")
    ui.sel = army[2:3]
    frame(g, ui)
    emb = [t for r, l, rr, t in ui.regions if r == ui.L["inv"]]
    check(bool(emb) and emb[0][1] == p.faction["name"], "nem hősnél a 2×3 helyén a fajcímer")
    ui.sel = [hq]
    frame(g, ui)
    check(ui.selection_kind() == "building" and ui.sel_cells()[0]["ents"][0] is hq, "épület: karakterlap és rács")


if __name__ == "__main__":
    pygame.init()
    d = data.load()
    test_layout()
    test_parity(d)
    test_guards(d)
    test_selection(d)
    print()
    print("HIBÁK:" if FAILS else "Minden HUD-teszt rendben.")
    for f in FAILS:
        print("  -", f)
    sys.exit(1 if FAILS else 0)
