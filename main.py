"""TOTU: Sky Rift – kreátor: Mesehordo. © 2026 Mesehordo; a kód MIT-licences, a világ és a történet
minden joga fenntartva (lásd LICENSE.md).

TOTU: Sky Rift – belépési pont.

    python main.py                       főmenü
    python main.py --faction ursan --enemy croftian --autostart
    python main.py --simulate 10 --faction elves --enemy croftian   gép a gép ellen teszt (fej nélkül)
    python main.py --mods pelda_mod      csak a megadott modok
"""
import argparse
import os
import random
import sys
import time

from rts import data as gamedata
from rts.ai import AI
from rts.game import Game


def make_fonts(pygame):
    from rts.fonts import make_fonts as mk
    return mk(pygame)


def simulate(data, fa, fb, minutes, seed):
    g = Game(data, [(fa, True, "Gép-1"), (fb, True, "Gép-2")], seed=seed)
    for p in g.players:
        g.ais.append(AI(g, p))
    dt = 1 / data.game["sim_hz"]
    t0 = time.time()
    for i in range(int(minutes * 60 / dt)):
        g.update(dt)
        if i % int(60 / dt) == 0:
            for p in g.players:
                nu = sum(1 for u in g.units if u.owner is p)
                nb = sum(1 for b in g.buildings if b.owner is p)
                print(f"{g.time / 60:5.1f}p {p.faction['name']:13s} egys.={nu:3d} ép.={nb:2d} "
                      f"ell.={p.supply_used:.0f}/{p.supply_cap} ölés={p.kills} fejl.={len(p.researched)}")
        if g.result:
            break
    w = g.result[1].faction["name"] if g.result and g.result[1] else "nincs"
    print(f"Eredmény: győztes = {w}, idő {g.time / 60:.1f} perc, valós idő {time.time() - t0:.1f} mp")
    return g


def skirmish_game(data, mine, enemy, seed, difficulty="normal"):
    g = Game(data, [(mine, False, "Játékos"), (enemy, True, "Gépi ellenfél", {"difficulty": difficulty})],
             seed=seed, creeps=True)
    g.ais.append(AI(g, g.players[1]))
    return g


def custom_game(data, cfg, seed=None):
    """Egyéni csata a beállítóképernyőről: cfg = {"map", "slots": [{kind, faction, team, color, start}], "options"}.
    kind: "human" | "ai_easy" | "ai_normal" | "ai_hard"."""
    import random as _r
    from rts.mapgen import get_map
    md = get_map(cfg["map"], data)
    if md is None:
        raise ValueError(f"Ismeretlen csatapálya: {cfg['map']}")
    rnd = _r.Random(seed)
    facs = [f["id"] for f in data.faction_list()]
    setup = []
    for i, sl in enumerate(cfg["slots"]):
        fid = sl.get("faction") or rnd.choice(facs)
        if fid == "random":
            fid = rnd.choice(facs)
        ai = sl["kind"].startswith("ai")
        opt = {"team": sl.get("team"), "color": sl.get("color", i), "start": sl.get("start"),
               "difficulty": sl["kind"][3:] if ai else "normal", "path": sl.get("path", "auto"),
               "ally": sl.get("ally", "auto")}
        setup.append((fid, ai, sl.get("name") or ("Gép %d" % (i + 1) if ai else "Játékos"), opt))
    o = cfg.get("options", {})
    g = Game(data, setup, seed=seed, mapdef=md, creeps=o.get("creeps", True), options=o)
    for p in g.players:
        if p.is_ai:
            g.ais.append(AI(g, p))
    g.local_id = cfg.get("local", 0)
    return g


def run_game(pygame, screen, fonts, data, game, screenshot=None, spectate=False, app=None, cam=None, seen=None,
             net=None):
    """A játék fő ciklusa. Visszatér: ("menu",) | ("quit",) | ("load", útvonal) | ("restart",).
    Egyéni csatában a meccs parancsnaplója (replay) a profil ``replays`` mappájába kerül."""
    holder = {}
    try:
        return _run_game(pygame, screen, fonts, data, game, screenshot, spectate, app, cam, seen, net, holder)
    finally:
        rec = holder.get("rec")
        if rec is not None and rec.g.tick > 0:
            try:
                _save_replay(rec, app)
            except Exception as e:  # a felvétel hibája ne akassza meg a kilépést
                print("Visszajátszás mentési hiba:", e)


