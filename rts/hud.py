"""Alsó HUD és felső sáv (TOTU: Sky Rift kezelőfelület-specifikáció szerint).

Alsó sáv balról jobbra (kötött sorrend):
    minimap | fajbanner | karakterlap (1:1 portré + adatok) | tárgyak 2×3 (nem hősnél: fajcímer)
    | kijelölt egységek 4×3 | fajbanner | parancsrács 4×3 (W E R T / S D F G / X C V B)
Felül középen: [szövetségesek] [meteorit | fa | arany | létszám] [fogaskerék = menü]
Bal felül: háttér nélküli, árnyékolt küldetéslista.

A geometria a hud_layout()-ban van (tiszta függvény, tesztelhető); a faj csak a kinézetet (skin) változtatja.
Billentyű és kattintás ugyanazt hívja: activate_slot() → a parancs saját függvénye → UI.cmd() (közös parancs-API,
hálózaton lockstep). A szimuláció dönt a jogosultságról, költségről, újratöltésről.
"""
import math

import pygame

from . import heroes as H
from .fonts import load_font
from .render import shade
from .world import TILE

SLOT_KEYS = "WERTSDFGXCVB"                      # 4 oszlop × 3 sor, a jobb alsó sarokban kiírva
KEY_SLOT = {getattr(pygame, "K_" + c.lower()): i for i, c in enumerate(SLOT_KEYS)}
RES_ORDER = ("meteor", "wood", "stone")         # meteorit, fa, arany (az arany belső kulcsa: "stone")
CTX_SLOTS = list(range(4, 12))                   # S D F G X C V B: kontextus-akciók
HUD_MAX_H = 0.33          # az alsó sáv legfeljebb a képernyőmagasság ennyied része
SEL_GRID_MODE = "auto"   # "auto": egyféle típusnál egyenként, vegyes seregnél típusonként összevonva | "group" | "single"


