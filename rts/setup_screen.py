"""Játékbeállító képernyő (egyéni csata és LAN-lobbi): pályaválasztás minitérképpel, 8 játékoshely
lenyíló menükkel (gazda / faj / csapat / szín / kezdőhely) és játékbeállítások."""
import random

import pygame

from .i18n import name as dname, pick, tr
from .menu import DIM, GOLD, TEXT, Button, Screen, fit_font, label, panel, wrap

SLOT_KINDS = ["human", "open", "closed", "ai_easy", "ai_normal", "ai_hard"]


def kind_label(k):
    return {"human": tr("Játékos"), "open": tr("Nyitott"), "closed": tr("Zárt"), "ai_easy": tr("Gép – könnyű"),
            "ai_normal": tr("Gép – közepes"), "ai_hard": tr("Gép – nehéz"), "remote": tr("Játékos (hálózat)")}.get(k, k)



def pick_path(pd):
    """Fejlődési irány neve az aktuális nyelven."""
    return tr(pd.get("name", ""))

class Dropdown:
    """Lenyíló menü: [(érték, felirat)]; a nyitott lista a képernyő legfelső rétegén.
    A felirat, ami nem fér el, kisebb betűre vált, végül „…”-ra rövidül."""

    def __init__(self, rect, options, value, on_change, enabled=True, colors=None, u=1.0):
        self.rect = pygame.Rect(rect)
        self.options = options
        self.value = value
        self.on_change = on_change
        self.enabled = enabled
        self.colors = colors  # érték -> szín (szín-választóhoz)
        self.open = False
        self.u = u

    def label(self):
        for v, lab in self.options:
            if v == self.value:
                return lab
        return str(self.value)

    def item_h(self):
        return int(round(26 * self.u))

    def list_w(self, fonts):
        sw = int(round(24 * self.u)) if self.colors else 0
        return max(self.rect.w, max(fonts["small"].size(lab)[0] for _, lab in self.options) + sw + int(16 * self.u))

    def list_rects(self, screen_h, fonts=None, screen_w=None):
        h = self.item_h()
        n = len(self.options)
        y = self.rect.bottom
        if y + n * h > screen_h - 4:
            y = self.rect.y - n * h
        w = self.list_w(fonts) if fonts else self.rect.w
        x = self.rect.x
        if screen_w is not None:
            w = min(w, screen_w)
            x = max(0, min(x, screen_w - w))
        return [pygame.Rect(x, y + i * h, w, h) for i in range(n)]

    def draw(self, s, fonts, mouse):
        r = self.rect
        hover = r.collidepoint(mouse) and self.enabled
        pygame.draw.rect(s, (40, 34, 24) if self.enabled else (30, 30, 32), r)
        pygame.draw.rect(s, GOLD if (hover or self.open) else (130, 104, 62), r, 1)
        pad = max(4, int(6 * self.u))
        tx = r.x + pad
        if self.colors and self.value in self.colors:
            sq = r.h - 2 * pad
            pygame.draw.rect(s, self.colors[self.value], (r.x + pad, r.y + pad, sq, sq))
            tx += sq + pad
        arrow = int(16 * self.u) if self.enabled else 0
        f, txt = fit_font(fonts, ["small", "tiny"], self.label(), r.right - arrow - tx - 2)
        s.blit(f.render(txt, True, TEXT if self.enabled else DIM), (tx, r.centery - f.get_height() / 2))
        if self.enabled:
            cx, cy, d = r.right - arrow / 2 - 1, r.centery, max(3, int(4 * self.u))
            pygame.draw.polygon(s, GOLD, [(cx - d, cy - d / 2), (cx + d, cy - d / 2), (cx, cy + d * 0.75)])

    def draw_list(self, s, fonts, mouse):
        pad = max(4, int(6 * self.u))
        for (v, lab), rr in zip(self.options, self.list_rects(s.get_height(), fonts, s.get_width())):
            hov = rr.collidepoint(mouse)
            pygame.draw.rect(s, (90, 70, 36) if hov else (24, 22, 18), rr)
            pygame.draw.rect(s, (110, 90, 56), rr, 1)
            tx = rr.x + pad
            if self.colors and v in self.colors:
                sq = rr.h - 2 * pad
                pygame.draw.rect(s, self.colors[v], (rr.x + pad, rr.y + pad, sq, sq))
                tx += sq + pad
            font, txt = fit_font(fonts, ["small", "tiny"], lab, max(0, rr.right - tx - pad))
            s.blit(font.render(txt, True, GOLD if v == self.value else TEXT),
                   (tx, rr.centery - font.get_height() / 2))

    def click(self, pos, screen_h, fonts=None, screen_w=None):
        """True, ha a kattintást elnyelte."""
        if self.open:
            for (v, _), rr in zip(self.options, self.list_rects(screen_h, fonts, screen_w)):
                if rr.collidepoint(pos):
                    self.open = False
                    if v != self.value:
                        self.value = v
                        self.on_change(v)
                    return True
            self.open = False
            return True
        if self.enabled and self.rect.collidepoint(pos):
            self.open = True
            return True
        return False