REPLAY_KEEP = 20


def _save_replay(rec, app):
    folder = os.path.join(os.path.dirname(app.profile.saves_dir), "replays")
    facs = "-".join(p.faction["id"] for p in rec.g.players)[:60]
    stem = os.path.join(folder, time.strftime("%Y%m%d_%H%M%S_") + facs)
    path, n = stem + ".trep", 2
    while os.path.exists(path):  # azonos másodpercben indított meccsek ne írják felül egymást
        path, n = f"{stem}_{n}.trep", n + 1
    rec.save(path)
    old = sorted(f for f in os.listdir(folder) if f.endswith(".trep"))
    for f in old[:-REPLAY_KEEP]:
        os.remove(os.path.join(folder, f))


def _run_game(pygame, screen, fonts, data, game, screenshot, spectate, app, cam, seen, net, holder):
    from rts.ui import UI
    g = game
    g.lockstep = net is not None
    if spectate and not any(ai.p is g.players[0] for ai in g.ais):
        g.ais.append(AI(g, g.players[0]))
    player = g.players[getattr(g, "local_id", 0)]
    loader = getattr(app, "loader", None) if app is not None else None
    if app is not None and loader is None:  # betöltött mentés / hálózati játék: itt is legyen betöltőkép
        loader = app.MN.Loader(app, tip=app.loading_tip())
    if loader is not None:
        loader.frame(0.75, "A világ megrajzolása")
    ui = UI(g, screen, player, fonts)
    from rts import audio as AU
    _mid = g.director.m.get("id") if getattr(g, "director", None) is not None else None
    AU.get(app.profile.settings if app is not None else None).game_music(_mid)
    if loader is not None:
        loader.finish()
        app.loader = None
    if app is not None:
        st = app.profile.settings
        ui.settings = st
        ui.speed = st.get("game_speed", 1.0)
    if cam is not None:
        ui.speed = g.speed
    elif getattr(g, "options", {}).get("speed"):
        ui.speed = g.options["speed"]
    g.speed = ui.speed
    if net is not None:  # hálózatban mindenkinél a közös (gazda által beállított) sebesség
        g.speed = ui.speed = float(getattr(g, "options", {}).get("speed", 1.0))
    if app is not None:  # HD grafikánál mindig 2× (a HD-képek natív mérete); a pixelnagyítás csak a retró módé
        ui.zoom = ui.wanted_zoom()
        ui.center_on(*g.players[getattr(g, "local_id", 0)].start_xy)
    ui.net = net
    if net is not None:
        net.local_id = g.local_id
    if cam:
        ui.cam_x, ui.cam_y = cam
        ui.clamp_cam()
    if seen:
        ui.seen = set(seen)
    rec = None
    if app is not None and net is None and g.director is None and not screenshot:
        from rts import replay
        try:
            rec = holder["rec"] = ui.recorder = replay.Recorder(g)
        except Exception as e:  # nem menthető állapot: a játék felvétel nélkül megy
            print("Visszajátszás nem indítható:", e)
    clock = pygame.time.Clock()
    step = 1 / data.game["sim_hz"]
    acc = 0.0
    shot_t = None
    if screenshot:
        # tesztkép: kicsit előre szimulálunk, és kijelöljük a sereget.
        # Az SDL dummy meghajtóban az egér (0,0)-n „fókuszban” áll → a szélgörgetés a bal felső sarokba vinné a kamerát.
        ui.settings = dict(ui.settings, edge_scroll=False)
        for _ in range(int(screenshot[1] / step)):
            g.update(step)
        army = [u for u in g.units if u.owner is g.players[0] and not u.is_worker]
        ui.sel = (army or [u for u in g.units if u.owner is g.players[0]])[:8]
        if army:
            ui.center_on(army[0].x, army[0].y)
        shot_t = 0
    while True:
        cap = int(ui.settings.get("fps_limit", 60) or 0)
        if not pygame.display.get_active() or not pygame.mouse.get_focused() and not pygame.key.get_focused():
            cap = 15  # a háttérben futó ablak ne egye a gépet
        frame = clock.tick(cap) / 1000
        for ev in pygame.event.get():
            if ev.type == pygame.QUIT:
                return ("quit",)
            if ev.type == pygame.VIDEORESIZE:
                screen = pygame.display.set_mode((max(1024, ev.w), max(640, ev.h)), pygame.RESIZABLE)
                ui.resize(screen)
                continue
            if net is not None and ev.type == pygame.KEYDOWN and ev.key in (pygame.K_F5, pygame.K_F9, pygame.K_p):
                continue  # hálózati játékban nincs mentés és szünet
            ui.handle(ev)
        if ui.quit_to_menu:
            if net is not None:
                net.close()
            return ("menu",)
        if app is not None and ui.request:
            req, ui.request = ui.request, None
            if req[0] == "quicksave":
                app.save(g, ui, "Gyorsmentés")
                g.notify(g.players[0], "Gyorsmentés kész (F9: betöltés).", (150, 230, 255))
            elif req[0] == "quickload":
                path = app.quicksave_path()
                if path:
                    return ("load", path)
                g.notify(g.players[0], "Még nincs gyorsmentés.", (255, 160, 90))
        if net is not None and ui.paused:  # hálózaton nincs szünet: a második Esc kilép
            ui.paused = False
            now = pygame.time.get_ticks()
            if now - getattr(ui, "esc_warn", -10000) < 3000:
                net.close()
                return ("menu",)
            ui.esc_warn = now
            g.notify(g.players[g.local_id], "Kilépéshez nyomd meg újra az Esc-et.", (255, 220, 120))
            continue
        if app is not None and ui.paused:
            ui.paused = False
            act = app.pause_menu(g, ui, screen.copy())
            if act is not None:
                return act
            clock.tick()
            continue
        choosing = g.director is not None and (g.director.pending or g.director.cine is not None)
        if net is not None:
            net.pump(g)
            if getattr(net, "lost", False) and g.result is None:
                g.result = ("end", None)
                g.notify(g.players[g.local_id], "Megszakadt a kapcsolat a gazdával.", (255, 120, 90))
            ui.net_wait = net.waiting
            ui.net_desync = net.desync
            if g.result is None:
                acc += min(frame, 0.1) * g.speed  # hálózatban a gazda által beállított közös sebesség
                n = 0
                while acc >= step and n < 8 and g.result is None and net.can_tick():
                    net.before_tick(g)
                    g.update(step)
                    acc -= step
                    n += 1
                if not net.can_tick():
                    acc = min(acc, step)
        elif not ui.paused and not ui.show_help and g.result is None and not choosing:
            acc += min(frame, 0.1) * ui.speed
            n = 0
            while acc >= step and n < 8 and g.result is None:
                g.update(step)
                if rec is not None:
                    rec.after_tick()
                acc -= step
                n += 1
        ui.update(frame)
        ui.draw(clock.get_fps())
        pygame.display.flip()
        if shot_t is not None:
            shot_t += 1
            if shot_t >= 3:
                pygame.image.save(screen, screenshot[0])
                print("Képernyőkép:", screenshot[0])
                return ("quit",)