# ---------------------------------------------------------------------------
# geometria
def _layout(sw, sh, s):
    R = pygame.Rect
    pad, gap, cg = max(4, round(8 * s)), max(6, round(11 * s)), max(2, round(4 * s))
    fp = max(3, round(5 * s))                     # a szekciók saját kerete
    cell = (round(214 * s) - 2 * pad - 2 * fp - 2 * cg) // 3
    inner = cell * 3 + cg * 2
    H_ = inner + 2 * fp + 2 * pad
    g2, g4 = cell * 2 + cg, cell * 4 + cg * 3
    ban = max(12, round(22 * s))
    bh, bgap = max(7, round(12 * s)), max(2, round(3 * s))
    P = inner - 2 * (bh + bgap)                   # portré 1:1, alatta életerő és mana/energia
    gin = max(5, round(9 * s))
    stats = round(176 * s)
    names = ("minimap", "banner1", "char", "inv", "sel", "banner2", "cmd")
    widths = (inner, ban, P + gin + stats, g2, g4, ban, g4)
    framed = {n: not n.startswith("banner") for n in names}
    W = 2 * pad + sum(widths) + gap * (len(widths) - 1) + sum(2 * fp for n in names if framed[n])
    x0, y0 = (sw - W) // 2, sh - H_
    L = {"s": s, "pad": pad, "gap": gap, "fp": fp, "cell": cell, "cg": cg, "hud": R(x0, y0, W, H_), "frames": []}
    x = x0 + pad
    for n, w in zip(names, widths):
        if framed[n]:
            L[n] = R(x + fp, y0 + pad + fp, w, inner)
            L["frames"].append(L[n].inflate(2 * fp, 2 * fp))
            x += w + 2 * fp + gap
        else:
            L[n] = R(x, y0 + pad, w, inner + 2 * fp)
            x += w + gap
    c = L["char"]
    L["portrait"] = R(c.x, c.y, P, P)
    L["hpbar"] = R(c.x, c.y + P + bgap, P, bh)
    L["mpbar"] = R(c.x, c.y + P + 2 * bgap + bh, P, bh)
    L["stats"] = R(c.x + P + gin, c.y, stats, inner)
    L["divider"] = c.x + P + gin // 2

    def grid(r, cols):
        return [R(r.x + (i % cols) * (cell + cg), r.y + (i // cols) * (cell + cg), cell, cell) for i in range(cols * 3)]
    L["inv_cells"], L["sel_cells"], L["cmd_cells"] = grid(L["inv"], 2), grid(L["sel"], 4), grid(L["cmd"], 4)
    th, rw, ty = max(24, round(34 * s)), round(128 * s), max(4, round(6 * s))
    TW = th + gap + 4 * rw + gap + th
    tx = (sw - TW) // 2
    L["allies"] = R(tx, ty, th, th)
    L["resbar"] = R(tx + th + gap, ty, 4 * rw, th)
    L["res"] = [R(tx + th + gap + i * rw, ty, rw, th) for i in range(4)]
    L["gear"] = R(tx + th + gap + 4 * rw + gap, ty, th, th)
    L["faith"] = R(tx + th + gap + 4 * rw + gap + th + gap, ty, round(rw * 0.9), th)  # Ö17: Áhítat / Tóének
    L["topbar"] = R(tx, ty, TW, th)
    iw = round(th * 1.9)
    hud = L["hud"]  # tétlen munkások jelzője: jobbra, közvetlenül a HUD fölött (kattintás / F8: a következő)
    L["idle"] = R(hud.right - iw - max(6, round(10 * s)), hud.y - th - max(2, round(4 * s)), iw, th)
    L["quest"] = (2 * pad + 4, ty + th + max(4, round(8 * s)))
    L["title"] = (2 * pad + 4, ty)
    L["font"] = {"name": max(12, round(19 * s)), "small": max(10, round(14 * s)), "tiny": max(10, round(12 * s)),
                 "key": max(10, round(13 * s)), "res": max(11, round(16 * s))}
    return L


def hud_layout(sw, sh):
    """A HUD összes téglalapja az adott képernyőméretre (16:9 a referencia; keskenyebb ablakon kicsinyít)."""
    # a sáv kitölti a képernyő szélességét (referencia: TOTU HUD-kép), de legfeljebb a magasság harmada
    base = _layout(sw, sh, 1.0)
    s = min((sw - 8) / base["hud"].w, sh * HUD_MAX_H / base["hud"].h)
    L = _layout(sw, sh, s)
    while L["hud"].w > sw - 8 and s > 0.3:
        s *= 0.98
        L = _layout(sw, sh, s)
    return L


_FONTS = {}


def hud_font(size, bold=False):
    key = (size, bold)
    if key not in _FONTS:
        from .i18n import TFont
        _FONTS[key] = TFont(load_font(pygame, size, bold=bold))
    return _FONTS[key]


def shadow_text(s, font, text, pos, col, sh=(0, 0, 0)):
    """Háttér nélküli, finoman árnyékolt felirat (küldetéslista, óra)."""
    img = font.render(text, True, sh)
    for ox, oy in ((1, 1), (2, 2), (-1, 1)):
        s.blit(img, (pos[0] + ox, pos[1] + oy))
    out = font.render(text, True, col)
    s.blit(out, pos)
    return out.get_width()


# ---------------------------------------------------------------------------
class HudMixin:
    """A UI osztály keveréke: a HUD állapota (view-model), bemenete és rajzolása."""

    cmd_page = None          # None | "build" | "learn" (a parancsrács aloldala)
    page_no = 0
    quests_open = False
    show_allies = False
    text_focus = False       # szövegbevitel alatt nincs játékparancs (most nincs játékon belüli szövegmező)

    # ---------------------------------------------------------------- állapot
    def hud_resize(self):
        self.L = hud_layout(self.sw, self.sh)
        self.mm_rect = self.L["minimap"]
        self.mini = None
        self.mini_base_ver = -1
        self._hud_bg = None

    def F(self, kind, bold=None):
        return hud_font(self.L["font"][kind], kind in ("name", "key", "res") if bold is None else bold)

    def over_hud(self, mx, my):
        L = self.L
        if L["hud"].collidepoint(mx, my) or L["topbar"].collidepoint(mx, my) or L["idle"].collidepoint(mx, my):
            return True
        if self.p.faction.get("faith") and L["faith"].collidepoint(mx, my):
            return True
        if self.show_allies and getattr(self, "allies_rect", None) and self.allies_rect.collidepoint(mx, my):
            return True
        return any(r.collidepoint(mx, my) for r, *_ in getattr(self, "quest_regions", []))

    def input_blocked(self):
        """Modális állapot (szünet, döntés, irányválasztás, eredmény, szövegbevitel): nincs parancs a rácsból."""
        g = self.g
        return bool(self.paused or self.text_focus or g.result is not None
                    or (g.director is not None and g.director.pending) or self.path_pending())

    def selection_kind(self):
        sel = [e for e in self.sel if e.alive]
        if not sel:
            return "none"
        if len(sel) > 1:
            return "multi"
        e = sel[0]
        if getattr(e, "is_resource", False):
            return "resource"
        if e.is_building:
            return "building"
        return "hero" if e.hero else "unit"

    def _type_order(self, e):
        f = e.owner.faction if getattr(e, "owner", None) else {}
        ks = list(f.get("units", {})) + list(f.get("buildings", {}))
        did = e.defn["id"]
        return (0 if getattr(e, "hero", False) else 1 if e.is_unit else 2, ks.index(did) if did in ks else 999, did)

    def sel_cells(self):
        """A 4×3-as kijelölés-rács cellái: [{"ents": [...], "kind": "one"|"group"|"queue"|"more", ...}]."""
        sel = [e for e in self.sel if e.alive and not getattr(e, "is_resource", False)]
        if not sel:
            return []
        if len(sel) == 1 and sel[0].is_building:
            b = sel[0]
            out = [{"kind": "one", "ents": [b]}]
            if b.owner is self.p:
                for i, it in enumerate(b.queue[:11]):
                    out.append({"kind": "queue", "ents": [b], "item": it, "idx": i})
            return out
        groups = {}
        for e in sorted(sel, key=lambda e: (self._type_order(e), e.id)):
            groups.setdefault(e.defn["id"], []).append(e)
        mode = getattr(self, "settings", {}).get("sel_grid", SEL_GRID_MODE)
        single = mode == "single" or (mode == "auto" and len(groups) == 1)
        if single:
            ents = [e for g in groups.values() for e in g]
            cells = [{"kind": "one", "ents": [e]} for e in ents]
        else:
            cells = [{"kind": "group", "ents": g} for g in groups.values()]
        if len(cells) > 12:
            rest = [e for c in cells[11:] for e in c["ents"]]
            cells = cells[:11] + [{"kind": "more", "ents": rest}]
        return cells

    def primary(self):
        cells = self.sel_cells()
        if cells:
            return cells[0]["ents"][0]
        sel = [e for e in self.sel if e.alive]
        return sel[0] if sel else None

    # ---------------------------------------------------------------- parancsrács (view-model)
    def _slot(self, cid, icon, label, fn=None, *, rfn=None, active=False, enabled=True, tip=None, cd=0.0, cost=None,
              passive=False, ab_id=None, defn=None, kind=None, prog=None, count=0, ally=None):
        return {"id": cid, "icon": icon, "label": label, "fn": fn, "rfn": rfn, "active": active, "enabled": enabled,
                "tip": tip, "cd": cd, "cost": cost, "passive": passive, "ab_id": ab_id, "defn": defn, "kind": kind,
                "prog": prog, "count": count, "ally": ally}

    def _paged(self, items, tail=None):
        """Elemek a 12 helyre; ha nem férnek el: V = következő oldal. tail: a B helyre kerülő elem (pl. Vissza)."""
        cap = 12 if tail is None else 11
        slots = [None] * 12
        if len(items) > cap:
            per = cap - 1
            pages = (len(items) + per - 1) // per
            self.page_no %= pages
            chunk = items[self.page_no * per:(self.page_no + 1) * per]
            for i, it in enumerate(chunk):
                slots[i] = it
            nxt_i = 10 if tail is not None else 11
            slots[nxt_i] = self._slot("page_next", "next", f"Következő oldal ({self.page_no + 1}/{pages})",
                                      lambda: setattr(self, "page_no", self.page_no + 1))
        else:
            for i, it in enumerate(items):
                slots[i] = it
        if tail is not None:
            slots[11] = tail
        return slots

    def _back(self):
        def go():
            self.cmd_page, self.page_no = None, 0
            self.learn_mode = False
            if self.mode and self.mode[0] == "place":
                self.mode = None
        return self._slot("back", "back", "Vissza", go)

    def _entry_slot(self, kind, d):
        avail, prog, cnt, miss = self.entry_state(kind, d)
        afford = self.p.can_afford(d.get("cost", {}))
        return self._slot(f"{kind}:{d['id']}", None, d["name"], lambda k=kind, dd=d: self.click_entry(k, dd),
                          rfn=lambda k=kind, dd=d: self.right_entry(k, dd), enabled=avail and afford,
                          active=bool(self.mode and self.mode[0] == "place" and self.mode[1] is d),
                          tip=(kind, d, miss), defn=d, kind=kind, prog=prog, count=cnt,
                          cost=None if avail else ("✓" if miss == ["kész"] else None, (150, 230, 150)))

    def command_card(self):
        """A 12 parancshely tartalma a kijelölés alapján (None = üres hely, nincs akció)."""
        g, p = self.g, self.p
        us = self.own_sel_units()
        bs = self.own_sel_buildings()
        hero = self.sel_hero()
        if hero is None or hero.skill_points <= 0:
            self.learn_mode = False
            if self.cmd_page == "learn":
                self.cmd_page = None
        if self.learn_mode and hero is not None:
            self.cmd_page = "learn"
        workers = [u for u in us if u.can_build]  # csak az építő munkások építhetnek
        if self.cmd_page == "build" and not workers:
            self.cmd_page = None
        if self.cmd_page == "learn":
            items = []
            for ab_id in hero.defn.get("hero_abilities", []):
                ab = g.data.abilities[ab_id]
                cur = hero.learned.get(ab_id, 0)
                ok = H.can_learn(g, hero, ab_id)
                items.append(self._slot(f"learn:{ab_id}", ab.get("icon", "bolt"),
                                        f"{ab['name']}  {cur}/{H.max_ability_level(g, ab_id)}",
                                        (lambda a=ab_id: self.learn(hero, a)), enabled=ok,
                                        tip=("learn", ab_id, hero), ab_id=ab_id))
            return self._paged(items, self._back())
        if self.cmd_page == "build":
            items = [self._entry_slot("building", d) for kind, d in self.build_entries()]
            return self._paged(items, self._back())
        slots = [None] * 12
        if us:
            mobile = any(u.speed_base() > 0 for u in us)
            if mobile:
                slots[0] = self._slot("move", "move", "Mozgás", lambda: setattr(self, "mode", ("move",)),
                                      active=self.mode == ("move",), tip=("info", "Mozgás", "Kattints a célpontra a térképen. "
                                                                          "Jobb klikk / Esc: mégse."))
            if mobile:
                slots[1] = self._slot("stop", "stop", "Állj", lambda: self.cmd("cmd_stop", self.own_sel_units()),
                                      tip=("info", "Állj", "Az egységek abbahagyják, amit csinálnak."))
            if any(u.attack for u in us):
                slots[2] = self._slot("attack", "attack", "Támadás", lambda: setattr(self, "mode", ("attack",)),
                                      active=self.mode == ("attack",),
                                      tip=("info", "Támadás", "Egységre: megtámadja. Talajra: támadó menet – útközben "
                                                              "minden ellenséggel harcol."))
            if mobile:
                slots[3] = self._slot("hold", "hold", "Helyben tartás", lambda: self.cmd("cmd_hold", self.own_sel_units()),
                                      tip=("info", "Helyben tartás", "Nem mozdul, de a lőtávba érő ellenséget támadja."))
            ctx = []
            if workers:
                ctx.append(self._slot("build_menu", "build", "Építés", lambda: self._open("build"),
                                      tip=("info", "Építés", "Megnyitja a felhúzható épületek listáját.")))
            if any(getattr(u, "packed", None) for u in us):
                ctx.append(self._slot("unpack", "tent", "Letelepítés", self.start_unpack,
                                      active=bool(self.mode and len(self.mode) > 2),
                                      tip=("info", "Letelepítés", "A vándorszekér tábort ver a kijelölt helyen.")))
            if hero is not None and hero.skill_points > 0:
                ctx.append(self._slot("learn_menu", "learn", f"Tanulás (+{hero.skill_points})", lambda: self._open("learn"),
                                      active=True, tip=("info", f"Tanulás (+{hero.skill_points})",
                                                        "Képességpont elköltése: új képesség vagy magasabb szint.")))
            act, pas = [], []
            for ab_id in self.sel_abilities():
                ab = hero.ab(g, ab_id) if hero is not None else g.data.abilities[ab_id]
                typ = ab.get("type")
                passive = typ in ("passive", "aura")
                on = False
                if typ in ("toggle", "chant"):
                    on = any(ab_id in u.toggles for u in us)
                elif ab.get("target_placement"):
                    on = self.mode == ("ability_place", ab_id)
                elif typ in ("area", "raise", "bolt", "blink"):
                    on = self.mode == ("cast", ab_id)
                elif typ == "anchor_rescue":
                    on = bool(self.mode and self.mode[0] in ("anchor_pick", "anchor_drop"))
                casters = [u for u in us if u.has_ability(ab_id)]
                cd = 0.0
                if casters and ab.get("cooldown"):
                    left = min(u.ab_ready.get(ab_id, 0) for u in casters) - g.time
                    cd = max(0.0, min(1.0, left / ab["cooldown"]))
                cost = None
                if ab.get("faith"):
                    cost = (str(ab["faith"]), (255, 214, 120))
                elif ab.get("energy"):
                    cost = (str(ab["energy"]), (130, 170, 255))
                elif ab.get("hp_cost"):
                    cost = (str(ab["hp_cost"]), (255, 110, 100))
                elif ab.get("spores"):
                    cost = (str(ab["spores"]), (150, 240, 110))
                lvl = hero.learned.get(ab_id) if hero is not None else None
                sl = self._slot(f"ab:{ab_id}", ab.get("icon", "bolt"), ab["name"],
                                (lambda a=ab_id: self.use_ability(a)) if not passive else None, active=on,
                                tip=("ability", ab, lvl, True) if passive else ("ability", ab, lvl), cd=cd, cost=cost,
                                enabled=passive or any(
                                    (typ in ("toggle", "chant") and ab_id in u.toggles)
                                    or (u.ab_ready.get(ab_id, 0) <= g.time and u.ab_ok(u.ab(g, ab_id)))
                                    for u in casters),
                                passive=passive, ab_id=ab_id)
                (pas if passive else act).append(sl)
            ctx += act + pas
            self._fill_ctx(slots, ctx)
            return slots
        if bs:
            b0 = bs[0]
            same = [b for b in bs if b.defn["id"] == b0.defn["id"]]
            items = []
            if b0.defn.get("diplomacy") and b0.complete:
                if getattr(p, "ally", None) is None:
                    return self._paged(self.ally_slots())
                f = g.data.factions.get(p.ally, {})
                slots[0] = self._slot("ally_info", None, f"Szövetséges: {f.get('name', p.ally)}", None, passive=True,
                                      tip=("info", f"Szövetséges: {f.get('name', p.ally)}", f.get("desc", "")), ally=f)
                return slots
            if b0.complete:
                f = p.faction
                own = set(f["units"])
                if getattr(p, "ally", None):  # a szövetséges nép épületei a saját egységeiket képezik
                    own |= set(g.data.factions.get(p.ally, {}).get("ally_pack", {}).get("units", []))
                for uid in b0.defn.get("produces", ()):
                    d = g.data.defs.get(uid)
                    if d and d.get("hidden"):
                        continue
                    if d and uid in own and self.path_visible(d):
                        sl = self._entry_slot("unit", d)
                        sl["fn"] = lambda dd=d, bb=same: self.train_from(dd, bb)
                        items.append(sl)
                for rid in b0.defn.get("researches", ()):
                    if rid in g.data.researches:
                        items.append(self._entry_slot("research", g.data.researches[rid]))
            if b0.complete and b0.defn.get("pray_slots") and p.faction.get("faith"):
                fa = p.faction["faith"]
                n = sum(1 for u in g.units if u.alive and u.owner is p and u.order and u.order.get("type") == "pray"
                        and u.order.get("b") is b0 and u.order.get("active"))
                items.append(self._slot("pray_info", "heal", f"{fa['name']}: {n}/{b0.defn['pray_slots']}", None,
                                        passive=True, tip=("info", f"{fa['name']} – {n}/{b0.defn['pray_slots']} hely",
                                                           fa.get("desc", "") + f" Összesen legfeljebb {fa.get('max', 8)}"
                                                           " munkás számít.")))
            for ab_id in (b0.defn.get("powers", ()) if b0.complete else ()):
                ab = g.data.abilities[ab_id]
                left = getattr(b0, "power_ready", {}).get(ab_id, 0) - g.time
                fa = p.faction.get("faith", {})
                gp = ab.get("type") == "gpower"
                cost = g.gpower_cost(ab) if gp else {"faith": ab.get("faith", 0)}
                ok = p.can_afford(cost) and (not ab.get("hq_hp") or b0.hp > ab["hq_hp"] + 50)
                if gp and ab.get("target"):  # Ö19: célzás a térképen
                    fn = (lambda a=ab_id, bb=b0: setattr(self, "mode", ("gcast", a, bb)))
                elif gp:
                    fn = (lambda a=ab_id, bb=b0: self.cmd("cmd_gpower", p, bb, a, 0.0, 0.0))
                else:
                    fn = (lambda a=ab_id, bb=b0: self.cmd("cmd_bpower", p, bb, a))
                ck, cv = next(iter(cost.items()), (None, 0))
                lab = (str(cv), tuple(g.data.game["resources"][ck]["color"])) if ck else \
                    ((str(ab["hq_hp"]), (255, 110, 100)) if ab.get("hq_hp") else None)
                items.append(self._slot(f"bp:{ab_id}", ab.get("icon", "bolt"), ab["name"], fn, enabled=ok,
                                        active=self.mode == ("gcast", ab_id, b0),
                                        tip=("ability", ab, None), cd=max(0.0, min(1.0, left / ab.get("cooldown", 60))),
                                        cost=lab, ab_id=ab_id))
            packs = [u for u in b0.defn.get("produces", ()) if u.endswith("_falka") and u in g.data.defs]
            if b0.complete and packs and not g.missing_requirements(p, g.data.defs[packs[0]]):
                on = getattr(p, "pack_mode", False)  # Wullof: falkában képez (hármasával, olcsóbban)
                items.append(self._slot("pack_mode", "merge", f"Falkában képez: {'Be' if on else 'Ki'}",
                                        lambda: setattr(p, "pack_mode", not getattr(p, "pack_mode", False)), active=on,
                                        tip=("info", f"Falkában képez: {'Be' if on else 'Ki'}",
                                             "Bekapcsolva a Lándzsás harcos és a Bumerángvető hármasával, olcsóbban "
                                             "képződik (egy falka három egység ára 2,6-szoros).")))
            if b0.defn.get("packable") and b0.complete:
                items.append(self._slot("pack", "pack", "Felszedés", self.pack_selected,
                                        tip=("info", "Felszedés", "A tábor szekérre pakol, és továbbvándorolhat.")))
            gb = [b for b in bs if b.garrison or getattr(b, "miners", None)]
            if gb:
                items.append(self._slot("unload", "house", f"Kiürítés ({sum(len(b.garrison) + len(b.miners) for b in gb)})",
                                        lambda: [self.cmd("unload", b) for b in gb],
                                        tip=("info", "Kiürítés", "A bent lévő egységek kijönnek.")))
            if p.ascended and p.path is None and b0.defn["id"] == p.faction["hq"]:
                items.append(self._slot("path", "learn", "Irány választása", lambda: setattr(self, "path_hidden", False),
                                        active=True, tip=("info", "Irány választása",
                                                          "A néped felemelkedett: válassz fejlődési irányt.")))
            tail = None
            if any(b.queue for b in same):
                qb = max(same, key=lambda b: len(b.queue))
                tail = self._slot("cancel", "stop", "Mégse (utolsó a sorban)",
                                  lambda: self.cmd("cancel_item", qb, len(qb.queue) - 1) if qb.queue else None,
                                  tip=("info", "Mégse", "Visszavonja a gyártósor utolsó elemét; az ár visszajár."))
            return self._paged(items, tail)
        return slots

    def _fill_ctx(self, slots, ctx):
        if len(ctx) <= len(CTX_SLOTS):
            for i, sl in zip(CTX_SLOTS, ctx):
                slots[i] = sl
            return
        per = len(CTX_SLOTS) - 1
        pages = (len(ctx) + per - 1) // per
        self.page_no %= pages
        for i, sl in zip(CTX_SLOTS, ctx[self.page_no * per:(self.page_no + 1) * per]):
            slots[i] = sl
        slots[11] = self._slot("page_next", "next", f"További parancsok ({self.page_no + 1}/{pages})",
                               lambda: setattr(self, "page_no", self.page_no + 1))

    def _open(self, page):
        self.cmd_page, self.page_no = page, 0
        if page == "learn":
            self.learn_mode = True

    def build_entries(self):
        f = self.p.faction
        out = [("building", self.g.data.defs[bid]) for bid in f["buildings"]
               if not self.g.data.defs[bid].get("hidden") and self.path_visible(self.g.data.defs[bid])]
        aid = getattr(self.p, "ally", None)
        if aid:  # a szövetséges nép épületei
            pack = self.g.data.factions.get(aid, {}).get("ally_pack", {})
            out += [("building", self.g.data.defs[bid]) for bid in pack.get("buildings", []) if bid in self.g.data.defs]
        return out

    def ally_slots(self):
        """Diplomácia csarnoka: a választható szövetséges népek (egyszer, véglegesen)."""
        g, p = self.g, self.p
        cost = p.faction.get("ally_cost", {})
        rd = g.data.game["resources"]
        price = ", ".join(f"{p.res_name(k, rd)} {v}" for k, v in cost.items())
        out = []
        for f in g.data.allies():
            pack = f.get("ally_pack", {})
            units = [g.data.defs[u]["name"] for u in pack.get("units", []) if u in g.data.defs]
            blds = [g.data.defs[b]["name"] for b in pack.get("buildings", []) if b in g.data.defs]
            desc = (f"{f.get('tagline', '')}. {f.get('desc', '')}  Épületek: {', '.join(blds)}.  "
                    f"Egységek: {', '.join(units)}.  Ára: {price}. A választás végleges.")
            out.append(self._slot(f"ally:{f['id']}", None, f"Szövetség: {f['name']}",
                                  (lambda a=f["id"]: self.cmd("cmd_choose_ally", p, a)), enabled=p.can_afford(cost),
                                  tip=("info", f"Szövetség: {f['name']}", desc), ally=f))
        return out

    def train_from(self, d, blds):
        """Kiképzés a kijelölt épület(ek)ből (Shift: 5×); ugyanaz a parancs, mint a szimuláció bármely más útján."""
        n = 5 if pygame.key.get_mods() & pygame.KMOD_SHIFT else 1
        fk = self.g.data.defs.get(d["id"] + "_falka")
        if fk and getattr(self.p, "pack_mode", False) and not self.g.missing_requirements(self.p, fk) \
                and any(fk["id"] in b.defn.get("produces", ()) for b in blds):
            d = fk  # falka-kapcsoló: a falka-változat képződik
        for _ in range(n):
            ok = [b for b in blds if b.alive and b.complete]
            if ok:
                done = self.cmd("cmd_train", self.p, d["id"], min(ok, key=lambda b: len(b.queue)))
            else:
                done = self.cmd("cmd_train", self.p, d["id"])
            if not done:
                break

    def activate_slot(self, i, button=1):
        """KÖZÖS parancs-dispatcher: a billentyű (W..B) és az egérkattintás is ezt hívja."""
        if self.input_blocked():
            return False
        slots = self.command_card()
        sl = slots[i] if 0 <= i < 12 else None
        if sl is None:
            return False
        fn = sl["fn"] if button == 1 else sl["rfn"]
        if fn is None:
            if button == 1 and sl["passive"]:
                self.g.notify(self.p, f"{sl['label']}: passzív képesség, magától hat.", (200, 200, 200))
            return False
        if button == 1 and sl["defn"] is not None and not self.p.can_afford(sl["defn"].get("cost", {})):
            for k in self.p.missing(sl["defn"].get("cost", {})):
                self.res_flash[k] = self.t
        self.slot_flash = (i, self.t)
        fn()
        return True

    # ---------------------------------------------------------------- bemenet
    def hud_key(self, ev):
        """Parancsrács-billentyűk. Igaz, ha a billentyűt elfogyasztotta."""
        if ev.key not in KEY_SLOT:
            return False
        mods = pygame.key.get_mods()
        if mods & (pygame.KMOD_CTRL | pygame.KMOD_ALT | pygame.KMOD_META):
            return False
        self.activate_slot(KEY_SLOT[ev.key])
        return True

    def hud_click(self, mx, my, button):
        """Kattintás a HUD-on. Igaz, ha a HUD fölött volt (a világ nem kapja meg)."""
        L = self.L
        if self.mm_rect.collidepoint(mx, my):
            self.minimap_click(mx, my, button)
            return True
        if button == 1 and L["gear"].collidepoint(mx, my):
            if self.net is not None:
                self.g.notify(self.p, "Hálózati játékban nincs szünetmenü – kilépés: Esc kétszer.", (255, 220, 120))
            else:
                self.paused = True
            return True
        if button == 1 and L["allies"].collidepoint(mx, my):
            self.show_allies = not self.show_allies
            return True
        if button == 1 and L["idle"].collidepoint(mx, my):
            self.select_idle_worker()
            return True
        for r, fn in getattr(self, "quest_regions", []):
            if r.collidepoint(mx, my):
                if button == 1:
                    fn()
                return True
        for i, r in enumerate(L["cmd_cells"]):
            if r.collidepoint(mx, my):
                if button in (1, 3):
                    self.activate_slot(i, button)
                return True
        for rect, left, right, _ in self.regions:
            if rect.collidepoint(mx, my):
                fn = left if button == 1 else right if button == 3 else None
                if fn:
                    fn()
                return True
        if self.show_allies and getattr(self, "allies_rect", None) and self.allies_rect.collidepoint(mx, my):
            return True
        return self.over_hud(mx, my)

    def minimap_click(self, mx, my, button):
        if button == 1:
            self.minimap_jump(mx, my)
            self.minimap_drag = True
        elif button == 3:
            wx, wy = self.minimap_world_pos(mx, my)
            us = self.own_sel_units()
            if us:
                self.cmd("cmd_move", us, wx, wy, attack=self.mode == ("attack",))
                self.mode = None

    def sel_cell_click(self, cell, button):
        mods = pygame.key.get_mods()
        if cell["kind"] == "queue":
            self.cmd("cancel_item", cell["ents"][0], cell["idx"])
            return
        ents = [e for e in cell["ents"] if e.alive]
        if button == 3 or (button == 1 and mods & pygame.KMOD_SHIFT):
            self.sel = [e for e in self.sel if e not in ents]  # kivétel a kijelölésből
            return
        if button == 1 and ents:
            now = pygame.time.get_ticks()
            if self.last_click[1] is cell.get("key") and now - self.last_click[0] < 350:
                self.center_on(ents[0].x, ents[0].y)
            self.sel = ents
            self.cmd_page, self.page_no = None, 0
            self.last_click = (now, cell.get("key"))

    # ---------------------------------------------------------------- rajzolás: közös elemek
    def well(self, r, fill=(14, 14, 16)):
        """Süllyesztett mező (a HUD paneljein)."""
        s = self.screen
        pygame.draw.rect(s, fill, r)
        th = self.th
        pygame.draw.line(s, (4, 4, 4), r.topleft, (r.right - 1, r.y))
        pygame.draw.line(s, (4, 4, 4), r.topleft, (r.x, r.bottom - 1))
        lo = tuple(int(c * 0.75) for c in th["light"])
        pygame.draw.line(s, lo, (r.x, r.bottom), (r.right, r.bottom))
        pygame.draw.line(s, lo, (r.right, r.y), (r.right, r.bottom))

    def section_frame(self, f):
        """Önálló, kiemelt szekció-keret: sötét árok, fajszínű perem, süllyesztett belső."""
        s, th = self.screen, self.th
        from . import faction_ui as FUI
        bd = max(8, round(16 * self.L["s"]))
        art = FUI.nine_slice(f"ui/frame_{self.p.faction['id']}.png", f.w + 2 * bd // 2, f.h + 2 * bd // 2, bd)
        if art is not None:  # fajonként megrajzolt szekció-keret (9 szeletes kép)
            pygame.draw.rect(s, (12, 12, 14), f)
            s.blit(art, (f.x - bd // 2, f.y - bd // 2))
            return
        hi = th["light"]
        lo = tuple(int(c * 0.45) for c in hi)
        pygame.draw.rect(s, (4, 4, 4), f.inflate(4, 4), 2)
        pygame.draw.rect(s, (12, 12, 14), f)
        pygame.draw.rect(s, lo, f, 2)
        pygame.draw.line(s, hi, f.topleft, (f.right - 1, f.y))
        pygame.draw.line(s, hi, f.topleft, (f.x, f.bottom - 1))
        k = max(3, self.L["fp"] + 1)
        for cx, cy in (f.topleft, (f.right - 1, f.y), (f.x, f.bottom - 1), (f.right - 1, f.bottom - 1)):
            pygame.draw.rect(s, hi, (cx - k // 2, cy - k // 2, k, k))
            pygame.draw.rect(s, (4, 4, 4), (cx - k // 2, cy - k // 2, k, k), 1)

    @staticmethod
    def ellipsize(text, font, w):
        """A szöveg „…”-ra rövidítve, ha nem fér el a megadott szélességen."""
        if font.size(text)[0] <= w:
            return text
        while text and font.size(text + "…")[0] > w:
            text = text[:-1]
        return text.rstrip(" ,–") + "…"

    def fit_icon(self, img, size, bg=None):
        """Kép négyzetbe illesztve (arányosan, középre)."""
        key = ("fit", id(img), size, bg)
        out = self.icons.get(key)
        if out is None:
            out = pygame.Surface((size, size), pygame.SRCALPHA)
            if bg is not None:
                top = shade(bg, 0.42)
                for yy in range(size):
                    t = yy / max(1, size - 1)
                    pygame.draw.line(out, tuple(int(top[k] * (1 - t) + 12 * t) for k in range(3)), (0, yy), (size, yy))
            w, h = img.get_size()
            if w != h and min(w, h) >= 0.66 * max(w, h):  # fekvő/álló ikon: a közepe négyzetre vágva (nem kicsinyítve)
                m = min(w, h)
                img = img.subsurface(((w - m) // 2, (h - m) // 2, m, m))
                w = h = m
            sc = min((size - 4) / w, (size - 4) / h)
            im = pygame.transform.smoothscale(img, (max(1, int(w * sc)), max(1, int(h * sc))))
            out.blit(im, ((size - im.get_width()) // 2, (size - im.get_height()) // 2))
            self.icons[key] = out
        return out

    def def_icon(self, d, kind, size, color=None):
        """Négyzetes, felirat nélküli ikon egységhez/épülethez/kutatáshoz."""
        key = ("sq", d["id"], kind, size, tuple(color or self.p.color))
        out = self.icons.get(key)
        if out is None:
            fac = self.p.faction
            if kind == "research":
                art = self.sprites.research_art(d["id"])
                out = self.fit_icon(art, size, tuple(fac["color"])) if art is not None else \
                    self.fit_icon(self.cmd_icon(d.get("icon", "learn")), size, tuple(fac["color"]))
            else:
                spr = self.sprites.icon(d["id"], color or self.p.color)
                if spr is None:
                    spr = self.icon(d["id"], d, kind)
                out = self.fit_icon(spr, size, tuple(fac["color"]))
            self.icons[key] = out
        return out

    def ally_icon(self, f, size):
        """Szövetséges nép ikonja: a címere, vagy az első egységének képe a nép színén."""
        key = ("allyic", f["id"], size)
        out = self.icons.get(key)
        if out is None:
            em = self.sprites.emblem(f, size)
            if em is None:
                pack = f.get("ally_pack", {})
                uid = (pack.get("units") or [None])[0]
                d = self.g.data.defs.get(uid) if uid else None
                spr = self.sprites.icon(d.get("sprite", uid), self.p.color) if d else None
                if spr is None and d is not None:
                    spr = self.icon(uid, d, "unit")
                out = self.fit_icon(spr, size, tuple(f.get("color", (90, 90, 90)))) if spr is not None else                     pygame.Surface((size, size), pygame.SRCALPHA)
            else:
                out = em
            self.icons[key] = out
        return out

    def corner_label(self, r, text, col=(255, 244, 210), font=None, where="br"):
        """Kis, sötét hátterű címke a mező sarkában (billentyű: jobb alul; darabszám: jobb alul)."""
        s = self.screen
        font = font or self.F("key")
        img = font.render(text, True, col)
        w, h = img.get_width() + 4, img.get_height()
        x = r.right - w - 1 if where[1] == "r" else r.x + 1
        y = r.bottom - h - 1 if where[0] == "b" else r.y + 1
        bg = pygame.Surface((w, h), pygame.SRCALPHA)
        bg.fill((0, 0, 0, 200))
        s.blit(bg, (x, y))
        s.blit(img, (x + 2, y))

    # ---------------------------------------------------------------- rajzolás: alsó sáv
    def draw_bottom_hud(self):
        s = self.screen
        L = self.L
        p = self.p
        g = self.g
        fid = p.faction["id"]
        self.regions = []
        from . import faction_ui as FUI
        aid = getattr(p, "ally", None)
        key = (L["hud"].size, fid, aid)
        if self._hud_bg is None or self._hud_bg[0] != key:
            em = self.sprites.emblem(p.faction, 64)
            col, acc = tuple(p.faction["color"]), tuple(p.faction["accent"])
            own = FUI.banner(fid, L["banner1"].w, L["banner1"].h, col, acc, em)
            right = own
            af = g.data.factions.get(aid) if aid else None
            if af is not None:  # a választott szövetséges nép zászlaja a jobb oldalon
                right = FUI.banner(aid, L["banner2"].w, L["banner2"].h, tuple(af.get("color", col)),
                                   tuple(af.get("accent", acc)), self.sprites.emblem(af, 64))
            self._hud_bg = (key, FUI.hud_panel(fid, *L["hud"].size), own, right)
        s.blit(self._hud_bg[1], L["hud"])
        s.blit(self._hud_bg[2], L["banner1"])
        s.blit(self._hud_bg[3], L["banner2"])
        for f in L["frames"]:
            self.section_frame(f)
        pygame.draw.line(s, (6, 6, 6), (L["divider"], L["char"].y), (L["divider"], L["char"].bottom))
        pygame.draw.line(s, tuple(int(c * 0.5) for c in self.th["light"]),
                         (L["divider"] + 1, L["char"].y), (L["divider"] + 1, L["char"].bottom))
        self.draw_minimap()
        self.draw_char_sheet()
        self.draw_middle()
        self.draw_sel_grid()
        self.draw_cmd_grid()

    def draw_char_sheet(self):
        s, g, p, L = self.screen, self.g, self.p, self.L
        pr, st = L["portrait"], L["stats"]
        self.well(pr)
        for k in ("hpbar", "mpbar"):
            self.well(L[k], (8, 8, 8))
        e = self.primary()
        P = pr.w
        x, y, w = st.x + 2, st.y + 2, st.w - 4
        if e is None:  # nincs kijelölés: faj-összefoglaló
            em = self.sprites.emblem(p.faction, P - 8)
            if em:
                s.blit(em, (pr.x + 4, pr.y + 4))
            s.blit(self.F("name").render(p.faction["name"], True, (240, 220, 160)), (x, y))
            y += self.F("name").get_linesize() + 2
            tag = p.faction.get("tagline", "")
            if tag:
                y = self.wrap(tag, x, y, w, self.F("tiny"), tuple(p.faction["accent"]), 2) + 4
            self.wrap(p.faction.get("desc", ""), x, y, w, self.F("tiny"), (170, 170, 160), max(1, (st.bottom - y) //
                                                                                          self.F("tiny").get_linesize()))
            return
        if getattr(e, "is_resource", False):
            rd = g.data.game["resources"][e.kind]
            sh = self.sprites.resource(e.kind) if hasattr(self.sprites, "resource") else None
            spr = None
            rotten = e.kind == "wood" and getattr(e, "rot", False)
            if sh is not None and e.kind == "wood" and hasattr(self, "_wood_sheet"):
                sh = self._wood_sheet(e) or sh
                if rotten:
                    used = 1 - e.amount / max(1, e.max_amount)
                    sh = self._rot_sheet(sh, 1 if used < 0.33 else 2 if used < 0.66 else 3)
            if sh is not None:  # a képlap aktuális változata (a Sheet maga nem kép)
                fr = sh.frame(None, getattr(e, "variant", 0) % max(1, sh.meta.get("frames", 1)))
                r0 = fr.get_bounding_rect()
                spr = fr.subsurface(r0).copy() if r0.w and r0.h else fr
            if spr is not None:
                s.blit(self.fit_icon(spr, P), pr)
            else:
                pygame.draw.circle(s, tuple(rd["color"]), pr.center, P // 3)
            nm = ("Rothadó fa" if getattr(e, "rot_t", -99) > g.time - 2.0 else "Rothadt fa") if e.kind == "wood" and                 getattr(e, "rot", False) else p.res_name(e.kind, g.data.game["resources"])
            s.blit(self.F("name").render(nm, True, (190, 170, 210) if nm.startswith("Roth") else tuple(rd["color"])), (x, y))
            y += self.F("name").get_linesize() + 4
            s.blit(self.F("small").render(f"Készlet: {int(e.amount)}", True, (220, 220, 210)), (x, y))
            if e.kind == "wood" and getattr(e, "rot", False):
                y += self.F("small").get_linesize() + 2
                live = getattr(e, "rot_t", -99) > g.time - 2.0
                by = next((q for q in g.players if q.id == getattr(e, "rot_by", -1)), None)
                who = f" ({by.name})" if by is not None and by is not p else ""
                self.wrap(("A mélység emészti" + who + ": lassan elporlad, a Croftianok fát nyernek belőle.")
                          if live else "A mélység már megkezdte; most nem rothad tovább.",
                          x, y, w, self.F("tiny"), (190, 170, 210), 5)
            if e.kind == "stone":
                y += self.F("small").get_linesize() + 2
                self.wrap("Aranybánya: a munkások bemennek a tárnába, és arannyal jönnek ki.", x, y, w, self.F("tiny"),
                          (170, 170, 160), 3)
            return
        own = e.owner is p
        # portré (1:1, csapatszínnel): egységnél az arckép, épületnél a képlapjából generált portré
        spr = self.sprites.icon(e.defn["id"], e.owner.color)
        por = self.fit_icon(spr, P) if spr is not None else None
        if por is None:  # még nincs grafika: a kódból rajzolt alak (felirat nélkül)
            key = ("ph", e.defn["id"], tuple(e.owner.color))
            ph = self.icons.get(key)
            if ph is None:
                from .render import make_icon
                ph = self.icons[key] = make_icon(self.base_fonts, e.defn, e.owner.faction, e.owner.color,
                                                 "unit" if e.is_unit else "building")
            por = self.fit_icon(ph, P)
        if por is not None:
            s.blit(por, pr)
        pygame.draw.rect(s, (200, 170, 100) if own else (220, 90, 80), pr, 1)
        if e.is_unit and e.hero:
            self.corner_label(pr, f"{e.level}", (255, 230, 150), self.F("name"), "tr")
        seconds = getattr(e, "summon_seconds", None)
        if seconds is not None:
            self.corner_label(pr, f"{seconds} mp", (255, 220, 130), self.F("small"), "br")
        tkey = self.unit_type(e)
        if tkey:  # Ö24: az egység típusa (súgóval)
            isz = max(16, P // 5)
            r = pygame.Rect(pr.x + 3, pr.y + 3, isz, isz)
            s.blit(self.type_icon(tkey, isz), r)
            tn, tdsc = self.type_info(tkey)
            self.regions.append((r, None, None, ("info", f"Típus: {tn}", tdsc)))
        # név és típus: ami nem fér el, kisebb betűvel; a hősök mellékneve („Akahana, a Vörös Buzogány”) külön sorban
        from .i18n import T
        nm, fn = T(e.name), self.F("name")
        epithet = ""
        if fn.size(nm)[0] > w and ", " in nm:
            nm, epithet = nm.split(", ", 1)
        if fn.size(nm)[0] > w and self.F("small").size(nm)[0] <= w:
            fn = self.F("small")
        s.blit(fn.render(self.ellipsize(nm, fn, w), True, (240, 220, 160) if own else (255, 140, 120)), (x, y))
        y += fn.get_linesize()
        if epithet:
            ft = self.F("tiny")
            s.blit(ft.render(self.ellipsize(epithet, ft, w), True, (214, 196, 150)), (x, y))
            y += ft.get_linesize()
        if not own:
            typ = f"{e.owner.name} – {e.owner.faction['name']}"
        elif e.is_building:
            typ = "Épület" if e.complete else f"Épül: {int(100 * e.progress / e.build_time)}%"
        elif e.hero:
            typ = f"Hős • {e.level}. szint"
        else:
            typ = "Munkás" if e.is_worker else (self.type_info(tkey)[0] if tkey else "Egység")
        n_sel = len([q for q in self.sel if q.alive])
        if n_sel > 1 and self.F("tiny").size(typ + f"  ({n_sel})")[0] <= w:
            typ += f"  ({n_sel})"
        s.blit(self.F("tiny").render(typ, True, tuple(e.owner.faction["accent"])), (x, y))
        y += self.F("tiny").get_linesize() + 3
        # a portré alatt: életerő, alatta mana/energia (vagy pajzs) – csak ha az egység használja
        hb, mb = L["hpbar"], L["mpbar"]
        self.hbar(hb.x, hb.y, hb.w, hb.h, e.hp / e.max_hp, (80, 210, 80), f"{int(e.hp)}/{int(e.max_hp)}",
                  ("info", "Életerő", f"{int(e.hp)} / {int(e.max_hp)}"))
        second = []
        if e.max_energy:
            second.append((e.energy / e.max_energy, (150, 100, 255), f"{int(e.energy)}/{int(e.max_energy)}",
                           ("info", "Energia / mana", "A képességek ára; lassan töltődik.")))
        if e.max_shield:
            second.append((e.shield / e.max_shield, (90, 170, 255), f"{int(e.shield)}/{int(e.max_shield)}",
                           ("info", "Pajzs", "Előbb a pajzs fogy, és magától töltődik.")))
        if e.is_unit and getattr(e, "morale", None) is not None:
            mmax = e.owner.faction.get("morale", {}).get("max", 100)
            br = getattr(e, "broken", False)
            second.append((e.morale / mmax, (230, 120, 60) if br else (240, 190, 80),
                           "MEGTÖRT" if br else f"{int(e.morale)}/{mmax}",
                           ("info", "Harci kedv", "Sérüléstől és a társak halálától fogy; ha elfogy, az egység megtörik "
                            "(−30% sebzés, +20% sebesség). A Taiko-dobos közelében gyorsan töltődik, lámpásfényben "
                            "nem esik 30 alá.")))
        if second:
            self.hbar(mb.x, mb.y, mb.w, mb.h, *second[0])
        bh = max(6, mb.h // 2 + 1)
        for frac, col, lab, tip in second[1:]:  # ha energia és pajzs is van: a pajzs az adatoszlopba kerül
            self.hbar(x, y, w, bh + 4, frac, col, lab, tip)
            y += bh + 7
        if e.is_unit and e.hero:
            nxt = "max." if e.level >= H.MAX_LEVEL else f"{int(e.xp)}/{H.XP_LEVELS[e.level]}"
            self.hbar(x, y, w, bh, H.xp_progress(e), (230, 190, 70), "",
                      ("info", "Tapasztalat", f"{nxt} – szintlépéskor képességpontot kap."))
            y += bh + 5
        # alapadatok: támadás, páncél, mozgás, látótáv (tooltipben bontva)
        stats = []
        if e.attack:
            base = e.attack["damage"]
            up = e.owner.stat(e.defn, "damage", base) - base
            bon = e.bonus.get("damage", 0)
            stats.append(("Támadás", f"{e.attack_damage(self.g):g}",
                          f"Alap {base:g}" + (f" • fejlesztés {up:+g}" if up else "") + (f" • hatás {bon:+g}" if bon else "")
                          + f" • lőtáv {e.atk_range() / TILE:g}"))
        arm_base = e.defn.get("armor", 0)
        sund = getattr(e, "sunder_armor", 0) if getattr(e, "sunder_until", 0.0) > self.g.time else 0
        arm_now = max(0, e.armor - sund)
        stats.append(("Páncél", f"{round(arm_now, 1):g}", f"Alap {arm_base:g}" +
                      (f" • módosítók {e.armor - arm_base:+.1f}" if abs(e.armor - arm_base) > 0.05 else "") +
                      (f" • páncéltörés −{sund:g}" if sund else "")))
        if e.is_unit:
            spd = e.speed(self.g) / TILE
            stats.append(("Mozgás", f"{spd:.3g}", f"Alap {e.defn.get('speed', 0):g}" +
                          (f" • hatás {e.bonus.get('speed_pct', 0):+g}%" if e.bonus.get("speed_pct") else "")))
        if hasattr(e, "sight"):
            stats.append(("Látótáv", f"{e.sight():g}", "Ennyi mezőnyire lát (köd ellen)."))
        ft = self.F("tiny")
        row_h = self.draw_counter_row(e, tkey, x, st.bottom, w)  # alulról felfelé; a magasságát adja vissza
        bottom = st.bottom - row_h
        need = max((ft.size(nm)[0] + ft.size(val)[0] + 18 for nm, val, _ in stats), default=0)
        ncol = 2 if need <= w // 2 else 1  # széles betűnél egy oszlop
        cw = w // ncol
        lh = ft.get_linesize()
        for i, (nm, val, tipd) in enumerate(stats):
            cx, cy = x + (i % ncol) * cw, y + (i // ncol) * lh
            if cy + lh > bottom - 2:
                break
            s.blit(ft.render(nm, True, (160, 160, 150)), (cx, cy))
            vi = ft.render(val, True, (240, 236, 210))
            s.blit(vi, (cx + cw - vi.get_width() - 8, cy))
            self.regions.append((pygame.Rect(cx, cy, cw - 4, lh), None, None, ("info", f"{nm}: {val}", tipd)))
        y += lh * ((len(stats) + ncol - 1) // ncol) + 2
        if e.is_unit or getattr(e, "sunder_until", 0.0) > self.g.time:  # mi hat rá éppen (hátralévő idővel)
            y = self.draw_effects_row(e, x, y, w, bottom)
        # állapotsor: mit csinál
        info = self.status_text(e)
        if info and y + lh <= bottom:
            self.wrap(info, x, y, w, ft, (170, 210, 170), max(1, (bottom - y) // lh))

    EFFECT_INFO = [  # (attribútum, név, szín, magyarázat)
        ("burn", "Ég", (255, 140, 60), "Tűz elemű találat: még néhány másodpercig sebez, páncélon át. Víz vagy jég eloltja."),
        ("poison", "Mérgezett", (140, 230, 90), "Lassú, páncélon átmenő sebzés."),
        ("frozen_until", "Fagyott", (170, 205, 255), "Jég elemű találat: lassabban mozog. Tűz felolvasztja."),
        ("slow_until", "Lelassult", (180, 160, 255), "Lassabban mozog."),
        ("wet_until", "Átázott", (130, 190, 255), "Nem gyullad meg; a villám kétszer hosszabb sokkot ad rá."),
        ("rooted_until", "Gúzsba kötve", (205, 170, 110), "Nem mozdulhat, de támadhat."),
        ("asleep_until", "Alszik", (190, 195, 225), "Nem cselekszik; az első sebzésre felébred."),
        ("stun_until", "Kábult", (255, 230, 110), "Nem cselekszik."),
        ("feared_until", "Megrémült", (215, 180, 255), "Menekül, nem támad."),
        ("blind_until", "Elvakult", (170, 170, 175), "Távolsági támadásainak egy része mellémegy."),
        ("silenced_until", "Elnémítva", (200, 160, 240), "Nem varázsolhat."),
        ("marked_until", "Megjelölt", (255, 110, 100), "Több sebzést kap, és az ellenfél a köd alól is látja."),
        ("sunder_until", "Páncéltörés", (215, 130, 90), "Kevesebb a páncélja."),
    ]

    def active_effects(self, e):
        """Az egységen éppen ható állapotok: [(név, hátralévő mp, szín, magyarázat)]; az erősítések a
        képesség nevével."""
        now = self.g.time
        out = []
        for attr, name, col, desc in self.EFFECT_INFO:
            v = getattr(e, attr, None)
            if v is None:
                continue
            until = v[2] if isinstance(v, tuple) else v
            if until > now:
                if attr == "slow_until" and getattr(e, "frozen_until", 0.0) > now:
                    continue          # a fagyás már szerepel
                if attr == "stun_until" and until - now > 1e6:
                    continue
                out.append((name, until - now, col, desc))
        for ab_id, until in (getattr(e, "buffs", None) or {}).items():
            if until > now:
                ab = self.g.data.abilities.get(ab_id, {})
                out.append((ab.get("name", ab_id), until - now, (255, 214, 110),
                            "Erősítés: " + ab.get("desc", "")))
        return out

    def draw_effects_row(self, e, x, y, w, bottom):
        """„Hatások” sor az adatlapon: színes címkék a hátralévő idővel (egér alatt magyarázat)."""
        eff = self.active_effects(e)
        if not eff:
            return y
        s = self.screen
        ft = self.F("tiny")
        lh = ft.get_linesize()
        if y + lh > bottom:
            return y
        s.blit(ft.render("Hatások", True, (160, 160, 150)), (x, y))
        cx = x + ft.size("Hatások")[0] + 6
        for i, (name, left, col, desc) in enumerate(eff):
            lab = f"{name} {left:.0f} mp" if left < 600 else name
            lab = self.ellipsize(lab, ft, max(1, w - 8))
            tw = min(w, ft.size(lab)[0] + 8)
            if cx + tw > x + w:
                if y + 2 * lh + 2 > bottom:      # nincs több sor: „+N” címke, az egér alatt a többi
                    rest = eff[i:]
                    more = f"+{len(rest)}"
                    mw = ft.size(more)[0] + 8
                    mx = min(cx, x + w - mw)
                    r = pygame.Rect(mx, y, mw, lh)
                    pygame.draw.rect(s, (24, 20, 16), r)
                    pygame.draw.rect(s, (220, 210, 180), r, 1)
                    s.blit(ft.render(more, True, (230, 220, 190)), (mx + 4, y))
                    self.regions.append((r, None, None, ("info", "További hatások", " • ".join(
                        f"{n} ({l:.0f} mp)" for n, l, _, _ in rest))))
                    break
                cx = x
                y += lh + 2
            r = pygame.Rect(cx, y, tw, lh)
            pygame.draw.rect(s, (24, 20, 16), r)
            pygame.draw.rect(s, col, r, 1)
            s.blit(ft.render(lab, True, col), (cx + 4, y))
            self.regions.append((r, None, None, ("info", f"{name} – még {left:.1f} mp", desc)))
            cx += tw + 4
        return y + lh + 3

    # ---- Ö24: egységtípusok – ki mi ellen erős / gyenge -----------------------
    TYPE_COL = {"kozelharcos": (220, 120, 90), "landzsas": (120, 170, 230), "lovesz": (130, 210, 110),
                "lovas": (210, 170, 90), "orias": (170, 130, 220), "varazslo": (120, 220, 230),
                "repulo": (230, 230, 150), "ostrom": (240, 140, 60), "munkas": (190, 180, 160),
                "epulet": (200, 190, 170)}

    def unit_type(self, e):
        if e.is_building:
            return "epulet"
        return e.defn.get("utype")

    def type_info(self, key):
        ut = self.g.data.game.get("unit_types", {})
        if key == "epulet":
            return ut.get("building", "Épületek"), "Épület – az ostromgépek és az óriások ellen gyenge."
        t = ut.get("types", {}).get(key, {})
        return t.get("name", key), t.get("desc", "")

    def counters(self, e, key):
        """(erős ez ellen: [(típus, szorzó)], gyenge ez ellen: [(típus, szorzó)])."""
        mat = self.g.data.game.get("unit_types", {}).get("matrix", {})
        strong = {}
        if key != "epulet" and not e.defn.get("type_nostrength"):
            strong.update(mat.get(key, {}))
        for k, v in (e.defn.get("type_bonus") or {}).items():
            strong[k] = max(strong.get(k, 1.0), v)
        weak = [(a, m[key]) for a, m in mat.items() if key in m]
        return sorted(strong.items(), key=lambda kv: -kv[1]), sorted(weak, key=lambda kv: -kv[1])

    def type_icon(self, key, size):
        ck = ("utype", key, size)
        img = self.icons.get(ck)
        if img is not None:
            return img
        art = self.sprites.art(f"ui/types/{key}", (size, size)) if hasattr(self.sprites, "art") else None
        if art is not None:
            self.icons[ck] = art
            return art
        img = pygame.Surface((size, size), pygame.SRCALPHA)
        col = self.TYPE_COL.get(key, (220, 220, 220))
        c = size / 2
        pygame.draw.circle(img, (16, 14, 18, 230), (c, c), c)
        pygame.draw.circle(img, col, (c, c), c, max(1, size // 14))
        k = size / 16.0
        lw = max(1, int(round(1.6 * k)))

        def P(*pts):
            return [(px * k, py * k) for px, py in pts]
        if key == "kozelharcos":  # két keresztezett kard
            pygame.draw.line(img, col, *P((4, 12), (12, 4)), lw)
            pygame.draw.line(img, col, *P((4, 4), (12, 12)), lw)
            pygame.draw.line(img, col, *P((3, 10), (6, 13)), lw)
            pygame.draw.line(img, col, *P((10, 13), (13, 10)), lw)
        elif key == "landzsas":  # pajzs
            pygame.draw.polygon(img, col, P((4, 4), (12, 4), (12, 9), (8, 13), (4, 9)), lw)
            pygame.draw.line(img, col, *P((8, 4), (8, 12)), lw)
        elif key == "lovesz":  # íj nyíllal
            pygame.draw.arc(img, col, pygame.Rect(3 * k, 3 * k, 9 * k, 10 * k), -1.4, 1.4, lw)
            pygame.draw.line(img, col, *P((10.5, 3.5), (10.5, 12.5)), max(1, lw // 2))
            pygame.draw.line(img, col, *P((3, 8), (13, 8)), lw)
        elif key == "lovas":  # patkó
            pygame.draw.arc(img, col, pygame.Rect(4 * k, 3.5 * k, 8 * k, 9 * k), 0, 3.14159, lw + 1)
            pygame.draw.line(img, col, *P((4.3, 8), (4.3, 12)), lw + 1)
            pygame.draw.line(img, col, *P((11.7, 8), (11.7, 12)), lw + 1)
        elif key == "orias":  # nagy ököl / szikla
            pygame.draw.circle(img, col, (8 * k, 9 * k), 4.2 * k)
            pygame.draw.circle(img, col, (8 * k, 4.2 * k), 2.0 * k)
        elif key == "varazslo":  # csillag
            import math as _m
            pts = [(8 * k + _m.cos(-1.5708 + i * _m.pi / 5) * (5 if i % 2 == 0 else 2.2) * k,
                    8 * k + _m.sin(-1.5708 + i * _m.pi / 5) * (5 if i % 2 == 0 else 2.2) * k) for i in range(10)]
            pygame.draw.polygon(img, col, pts)
        elif key == "repulo":  # szárnyak
            pygame.draw.lines(img, col, False, P((2, 6), (8, 11), (14, 6)), lw)
            pygame.draw.lines(img, col, False, P((4, 5), (8, 8), (12, 5)), lw)
        elif key == "ostrom":  # robbanás
            import math as _m
            for i in range(8):
                a = i * _m.pi / 4
                pygame.draw.line(img, col, (8 * k + _m.cos(a) * 2 * k, 8 * k + _m.sin(a) * 2 * k),
                                 (8 * k + _m.cos(a) * 5.5 * k, 8 * k + _m.sin(a) * 5.5 * k), lw)
        elif key == "munkas":  # csákány
            pygame.draw.line(img, col, *P((5, 12), (11, 5)), lw)
            pygame.draw.arc(img, col, pygame.Rect(6 * k, 2 * k, 9 * k, 7 * k), 2.0, 3.9, lw)
        else:  # épület
            pygame.draw.polygon(img, col, P((3, 8), (8, 3.5), (13, 8)), lw)
            pygame.draw.rect(img, col, pygame.Rect(4.5 * k, 8 * k, 7 * k, 5 * k), lw)
        self.icons[ck] = img
        return img

    def draw_counter_row(self, e, key, x, bottom, w):
        """A karakterlap alján: ▲ (zöld) ami ellen erős, ▼ (piros) ami ellen gyenge. Visszaadja a sor magasságát."""
        if not key:
            return 0
        strong, weak = self.counters(e, key)
        if not strong and not weak:
            return 0
        s = self.screen
        ft = self.F("tiny")
        isz = max(16, min(34, ft.get_linesize() + 8))
        n = len(strong) + len(weak)
        gap = 2
        avail = w - 2 * (isz // 2 + 4)
        if n * (isz + gap) > avail:  # szűk helyen kisebb ikonok
            isz = max(10, avail // max(1, n) - gap)
        y = bottom - isz - 2
        cx = x

        def tri(up, col):
            nonlocal cx
            h = isz * 0.6
            if up:
                pts = [(cx, y + isz / 2 + h / 2), (cx + h, y + isz / 2 + h / 2), (cx + h / 2, y + isz / 2 - h / 2)]
            else:
                pts = [(cx, y + isz / 2 - h / 2), (cx + h, y + isz / 2 - h / 2), (cx + h / 2, y + isz / 2 + h / 2)]
            pygame.draw.polygon(s, col, pts)
            cx += int(h) + 3
        if strong:
            tri(True, (110, 220, 110))
            for k2, m in strong:
                r = pygame.Rect(cx, y, isz, isz)
                s.blit(self.type_icon(k2, isz), r)
                nm = self.type_info(k2)[0]
                self.regions.append((r, None, None, ("info", f"Erős ez ellen: {nm}",
                                                     f"×{m:g} sebzést okoz nekik.".replace(f"{m:g}", f"{m:g}".replace(".", ",")))))
                cx += isz + gap
            cx += 4
        if weak:
            tri(False, (235, 100, 90))
            for k2, m in weak:
                r = pygame.Rect(cx, y, isz, isz)
                s.blit(self.type_icon(k2, isz), r)
                nm = self.type_info(k2)[0]
                self.regions.append((r, None, None, ("info", f"Gyenge ez ellen: {nm}",
                                                     f"Ők ×{m:g} sebzést okoznak neki.".replace(f"{m:g}", f"{m:g}".replace(".", ",")))))
                cx += isz + gap
        return isz + 4

    def status_text(self, e):
        g, p = self.g, self.p
        if e.owner is not p:
            return ""
        if e.is_building:
            notes = []
            if not e.complete:
                return "Építés alatt – a munkások dolgoznak rajta."
            if e.queue:
                it = e.queue[0]
                d = g.data.defs.get(it["id"]) or g.data.researches.get(it["id"])
                left = max(0, it["time"] - it["progress"])
                notes.append(f"Gyárt: {d['name']} – {left:.0f} mp" + (f" (+{len(e.queue) - 1} a sorban)" if len(e.queue) > 1 else ""))
            if e.defn.get("larva"):
                notes.append(f"Lárvák: {e.larva}/{e.defn['larva']}")
            if not e.powered:
                notes.append("NINCS ENERGIA!")
            if e.defn.get("supply"):
                notes.append(f"+{e.defn['supply']} létszám")
            if e.defn.get("garrison"):
                notes.append(f"Helyőrség {len(e.garrison)}/{e.defn['garrison']}")
            if e.defn.get("produces") and not e.queue:
                notes.append("Jobb klikk a térképen: gyülekezőpont")
            return "  •  ".join(notes)
        bits = []
        if e.carry_amt:
            bits.append(f"Szállít: {e.carry_amt} {p.res_name(e.carry_kind, g.data.game['resources'])}")
        elif e.order:
            o = e.order
            t = {"move": "Mozog", "amove": "Támadó menet", "attack": "Támad", "gather": "Gyűjt", "build": "Építeni megy",
                 "construct": "Épít", "repair": "Javít", "hold": "Tartja a helyét", "cast": "Varázsol"}.get(o["type"], "")
            if o["type"] == "gather" and self.in_mine(e):
                t = "Bent a bányában"
            elif o["type"] == "place_ability":
                t = g.data.abilities[o["ab"]]["name"] + " – úton"
            elif o["type"] == "anchor_rescue":
                t = "Lélekhorgonyt szállít" if o["anchor"].inside is e else "A Lélekhorgonyért megy"
            bits.append(t)
        else:
            bits.append("Pihen")
        if e.owner.faction.get("spell_resource") == "spores" or e.passive(g, "split_kills"):
            n = e.passive(g, "split_kills")
            bits.append(f"Spórák: {e.spores}/6" + (f" • táplálék {e.meals}/{n}" if n else ""))
        if e.origin is not None:
            bits.append("Lélekhorgony – Fabrikátorral javítható")
        if e.morph is not None:
            bits.append(f"Mutáció: {g.data.defs[e.morph[0]]['name']} – {e.morph[1]:.0f} mp")
        return "  •  ".join(b for b in bits if b)

    def hbar(self, x, y, w, h, frac, col, label, tip=None):
        s = self.screen
        pygame.draw.rect(s, (8, 8, 8), (x, y, w, h))
        pygame.draw.rect(s, shade(col, 0.75), (x + 1, y + 1, (w - 2) * max(0.0, min(1.0, frac)), h - 2))
        if label:
            f = self.F("tiny", False)
            img = f.render(label, True, (250, 250, 240))
            if img.get_height() <= h + 6:
                s.blit(img, (x + w / 2 - img.get_width() / 2, y + h / 2 - img.get_height() / 2))
        if tip:
            self.regions.append((pygame.Rect(x, y, w, h), None, None, tip))

    def draw_middle(self):
        """2×3: hősnél a 6 tárgyhely, minden más esetben a fajcímer."""
        s, g, L = self.screen, self.g, self.L
        e = self.primary()
        hero = e if (e is not None and getattr(e, "is_unit", False) and e.hero and e.owner is self.p) else None
        if hero is None:
            r = L["inv"]
            self.well(r, (16, 16, 18))
            fac = e.owner.faction if (e is not None and getattr(e, "owner", None)) else self.p.faction
            sz = min(r.w, r.h) - 8
            em = self.sprites.emblem(fac, sz)
            if em is not None:
                s.blit(em, (r.centerx - em.get_width() // 2, r.centery - em.get_height() // 2))
            self.regions.append((r, None, None, ("info", fac["name"], fac.get("tagline", ""))))
            return
        for i, r in enumerate(L["inv_cells"]):
            self.well(r)
            iid = hero.inventory[i] if i < len(hero.inventory) else None
            if not iid:
                continue
            ic = self.sprites.item_icon(iid, r.w)
            if ic is not None:
                s.blit(ic, r)
            else:
                s.blit(self.F("tiny").render(g.data.items[iid]["name"][:4], True, (230, 220, 190)), (r.x + 3, r.y + 3))
            self.regions.append((r, (lambda i=i: self.cmd("cmd_use_item", hero, i)),
                                 (lambda i=i: self.cmd("cmd_drop_item", hero, i)), ("item", g.data.items[iid])))

    def selection_lifetime(self, rect, entities):
        timers = [t for e in entities if e.alive and (t := getattr(e, "summon_seconds", None)) is not None]
        if not timers:
            return ""
        left = min(timers)
        color = (255, 150, 100) if left <= 5 else (255, 220, 130)
        self.corner_label(rect, f"{left} mp", color, self.F("tiny"), "tr")
        if len(entities) > 1:
            return f" Leghamarabb lejáró idézés: {left} mp."
        return f" Hátralévő idő: {left} mp."

    def draw_sel_grid(self):
        s, L = self.screen, self.L
        cells = self.sel_cells()
        for i, r in enumerate(L["sel_cells"]):
            self.well(r)
            if i >= len(cells):
                continue
            c = cells[i]
            c["key"] = ("cell", c["kind"], c["ents"][0].defn["id"], i)
            if c["kind"] == "queue":
                it = c["item"]
                d = self.g.data.defs.get(it["id"]) or self.g.data.researches[it["id"]]
                kind = "research" if it["kind"] == "research" else "unit"
                s.blit(self.def_icon(d, kind, r.w), r)
                self.clock_wipe(r, it["progress"] / it["time"])
                if c["idx"] == 0:
                    self.corner_label(r, f"{max(0, it['time'] - it['progress']):.0f}", (255, 230, 150), self.F("tiny"))
                self.regions.append((r, (lambda c=c: self.sel_cell_click(c, 1)), (lambda c=c: self.sel_cell_click(c, 1)),
                                     ("info", d["name"], "Gyártósor. Kattintás: visszavonás (az ár visszajár).")))
                continue
            if c["kind"] == "more":
                img = self.F("name").render(f"+{len(c['ents'])}", True, (220, 220, 220))
                s.blit(img, img.get_rect(center=r.center))
                lifetime_tip = self.selection_lifetime(r, c["ents"])
                self.regions.append((r, lambda c=c: self.sel_cell_click(c, 1),
                                     lambda c=c: self.sel_cell_click(c, 3),
                                     ("info", f"További egységek: {len(c['ents'])}",
                                      "Kattintás: csak ezek kijelölése. Shift/jobb klikk: kivétel." + lifetime_tip)))
                continue
            e0 = c["ents"][0]
            kind = "unit" if e0.is_unit else "building"
            s.blit(self.def_icon(e0.defn, kind, r.w, e0.owner.color), r)
            alive = [e for e in c["ents"] if e.alive]
            avg = sum(e.hp / e.max_hp for e in alive) / max(1, len(alive))
            wounded = sum(1 for e in alive if e.hp < e.max_hp * 0.999)
            col = (80, 210, 80) if avg > 0.5 else (235, 170, 50) if avg > 0.25 else (230, 60, 50)
            hb = max(3, round(4 * L["s"]))
            pygame.draw.rect(s, (0, 0, 0), (r.x + 2, r.bottom - hb - 2, r.w - 4, hb))
            pygame.draw.rect(s, col, (r.x + 2, r.bottom - hb - 2, (r.w - 4) * avg, hb))
            if avg <= 0.35:  # veszélyben: visszafogott piros keret
                pygame.draw.rect(s, (200, 70, 50), r, 1)
            if c["kind"] == "group" and len(alive) > 1:
                self.corner_label(pygame.Rect(r.x, r.y, r.w, r.h - hb - 2), f"{len(alive)}×", (255, 255, 255))
            if c["kind"] == "group":
                tip = ("info", f"{e0.name} – {len(alive)} db",
                       f"Átlagos életerő: {avg * 100:.0f}%  •  sérült: {wounded}.  Kattintás: csak ezek; "
                       "Shift/jobb klikk: kivétel a kijelölésből.")
            else:
                tip = ("info", e0.name, f"Életerő: {int(e0.hp)}/{int(e0.max_hp)}.  Kattintás: csak ő; "
                                        "Shift/jobb klikk: kivétel a kijelölésből.")
            lifetime_tip = self.selection_lifetime(r, alive)
            self.regions.append((r, (lambda c=c: self.sel_cell_click(c, 1)), (lambda c=c: self.sel_cell_click(c, 3)),
                                 (tip[0], tip[1], tip[2] + lifetime_tip)))

    def draw_cmd_grid(self):
        s, L = self.screen, self.L
        slots = self.command_card()
        fl = getattr(self, "slot_flash", (None, -9))
        for i, r in enumerate(L["cmd_cells"]):
            self.well(r)
            sl = slots[i]
            if sl is None:  # nincs akció: üres hely, nincs betű
                continue
            if sl["defn"] is not None:
                img = self.def_icon(sl["defn"], sl["kind"], r.w)
            elif sl.get("ally"):
                img = self.ally_icon(sl["ally"], r.w)
            else:
                img = self.cmd_icon(sl["icon"], sl["passive"], sl["ab_id"])
                if img.get_width() != r.w:
                    key = ("cmdsc", id(img), r.w)
                    sc = self.icons.get(key)
                    if sc is None:
                        sc = self.icons[key] = pygame.transform.smoothscale(img, (r.w, r.h))
                    img = sc
            s.blit(img, r)
            if not sl["enabled"] or sl["passive"]:
                dim = pygame.Surface(r.size, pygame.SRCALPHA)
                dim.fill((0, 0, 0, 120 if sl["passive"] else 150))
                s.blit(dim, r)
            if sl["prog"] is not None:
                self.clock_wipe(r, sl["prog"])
            if sl["cd"] > 0:
                self.clock_wipe(r, 1 - sl["cd"])
            if fl[0] == i and self.t - fl[1] < 0.18:  # ikonvillanás: ugyanaz billentyűre és kattintásra
                pygame.draw.rect(s, (255, 255, 255), r, 2)
            elif sl["active"]:
                pygame.draw.rect(s, (255, 214, 110), r, 2)
            else:
                pygame.draw.rect(s, (90, 90, 96) if sl["passive"] else (150, 130, 90), r, 1)
            if sl["cost"] and sl["cost"][0]:
                self.corner_label(r, sl["cost"][0], sl["cost"][1], self.F("tiny"), "tr")
            if sl["count"] > 1:
                self.corner_label(r, str(sl["count"]), (255, 255, 255), self.F("tiny"), "tl")
            self.corner_label(r, SLOT_KEYS[i])  # a billentyű mindig látszik, jobb alul
            tip = sl["tip"] or ("info", sl["label"], "")
            self.regions.append((r, None, None, ("slot", SLOT_KEYS[i], tip)))

    # ---------------------------------------------------------------- rajzolás: felső sáv, küldetések
    def idle_workers(self):
        return [u for u in self.g.units if u.alive and u.owner is self.p and u.is_worker and u.order is None
                and not u.queue and u.inside is None]

    def select_idle_worker(self):
        """A következő tétlen munkás kijelölése (körbe), és odaugrik a kamera."""
        ws = sorted(self.idle_workers(), key=lambda u: u.id)
        if not ws:
            return
        cur = self.sel[0].id if len(self.sel) == 1 else -1
        nxt = next((u for u in ws if u.id > cur), ws[0])
        self.sel = [nxt]
        self.center_on(nxt.x, nxt.y)

    def draw_idle_workers(self, mx, my):
        s, L = self.screen, self.L
        r = L["idle"]
        n = len(self.idle_workers())
        self.bevel(r, fill=(60, 52, 36) if r.collidepoint(mx, my) else (26, 24, 20))
        isz = r.h - 6
        s.blit(self.type_icon("munkas", isz), (r.x + 3, r.y + 3))
        fr = self.F("res")
        img = fr.render(str(n), True, (255, 210, 90) if n else (150, 146, 136))
        s.blit(img, (r.right - img.get_width() - max(5, r.h // 5), r.centery - img.get_height() // 2))
        self.regions.append((r, None, None, ("info", f"Tétlen munkások: {n}",
                                             "Kattintás vagy F8: a következő tétlen munkás kijelölése.")))

    def draw_topbar(self):
        s, g, p, L = self.screen, self.g, self.p, self.L
        mx, my = pygame.mouse.get_pos()
        self.draw_idle_workers(mx, my)
        for key in ("allies", "gear"):
            r = L[key]
            self.bevel(r, fill=(60, 52, 36) if r.collidepoint(mx, my) else (26, 24, 20))
            size = max(1, round(r.h * .92))
            art = self.sprites.art(f"ui/icons/{key}", (size, size))
            if art is None:
                (self._draw_allies_icon if key == "allies" else self._draw_gear)(r)
            else:
                s.blit(art, art.get_rect(center=r.center))
        rb = L["resbar"]
        self.bevel(rb, fill=(20, 20, 22))
        pygame.draw.rect(s, (6, 6, 6), rb.inflate(-6, -6))  # sötét, süllyesztett sáv: a számok jól olvashatók
        rd = g.data.game["resources"]
        self._res_sample()
        fr = self.F("res")
        for i, r in enumerate(L["res"]):
            if i:
                pygame.draw.line(s, (70, 64, 52), (r.x, r.y + 6), (r.x, r.bottom - 7))
            if i < 3:
                k = RES_ORDER[i]
                val, col = int(p.res.get(k, 0)), (250, 240, 170)
                name = p.res_name(k, rd)
                fl = self.t - self.res_flash.get(k, -99)
                if fl < 1.6:  # hiány: diszkrét, elhalványuló piros jelzés
                    a = int(200 * (1 - fl / 1.6))
                    hl = pygame.Surface((r.w - 8, r.h - 8), pygame.SRCALPHA)
                    hl.fill((200, 40, 30, a // 2))
                    s.blit(hl, (r.x + 4, r.y + 4))
                    col = (255, 140, 110)
                self._res_icon(k, r, tuple(rd[k]["color"]))
                d = self._res_delta(k)
                tip = ("info", f"{name}: {val}", (f"Az utolsó 10 mp-ben: {d:+d}" if d else "Nincs változás az utolsó 10 mp-ben."))
                txt = str(val)
            else:
                full = p.supply_used >= p.supply_cap
                uf = g.upkeep_factor(p)  # Hadtáp: a sáv színe jelzi, mennyi arany jön be
                col = (255, 120, 90) if full else (250, 240, 170) if uf >= 1 else (255, 210, 90) if uf >= 0.7 \
                    else (255, 150, 70)
                self._res_icon("supply", r, (240, 210, 120))
                txt = f"{math.ceil(p.supply_used - 1e-6)}/{p.supply_cap}"
                how = p.faction.get("supply_desc", "épületekkel")
                hadtap = ""
                if getattr(g, "upkeep_on", False):
                    hadtap = (f" Hadtáp: az aranybevétel most {round(uf * 100)}% "
                              "(0–80 létszám: 100%, 81–140: 70%, 141–200: 40%).")
                tip = ("info", f"{g.data.game.get('supply_name', 'Létszám')}: {txt}",
                       ("ELÉRTED A KORLÁTOT – új egység nem képezhető. " if full else "") + f"Növelése: {how}." + hadtap)
            img = fr.render(txt, True, col)
            s.blit(img, (r.right - img.get_width() - max(6, r.h // 4), r.centery - img.get_height() // 2))
            self.regions.append((r, None, None, tip))
        fa = p.faction.get("faith")
        if fa:  # Ö17: Áhítat (elfek) / Tóének (Huk’ucha)
            r = L["faith"]
            self.bevel(r, fill=(20, 20, 22))
            pygame.draw.rect(s, (6, 6, 6), r.inflate(-6, -6))
            self._res_icon("faith", r, tuple(rd["faith"]["color"]))
            val = int(p.res.get("faith", 0))
            n = getattr(p, "praying", 0)
            img = fr.render(str(val), True, (255, 226, 150))
            s.blit(img, (r.right - img.get_width() - max(6, r.h // 4), r.centery - img.get_height() // 2))
            rate = p.rules.get("faith_rate", fa.get("rate", 0.3))
            self.regions.append((r, None, None, ("info", f"{fa['name']}: {val}",
                                                 f"Most {n} munkás {fa.get('verb', 'fohászkodik')} "
                                                 f"(legfeljebb {fa.get('max', 8)}), {n * rate:.1f}/mp. "
                                                 + fa.get("desc", ""))))
        self.regions.append((L["allies"], None, None, ("info", "Szövetségesek", "A játékosok és szövetségeik.")))
        self.regions.append((L["gear"], None, None, ("info", "Menü", "Szünet, mentés, betöltés, beállítások, kilépés.")))
        if self.show_allies:
            self.draw_allies_panel()

    def _res_icon(self, k, r, col):
        s = self.screen
        sz = r.h - max(8, r.h // 3)
        art = self.sprites.art(f"ui/res/{k}", (sz, sz)) if k != "supply" else \
            self.sprites.art(f"ui/res/supply_{self.p.faction['id']}", (sz, sz))
        x, cy = r.x + max(6, r.h // 4), r.centery
        if art is not None:
            s.blit(art, (x, cy - sz // 2))
            return
        h = sz // 2
        if k == "meteor":  # lila kristály
            pts = [(x + h, cy - h), (x + h + h // 2, cy - h // 3), (x + h + h // 3, cy + h), (x + h - h // 3, cy + h),
                   (x + h // 2, cy - h // 3)]
            pygame.draw.polygon(s, col, pts)
            pygame.draw.polygon(s, (240, 200, 255), pts, 1)
        elif k == "wood":  # rönk
            pygame.draw.rect(s, (120, 80, 44), (x + 2, cy - h // 2, sz - 4, h))
            pygame.draw.ellipse(s, (210, 170, 110), (x + sz - h // 2 - 4, cy - h // 2, h // 2 + 2, h))
            pygame.draw.ellipse(s, (120, 80, 44), (x + sz - h // 2 - 2, cy - h // 4, h // 4, h // 2), 1)
        elif k == "stone":  # aranyrög
            pygame.draw.ellipse(s, (150, 110, 20), (x + 1, cy - h // 2 + 1, sz - 2, h + 2))
            pygame.draw.ellipse(s, col, (x + 1, cy - h // 2, sz - 3, h))
            pygame.draw.ellipse(s, (255, 245, 190), (x + sz // 4, cy - h // 3, sz // 4, h // 3))
        else:  # létszám: alak
            pygame.draw.circle(s, col, (x + h, cy - h // 2), max(2, h // 3))
            pygame.draw.rect(s, col, (x + h // 2, cy - h // 6, h, h // 2 + 3), border_radius=3)

    def _draw_allies_icon(self, r):
        s = self.screen
        cx, cy, u = r.centerx, r.centery, max(3, r.h // 7)
        for dx, sc, col in ((-2 * u, 0.85, (180, 170, 150)), (2 * u, 0.85, (180, 170, 150)), (0, 1.0, (240, 226, 190))):
            hr = max(2, int(u * sc))
            pygame.draw.circle(s, col, (cx + dx, cy - int(u * 1.2)), hr)
            pygame.draw.rect(s, col, (cx + dx - int(u * 1.3 * sc), cy, int(u * 2.6 * sc), int(u * 2.2 * sc)),
                             border_radius=hr)

    def _draw_gear(self, r):
        s = self.screen
        cx, cy = r.center
        R1, R0 = r.h * 0.34, r.h * 0.24
        pts = []
        for k in range(16):
            a = k / 16 * 2 * math.pi
            rr = R1 if k % 2 == 0 else R0
            pts.append((cx + math.cos(a) * rr, cy + math.sin(a) * rr))
        pygame.draw.polygon(s, (220, 210, 186), pts)
        pygame.draw.circle(s, (26, 24, 20), (cx, cy), int(R0 * 0.55))

    def _res_sample(self):
        """Erőforrás-előzmények (tooltip: változás az utolsó 10 mp-ben); másodpercenként egy minta."""
        h = self.__dict__.setdefault("_res_hist", [])
        if not h or self.g.time - h[-1][0] >= 1.0:
            h.append((self.g.time, dict(self.p.res)))
            del h[:-12]

    def _res_delta(self, k):
        h = getattr(self, "_res_hist", [])
        if len(h) < 2:
            return 0
        old = next((v for t, v in h if self.g.time - t <= 10.5), h[0][1])
        return int(self.p.res.get(k, 0) - old.get(k, 0))

    def draw_allies_panel(self):
        s, g, L = self.screen, self.g, self.L
        F = self.F("small")
        rows = [q for q in g.players if not q.neutral]
        lh = F.get_linesize() + 4
        w = round(360 * L["s"]) + 60
        r = pygame.Rect(L["topbar"].centerx - w // 2, L["topbar"].bottom + 6, w, lh * len(rows) + 16)
        self.allies_rect = r
        self.bevel(r, fill=(16, 16, 18))
        y = r.y + 8
        for q in rows:
            rel = "Te" if q is self.p else "Szövetséges" if self.p.allied(q) else "Ellenfél"
            col = (150, 230, 150) if rel != "Ellenfél" else (240, 140, 120)
            pygame.draw.rect(s, q.color, (r.x + 10, y + 3, 12, lh - 10))
            nm = q.name + ("  (kiesett)" if q.defeated else "")
            s.blit(F.render(nm, True, (230, 226, 210) if not q.defeated else (130, 130, 130)), (r.x + 30, y))
            s.blit(self.F("tiny").render(q.faction["name"], True, (180, 176, 160)), (r.x + 30 + w // 2 - 40, y + 2))
            img = self.F("tiny").render(rel, True, col)
            s.blit(img, (r.right - img.get_width() - 10, y + 2))
            y += lh

    def quest_lines(self):
        g = self.g
        if g.director is not None:
            return g.director.status_lines()
        return [(g.result is not None and g.result[0] == "win", "Győzd le az összes ellenfelet")]

    def draw_quests(self):
        """Bal felül: háttér nélküli, árnyékolt célkitűzés-lista jelölőnégyzetekkel; 3 sor felett összecsukható."""
        s = self.screen
        x, y = self.L["quest"]
        self.quest_regions = []
        lines = self.quest_lines()
        if not lines:
            return y
        F = self.F("small", False)
        lh = F.get_linesize() + 3
        box = max(8, lh - 9)
        hw = shadow_text(s, self.F("res"), "Küldetések", (x, y), (255, 214, 120))
        k = max(4, self.F("res").get_linesize() // 4)
        cx, cy = x + hw + 6 + k, y + self.F("res").get_linesize() // 2
        open_ = self.quests_open or len(lines) <= 3
        tri = [(cx - k, cy - k // 2), (cx + k, cy - k // 2), (cx, cy + k)] if open_ else             [(cx - k // 2, cy - k), (cx + k, cy), (cx - k // 2, cy + k)]
        pygame.draw.polygon(s, (0, 0, 0), [(a + 1, b + 1) for a, b in tri])
        pygame.draw.polygon(s, (220, 210, 170), tri)
        self.quest_regions.append((pygame.Rect(x, y, hw + 20, lh),
                                   lambda: setattr(self, "quests_open", not self.quests_open)))
        y += self.F("res").get_linesize() + 2
        show = lines if (self.quests_open or len(lines) <= 3) else lines[:3]
        for done, txt in show:
            col = (150, 235, 150) if done else (240, 236, 214)
            r = pygame.Rect(x, y + (lh - box) // 2 - 1, box, box)
            pygame.draw.rect(s, (0, 0, 0), r.move(1, 1), 1)
            pygame.draw.rect(s, col, r, 1)
            if done:  # kipipálva
                pygame.draw.lines(s, col, False, [(r.x + 2, r.centery), (r.centerx - 1, r.bottom - 3), (r.right + 2, r.y - 2)], 2)
            shadow_text(s, F, txt, (x + box + 8, y), col)
            y += lh
        if len(lines) > 3:
            lab = "kevesebb" if self.quests_open else f"még {len(lines) - 3} cél…"
            w = shadow_text(s, self.F("tiny", False), lab, (x + box + 8, y), (200, 200, 170))
            self.quest_regions.append((pygame.Rect(x, y, w + box + 8, lh),
                                       lambda: setattr(self, "quests_open", not self.quests_open)))
            y += lh
        return y + 4

    def draw_clock(self, fps):
        """Jobb felül, visszafogottan: játékidő, sebesség −/+ (nem a felső erőforrássáv része)."""
        s, g, L = self.screen, self.g, self.L
        F = self.F("small", True)
        t = int(g.time)
        h = L["topbar"].h
        x = self.sw - L["pad"] * 2
        y = L["topbar"].y
        mx, my = pygame.mouse.get_pos()
        self.speed_rects = []
        st = self.settings
        items = [("+", 1), (f"×{self.cur_speed():g}", None), ("−", -1)] if st.get("show_speed", True) else []
        for lab, d in items:
            img = F.render(lab, True, (255, 230, 160) if d else (200, 220, 255))
            if d is None:
                x -= img.get_width()
                shadow_text(s, F, lab, (x, y + (h - img.get_height()) // 2), (200, 220, 255))
                x -= 6
                continue
            r = pygame.Rect(x - (h - 6), y + 3, h - 6, h - 6)
            pygame.draw.rect(s, (70, 60, 36) if r.collidepoint(mx, my) else (20, 18, 14), r, border_radius=4)
            pygame.draw.rect(s, (160, 136, 80), r, 1, border_radius=4)
            s.blit(img, img.get_rect(center=r.center))
            self.speed_rects.append((r, d))
            x = r.x - 6
        if st.get("show_clock", True):
            txt = f"{t // 60:02d}:{t % 60:02d}"
            img = F.render(txt, True, (240, 235, 200))
            x -= img.get_width() + 4
            shadow_text(s, F, txt, (x, y + (h - img.get_height()) // 2), (240, 235, 200))
        parts = ([f"{fps:.0f} fps"] if st.get("show_fps", True) else []) +             (["F1: súgó"] if st.get("show_help_hint", True) else [])
        if parts:
            ft = self.F("tiny", False)
            lab = "  •  ".join(parts)
            shadow_text(s, ft, lab, (self.sw - L["pad"] * 2 - ft.size(lab)[0], y + h + 2), (170, 170, 160))
        self.clock_rect = pygame.Rect(x, y, self.sw - x, h)