class SetupScreen(Screen):
    BG = "setup"      # csataindítás
    """mode: "skirmish" | "host" | "client". A LAN-mód a lobby objektumon keresztül szinkronizál."""

    def __init__(self, app, mode="skirmish", lobby=None):
        super().__init__(app)
        from .mapgen import list_maps
        self.mode = mode
        self.lobby = lobby
        self.maps = list_maps(app.data)
        prof = app.profile.data.get("setup", {})
        self.map_id = prof.get("map", self.maps[0]["id"])
        if not any(m["id"] == self.map_id for m in self.maps):
            self.map_id = self.maps[0]["id"]
        self.facs = [f["id"] for f in app.data.faction_list()]
        mine = app.profile.data.get("emblem") if app.profile.data.get("emblem") in self.facs else "random"
        self.slots = [{"kind": "human", "faction": mine, "team": None, "color": 0, "start": None,
                       "name": app.profile.name}]
        for i in range(1, 8):
            self.slots.append({"kind": "ai_normal" if i == 1 else "closed", "faction": "random", "team": None,
                               "color": i, "start": None, "name": ""})
        self.options = dict({"gametype": "ffa", "resources": "medium", "creeps": True, "speed": 1.0,
                             "theme": "map", "fog": "normal", "upkeep": True}, **prof.get("options", {}))
        self.map_scroll = 0
        self.thumbs = {}
        self.dropdowns = []
        self.chat_input = ""
        self._apply_map()

    # ------------------------------------------------------------------
    def md(self):
        return next(m for m in self.maps if m["id"] == self.map_id)

    def _apply_map(self):
        n = self.md()["players"]
        for i, sl in enumerate(self.slots):
            if i >= n:
                sl["kind"] = "closed" if sl["kind"] != "remote" else sl["kind"]
            if sl["start"] is not None and sl["start"] >= n:
                sl["start"] = None
        if self.md().get("layout") == "rows" and self.options["gametype"] == "ffa":
            self.options["gametype"] = "topbottom"
        self._sync()

    def _sync(self):
        if self.lobby is not None and self.mode == "host":
            self.lobby.push_setup(self.config())

    def editable(self, i):
        if self.mode == "client":
            return False
        return True

    def config(self):
        n = self.md()["players"]
        slots = []
        gt = self.options["gametype"]
        for i, sl in enumerate(self.slots[:n]):
            if sl["kind"] in ("closed", "open"):
                continue
            d = dict(sl)
            if gt == "ffa":
                d["team"] = None
            slots.append(d)
        if gt == "topbottom":  # a pálya két oldala
            for k, d in enumerate(slots):
                d["team"] = 1 + k % 2
        return {"map": self.map_id, "slots": slots, "options": dict(self.options)}

    # ------------------------------------------------------------------
    def layout(self):
        W, H = self.s.get_size()
        self.W, self.H = W, H
        k, u = self.k, self.u
        open_dd = next((d for d in self.dropdowns if d.open), None)
        dds = []
        top = self.top
        self.lx, self.lw = k(24), k(270)  # pályalista
        x0 = self.lx + self.lw + k(24)
        # annyi pálya látszik, hogy alatta elférjen az előnézet és a leírás
        self.nmaps = max(5, min(10, (H - self.top - k(100) - k(36) - k(330)) // k(26)))
        self.map_scroll = max(0, min(self.map_scroll, len(self.maps) - self.nmaps))
        n = self.md()["players"]
        # oszlopok: #, résztvevő, faj, csapat, szín, hely, irány (gép), szövetség (gép) – a szélességhez igazítva
        weights = [24, 167, 156, 70, 122, 112, 112, 140]  # a leghosszabb feliratok szerint
        gap = k(4)
        sc = (W - x0 - k(30) - gap * (len(weights) - 1)) / sum(weights)
        cols, x = [], x0
        for w in weights:
            cols.append((x, int(w * sc)))
            x += int(w * sc) + gap
        self.row_y0, self.row_h = top + k(36), k(34)
        dh = k(27)
        colors = {i: tuple(c) for i, c in enumerate(self.app.data.game["player_colors"])}
        cnames = self.app.data.game.get("player_color_names", {})
        from . import i18n
        cn = cnames.get(i18n.LANG, cnames.get("hu", []))
        for i, sl in enumerate(self.slots[:8]):
            y = self.row_y0 + i * self.row_h
            active = i < n
            ed = active and self.editable(i)
            kinds = ["human"] if i == 0 else (["open", "closed", "ai_easy", "ai_normal", "ai_hard"] if self.mode == "host"
                                              else ["closed", "ai_easy", "ai_normal", "ai_hard"])
            if sl["kind"] == "remote":
                kinds = ["remote", "closed"]
            klabs = [(kk, sl.get("name") if kk in ("human", "remote") and sl.get("name") else kind_label(kk))
                     for kk in kinds]
            dds.append(Dropdown((cols[1][0], y, cols[1][1], dh), klabs, sl["kind"],
                                (lambda v, sl=sl: (sl.update(kind=v), self._sync())), ed and i > 0, u=u))
            mine_ok = ed or (self.lobby is not None and self.lobby.my_slot == i)
            facs = [("random", tr("Véletlen"))] + [(f, dname(self.app.data.factions[f])) for f in self.facs]
            dds.append(Dropdown((cols[2][0], y, cols[2][1], dh), facs, sl["faction"],
                                (lambda v, i=i: self.set_slot(i, faction=v)),
                                mine_ok and sl["kind"] not in ("closed", "open"), u=u))
            teams = [(None, "–")] + [(t, str(t)) for t in range(1, 5)]
            dds.append(Dropdown((cols[3][0], y, cols[3][1], dh), teams, sl["team"],
                                (lambda v, i=i: self.set_slot(i, team=v)),
                                mine_ok and self.options["gametype"] == "teams" and sl["kind"] not in ("closed", "open"),
                                u=u))
            dds.append(Dropdown((cols[4][0], y, cols[4][1], dh), [(c, cn[c] if c < len(cn) else str(c)) for c in colors],
                                sl["color"], (lambda v, i=i: self.set_slot(i, color=v)),
                                mine_ok and sl["kind"] not in ("closed", "open"), colors=colors, u=u))
            starts = [(None, tr("Véletlen"))] + [(kk, str(kk + 1)) for kk in range(n)]
            dds.append(Dropdown((cols[5][0], y, cols[5][1], dh), starts, sl["start"],
                                (lambda v, i=i: self.set_slot(i, start=v)),
                                mine_ok and sl["kind"] not in ("closed", "open"), u=u))
            # gépi ellenfél fejlődési iránya (Felemelkedés után): automatikus vagy megadott
            fpaths = self.app.data.factions.get(sl["faction"], {}).get("paths", {}) if sl["faction"] != "random" else {}
            if sl.get("path") not in fpaths:
                sl["path"] = "auto"
            popts = [("auto", tr("Gép dönt"))] + [(pid, pick_path(pd)) for pid, pd in fpaths.items()]
            dds.append(Dropdown((cols[6][0], y, cols[6][1], dh), popts, sl["path"],
                                (lambda v, i=i: self.set_slot(i, path=v)),
                                ed and sl["kind"].startswith("ai") and bool(fpaths), u=u))
            # gépi ellenfél szövetséges népe (Diplomácia csarnoka): automatikus vagy megadott
            aopts = [("auto", tr("Gép dönt"))] + [(f["id"], dname(f)) for f in self.app.data.allies()]
            if sl.get("ally") not in [a for a, _ in aopts]:
                sl["ally"] = "auto"
            dds.append(Dropdown((cols[7][0], y, cols[7][1], dh), aopts, sl["ally"],
                                (lambda v, i=i: self.set_slot(i, ally=v)), ed and sl["kind"].startswith("ai"), u=u))
        self.slots_rect = pygame.Rect(x0 - k(10), top, W - x0 - k(14), self.row_y0 + 8 * self.row_h + k(8) - top)
        oy = self.slots_rect.bottom + k(16)
        opt_defs = [
            ("gametype", tr("Játéktípus"), [("ffa", tr("Mindenki mindenki ellen")), ("teams", tr("Csapatok")),
                                             ("topbottom", tr("Fent kontra lent"))]),
            ("resources", tr("Nyersanyag"), [("low", tr("Kevés")), ("medium", tr("Közepes")), ("high", tr("Sok"))]),
            ("creeps", tr("Szörnytáborok"), [(True, tr("Be")), (False, tr("Ki"))]),
            ("speed", tr("Sebesség"), [(0.5, "x0.5"), (0.75, tr("Lassú") + " (x0.75)"), (1.0, tr("Normál")),
                                        (1.25, "x1.25"), (1.5, tr("Gyors") + " (x1.5)"), (2.0, "x2"), (3.0, "x3")]),
            ("theme", tr("Táj"), [("map", tr("Pálya szerint")), ("forest", tr("Erdő")), ("winter", tr("Tél")),
                                   ("wasteland", tr("Pusztaság")), ("swamp", tr("Mocsár")), ("autumn", tr("Ősz")),
                                   ("rift", tr("Hasadékvidék"))]),
            ("fog", tr("Köd"), [("normal", tr("Normál")), ("explored", tr("Felderített térkép")),
                                 ("none", tr("Nincs köd"))]),
            ("upkeep", tr("Hadtáp"), [(True, tr("Be")), (False, tr("Ki"))]),
        ]
        cw = min(k(300), (W - x0 - k(24)) // 3)
        self.opt_labels = []
        for kk, (key, lab, opts) in enumerate(opt_defs):
            x = x0 + (kk % 3) * cw
            y = oy + k(10) + (kk // 3) * k(62)
            self.opt_labels.append((lab, x, y, cw - k(20)))
            dds.append(Dropdown((x, y + k(24), cw - k(20), dh), opts, self.options[key],
                                (lambda v, key=key: (self.options.__setitem__(key, v), self._sync())),
                                self.mode != "client", u=u))
        self.opt_rect = pygame.Rect(x0 - k(10), oy, 3 * cw + k(4), ((len(opt_defs) + 2) // 3) * k(62) + k(14))
        if open_dd is not None:  # a nyitott lista megmarad a következő képkockában is
            for d in dds:
                if d.rect == open_dd.rect and d.options == open_dd.options:
                    d.open = True
        self.dropdowns = dds
        self.cols = cols
        self.buttons = []
        if open_dd is None:
            start_lab = tr("INDÍTÁS") if self.mode != "client" else (tr("KÉSZ") if not self.ready() else tr("NEM KÉSZ"))
            self.buttons.append(self.main_button(start_lab, self.start))
            self.buttons.append(Button((k(36), H - k(76), k(190), k(50)), tr("Vissza"), self.back))
            for kk, m in enumerate(self.visible_maps()):
                self.buttons.append(Button(self.map_row(kk), "", (lambda mid=m["id"]: self.pick_map(mid)),
                                           invisible=True, enabled=self.mode != "client"))

    def map_row(self, kk):
        return pygame.Rect(self.lx + self.k(6), self.top + self.k(32) + kk * self.k(26), self.lw - self.k(12), self.k(24))

    def visible_maps(self):
        return self.maps[self.map_scroll:self.map_scroll + getattr(self, "nmaps", 10)]

    def ready(self):
        return self.lobby is not None and self.lobby.ready

    def set_slot(self, i, **kv):
        if "faction" in kv:  # új fajnál az irány újra automatikus
            kv.setdefault("path", "auto")
        if self.lobby is not None and self.mode == "client":
            self.lobby.request_slot(kv)
            return
        self.slots[i].update(kv)
        self._sync()

    def pick_map(self, mid):
        self.map_id = mid
        self._apply_map()

    def back(self):
        if self.lobby is not None:
            self.lobby.close()
        return ("back",)

    def start(self):
        cfg = self.config()
        if self.mode == "client":
            self.lobby.set_ready(not self.lobby.ready)
            return None
        humans = [s for s in cfg["slots"] if s["kind"] in ("human", "remote")]
        if len(cfg["slots"]) < 2:
            self.app.toast = tr("Legalább két résztvevő kell.")
            return None
        if self.options["gametype"] == "teams" and len({s.get("team") for s in cfg["slots"]}) < 2:
            self.app.toast = tr("Csapatjátékhoz legalább két különböző csapat kell.")
            return None
        if self.mode == "host":
            if not self.lobby.all_ready():
                self.app.toast = tr("Nem minden játékos kész.")
                return None
        prof = self.app.profile.data
        prof["setup"] = {"map": self.map_id, "options": self.options}
        self.app.profile.save()
        cfg["seed"] = random.randrange(1 << 30)
        if self.mode == "host":
            return ("start_lan", self.lobby.start_game(cfg))
        return ("start_custom", cfg, humans)

    # ------------------------------------------------------------------
    def handle(self, ev):
        if self.lobby is not None:
            self.lobby.poll(self)
        if ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            for d in self.dropdowns:  # előbb a nyitott lista
                if d.open:
                    d.click(ev.pos, self.H, self.fonts, self.s.get_width())
                    for o in self.dropdowns:
                        o.open = False
                    return
            for d in self.dropdowns:
                if d.click(ev.pos, self.H, self.fonts, self.s.get_width()):
                    return
        elif ev.type == pygame.MOUSEBUTTONDOWN and ev.button in (4, 5) and ev.pos[0] < self.lx + self.lw:
            self.map_scroll = max(0, min(len(self.maps) - self.nmaps, self.map_scroll + (-1 if ev.button == 4 else 1)))
        elif ev.type == pygame.KEYDOWN and self.lobby is not None:
            if ev.key == pygame.K_RETURN and self.chat_input.strip():
                self.lobby.chat(self.chat_input.strip())
                self.chat_input = ""
            elif ev.key == pygame.K_BACKSPACE:
                self.chat_input = self.chat_input[:-1]
            elif ev.unicode and ev.unicode.isprintable() and len(self.chat_input) < 80:
                self.chat_input += ev.unicode

    def thumb(self, md):
        t = self.thumbs.get(md["id"])
        if t is None:
            from .mapgen import generate
            m = generate(self.app.data, md, 1)
            surf = pygame.Surface((m.w, m.h))
            cols = {0: (70, 112, 52), 1: (128, 104, 70), 2: (40, 80, 150), 3: (96, 92, 88)}
            for y in range(m.h):
                for x in range(m.w):
                    surf.set_at((x, y), cols[m.terrain[y * m.w + x]])
            for r in m.resources:
                c = {"wood": (24, 64, 26), "stone": (210, 210, 210), "meteor": (90, 230, 255)}.get(r.kind, (255, 0, 0))
                pygame.draw.rect(surf, c, (r.tx, r.ty, r.w, r.h))
            t = (pygame.transform.smoothscale(surf, (480, 480)), [(sx / m.w, sy / m.h) for sx, sy in m.starts])
            self.thumbs[md["id"]] = t
        return t

    def draw(self):
        s = self.s
        if self.lobby is not None:
            self.lobby.poll(self)
        W, H = s.get_size()
        F, k = self.fonts, self.k
        mouse = pygame.mouse.get_pos()
        self.title(tr("EGYÉNI CSATA") if self.mode == "skirmish" else tr("TÖBBJÁTÉKOS – LAN"))
        top = self.top
        # ---- pályalista
        lp = pygame.Rect(self.lx, top, self.lw, H - top - k(100))
        panel(s, lp)
        label(s, F["med"], tr("Pálya"), (lp.x + k(10), lp.y + k(6)), GOLD)
        iw = lp.w - k(24)
        for kk, m in enumerate(self.visible_maps()):
            r = self.map_row(kk)
            sel = m["id"] == self.map_id
            if sel or r.collidepoint(mouse):
                pygame.draw.rect(s, (90, 70, 36) if sel else (50, 40, 26), r)
            f, txt = fit_font(F, ["small", "tiny"], f"[{m['players']}]  " + pick(m["name"]), iw)
            s.blit(f.render(txt, True, GOLD if sel else TEXT), (r.x + k(6), r.centery - f.get_height() / 2))
        nm = self.nmaps
        ty = self.map_row(nm).y
        if len(self.maps) > nm:
            label(s, F["tiny"], tr("görgess: további pályák") + f" ({self.map_scroll + 1}–"
                  f"{min(len(self.maps), self.map_scroll + nm)}/{len(self.maps)})", (lp.x + k(10), ty), DIM, iw)
        ty += F["tiny"].get_linesize() + k(6)
        md = self.md()
        img, starts = self.thumb(md)
        size = min(lp.w - k(12), lp.bottom - ty - k(104))
        if size > k(60):
            sm = pygame.transform.smoothscale(img, (size, size))
            tx = lp.x + (lp.w - size) // 2
            s.blit(sm, (tx, ty))
            pygame.draw.rect(s, GOLD, (tx, ty, size, size), 1)
            for kk, (fx, fy) in enumerate(starts):
                px, py = tx + fx * size, ty + fy * size
                pygame.draw.circle(s, (255, 255, 255), (px, py), k(8))
                lab = F["tiny"].render(str(kk + 1), True, (0, 0, 0))
                s.blit(lab, (px - lab.get_width() / 2, py - lab.get_height() / 2))
            ty += size + k(6)
        theme = {"forest": tr("Erdő"), "winter": tr("Tél"), "wasteland": tr("Pusztaság"), "swamp": tr("Mocsár"),
                 "autumn": tr("Ősz"), "rift": tr("Hasadékvidék")}
        label(s, F["med"], pick(md["name"]), (lp.x + k(10), ty), GOLD, iw)
        ty += F["med"].get_linesize()
        label(s, F["tiny"], tr("{n} játékos • {s}×{s} • {t}", n=md["players"], s=md["size"],
                               t=theme.get(md.get("theme"), "")), (lp.x + k(10), ty), DIM, iw)
        ty += F["tiny"].get_linesize() + k(4)
        camps = (md.get("plan") or {}).get("taborok")
        if camps:  # a semleges táborok népei (szintenként egy nép; ami nincs megadva, az vadon)
            ppl = list(dict.fromkeys(list(camps.values()) + ["Vadon"]))
            ty = wrap(s, F["tiny"], tr("Táborok: ") + ", ".join(ppl), lp.x + k(10), ty, iw, (190, 215, 160),
                      max_lines=2) + k(2)
        lines = max(1, (lp.bottom - ty - k(6)) // F["tiny"].get_linesize())
        wrap(s, F["tiny"], pick(md.get("desc", "")), lp.x + k(10), ty, iw, TEXT, max_lines=lines)
        # ---- játékoshelyek
        panel(s, self.slots_rect)
        heads = [tr("#"), tr("Résztvevő"), tr("Faj"), tr("Csapat"), tr("Szín"), tr("Hely"), tr("Irány"), tr("Szövetség")]
        for (cx, cw), hd in zip(self.cols, heads):
            f, txt = fit_font(F, ["small", "tiny"], hd, cw)
            s.blit(f.render(txt, True, GOLD), (cx, self.slots_rect.y + k(10)))
        n = md["players"]
        x0 = self.cols[0][0]
        for i in range(8):
            y = self.row_y0 + i * self.row_h
            col = TEXT if i < n else (100, 98, 92)
            img = F["med"].render(str(i + 1), True, col)
            s.blit(img, (x0 + (self.cols[0][1] - img.get_width()) / 2, y + (k(27) - img.get_height()) / 2))
            if self.lobby is not None and i < n and self.slots[i]["kind"] == "remote":
                ok = self.lobby.slot_ready(i)
                cx = self.cols[1][0] - k(2)
                pygame.draw.circle(s, (90, 230, 110) if ok else (200, 80, 60), (cx, y + k(13)), k(5))
        # ---- játékbeállítások (saját panelen, hogy a háttér előtt is olvasható legyen)
        panel(s, self.opt_rect)
        for lab, x, y, w in self.opt_labels:
            label(s, F["small"], lab, (x, y), GOLD, w)
        # ---- LAN: csevegés
        if self.lobby is not None:
            cy = H - k(214)
            cw = W - x0 - k(300)
            panel(s, (x0 - k(10), cy, cw, k(126)))
            lh = F["small"].get_linesize()
            for kk, (who, msg) in enumerate(self.lobby.messages[-4:]):
                label(s, F["small"], f"{who}: {msg}", (x0, cy + k(6) + kk * lh), TEXT, cw - k(20), shadow=False)
            pygame.draw.rect(s, (16, 16, 20), (x0, cy + k(98), cw - k(20), k(24)))
            label(s, F["small"], "> " + self.chat_input, (x0 + k(4), cy + k(100)), GOLD, cw - k(28), shadow=False)
            label(s, F["tiny"], self.lobby.status_text(), (x0, H - k(84)), DIM, cw)
        toast = getattr(self.app, "toast", None)
        if toast:
            label(s, F["med"], toast, (W / 2, H - k(124)), (255, 150, 110), W - k(80), align="center")

    def overlay(self):
        mouse = pygame.mouse.get_pos()
        for d in self.dropdowns:
            d.draw(self.s, self.fonts, mouse)
        for d in self.dropdowns:
            if d.open:
                d.draw_list(self.s, self.fonts, mouse)


class LanMenu(Screen):
    BG = "lan"
    """Többjátékos (LAN): játék létrehozása, a helyi hálózaton futó játékok keresése, csatlakozás."""

    def __init__(self, app):
        super().__init__(app)
        from .menu import TextInput
        self.name_in = TextInput((0, 0, 260, 34), app.profile.name, 20)
        self.ip_in = TextInput((0, 0, 260, 34), app.profile.data.get("last_ip", ""), 40)
        self.focus = self.ip_in
        self.found = []
        self.msg = ""

    def layout(self):
        W, H = self.s.get_size()
        k = self.k
        cw = min(k(640), W - k(72))
        self.cx = cx = W / 2 - cw / 2
        top = self.top + k(20)
        half = (cw - k(20)) // 2
        self.name_in.rect = pygame.Rect(cx, top + k(30), half, k(40))
        self.ip_in.rect = pygame.Rect(cx, top + k(120), half, k(40))
        self.buttons = [Button((cx + half + k(20), top + k(30), half, k(40)), tr("Játék létrehozása"), self.host,
                               small=True),
                        Button((cx + half + k(20), top + k(120), half, k(40)), tr("Csatlakozás"), self.join_ip,
                               small=True),
                        Button((cx, top + k(190), cw, k(42)), tr("Játékok keresése a helyi hálózaton"), self.scan,
                               small=True),
                        self.back_button(tr("Vissza"))]
        n = max(1, min(8, (H - k(230) - top - k(292)) // k(46)))
        for kk, (addr, info) in enumerate(self.found[:n]):
            lab = f"{info.get('name', '?')} – {pick(info.get('map', ''))} ({info.get('taken', 1)}/" \
                  f"{info.get('players', '?')}) – {addr}"
            self.buttons.append(Button((cx, top + k(292) + kk * k(46), cw, k(40)), lab, (lambda a=addr: self.join(a)),
                                       small=True))
        self.cw, self.ltop = cw, top

    def handle(self, ev):
        if ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            for inp in (self.name_in, self.ip_in):
                if inp.rect.collidepoint(ev.pos):
                    self.focus = inp
        elif ev.type == pygame.KEYDOWN:
            self.focus.handle(ev)

    def _name(self):
        return self.name_in.text.strip() or self.app.profile.name

    def host(self):
        from .net import Lobby, content_fingerprint
        try:
            lobby = Lobby(True, self._name(), fingerprint=content_fingerprint(self.app.data))
        except OSError as e:
            self.msg = tr("Nem sikerült megnyitni a portot: {e}", e=e)
            return None
        sc = SetupScreen(self.app, "host", lobby)
        sc.slots[0]["name"] = self._name()
        for sl in sc.slots[1:]:
            if sl["kind"] != "closed":
                sl["kind"] = "open"
        sc.slots[1]["kind"] = "open"
        lobby.screen = sc
        r = sc.run()
        if r[0] != "start_lan":
            lobby.close()
            return None
        return r

    def join_ip(self):
        ip = self.ip_in.text.strip()
        if not ip:
            self.msg = tr("Add meg a gazda IP-címét.")
            return None
        self.app.profile.data["last_ip"] = ip
        self.app.profile.save()
        return self.join(ip)

    def join(self, addr):
        from .net import Lobby, content_fingerprint
        try:
            lobby = Lobby(False, self._name(), addr, fingerprint=content_fingerprint(self.app.data))
        except OSError as e:
            self.msg = tr("Nem sikerült csatlakozni: {e}", e=e)
            return None
        sc = SetupScreen(self.app, "client", lobby)
        r = sc.run()
        if r[0] != "start_lan":
            lobby.close()
            if lobby.error:
                self.msg = lobby.error
            return None
        return r

    def scan(self):
        from .net import discover
        self.found = discover()
        self.msg = tr("Talált játékok: {n}", n=len(self.found))

    def draw(self):
        s = self.s
        W, H = s.get_size()
        F, k = self.fonts, self.k
        self.title(tr("TÖBBJÁTÉKOS – LAN"))
        cx, cw, top = self.cx, self.cw, self.ltop
        panel(s, (cx - k(16), top - k(12), cw + k(32), k(290)), 200)
        label(s, F["med"], tr("A neved:"), (cx, top + k(2)), GOLD, cw)
        label(s, F["med"], tr("A gazda IP-címe:"), (cx, top + k(92)), GOLD, cw)
        if self.msg:
            label(s, F["small"], self.msg, (cx, top + k(246)), (255, 190, 120), cw)
        help_t = tr("A gazda a játékot létrehozva lesz az 1. játékos; a többiek a helyi hálózaton "
                    "megkeresik, vagy IP-címmel csatlakoznak (TCP 47810, UDP 47811).")
        hh = wrap(None, F["tiny"], help_t, 0, 0, cw, TEXT, max_lines=3)
        hy = H - k(100) - hh
        panel(s, (cx - k(16), hy - k(10), cw + k(32), hh + k(20)), 200)
        wrap(s, F["tiny"], help_t, cx, hy, cw, (214, 208, 192), max_lines=3)

    def overlay(self):
        self.name_in.draw(self.s, self.fonts, self.focus is self.name_in)
        self.ip_in.draw(self.s, self.fonts, self.focus is self.ip_in)