class App:
    """A menük, a profil és a játékmenetek közti vezérlés."""

    def __init__(self, pygame, data, args):
        from rts import menu as MN
        from rts import profiles as PR
        from rts.sprites import SpriteBank
        self.pg = pygame
        self.MN = MN
        self.data = data
        self.args = args
        self.profile = PR.ensure_profile()
        from rts.i18n import set_lang
        set_lang(self.profile.settings.get("language", "hu"))
        self.apply_display(first=True)
        self.fonts = make_fonts(pygame)
        self.bank = SpriteBank()
        self.bg = MN.Background(data, pygame.display.get_surface().get_size())

    def set_profile(self, p):
        self.profile = p
        p.save()
        from rts.i18n import set_lang
        set_lang(p.settings.get("language", "hu"))

    def apply_display(self, first=False):
        pg = self.pg
        st = self.profile.settings
        res = self.args.size if (first and self.args.size) else st.get("resolution", "1280x760")
        w, h = (int(v) for v in res.lower().split("x"))
        full = st.get("fullscreen") or self.args.fullscreen
        pg.display.set_mode((w, h), pg.FULLSCREEN if full else pg.RESIZABLE)
        if not first:
            self.bg = self.MN.Background(self.data, (w, h))

    # ---- mentés ----------------------------------------------------------
    def save(self, g, ui, slot, frame=None):
        from rts import savegame as SG
        src = frame if frame is not None else ui.screen
        thumb = src.subsurface((0, 0, ui.vw, ui.vh)).copy()
        SG.save_game(g, self.profile.saves_dir, slot, (ui.cam_x, ui.cam_y), ui.seen, thumb,
                     dict(getattr(g, "session", {})))

    def quicksave_path(self):
        from rts import savegame as SG
        for m in SG.list_saves(self.profile.saves_dir):
            if m["slot"] == "Gyorsmentés":
                return m["path"]
        return None

    def pause_menu(self, g, ui, snapshot):
        MN = self.MN
        while True:
            act = MN.PauseMenu(self, snapshot).run()
            if act[0] == "resume":
                return None
            if act[0] == "save":
                name = g.director.m.get("title") if g.director else g.players[0].faction["name"]
                r = MN.SaveMenu(self, f"{name} {int(g.time) // 60}p").run()
                if r and r[0] == "save_as":
                    self.save(g, ui, r[1], snapshot)
                    g.notify(g.players[0], f"Mentve: {r[1]}", (150, 230, 255))
                    return None
                continue
            if act[0] == "settings":  # játék közben is: a jobb felső elemek, grafika, görgetés…
                MN.SettingsMenu(self).run()
                continue
            if act[0] == "music":  # zeneválasztó játék közben
                MN.MusicMenu(self, snapshot).run()
                continue
            if act[0] == "load":
                r = MN.LoadMenu(self, in_game=True).run()
                if r and r[0] == "load_path":
                    return ("load", r[1])
                continue
            return act  # restart | menu | quit

    # ---- játékmenet -------------------------------------------------------
    def _factory_for(self, session):
        from rts import campaign as C
        if session.get("campaign"):
            for camp, _ in C.list_campaigns(self.data):
                if camp["id"] == session["campaign"]:
                    for md in camp["missions_data"]:
                        if md["id"] == session.get("mission_id"):
                            return lambda md=md: C.build_game(self.data, md, flags=self.profile.flags(),
                                                              heroes=self.profile.heroes())
        if session.get("novella"):
            md = C.novella(session["novella"])
            if md is not None:  # önálló pálya: nem visz át jelzőt és hősállapotot a kampányból
                return lambda md=md: C.build_game(self.data, md, flags={})
        if session.get("skirmish"):
            a = session["skirmish"]
            return lambda: skirmish_game(self.data, a[0], a[1], session.get("seed"), a[2])
        if session.get("custom"):
            cfg = session["custom"]
            return lambda: custom_game(self.data, cfg, cfg.get("seed"))
        return None

    def load_with_animation(self, factory):
        """A csata felépítése háttérszálon, közben animált betöltőképernyő (a UI-t a run_game építi, ugyanitt)."""
        import threading
        self.loader = self.MN.Loader(self, tip=self.loading_tip())
        box = {}

        def work():
            try:
                box["g"] = factory()
            except Exception as e:  # a hibát a fő szálon dobjuk tovább
                box["e"] = e
        th = threading.Thread(target=work, daemon=True)
        th.start()
        k = 0.0
        while th.is_alive():
            k += (0.7 - k) * 0.02  # a pálya és a világ felépítése: 0–70%
            self.loader.frame(k, "A pálya felépítése")
        th.join()
        if "e" in box:
            raise box["e"]
        return box["g"]

    TIPS = ["Tipp: F8 – a következő tétlen munkás.", "Tipp: Szóköz – ugrás a bázisra vagy az utolsó riasztáshoz.",
            "Tipp: Shifttel több parancsot is sorba tehetsz.", "Tipp: a főépület gombjáról indul az Égi kegy.",
            "Tipp: a sok egység kevesebb aranyat hoz (Hadtáp).", "Tipp: F1 – súgó és gyorsbillentyűk."]

    def loading_tip(self):
        from rts import tips as TP
        return "Tipp: " + TP.one()

    def play(self, session, game=None, cam=None, seen=None, net=None):
        """Játékmenet; a betöltés és az újrakezdés is itt marad. False = kilépés a programból."""
        from rts import savegame as SG
        from rts.campaign import MissionLayoutError
        while True:
            if game is None:
                factory = self._factory_for(session)
                if factory is None:
                    return True
                try:
                    game = self.load_with_animation(factory)
                except MissionLayoutError as exc:
                    self.loader = None
                    print(f"Küldetés betöltési hiba: {exc}")
                    return True
            game.session = session
            self.in_game = True   # a szünet-, mentés- és beállításmenüben is a csata zenéje szól
            try:
                act = run_game(self.pg, self.pg.display.get_surface(), self.fonts, self.data, game, app=self,
                               cam=cam, seen=seen, net=net, spectate=self.args.spectate and net is None)
            finally:
                self.in_game = False
            if net is not None:  # hálózati játék: nincs újrakezdés / betöltés
                if game.result and not getattr(game, "_recorded", False):
                    me = game.players[game.local_id]
                    self.profile.record_game(me.faction["id"], game.result[0], game.time, me.kills)
                net.close()
                return act[0] != "quit"
            if game.result and not getattr(game, "_recorded", False):
                game._recorded = True
                me = game.players[getattr(game, "local_id", 0)]
                self.profile.record_game(me.faction["id"], game.result[0], game.time, me.kills)
                if game.result[0] == "win" and session.get("campaign"):
                    if game.director is not None:
                        self.profile.set_flags(game.director.new_flags)
                    from rts import heroes as H
                    states = dict(me.hero_states)  # az elesett hősök is fejlődve maradnak
                    states.update({u.defn["id"]: H.snapshot(u) for u in game.units
                                   if u.alive and u.hero and u.owner is me})
                    self.profile.complete_mission(session["campaign"], session["mission_id"], hero_states=states)
                if game.result[0] == "win" and session.get("novella"):
                    self.profile.complete_novella(session["novella"])
            cam = seen = None
            if act[0] == "restart":
                game = None
                continue
            if act[0] == "load":
                try:
                    game, cam, seen, session = SG.load_game(act[1], self.data)
                except Exception as e:  # sérült vagy régebbi verziójú mentés
                    print("Betöltési hiba:", e)
                    return True
                continue
            return act[0] != "quit"

    def loop(self):
        MN = self.MN
        from rts import savegame as SG
        if not MN.intro(self):  # nyitókép
            return
        while True:
            act = MN.MainMenu(self).run()
            if act[0] in ("quit", "back"):
                return
            r = ("back",)
            if act[0] == "skirmish":
                from rts.setup_screen import SetupScreen
                r = SetupScreen(self, "skirmish").run()
                self.toast = None
                if r[0] == "start_custom":
                    r = ("ok",) if self.play({"custom": r[1]}) else ("quit",)
            elif act[0] == "lan":
                from rts.setup_screen import LanMenu
                r = LanMenu(self).run()
                self.toast = None
                if r[0] == "start_lan":
                    session, cfg = r[1]
                    r = ("ok",) if self.play({"custom": cfg, "lan": True}, net=session) else ("quit",)
            elif act[0] == "campaign":
                r = MN.CampaignMenu(self).run()
                if r[0] == "start_mission":
                    _, camp, md = r
                    r = ("ok",) if self.play({"campaign": camp["id"], "mission_id": md["id"]}) else ("quit",)
            elif act[0] == "novellas":
                r = MN.NovellaMenu(self).run()
                if r[0] == "start_novella":
                    r = ("ok",) if self.play({"novella": r[1]["id"]}) else ("quit",)
            elif act[0] == "load":
                r = MN.LoadMenu(self).run()
                if r[0] == "load_path":
                    try:
                        g, cam, seen, session = SG.load_game(r[1], self.data)
                    except Exception as e:
                        print("Betöltési hiba:", e)
                        continue
                    r = ("ok",) if self.play(session, g, cam, seen) else ("quit",)
            elif act[0] == "profile":
                r = MN.ProfileMenu(self).run()
            elif act[0] == "settings":
                r = MN.SettingsMenu(self).run()
            elif act[0] == "credits":
                r = MN.CreditsMenu(self).run()
            elif act[0] == "mods":
                r = MN.ModsMenu(self).run()
            elif act[0] == "stories":
                r = MN.StoriesMenu(self).run()
                if r[0] == "start_novella":
                    r = ("ok",) if self.play({"novella": r[1]["id"]}) else ("quit",)
            if r[0] == "quit":
                return


def _fixed_hash_seed():
    """Hálózati játékhoz minden gépen azonos legyen a szöveg-hash (a halmazok bejárási sorrendje):
    ha nincs rögzítve, a program PYTHONHASHSEED=0-val újraindítja önmagát."""
    if os.environ.get("PYTHONHASHSEED") != "0" and not getattr(sys, "frozen", False):
        import subprocess
        env = dict(os.environ, PYTHONHASHSEED="0")
        sys.exit(subprocess.call([sys.executable] + sys.argv, env=env))


def main():
    _fixed_hash_seed()
    ap = argparse.ArgumentParser(description="TOTU: Sky Rift RTS")
    ap.add_argument("--faction", default=None, help="saját nép: elves | ursan | croftian | wullof | mycel | imperium")
    ap.add_argument("--enemy", default=None, help="ellenfél népe (vagy random)")
    ap.add_argument("--seed", type=int, default=None)
    ap.add_argument("--size", default=None, help="ablakméret, pl. 1600x900 (alapból a profil beállítása)")
    ap.add_argument("--fullscreen", action="store_true")
    ap.add_argument("--autostart", action="store_true")
    ap.add_argument("--mods", default=None, help="vesszővel elválasztott modlista (felülírja a mod.json-t)")
    ap.add_argument("--no-mods", action="store_true")
    ap.add_argument("--simulate", type=float, default=None, help="gép a gép ellen szimuláció N percig, ablak nélkül")
    ap.add_argument("--spectate", action="store_true", help="a saját oldaladat is gép irányítja (nézőmód)")
    ap.add_argument("--screenshot", default=None, help=argparse.SUPPRESS)
    ap.add_argument("--screenshot-time", type=float, default=90, help=argparse.SUPPRESS)
    args = ap.parse_args()
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

    if args.no_mods:
        mods = []
    elif args.mods:
        mods = args.mods.split(",")
    else:  # a főmenü Modok ablakában beállított lista (saves/mods.json)
        from rts import modmgr
        mods = modmgr.enabled_list()
    data = gamedata.load(enabled_mods=mods, use_mods=not args.no_mods)
    for w in data.warnings:
        print("[adat] figyelmeztetés:", w)
    ids = [f["id"] for f in data.faction_list()]
    if args.faction is not None and args.faction not in ids:
        ap.error(f"--faction: ismeretlen nép: {args.faction}")
    if args.enemy is not None and args.enemy != "random" and args.enemy not in ids:
        ap.error(f"--enemy: ismeretlen nép: {args.enemy}")

    if args.simulate is not None:
        import math
        if not math.isfinite(args.simulate) or args.simulate < 0:
            ap.error("--simulate: véges, nem negatív időtartam kell")
        # azonos --seed → azonos fajpár is (reprodukálható mérés); seed nélkül marad a véletlen
        pick = random.Random(args.seed) if args.seed is not None else random
        fa = args.faction or pick.choice(ids)
        fb = args.enemy if args.enemy in ids else pick.choice(ids)
        simulate(data, fa, fb, args.simulate, args.seed)
        return

    if args.screenshot:
        os.environ.setdefault("SDL_VIDEODRIVER", "dummy")
    import pygame
    pygame.init()
    pygame.display.set_caption(data.game.get("title", "TOTU: Sky Rift"))
    if args.screenshot:
        w, h = (int(v) for v in (args.size or "1280x760").lower().split("x"))
        screen = pygame.display.set_mode((w, h))
        fonts = make_fonts(pygame)
        mine = args.faction or ids[0]
        enemy = args.enemy if args.enemy in ids else ids[-1]
        g = skirmish_game(data, mine, enemy, args.seed)
        run_game(pygame, screen, fonts, data, g, (args.screenshot, args.screenshot_time), args.spectate)
        pygame.quit()
        return
    app = App(pygame, data, args)
    if args.autostart:
        mine = args.faction if args.faction in ids else ids[0]
        enemy = args.enemy if args.enemy in ids else random.choice(ids)
        if not app.play({"skirmish": [mine, enemy, "normal"], "seed": args.seed}):
            pygame.quit()
            return
    app.loop()
    pygame.quit()


if __name__ == "__main__":
    main()
