"""Menük: főmenü, kampány, betöltés/mentés, profilok, beállítások, készítők, szünet.

Minden képernyő egy modális ciklus (`run`), ami egy műveletet ad vissza a main.py-nak. Közös elemek:
lassan úszó festett háttér hulló meteorokkal, lebegő-kiemelős gombok, szövegmező.

A főmenü 0 A.D.-szerű: bal oldali sáv a logóval és a menüpontokkal, a csoportoknak kinyíló almenüvel.
A menük mérete a képernyőhöz igazodik (`ui_scale`: 1280×720 = 1, 1920×1080 = 1,5); a szöveg, ami nem fér el,
kisebb betűre vált, végül „…”-ra rövidül.
"""
import json
import math
import os
import re
import random
import time

import pygame

from . import campaign as C
from . import profiles as PR
from . import savegame as SG
from .render import shade
from .sprites import SpriteBank
from .data import ROOT

GOLD = (255, 214, 120)
TEXT = (232, 228, 214)
DIM = (168, 164, 154)
FRAME = (150, 120, 70)
FRAME_DARK = (58, 44, 28)
SHADOW = (8, 6, 10)


# ---------------------------------------------------------------------------
# méretarány, betűk, szöveg
_FONTS = {}


def ui_scale(size):
    """A menük méretaránya: 1280×720-on 1, nagyobb képernyőn arányosan nagyobb."""
    W, H = size
    return max(0.8, min(3.0, W / 1280, H / 720))


def menu_fonts(u):
    key = round(u, 2)
    if key not in _FONTS:
        from .fonts import MENU, make_fonts
        _FONTS[key] = make_fonts(pygame, MENU, key)
    return _FONTS[key]


def fit_text(font, text, maxw):
    """A szöveg (lefordítva) a megadott szélességre: ha nem fér el, „…”-ra rövidítve."""
    from .i18n import T
    t = T(text)
    if maxw is None or font.size(t)[0] <= maxw:
        return t
    while len(t) > 1 and font.size(t + "…")[0] > maxw:
        t = t[:-1]
    return t.rstrip(" –-•,:(") + "…"


def fit_font(fonts, keys, text, maxw):
    """Az első betűméret, amelyben a szöveg elfér; ha egyikben sem, a legkisebb, rövidítve."""
    for k in keys:
        if fonts[k].size(text)[0] <= maxw:
            return fonts[k], text
    f = fonts[keys[-1]]
    return f, fit_text(f, text, maxw)


def label(s, font, text, pos, col, maxw=None, align="left", shadow=True):
    """Árnyékolt felirat (a festett háttér előtt is olvasható); visszaadja a szélességét."""
    t = fit_text(font, text, maxw)
    img = font.render(t, True, col)
    x, y = pos
    if align == "center":
        x -= img.get_width() / 2
    elif align == "right":
        x -= img.get_width()
    if shadow:
        d = max(1, font.get_height() // 14)
        s.blit(font.render(t, True, SHADOW), (x + d, y + d))
    s.blit(img, (x, y))
    return img.get_width()


def wrap(s, font, text, x, y, w, col, shadow=False, max_lines=None):
    """Tördelt szöveg; visszaadja az utolsó sor alatti y-t. s=None: csak méri."""
    from .i18n import T
    lines = []
    for para in T(text).split("\n"):
        line = ""
        for wd in para.split():
            t = (line + " " + wd).strip()
            if font.size(t)[0] > w and line:
                lines.append(line)
                line = wd
            else:
                line = t
        lines.append(line)
    if max_lines and len(lines) > max_lines:
        lines = lines[:max_lines]
        lines[-1] = fit_text(font, lines[-1] + " …", w) if font.size(lines[-1] + " …")[0] > w else lines[-1] + " …"
    for ln in lines:
        if s is not None and ln:
            if shadow:
                label(s, font, ln, (x, y), col, w)
            else:
                s.blit(font.render(fit_text(font, ln, w), True, col), (x, y))
        y += font.get_linesize()
    return y


def panel(s, rect, alpha=200):
    """Áttetsző sötét panel kettős kerettel."""
    r = pygame.Rect(rect)
    bg = pygame.Surface(r.size, pygame.SRCALPHA)
    bg.fill((10, 9, 14, alpha))
    s.blit(bg, r)
    pygame.draw.rect(s, FRAME_DARK, r.inflate(2, 2), 1)
    pygame.draw.rect(s, FRAME, r, 1)


def ornament(s, cx, y, w, u=1.0):
    """Díszvonal középen rombusszal (a 0 A.D. oldalsávjának elválasztói mintájára)."""
    half = w / 2
    for k in range(int(half)):
        a = 1 - k / half
        c = tuple(int(v * a + 20 * (1 - a)) for v in FRAME)
        s.set_at((int(cx - k), int(y)), c)
        s.set_at((int(cx + k), int(y)), c)
    d = max(3, int(5 * u))
    pygame.draw.polygon(s, GOLD, [(cx, y - d), (cx + d, y), (cx, y + d), (cx - d, y)])
    pygame.draw.polygon(s, FRAME_DARK, [(cx, y - d), (cx + d, y), (cx, y + d), (cx - d, y)], 1)


# ---------------------------------------------------------------------------
# háttér
def load_cover(rel, size):
    """assets/<rel> kép a megadott méretre „kitöltve” (arányosan nagyítva, középre vágva); None, ha nincs."""
    from rts.data import ROOT
    p = os.path.join(ROOT, "assets", rel)
    if not os.path.isfile(p):
        return None
    try:
        img = pygame.image.load(p).convert()
    except pygame.error:
        return None
    W, H = size
    sc = max(W / img.get_width(), H / img.get_height())
    img = pygame.transform.smoothscale(img, (int(img.get_width() * sc + 1), int(img.get_height() * sc + 1)))
    out = pygame.Surface((W, H))
    out.blit(img, ((W - img.get_width()) // 2, (H - img.get_height()) // 2))
    return out


def load_logo(max_w, max_h):
    """assets/ui/logo.png (átlátszó) a megadott keretbe kicsinyítve; None, ha nincs."""
    from rts.data import ROOT
    p = os.path.join(ROOT, "assets", "ui", "logo.png")
    if not os.path.isfile(p):
        return None
    try:
        img = pygame.image.load(p).convert_alpha()
    except pygame.error:
        return None
    sc = min(max_w / img.get_width(), max_h / img.get_height(), 1.0)
    return pygame.transform.smoothscale(img, (int(img.get_width() * sc), int(img.get_height() * sc)))


def intro(app, seconds=7.0):
    """Nyitókép (assets/ui/bg/intro.png): elhalványulással, billentyűre / kattintásra vagy idővel továbblép."""
    s = pygame.display.get_surface()
    from . import faction_ui as FUI
    FUI.reset_cursor()  # semleges kurzor
    from . import audio as AU
    AU.get(app.profile.settings).menu_music()   # indításkor a menüzene (menuzene.mp3)
    img = load_cover("ui/bg/intro.png", s.get_size())
    if img is None:
        return True
    clock = pygame.time.Clock()
    t0 = time.time()
    leave = None
    F = menu_fonts(ui_scale(s.get_size()))
    while True:
        for ev in pygame.event.get():
            if ev.type == pygame.QUIT:
                return False
            if ev.type in (pygame.KEYDOWN, pygame.MOUSEBUTTONDOWN) and leave is None and time.time() - t0 > 0.3:
                leave = time.time()
        t = time.time() - t0
        if leave is None and t > seconds:
            leave = time.time()
        a = min(1.0, t / 1.2)
        if leave is not None:
            a = min(a, 1 - (time.time() - leave) / 0.6)
            if a <= 0:
                return True
        s.fill((0, 0, 0))
        img.set_alpha(int(255 * a))
        s.blit(img, (0, 0))
        if leave is None and t > 1.5 and int(t * 1.6) % 2 == 0:
            label(s, F["med"], "Nyomj meg egy gombot", (s.get_width() / 2, s.get_height() - F["med"].get_height() * 3),
                  (230, 220, 200), align="center")
        pygame.display.flip()
        clock.tick(60)


class Loader:
    """Betöltőképernyő: a csata előtt véletlenszerűen az egyik tusrajzos térkép (assets/ui/bg/loading/*.png),
    forgó rúnakörrel, töltősávval és fajonkénti tippel."""

    def __init__(self, app, tip=None):
        s = pygame.display.get_surface()
        self.s = s
        W, H = s.get_size()
        ldir = os.path.join(ROOT, "assets", "ui", "bg", "loading")
        pics = sorted(f for f in os.listdir(ldir) if f.lower().endswith(".png")) if os.path.isdir(ldir) else []
        rng = random.Random()
        self.image = load_cover("ui/bg/loading/" + rng.choice(pics), (W, H)) if pics else None
        self.F = menu_fonts(ui_scale((W, H)))
        self.t0 = time.time()
        self.p = 0.0
        self.shown = 0.0
        self.tip = tip
        self.clock = pygame.time.Clock()

    def frame(self, target=None, text="Betöltés"):
        """Egy képkocka; a sáv simán közelít a célhoz."""
        for ev in pygame.event.get(pygame.QUIT):
            pygame.event.post(ev)  # a kilépést a játék kezeli
        pygame.event.pump()
        if target is not None:
            self.p = max(self.p, target)
        self.shown += (self.p - self.shown) * 0.12
        s, F = self.s, self.F
        W, H = s.get_size()
        t = time.time() - self.t0
        s.fill((14, 12, 10))
        if self.image is not None:
            s.blit(self.image, (0, 0))
        band = pygame.Surface((W, int(H * 0.26)), pygame.SRCALPHA)
        for y in range(band.get_height()):
            band.fill((8, 6, 4, int(215 * y / band.get_height())), (0, y, W, 1))
        s.blit(band, (0, H - band.get_height()))
        # forgó rúnakör (meteorkristály a közepén)
        cx, cy, r = int(W - H * 0.11), int(H - H * 0.11), int(H * 0.055)
        for i in range(12):
            a = t * 1.6 + i * math.pi / 6
            k = (math.sin(t * 3 - i * 0.5) + 1) / 2
            col = (int(150 + 80 * k), int(90 + 60 * k), 255)
            pygame.draw.circle(s, col, (int(cx + math.cos(a) * r), int(cy + math.sin(a) * r)), max(2, r // 9))
        pygame.draw.circle(s, (120, 70, 200), (cx, cy), int(r * 0.62), 2)
        q = r * 0.42 * (0.85 + 0.15 * math.sin(t * 4))
        pygame.draw.polygon(s, (190, 130, 255), [(cx, cy - q), (cx + q * 0.6, cy), (cx, cy + q), (cx - q * 0.6, cy)])
        # töltősáv
        bw, bh = int(W * 0.46), max(8, int(H * 0.014))
        bx, by = int(W / 2 - bw / 2), int(H - H * 0.075)
        pygame.draw.rect(s, (20, 16, 10), (bx - 3, by - 3, bw + 6, bh + 6), border_radius=6)
        pygame.draw.rect(s, (120, 96, 56), (bx - 3, by - 3, bw + 6, bh + 6), 2, border_radius=6)
        fw = int(bw * min(1.0, self.shown))
        if fw > 0:
            pygame.draw.rect(s, (226, 186, 96), (bx, by, fw, bh), border_radius=4)
            gx = bx + int((t * 0.6 % 1.0) * fw)  # csillanás a sávon
            pygame.draw.rect(s, (255, 240, 190), (max(bx, gx - 10), by, min(20, fw), bh), border_radius=4)
        dots = "." * (int(t * 2.5) % 4)
        label(s, F["med"], text + dots, (W / 2, by - F["med"].get_height() - 6), (240, 226, 190), align="center")
        if self.tip:
            label(s, F["small"], self.tip, (W / 2, by + bh + 8), (200, 190, 160), align="center")
        pygame.display.flip()
        self.clock.tick(60)

    def finish(self, text="Indul a csata"):
        """A sáv végigfut, majd rövid elhalványulás."""
        t1 = time.time()
        while time.time() - t1 < 0.6 or self.shown < 0.985 or time.time() - self.t0 < 1.8:  # legalább ~2 mp látszik
            self.frame(1.0, text)
            if time.time() - t1 > 3.0:
                break
        snap = self.s.copy()
        for k in range(12):
            self.s.blit(snap, (0, 0))
            dim = pygame.Surface(self.s.get_size(), pygame.SRCALPHA)
            dim.fill((0, 0, 0, int(255 * (k + 1) / 12)))
            self.s.blit(dim, (0, 0))
            pygame.display.flip()
            self.clock.tick(60)


class Background:
    """Festett menü-háttér (assets/ui/bg/menu.png), amely lassan úszik (0 A.D.-szerűen), hulló meteorokkal;
    ha nincs kép, rajzolt éjszakai táj menetelő sereggel."""
    PAN = 1.08  # a kép ennyivel nagyobb a képernyőnél, ezen belül úszik

    def __init__(self, data, size):
        self.data = data
        self.size = size
        W, H = size
        # a főmenü háttere: véletlenszerűen az egyik városkép / tájkép (assets/ui/bg/menu/*.png)
        mdir = os.path.join(ROOT, "assets", "ui", "bg", "menu")
        pics = sorted(f for f in os.listdir(mdir) if f.lower().endswith(".png")) if os.path.isdir(mdir) else []
        rel = "ui/bg/menu/" + random.Random().choice(pics) if pics else "ui/bg/menu.png"
        self.image = load_cover(rel, (int(W * self.PAN), int(H * self.PAN))) or \
            load_cover("ui/bg/menu.png", (int(W * self.PAN), int(H * self.PAN)))
        self.screens = {}  # képernyőnkénti háttér (assets/ui/bg/<kulcs>.png): setup, campaign, options, lan
        rng = random.Random(7)
        self.stars = [(rng.uniform(0, W), rng.uniform(0, H * 0.7), rng.uniform(0.3, 1.0)) for _ in range(220)]
        self.meteors = []
        self.flashes = []
        self.layers = []
        for k, (col, base, amp) in enumerate((((40, 30, 52), 0.62, 90), ((28, 22, 38), 0.72, 60), ((16, 14, 22), 0.82, 40))):
            pts = [(0, H)]
            x = 0
            y = H * base
            while x < W + 40:
                y = max(H * (base - 0.15), min(H * (base + 0.08), y + rng.uniform(-amp, amp) * 0.35))
                pts.append((x, y))
                x += rng.uniform(30, 80)
            pts += [(W + 40, H)]
            self.layers.append((col, pts, 4 + k * 6))
        self.craters = [(rng.uniform(0.1, 0.9) * W, H * rng.uniform(0.84, 0.95)) for _ in range(4)]
        self.parade = self._parade() if self.image is None else []
        self.t0 = time.time()

    def _parade(self):
        bank = SpriteBank()
        if not bank.ok:
            return []
        out = []
        colors = self.data.game["player_colors"]
        for i, f in enumerate(self.data.faction_list()):
            for uid in list(f["units"])[1:3]:
                sh = bank.unit(uid)
                if sh and "walk" in sh.anims and not self.data.defs[uid].get("flying"):
                    out.append((sh, colors[i % len(colors)]))
        return out

    def _meteors(self, s, W, horizon, rate):
        if random.random() < rate:
            self.meteors.append([random.uniform(-200, W), -20, random.uniform(5, 9), random.uniform(3.5, 6), 0])
        for m in self.meteors:
            m[0] += m[2]
            m[1] += m[3]
            for k in range(14):
                a = 1 - k / 14
                pygame.draw.circle(s, (int(90 + 160 * a), int(150 + 90 * a), 255),
                                   (int(m[0] - m[2] * k * 1.6), int(m[1] - m[3] * k * 1.6)), max(1, int(3 * a)))
        self.meteors = [m for m in self.meteors if m[1] < horizon]

    def screen_image(self, key, W, H):
        k = (key, W, H)
        if k not in self.screens:
            self.screens[k] = load_cover(f"ui/bg/{key}.png", (int(W * self.PAN), int(H * self.PAN)))
        return self.screens[k]

    def draw(self, s, key=None):
        W, H = s.get_size()
        t = time.time() - self.t0
        img = (self.screen_image(key, W, H) if key else None) or self.image
        if img is not None and img.get_size() == (int(W * self.PAN), int(H * self.PAN)):
            iw, ih = img.get_size()
            ph = t * 2 * math.pi / 140  # egy teljes úszás ~2⅓ perc
            ox = (iw - W) * (0.5 + 0.5 * math.sin(ph))
            oy = (ih - H) * (0.5 + 0.5 * math.sin(ph * 0.63 + 1.3))
            # alpixeles mozgás: az egész pixeles kép fölé a következő pixelre tolt kép kerül, a tört résszel
            # áttetszőn – így nincs pixelenkénti „ugrálás” a lassú úszásnál
            ix, iy = int(ox), int(oy)
            fx, fy = ox - ix, oy - iy
            s.blit(img, (-ix, -iy))
            f = (fx + fy) / 2
            if f > 0.02:
                img.set_alpha(int(255 * f))
                s.blit(img, (-ix - (1 if fx >= 0.5 * f else 0), -iy - (1 if fy >= 0.5 * f else 0)))
                img.set_alpha(None)
            if img is self.image:
                self._meteors(s, W, H * 0.55, 0.03)
            return
        for y in range(0, H, 6):  # égbolt-átmenet
            k = y / H
            pygame.draw.rect(s, (int(10 + 30 * k), int(10 + 14 * k), int(26 + 26 * k)), (0, y, W, 6))
        pygame.draw.circle(s, (60, 40, 70), (int(W * 0.78), int(H * 0.2)), 70)  # hold
        pygame.draw.circle(s, (86, 64, 96), (int(W * 0.78) - 10, int(H * 0.2) - 8), 62)
        for x, y, b in self.stars:
            tw = 0.6 + 0.4 * math.sin(t * 2 + x)
            c = int(120 + 120 * b * tw)
            s.set_at((int(x), int(y)), (c, c, min(255, c + 20)))
        n0 = len(self.meteors)
        self._meteors(s, W, H * 0.66, 0.08)
        if len(self.meteors) < n0:  # becsapódás a horizonton
            self.flashes.append([random.uniform(0, W), H * 0.66, 0])
        for col, pts, sp in self.layers:
            off = (t * sp) % 40
            pygame.draw.polygon(s, col, [(x - off, y) for x, y in pts])
        for f in self.flashes:
            f[2] += 1
            if f[2] < 25:
                pygame.draw.circle(s, (180, 230, 255), (int(f[0]), int(f[1])), f[2] * 3, 2)
        self.flashes = [f for f in self.flashes if f[2] < 25]
        pygame.draw.rect(s, (22, 28, 20), (0, int(H * 0.86), W, H))
        for cx, cy in self.craters:
            g = 0.6 + 0.4 * math.sin(t * 2 + cx)
            pygame.draw.ellipse(s, (50, 40, 36), (cx - 26, cy - 8, 52, 16))
            pygame.draw.ellipse(s, (int(60 * g), int(170 * g), int(220 * g)), (cx - 10, cy - 4, 20, 8))
        if self.parade:  # menetelő sereg a földön
            n = len(self.parade)
            for i, (sh, col) in enumerate(self.parade):
                x = (t * 40 + i * (W + 200) / n) % (W + 200) - 100
                fr = sh.anim_frame(col, "walk", t + i * 0.3, 0)
                img = pygame.transform.scale(fr, (fr.get_width() * 2, fr.get_height() * 2))
                s.blit(img, (x - sh.anchor[0] * 2, H * 0.93 - sh.anchor[1] * 2))


# ---------------------------------------------------------------------------
class Button:
    def __init__(self, rect, label, action, enabled=True, small=False, key=None, invisible=False):
        self.invisible = invisible
        self.rect = pygame.Rect(rect)
        self.label = label
        self.action = action
        self.enabled = enabled
        self.small = small
        self.key = key

    def draw(self, s, fonts, mouse, selected=False):
        if self.invisible:
            return
        hover = self.rect.collidepoint(mouse) and self.enabled
        r = self.rect
        base = (64, 52, 34) if self.enabled else (38, 36, 40)
        if hover or selected:
            base = (124, 92, 44)
        d = max(2, r.h // 16)
        pygame.draw.rect(s, shade(base, 0.6), r.move(d, d))
        pygame.draw.rect(s, base, r)
        pygame.draw.line(s, shade(base, 1.6), r.topleft, r.topright, 2)
        pygame.draw.line(s, shade(base, 1.6), r.topleft, r.bottomleft, 2)
        pygame.draw.rect(s, GOLD if (hover or selected) else FRAME, r, 1)
        keys = ["med", "small", "tiny"] if self.small else ["big", "med", "small"]
        keys = [key for key in keys if fonts[key].get_height() <= r.h - 2] or ["tiny"]
        f, txt = fit_font(fonts, keys, self.label, r.w - max(12, r.h // 2))
        if f.get_height() > r.h - 2:
            img = f.render(txt, True, (255, 244, 214) if self.enabled else (136, 132, 124))
            h = max(1, r.h - 2)
            img = pygame.transform.smoothscale(img, (max(1, round(img.get_width() * h / img.get_height())), h))
            s.blit(img, img.get_rect(center=r.center))
            return
        col = (255, 244, 214) if self.enabled else (136, 132, 124)
        label(s, f, txt, (r.centerx, r.centery - f.get_height() / 2), col, align="center", shadow=self.enabled)


class MenuItem(Button):
    """A főmenü oldalsávjának menüpontja (0 A.D.-szerű): keret nélküli felirat, egérre aranyló sáv;
    a ▸ jel almenüt nyit."""
    _bars = {}

    def __init__(self, rect, label, action, sub=False, hint="", key=None, font="big"):
        super().__init__(rect, label, action, key=key)
        self.sub = sub
        self.hint = hint
        self.font = font  # egy menün belül minden pont azonos betűméretű

    @staticmethod
    def text_w(w, h):
        return w - h * 0.9 - h * 0.5  # két oldali margó + a ▸ helye

    @classmethod
    def font_for(cls, fonts, labels, w, h):
        """A legnagyobb betűméret, amelyben a menü minden pontja elfér."""
        tw = cls.text_w(w, h)
        for key in ("big", "med", "small"):
            if all(fonts[key].size(lab)[0] <= tw for lab in labels):
                return key
        return "tiny"

    @classmethod
    def bar(cls, w, h):
        if (w, h) not in cls._bars:
            b = pygame.Surface((w, h), pygame.SRCALPHA)
            for x in range(w):
                a = (1 - abs(x / max(1, w - 1) * 2 - 1)) ** 0.6
                pygame.draw.line(b, (190, 140, 60, int(120 * a)), (x, 1), (x, h - 2))
                b.set_at((x, 0), (255, 214, 120, int(220 * a)))
                b.set_at((x, h - 1), (255, 214, 120, int(220 * a)))
            cls._bars[(w, h)] = b
        return cls._bars[(w, h)]

    def draw(self, s, fonts, mouse, selected=False):
        r = self.rect
        hover = r.collidepoint(mouse) and self.enabled
        if hover or selected:
            s.blit(self.bar(r.w, r.h), r)
        col = (255, 240, 200) if (hover or selected) else (TEXT if self.enabled else (120, 116, 108))
        f, txt = fonts[self.font], fit_text(fonts[self.font], self.label, self.text_w(r.w, r.h))
        label(s, f, txt, (r.centerx, r.centery - f.get_height() / 2), col, align="center")
        if self.sub:
            cx, cy, d = r.right - r.h * 0.45, r.centery, max(4, r.h // 7)
            pygame.draw.polygon(s, GOLD if (hover or selected) else FRAME, [(cx - d, cy - d), (cx + d * 0.6, cy),
                                                                           (cx - d, cy + d)])


class TextInput:
    def __init__(self, rect, text="", maxlen=24):
        self.rect = pygame.Rect(rect)
        self.text = text
        self.maxlen = maxlen

    def handle(self, ev):
        if ev.type == pygame.KEYDOWN:
            if ev.key == pygame.K_BACKSPACE:
                self.text = self.text[:-1]
            elif ev.unicode and ev.unicode.isprintable() and len(self.text) < self.maxlen:
                self.text += ev.unicode

    def draw(self, s, fonts, focused=True):
        pygame.draw.rect(s, (16, 16, 20), self.rect)
        pygame.draw.rect(s, GOLD if focused else FRAME, self.rect, 1)
        f = fonts["med"]
        cur = "_" if focused and int(time.time() * 2) % 2 else ""
        w = self.rect.w - 16
        t = self.text + cur
        while t and f.size(t)[0] > w:  # hosszú szövegnél a vége látszik
            t = t[1:]
        s.blit(f.render(t, True, TEXT), (self.rect.x + 8, self.rect.centery - f.get_height() / 2))


class Screen:
    """Közös modális ciklus: háttér + gombok + billentyűk."""

    def __init__(self, app):
        self.app = app
        self.buttons = []
        self.result = None

    @property
    def s(self):
        return pygame.display.get_surface()

    @property
    def u(self):
        return ui_scale(self.s.get_size())

    def k(self, v):
        """Méret 1280×720-as alapról a mostani képernyőre."""
        return int(round(v * self.u))

    @property
    def fonts(self):
        return menu_fonts(self.u)

    @property
    def top(self):
        """A tartalom teteje a cím alatt."""
        return self.k(96)

    def back_button(self, text="Vissza"):
        W, H = self.s.get_size()
        return Button((self.k(36), H - self.k(76), self.k(190), self.k(50)), text, lambda: ("back",))

    def main_button(self, text, fn, key=pygame.K_RETURN, enabled=True, width=240):
        W, H = self.s.get_size()
        return Button((W - self.k(36 + width), H - self.k(76), self.k(width), self.k(50)), text, fn, key=key,
                      enabled=enabled)

    def handle(self, ev):
        pass

    def is_selected(self, b):
        return False

    def overlay(self):
        """A gombok fölé rajzolt elemek (pl. szövegmező)."""

    def layout(self):
        pass

    def draw(self):
        pass

    def run(self):
        clock = pygame.time.Clock()
        self.result = None
        from . import faction_ui as FUI
        FUI.reset_cursor()  # a menükben a semleges kurzor
        from . import audio as AU
        A = AU.get(self.app.profile.settings)
        if not getattr(self.app, "in_game", False):  # játék közbeni menüben (szünet, mentés) a csata zenéje szól
            A.menu_music()
        while self.result is None:
            self.layout()
            for ev in pygame.event.get():
                if ev.type == pygame.QUIT:
                    return ("quit",)
                if ev.type == pygame.KEYDOWN and ev.key == pygame.K_ESCAPE and not getattr(self, "modal_input", False):
                    return ("back",)
                if ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
                    for b in list(self.buttons):
                        if b.enabled and b.rect.collidepoint(ev.pos):
                            r = b.action()
                            if r is not None:
                                self.result = r
                            break
                if ev.type == pygame.KEYDOWN:
                    for b in self.buttons:
                        if b.key is not None and ev.key == b.key and b.enabled:
                            r = b.action()
                            if r is not None:
                                self.result = r
                self.handle(ev)
            if self.result is not None:
                break
            self.app.bg.draw(self.s, getattr(self, "BG", None))
            self.draw()
            mouse = pygame.mouse.get_pos()
            for b in self.buttons:
                b.draw(self.s, self.fonts, mouse, self.is_selected(b))
            self.overlay()
            pygame.display.flip()
            clock.tick(60)
        return self.result

    def title(self, text, y=None):
        s = self.s
        F = self.fonts["huge"]
        y = self.k(14) if y is None else y
        from .i18n import T
        text = fit_text(F, T(text), s.get_width() - self.k(80))
        for d, col in ((self.k(4), (40, 20, 10)), (self.k(2), (120, 70, 30))):
            img = F.render(text, True, col)
            s.blit(img, (s.get_width() / 2 - img.get_width() / 2 + d, y + d))
        img = F.render(text, True, GOLD)
        s.blit(img, (s.get_width() / 2 - img.get_width() / 2, y))


# ---------------------------------------------------------------------------
# főmenü (0 A.D.-szerű oldalsáv)
MAIN_ITEMS = [
    ("Egyjátékos", [("Kampány", ("campaign",), "A történet küldetései, egymás után."),
                    ("Novellák", ("novellas",), "A novellák önálló pályái – bármikor, bármilyen sorrendben."),
                    ("Egyéni csata", ("skirmish",), "Csata gépi ellenfelek ellen, saját beállításokkal."),
                    ("Betöltés", ("load",), "Egy korábban mentett játék folytatása.")],
     "Kampány, egyéni csata és mentett játékok."),
    ("Többjátékos", ("lan",), "Játék a helyi hálózaton (LAN): létrehozás vagy csatlakozás."),
    ("Történetek", ("stories",), "A világ, a népek és a novellák – olvasnivaló képekkel."),
    ("Beállítások", [("Játékbeállítások", ("settings",), "Nyelv, görgetés, felbontás, kijelzés."),
                     ("Profilok", ("profile",), "Profil választása, létrehozása és statisztikái.")],
     "A játék és a profil beállításai."),
    ("Modok", ("mods",), "Modok engedélyezése, letiltása és betöltési sorrendje."),
    ("Készítők", ("credits",), "A játék készítői."),
    ("Kilépés", ("quit",), "Kilépés a játékból."),
]


class MainMenu(Screen):
    def __init__(self, app):
        super().__init__(app)
        self.open = None        # a kinyitott almenü (MAIN_ITEMS indexe)
        self.open_t = 0.0
        self.sub_rect = None
        self.tip = 0
        self.tip_t = time.time()
        self.tip_rect = None
        self._logo = (None, None)
        self._side = (None, None)

    @property
    def modal_input(self):  # nyitott almenünél az Esc csak az almenüt zárja be
        return self.open is not None

    def toggle(self, i):
        self.open = None if self.open == i else i
        self.open_t = time.time()

    def logo(self, w, h):
        if self._logo[0] != (w, h):
            self._logo = ((w, h), load_logo(w, h))
        return self._logo[1]

    def layout(self):
        W, H = self.s.get_size()
        k = self.k
        self.sw = sw = k(320)
        lg = self.logo(sw - k(36), int(H * 0.27))
        y = k(14) + (lg.get_height() if lg else k(90)) + k(48)
        ih, gap = k(46), k(6)
        self.buttons = []
        self.item_y = {}
        fk = MenuItem.font_for(self.fonts, [m[0] for m in MAIN_ITEMS], sw - k(28), ih)
        for i, (lab, act, hint) in enumerate(MAIN_ITEMS):
            sub = isinstance(act, list)
            fn = (lambda i=i: self.toggle(i)) if sub else (lambda a=act: a)
            self.buttons.append(MenuItem((k(14), y, sw - k(28), ih), lab, fn, sub=sub, hint=hint, font=fk))
            self.item_y[i] = y
            y += ih + gap
        self.sub_rect = None
        if self.open is not None:
            subs = MAIN_ITEMS[self.open][1]
            a = min(1.0, (time.time() - self.open_t) / 0.14)  # kicsúszik az oldalsáv mögül
            ease = 1 - (1 - a) ** 3
            pw = k(300)
            ph = len(subs) * (ih + gap) + k(18)
            px = sw + k(4) - int((1 - ease) * k(40))
            py = min(self.item_y[self.open] - k(9), H - ph - k(20))
            self.sub_rect = pygame.Rect(px, py, pw, ph)
            sk = MenuItem.font_for(self.fonts, [m[0] for m in subs], pw - k(24), ih)
            for j, (lab, act, hint) in enumerate(subs):
                self.buttons.append(MenuItem((px + k(12), py + k(9) + j * (ih + gap), pw - k(24), ih), lab,
                                             (lambda a=act: a), hint=hint, font=sk))

    def handle(self, ev):
        if ev.type == pygame.KEYDOWN and ev.key == pygame.K_ESCAPE and self.open is not None:
            self.open = None
        elif ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            if self.tip_rect and self.tip_rect.collidepoint(ev.pos):
                self.tip += 1
                self.tip_t = time.time()
            if getattr(self, "hint_rect", None) and self.hint_rect.collidepoint(ev.pos):
                from . import tips as TP
                self.hints = TP.pick(7)  # kattintásra új tippek
            if self.open is not None and not any(b.rect.collidepoint(ev.pos) for b in self.buttons) \
                    and not (self.sub_rect and self.sub_rect.collidepoint(ev.pos)):
                self.open = None

    def is_selected(self, b):
        return self.open is not None and b.label == MAIN_ITEMS[self.open][0]

    def sidebar(self, W, H):
        sw = self.sw
        if self._side[0] != (sw, H):
            side = pygame.Surface((sw, H), pygame.SRCALPHA)
            for y in range(H):  # függőleges átmenet: fent kicsit világosabb
                g = 1 - y / H
                pygame.draw.line(side, (int(16 + 14 * g), int(12 + 9 * g), int(18 + 10 * g), 222), (0, y), (sw, y))
            pygame.draw.line(side, FRAME_DARK, (sw - 5, 0), (sw - 5, H))
            pygame.draw.line(side, FRAME, (sw - 3, 0), (sw - 3, H), 2)
            pygame.draw.line(side, FRAME_DARK, (sw - 1, 0), (sw - 1, H))
            for y in range(int(H * 0.08), H, max(40, H // 9)):  # díszszegecsek a szegélyen
                d = self.k(4)
                cx = sw - 3
                pygame.draw.polygon(side, GOLD, [(cx, y - d), (cx + d, y), (cx, y + d), (cx - d, y)])
            self._side = ((sw, H), side)
        return self._side[1]

    def draw(self):
        s = self.s
        W, H = s.get_size()
        k, F = self.k, self.fonts
        sw = self.sw
        mouse = pygame.mouse.get_pos()
        s.blit(self.sidebar(W, H), (0, 0))
        lg = self.logo(sw - k(36), int(H * 0.27))
        ly = k(14)
        if lg is not None:
            s.blit(lg, (sw / 2 - lg.get_width() / 2, ly))
            ly += lg.get_height()
        else:
            label(s, F["big"], self.app.data.game.get("title", "TOTU: Sky Rift"), (sw / 2, ly + k(30)), GOLD,
                  sw - k(30), align="center")
            ly += k(90)
        f, sub = fit_font(F, ["small", "tiny"], self.app.data.game.get("subtitle", ""), sw - k(30))
        label(s, f, sub, (sw / 2, ly + k(4)), (210, 196, 160), align="center")
        ornament(s, sw / 2, ly + k(32), sw - k(60), self.u)
        # súgó a kijelölt menüponthoz (az oldalsáv alján, mint a 0 A.D.-ben)
        hov = next((b for b in self.buttons if isinstance(b, MenuItem) and b.rect.collidepoint(mouse)), None)
        foot = H - k(70)
        ornament(s, sw / 2, foot - k(70), sw - k(60), self.u)
        if hov is not None and hov.hint:
            wrap(s, F["tiny"], hov.hint, k(22), foot - k(58), sw - k(44), (214, 204, 180), shadow=True, max_lines=3)
        label(s, F["tiny"], "Kreátor: Mesehordo", (sw / 2, foot + k(10)), DIM, sw - k(30), align="center")
        mods = self.app.data.mods
        ver = "v0.3 • moddolható" + (" • " + ", ".join(m[0] for m in mods) if mods else "")
        label(s, F["tiny"], ver, (sw / 2, foot + k(34)), (130, 124, 112), sw - k(30), align="center")
        if self.sub_rect is not None:  # almenü-panel
            r = self.sub_rect
            panel(s, r, 228)
            pygame.draw.line(s, GOLD, (r.x + 1, r.y + 1), (r.right - 2, r.y + 1))
        self.draw_profile(W, H)
        self.tip_rect = None  # „A Hasadék népei” kártya kikerült; a helyén a tippek
        self.draw_hints(W, H)

    def draw_hints(self, W, H):
        """Tippek a jobb alsó sarokban: véletlenszerűen, legfeljebb 7; kattintásra újak."""
        from . import tips as TP
        s, k, F = self.s, self.k, self.fonts
        if not hasattr(self, "hints"):
            self.hints = TP.pick(7)
        w = min(k(460), W - self.sw - k(32))
        x = W - w - k(16)  # a jobb alsó sarokban
        if w < k(220):
            self.hint_rect = None
            return
        f = F["tiny"]
        lh = f.get_linesize()
        maxh = int(H * 0.42)
        rows = []
        for t in self.hints:  # tördelés; annyi tipp fér ki, amennyi (legfeljebb 7)
            words, cur, lines = t.split(), "", []
            for wd in words:
                c = (cur + " " + wd).strip()
                if f.size("• " + c)[0] > w - k(28) and cur:
                    lines.append(cur)
                    cur = wd
                else:
                    cur = c
            lines.append(cur)
            if k(44) + (sum(len(r) for r in rows) + len(lines)) * lh + len(rows) * k(4) > maxh:
                break
            rows.append(lines)
        h = k(44) + sum(len(r) for r in rows) * lh + max(0, len(rows) - 1) * k(4) + k(10)
        r = pygame.Rect(x, H - h - k(16), w, h)
        self.hint_rect = r
        panel(s, r, 200)
        label(s, F["med"], "Tippek", (r.x + k(14), r.y + k(10)), GOLD, w - k(28))
        y = r.y + k(40)
        for lines in rows:
            for i, ln in enumerate(lines):
                label(s, f, ("• " if i == 0 else "  ") + ln, (r.x + k(14), y), (214, 206, 186))
                y += lh
            y += k(4)

    def draw_profile(self, W, H):
        s, k, F = self.s, self.k, self.fonts
        p = self.app.profile
        st = p.data["stats"]
        r = pygame.Rect(W - k(396), k(16), k(380), k(96))
        panel(s, r, 210)
        f = self.app.data.factions.get(p.data.get("emblem"))
        es = k(76)
        em = self.app.bank.emblem(f, es) if f else None
        if em:
            s.blit(em, (r.x + k(8), r.y + (r.h - em.get_height()) // 2))
        tx = r.x + k(16) + es
        tw = r.right - tx - k(10)
        label(s, F["big"], p.name, (tx, r.y + k(8)), GOLD, tw)
        pt = int(st["play_time"] // 60)
        for txt, y, col in ((f"{st['games']} csata  •  {st['wins']} győzelem", k(44), TEXT),
                            (f"Játékidő: {pt // 60} ó {pt % 60} p", k(68), DIM)):
            f, txt = fit_font(F, ["small", "tiny"], txt, tw)
            label(s, f, txt, (tx, r.y + y), col)

    def draw_tip(self, W, H):
        """A Hasadék népei: váltakozó kártya a jobb alsó sarokban (kattintásra a következő)."""
        s, k, F = self.s, self.k, self.fonts
        facs = self.app.data.faction_list()
        if not facs or W - self.sw < k(520):
            self.tip_rect = None
            return
        if time.time() - self.tip_t > 11:
            self.tip += 1
            self.tip_t = time.time()
        f = facs[self.tip % len(facs)]
        w = min(k(500), W - self.sw - k(60))
        h = k(178)
        r = pygame.Rect(W - w - k(16), H - h - k(16), w, h)
        self.tip_rect = r
        panel(s, r, 215)
        label(s, F["med"], "A Hasadék népei", (r.x + k(14), r.y + k(10)), GOLD, w - k(28))
        es = k(96)
        em = self.app.bank.emblem(f, es)
        y0 = r.y + k(40)
        if em:
            s.blit(em, (r.x + k(12), y0))
        tx = r.x + k(22) + es
        tw = r.right - tx - k(14)
        label(s, F["big"], f["name"], (tx, y0), TEXT, tw)
        tf, tag = fit_font(F, ["small", "tiny"], f.get("tagline", ""), tw)
        label(s, tf, tag, (tx, y0 + k(32)), shade(f.get("accent", GOLD), 1.15))
        wrap(s, F["tiny"], f.get("desc", ""), tx, y0 + k(56), tw, (206, 200, 186), max_lines=3)
        n = len(facs)  # lapozó pöttyök
        for i in range(n):
            cx = r.right - k(14) - (n - 1 - i) * k(14)
            pygame.draw.circle(s, GOLD if i == self.tip % n else FRAME_DARK, (cx, r.y + k(20)), max(2, k(4)))


# ---------------------------------------------------------------------------
class CampaignMenu(Screen):
    BG = "campaign"   # színes hadjárati térkép
    def __init__(self, app):
        super().__init__(app)
        self.camps = C.list_campaigns(app.data)
        self.ci = 0
        self.mi = 0

    def layout(self):
        W, H = self.s.get_size()
        k = self.k
        self.buttons = []
        self.x1, self.lw = k(36), k(400)
        self.x2, self.w2 = self.x1 + self.lw + k(12), k(300)
        y = self.top
        prog0 = self.app.profile.progress()
        for i, (c, _) in enumerate(self.camps):
            avail = C.campaign_available(c, prog0, self.camps)
            mark = "• " if C.campaign_done(c, prog0) else ""
            self.buttons.append(Button((self.x1, y, self.lw, k(34)), mark + c["title"],
                                       (lambda i=i: (setattr(self, "ci", i), setattr(self, "mi", 0)) and None),
                                       small=True, enabled=avail))
            y += k(38)
        self.list_bottom = y
        if self.camps:
            camp = self.camps[self.ci][0]
            prog = self.app.profile.progress()
            ok = C.unlocked(camp, prog) if C.campaign_available(camp, prog, self.camps) else []
            y = self.top  # küldetések a második hasábban
            done = set(prog.get(camp["id"], {}).get("completed", []))
            step = min(k(46), (H - self.top - k(100)) // max(1, len(camp["missions_data"])))
            for i, md in enumerate(camp["missions_data"]):
                mark = "• " if md["id"] in done else ""
                self.buttons.append(Button((self.x2, y, self.w2, step - k(6)), mark + md["title"],
                                           (lambda i=i: setattr(self, "mi", i)), enabled=md["id"] in ok, small=True))
                y += step
            self.buttons.append(self.main_button("INDÍTÁS", self.start))
        self.buttons.append(self.back_button())

    def is_selected(self, b):
        if not self.camps:
            return False
        camp = self.camps[self.ci][0]
        lab = b.label.replace("• ", "")
        return (b.rect.x == self.x1 and lab == camp["title"]) or \
            (b.rect.x == self.x2 and lab == camp["missions_data"][self.mi]["title"])

    def start(self):
        camp = self.camps[self.ci][0]
        md = camp["missions_data"][self.mi]
        prog = self.app.profile.progress()
        if not C.campaign_available(camp, prog, self.camps) or md["id"] not in C.unlocked(camp, prog):
            return None
        return ("start_mission", camp, md)

    def draw(self):
        s = self.s
        W, H = s.get_size()
        k, F = self.k, self.fonts
        if not self.camps:
            self.title("KAMPÁNY")
            return
        camp = self.camps[self.ci][0]
        md = camp["missions_data"][self.mi]
        cover = self.app.bank.mission_cover(md["id"], s.get_size())
        if cover:
            s.blit(cover, (0, 0))
            shade_overlay = pygame.Surface(s.get_size(), pygame.SRCALPHA)
            shade_overlay.fill((0, 0, 0, 105))
            s.blit(shade_overlay, (0, 0))
        self.title("KAMPÁNY")
        # a hadjárat leírása a lista alatt, saját panelen
        desc = camp.get("desc", "")
        if desc:
            dy = self.list_bottom + k(8)
            avail = H - k(96) - dy
            lines = max(1, (avail - k(16)) // F["tiny"].get_linesize())
            hgt = wrap(None, F["tiny"], desc, 0, 0, self.lw - k(24), DIM, max_lines=lines)
            if avail > k(40):
                panel(s, (self.x1, dy, self.lw, min(avail, hgt + k(16))), 200)
                wrap(s, F["tiny"], desc, self.x1 + k(12), dy + k(8), self.lw - k(24), (214, 208, 192), max_lines=lines)
        bx = self.x2 + self.w2 + k(16)
        pr = pygame.Rect(bx, self.top, W - bx - k(36), H - self.top - k(100))
        panel(s, pr, 220)
        clip = s.get_clip()
        s.set_clip(pr)
        es = k(72)
        faction_art = self.app.bank.art("ui/factions/" + md["faction"])
        if faction_art:
            width, height = faction_art.get_size()
            scale = min(k(54) / width, k(71) / height)
            faction_art = self.app.bank.art("ui/factions/" + md["faction"],
                                           (max(1, round(width * scale)), max(1, round(height * scale))))
        if faction_art:
            s.blit(faction_art, (pr.right - k(66), pr.y + k(8)))
        else:
            emblem = self.app.bank.emblem(self.app.data.factions[md["faction"]], es)
            if emblem:
                s.blit(emblem, (pr.right - es - k(10), pr.y + k(8)))
        tw = pr.w - k(36) - es
        tf, title = fit_font(F, ["big", "med", "small"], md["title"], tw)
        label(s, tf, title, (pr.x + k(18), pr.y + k(14)), GOLD)
        fac = self.app.data.factions[md["faction"]]["name"]
        label(s, F["small"], "Faj: " + fac, (pr.x + k(18), pr.y + k(50)), DIM, tw)
        bw = pr.w - k(36)

        def body(f, draw):
            yy = pr.y + k(84)
            for para in md.get("briefing", []):
                yy = wrap(s if draw else None, f, para, pr.x + k(18), yy, bw, TEXT) + k(8)
            yy += k(6)
            if draw:
                label(s, F["med"], "Célok", (pr.x + k(18), yy), GOLD)
            yy += F["med"].get_linesize() + k(4)
            for o in md.get("objectives", []):
                if draw:
                    s.blit(f.render("•", True, (200, 230, 200)), (pr.x + k(22), yy))
                yy = wrap(s if draw else None, f, o["text"], pr.x + k(40), yy, bw - k(24), (200, 230, 200)) + k(2)
            return yy
        f = F["small"] if body(F["small"], False) <= pr.bottom - k(8) else F["tiny"]  # hosszú eligazításnál kisebb betű
        body(f, True)
        s.set_clip(clip)


class NovellaMenu(CampaignMenu):
    """Egyjátékos → Novellák: novellánként egy önálló pálya (novellak/*.json). Népenként csoportosítva, saját
    haladással; a kampánytól független, nem kell hozzá semmit teljesíteni."""
    BG = "campaign"

    def __init__(self, app):
        Screen.__init__(self, app)
        self.groups = C.list_novellas()
        self.ci = 0
        self.mi = 0
        self.gscroll = 0

    def layout(self):
        W, H = self.s.get_size()
        k = self.k
        self.buttons = []
        self.x1, self.lw = k(36), k(260)
        self.x2, self.w2 = self.x1 + self.lw + k(12), k(340)
        done = set(self.app.profile.novellas_done())
        rh = k(30)
        vis = self.visible_group_count()
        self.gscroll = max(0, min(self.gscroll, len(self.groups) - vis))
        for i in range(self.gscroll, min(len(self.groups), self.gscroll + vis)):
            people, ms = self.groups[i]
            n = sum(1 for md in ms if md["id"] in done)
            self.buttons.append(Button((self.x1, self.top + (i - self.gscroll) * rh, self.lw, rh - k(4)),
                                       f"{people}  {n}/{len(ms)}",
                                       (lambda i=i: (setattr(self, "ci", i), setattr(self, "mi", 0)) and None),
                                       small=True))
        if self.groups:
            ms = self.groups[self.ci][1]
            y = self.top
            for i, md in enumerate(ms):
                mark = "• " if md["id"] in done else ""
                self.buttons.append(Button((self.x2, y, self.w2, k(40)), mark + md["title"],
                                           (lambda i=i: setattr(self, "mi", i)), small=True))
                y += k(46)
            self.buttons.append(self.main_button("INDÍTÁS", self.start))
        self.buttons.append(self.back_button())

    def visible_group_count(self):
        return max(1, (self.s.get_height() - self.top - self.k(100)) // max(1, self.k(30)))

    def handle(self, ev):
        if ev.type == pygame.MOUSEWHEEL and pygame.mouse.get_pos()[0] < self.x1 + self.lw:
            self.gscroll -= ev.y
        elif ev.type == pygame.KEYDOWN and ev.key in (pygame.K_UP, pygame.K_DOWN) and self.groups:
            self.ci = (self.ci + (1 if ev.key == pygame.K_DOWN else -1)) % len(self.groups)
            self.mi = 0
            visible = self.visible_group_count()
            if self.ci < self.gscroll:
                self.gscroll = self.ci
            elif self.ci >= self.gscroll + visible:
                self.gscroll = self.ci - visible + 1

    def is_selected(self, b):
        if not self.groups:
            return False
        people, ms = self.groups[self.ci]
        lab = b.label.replace("• ", "")
        return (b.rect.x == self.x1 and lab.startswith(people + "  ")) or             (b.rect.x == self.x2 and lab == ms[self.mi]["title"])

    def start(self):
        return ("start_novella", self.groups[self.ci][1][self.mi])

    def draw(self):
        s = self.s
        W, H = s.get_size()
        k, F = self.k, self.fonts
        self.title("NOVELLÁK")
        if not self.groups:
            label(s, F["med"], "Nincsenek novella-pályák.", (W / 2, H / 2), DIM, align="center")
            return
        md = self.groups[self.ci][1][self.mi]
        bx = self.x2 + self.w2 + k(16)
        pr = pygame.Rect(bx, self.top, W - bx - k(36), H - self.top - k(100))
        panel(s, pr, 220)
        clip = s.get_clip()
        s.set_clip(pr)
        es = k(64)
        em = self.app.bank.emblem(self.app.data.factions.get(md["faction"]), es)             if md["faction"] in self.app.data.factions else None
        if em:
            s.blit(em, (pr.right - es - k(10), pr.y + k(8)))
        tw = pr.w - k(36) - es
        tf, title = fit_font(F, ["big", "med", "small"], md["title"], tw)
        label(s, tf, title, (pr.x + k(18), pr.y + k(14)), GOLD)
        label(s, F["small"], "Nép: " + md.get("people", ""), (pr.x + k(18), pr.y + k(50)), DIM, tw)
        bw = pr.w - k(36)

        def body(f, draw):
            yy = pr.y + k(84)
            for para in md.get("briefing", []):
                yy = wrap(s if draw else None, f, para, pr.x + k(18), yy, bw, TEXT) + k(8)
            yy += k(6)
            if draw:
                label(s, F["med"], "Célok", (pr.x + k(18), yy), GOLD)
            yy += F["med"].get_linesize() + k(4)
            for o in md.get("objectives", []):
                if o.get("hidden"):
                    continue
                txt = ("(mellékcél) " if o.get("optional") else "") + o["text"]
                if draw:
                    s.blit(f.render("•", True, (200, 230, 200)), (pr.x + k(22), yy))
                yy = wrap(s if draw else None, f, txt, pr.x + k(40), yy, bw - k(24), (200, 230, 200)) + k(2)
            return yy
        f = F["small"] if body(F["small"], False) <= pr.bottom - k(8) else F["tiny"]
        body(f, True)
        s.set_clip(clip)
        if md["id"] in set(self.app.profile.novellas_done()):
            label(s, F["small"], "Teljesítve", (pr.right - k(16), pr.bottom - k(30)), (150, 230, 150), align="right")


class LoadMenu(Screen):
    BG = "options"    # harcigép-vázlatok a tölgyfaasztalon
    """Mentések listája: bélyegkép, név, dátum, fajok; betöltés / törlés."""

    title_text = "BETÖLTÉS"

    def __init__(self, app, in_game=False):
        super().__init__(app)
        self.sel = 0
        self.in_game = in_game
        self.thumbs = {}
        self.save_scroll = 0

    def saves(self):
        return SG.list_saves(self.app.profile.saves_dir)

    def list_bottom(self):
        return self.s.get_height() - self.k(110)

    def row_rect(self, i):
        k = self.k
        return pygame.Rect(k(36), self.top + i * k(66), min(k(620), self.s.get_width() - k(72)), k(60))

    def rows(self):
        return max(1, min(8, (self.list_bottom() - self.top) // self.k(66)))

    def visible_saves(self, saves):
        self.save_scroll = max(0, min(self.save_scroll, len(saves) - self.rows()))
        return saves[self.save_scroll:self.save_scroll + self.rows()]

    def handle(self, ev):
        if ev.type == pygame.MOUSEWHEEL:
            self.save_scroll -= ev.y
            self.visible_saves(self.saves())

    def layout(self):
        self.buttons = []
        sv = self.saves()
        for i, m in enumerate(self.visible_saves(sv)):
            self.buttons.append(Button(self.row_rect(i), "", (lambda i=i + self.save_scroll: setattr(self, "sel", i)), small=True,
                                       invisible=True))
        has = bool(sv)
        self.buttons.append(self.main_button("BETÖLTÉS", self.load, enabled=has))
        W, H = self.s.get_size()
        self.buttons.append(Button((W - self.k(36 + 240 + 20 + 180), H - self.k(76), self.k(180), self.k(50)),
                                   "Törlés", self.delete, enabled=has, small=True, key=pygame.K_DELETE))
        self.buttons.append(self.back_button())

    def load(self):
        sv = self.saves()
        if sv:
            return ("load_path", sv[min(self.sel, len(sv) - 1)]["path"])

    def delete(self):
        sv = self.saves()
        if sv:
            SG.delete_save(sv[min(self.sel, len(sv) - 1)]["path"])
            self.sel = 0

    def draw(self):
        s = self.s
        W, H = s.get_size()
        k, F = self.k, self.fonts
        self.title(self.title_text)
        sv = self.saves()
        if not sv:
            r = self.row_rect(0)
            panel(s, r, 200)
            label(s, F["med"], "Még nincs mentés ebben a profilban.", (r.x + k(16), r.centery - F["med"].get_height() / 2),
                  TEXT, r.w - k(32))
        for i, m in enumerate(self.visible_saves(sv)):
            r = self.row_rect(i)
            th = self._thumb(m["path"])
            panel(s, r, 230)
            selected = i + self.save_scroll == self.sel
            pygame.draw.rect(s, GOLD if selected else (110, 90, 60), r, 2 if selected else 1)
            tw, thh = k(88), k(52)
            if th:
                s.blit(pygame.transform.smoothscale(th, (tw, thh)), (r.x + k(4), r.y + k(4)))
            tx = r.x + tw + k(14)
            label(s, F["med"], m["slot"], (tx, r.y + k(6)), GOLD, r.right - tx - k(8))
            gt = m.get("game_time", 0)
            info = f"{' – '.join(m.get('factions', []))}  •  {gt // 60}:{gt % 60:02d}"
            label(s, F["tiny"], info, (tx, r.y + k(34)), TEXT, r.right - tx - k(8))
        px = k(36) + min(k(620), W - k(72)) + k(20)
        if sv and W - px - k(36) >= k(240):
            m = sv[min(self.sel, len(sv) - 1)]
            th = self._thumb(m["path"])
            pw = min(k(400), W - px - k(36))
            ih = int((pw - k(20)) * 0.6)
            extra = [m["date"]] + ([m["mission"]] if m.get("mission") else [])
            lh = F["tiny"].get_linesize()
            ph = ih + k(20) + F["small"].get_linesize() + len(extra) * lh + k(8)
            panel(s, (px, self.top, pw, ph), 220)
            if th:
                s.blit(pygame.transform.smoothscale(th, (pw - k(20), ih)), (px + k(10), self.top + k(10)))
            y = self.top + ih + k(16)
            label(s, F["small"], m["slot"], (px + k(10), y), GOLD, pw - k(20))
            y += F["small"].get_linesize()
            for ln in extra:
                label(s, F["tiny"], ln, (px + k(10), y), TEXT, pw - k(20))
                y += lh

    def _thumb(self, base):
        if base not in self.thumbs:
            p = base + ".png"
            try:
                self.thumbs[base] = pygame.image.load(p).convert() if os.path.isfile(p) else None
            except (OSError, pygame.error):
                self.thumbs[base] = None
        return self.thumbs[base]


class SaveMenu(LoadMenu):
    """Játék közbeni mentés: új név vagy meglévő hely felülírása."""
    title_text = "MENTÉS"

    def __init__(self, app, default_name):
        super().__init__(app, in_game=True)
        self.input = TextInput((0, 0, 620, 40), default_name, 40)
        self.modal_input = False

    def list_bottom(self):
        return self.s.get_height() - self.k(200)

    def layout(self):
        W, H = self.s.get_size()
        super().layout()
        self.input.rect = pygame.Rect(self.k(36), H - self.k(150), min(self.k(620), W - self.k(72)), self.k(40))
        self.buttons = [b for b in self.buttons if b.label not in ("BETÖLTÉS",)]
        self.buttons.append(self.main_button("MENTÉS", self.save))

    def handle(self, ev):
        super().handle(ev)
        self.input.handle(ev)
        if ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            sv = self.saves()
            for i, m in enumerate(self.visible_saves(sv)):
                if self.row_rect(i).collidepoint(ev.pos):
                    self.input.text = m["slot"]

    def save(self):
        name = self.input.text.strip() or "Mentés"
        return ("save_as", name)

    def overlay(self):
        s = self.s
        label(s, self.fonts["med"], "Mentés neve:", (self.input.rect.x, self.input.rect.y - self.k(28)), TEXT)
        self.input.draw(s, self.fonts)


class ProfileMenu(Screen):
    BG = "lan"        # színes tusrajz-térkép
    def __init__(self, app):
        super().__init__(app)
        self.creating = False
        self.profile_scroll = 0
        self.input = TextInput((0, 0, 360, 40), "", 24)
        self.emblem = app.data.faction_list()[0]["id"]
        self.confirm = None

    @property
    def modal_input(self):
        return self.creating

    def layout(self):
        W, H = self.s.get_size()
        k = self.k
        self.buttons = []
        ps = PR.list_profiles()
        n = max(1, min(9, (H - k(160) - self.top) // k(52)))
        self.profile_scroll = max(0, min(self.profile_scroll, len(ps) - n))
        for i, p in enumerate(ps[self.profile_scroll:self.profile_scroll + n]):
            self.buttons.append(Button((k(36), self.top + i * k(52), k(380), k(46)), p.name,
                                       (lambda p=p: self.select(p)), small=True))
        self.px = k(450)
        if self.creating:
            px, py = self.px, self.top + k(10)
            self.input.rect = pygame.Rect(px + k(20), py + k(52), k(360), k(40))
            facs = self.app.data.faction_list()
            gy = py + k(150)
            for i, f in enumerate(facs):
                self.buttons.append(Button((px + k(20) + (i % 4) * k(140), gy + (i // 4) * k(46), k(134), k(40)),
                                           f["name"], (lambda f=f: setattr(self, "emblem", f["id"])), small=True))
            by = gy + ((len(facs) + 3) // 4) * k(46) + k(14)
            self.buttons.append(Button((px + k(20), by, k(200), k(46)), "Létrehozás", self.create, small=True,
                                       key=pygame.K_RETURN))
            self.buttons.append(Button((px + k(240), by, k(170), k(46)), "Mégse", lambda: setattr(self, "creating", False),
                                       small=True))
            self.create_rect = pygame.Rect(px, py, k(600), by + k(66) - py)
        else:
            self.buttons.append(Button((k(36), H - k(140), k(260), k(46)), "Új profil", self.new, small=True))
            lab = "Biztosan törlöd?" if self.confirm else "Profil törlése"
            self.buttons.append(Button((k(316), H - k(140), k(260), k(46)), lab, self.delete, small=True,
                                       enabled=len(ps) > 1))
        self.buttons.append(self.back_button())

    def handle(self, ev):
        if ev.type == pygame.MOUSEWHEEL and not self.creating:
            self.profile_scroll = max(0, self.profile_scroll - ev.y)
        if self.creating:
            if ev.type == pygame.KEYDOWN and ev.key == pygame.K_ESCAPE:
                self.creating = False
            else:
                self.input.handle(ev)

    def is_selected(self, b):
        if self.creating:
            return b.label == self.app.data.factions[self.emblem]["name"]
        return b.label == self.app.profile.name

    def overlay(self):
        if self.creating:
            self.input.draw(self.s, self.fonts)

    def select(self, p):
        self.app.set_profile(p)
        self.confirm = None

    def new(self):
        self.creating = True
        self.input.text = ""

    def create(self):
        name = self.input.text.strip()
        if not name or any(p.name == name for p in PR.list_profiles()):
            return None
        try:
            profile = PR.create(name, self.emblem)
        except OSError as e:
            self.app.toast = f"A profil nem hozható létre: {e}"
            return None
        self.app.set_profile(profile)
        self.creating = False

    def delete(self):
        if not self.confirm:
            self.confirm = True
            return None
        PR.delete(self.app.profile)
        self.app.set_profile(PR.ensure_profile())
        self.confirm = None

    def draw(self):
        s = self.s
        W, H = s.get_size()
        k, F = self.k, self.fonts
        self.title("PROFILOK")
        if self.creating:
            r = self.create_rect
            panel(s, r, 230)
            label(s, F["med"], "Profil neve:", (r.x + k(20), r.y + k(18)), TEXT)
            label(s, F["med"], "Címer (kedvenc faj):", (r.x + k(20), r.y + k(112)), TEXT)
            em = self.app.bank.emblem(self.app.data.factions[self.emblem], k(110))
            if em:
                s.blit(em, (r.right - k(130), r.y + k(14)))
            return
        p = self.app.profile
        st = p.data["stats"]
        pr = pygame.Rect(self.px, self.top, W - self.px - k(36), min(k(420), H - self.top - k(170)))
        panel(s, pr, 220)
        f = self.app.data.factions.get(p.data.get("emblem"))
        es = k(130)
        em = self.app.bank.emblem(f, es) if f else None
        if em:
            s.blit(em, (pr.right - es - k(14), pr.y + k(12)))
        tw = pr.w - es - k(50)
        label(s, F["big"], p.name, (pr.x + k(20), pr.y + k(14)), GOLD, tw)
        lines = [f"Létrehozva: {time.strftime('%Y-%m-%d', time.localtime(p.data.get('created', 0)))}",
                 f"Csaták: {st['games']}   Győzelmek: {st['wins']}   Vereségek: {st['losses']}",
                 f"Megölt ellenség: {st.get('kills', 0)}",
                 f"Játékidő: {int(st['play_time'] // 3600)} ó {int(st['play_time'] % 3600 // 60)} p", ""]
        for fid, v in st["by_faction"].items():
            if fid in self.app.data.factions:
                lines.append(f"{self.app.data.factions[fid]['name']}: {v['games']} csata, {v['wins']} győzelem")
        camp = p.progress()
        n = sum(len(c.get("completed", [])) for c in camp.values())
        lines.append(f"Teljesített küldetések: {n}")
        clip = s.get_clip()
        s.set_clip(pr)
        y = pr.y + k(58)
        lh = F["small"].get_linesize() + k(4)
        for i, ln in enumerate(lines):
            w = tw if y < pr.y + es + k(14) else pr.w - k(40)  # a címer mellett keskenyebb
            label(s, F["small"], ln, (pr.x + k(20), y), TEXT, w)
            y += lh
        s.set_clip(clip)


class SettingsMenu(Screen):
    BG = "options"
    sound_panel = False
    dragging_volume = None
    RES = ["1024x640", "1280x720", "1280x760", "1366x768", "1600x900", "1920x1080", "2560x1440"]

    def layout(self):
        W, H = self.s.get_size()
        k = self.k
        st = self.app.profile.settings
        self.buttons = []
        if self.sound_panel:
            self._layout_sound(W, H)
            return
        rows = [
            ("Nyelv", "Magyar" if st.get("language", "hu") == "hu" else "English",
             lambda: self._cycle("language", ["hu", "en"])),
            ("Kamera-távolság", {1.0: "Közel", 0.85: "Közepes", 0.75: "Távolabb", 0.65: "Messze", 0.55: "Nagyon messze"}
             .get(st.get("view_scale", 0.75), "Távolabb"), lambda: self._cycle("view_scale", [1.0, 0.85, 0.75, 0.65, 0.55])),
            ("Játéksebesség", f"x{st['game_speed']:g}", lambda: self._cycle("game_speed", [0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0])),
            ("Görgetés sebessége", f"{st['scroll_speed']}", lambda: self._cycle("scroll_speed", [500, 700, 900, 1200, 1600])),
            ("Képernyőszéli görgetés", "Be" if st["edge_scroll"] else "Ki", lambda: self._toggle("edge_scroll")),
            ("Játékidő (jobb fent)", "Be" if st.get("show_clock", True) else "Ki", lambda: self._toggle("show_clock")),
            ("Sebesség-gombok (jobb fent)", "Be" if st.get("show_speed", True) else "Ki",
             lambda: self._toggle("show_speed")),
            ("FPS-kijelzés", "Be" if st["show_fps"] else "Ki", lambda: self._toggle("show_fps")),
            ("Képkocka-korlát", {30: "30", 60: "60", 120: "120", 0: "Nincs"}.get(st.get("fps_limit", 60), "60"),
             lambda: self._cycle("fps_limit", [30, 60, 120, 0])),
            ("„F1: súgó” felirat", "Be" if st.get("show_help_hint", True) else "Ki",
             lambda: self._toggle("show_help_hint")),
            ("Hangerő és hangkategóriák", "Hangbeállítások", self._open_sound),
            ("Árnyékok", "Be" if st.get("shadows", True) else "Ki", lambda: self._toggle("shadows")),
            ("Hős-jelölés", {"talaj": "Talaj-jel", "korona": "Népjelvény", "talaj+korona": "Talaj-jel és népjelvény"}.get(
                st.get("hero_mark", "talaj+korona"), "Talaj-jel és népjelvény"),
             lambda: self._cycle("hero_mark", ["talaj+korona", "talaj", "korona"])),
            ("Nyugodt figurák (körvonal, simított felület)", "Be" if st.get("calm_sprites", True) else "Ki",
             lambda: self._toggle("calm_sprites")),
            ("Állapotok színe a figurán (pulzál)", "Be" if st.get("status_pulse", True) else "Ki",
             lambda: self._toggle("status_pulse")),
            ("Állapotok animációja a figurán", "Be" if st.get("status_fx", True) else "Ki",
             lambda: self._toggle("status_fx")),
            ("Környezeti részecskék", "Be" if st.get("ambient", True) else "Ki", lambda: self._toggle("ambient")),
            ("Mozgó víz", "Be" if st.get("water_anim", True) else "Ki", lambda: self._toggle("water_anim")),
            ("Nehézség (csata)", {"easy": "Könnyű", "normal": "Közepes", "hard": "Nehéz"}[st["difficulty"]],
             lambda: self._cycle("difficulty", ["easy", "normal", "hard"])),
            ("Felbontás", st["resolution"], lambda: self._cycle("resolution", self.RES)),
            ("Teljes képernyő", "Be" if st["fullscreen"] else "Ki", lambda: self._toggle("fullscreen")),
        ]
        self.rows = rows
        pw = min(k(720), W - k(72))
        self.step = min(k(54), (H - self.top - k(150)) // len(rows))
        bh = max(1, self.step - k(2))
        self.prect = pygame.Rect(W / 2 - pw / 2, self.top, pw, len(rows) * self.step + k(60))
        bw = k(270)
        for i, (lab, val, fn) in enumerate(rows):
            self.buttons.append(Button((self.prect.right - k(24) - bw, self.prect.y + k(16) + i * self.step, bw, bh),
                                       val, fn, small=True))
        self.buttons.append(self.back_button())

    def _open_sound(self):
        self.sound_panel = True
        self.layout()

    def _close_sound(self):
        self._finish_volume_drag()
        self.sound_panel = False
        self.layout()

    def _layout_sound(self, width, height):
        from . import audio as AU
        k = self.k
        settings = self.app.profile.settings
        rows = AU.VOLUME_ROWS
        if not AU.get(settings).environment_available():
            rows = rows[:-1]
        panel_width = min(k(770), width - k(72))
        step = min(k(61), (height - self.top - k(120)) // len(rows))
        self.prect = pygame.Rect((width - panel_width) // 2, self.top, panel_width, step * len(rows) + k(25))
        self.sound_rows = {}
        for i, (category, name) in enumerate(rows):
            y = self.prect.y + k(12) + i * step
            button = pygame.Rect(self.prect.right - k(105), y, k(80), step - k(8))
            left = self.prect.x + panel_width // 2
            slider = pygame.Rect(left, y, button.x - k(70) - left, button.height)
            enabled = settings.get(category + "_enabled", True)
            self.sound_rows[category] = (name, slider)
            self.buttons.append(Button(button, "Be" if enabled else "Ki",
                                       lambda cat=category: self._toggle_sound(cat), small=True))
        back = self.back_button()
        back.action = self._close_sound
        self.buttons.append(back)

    def _toggle_sound(self, category):
        from . import audio as AU
        settings = self.app.profile.settings
        key = category + "_enabled"
        settings[key] = not settings.get(key, True)
        AU.get(settings).apply_volume()
        self.app.profile.save()

    def _set_volume(self, category, x):
        from . import audio as AU
        rect = self.sound_rows[category][1]
        value = max(0, min(100, round(100 * (x - rect.left) / (rect.width - 1))))
        settings = self.app.profile.settings
        settings[category + "_volume"] = value
        AU.get(settings).apply_volume()

    def _finish_volume_drag(self):
        if self.dragging_volume is not None:
            self.dragging_volume = None
            self.app.profile.save()

    def handle(self, ev):
        if not self.sound_panel or not hasattr(self, "sound_rows"):
            return
        if ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            for category, (_, rect) in self.sound_rows.items():
                if rect.collidepoint(ev.pos):
                    self.dragging_volume = category
                    self._set_volume(category, ev.pos[0])
                    break
        elif ev.type == pygame.MOUSEMOTION and self.dragging_volume is not None:
            self._set_volume(self.dragging_volume, ev.pos[0])
        elif ev.type == pygame.MOUSEBUTTONUP and ev.button == 1:
            if self.dragging_volume is not None:
                self._set_volume(self.dragging_volume, ev.pos[0])
                self._finish_volume_drag()
        elif ev.type == pygame.KEYDOWN and ev.key == pygame.K_ESCAPE:
            self._close_sound()

    @property
    def modal_input(self):
        return self.sound_panel

    def run(self):
        try:
            return super().run()
        finally:
            self._finish_volume_drag()

    def _draw_sound(self):
        from . import audio as AU
        self.title("HANGBEÁLLÍTÁSOK")
        s, k, fonts = self.s, self.k, self.fonts
        panel(s, self.prect, 215)
        settings = self.app.profile.settings
        for category, (name, slider) in self.sound_rows.items():
            value = AU.volume_setting(settings, category)
            color = GOLD if settings.get(category + "_enabled", True) else DIM
            font, text = fit_font(fonts, ["med", "small", "tiny"], name, slider.x - self.prect.x - k(42))
            label(s, font, text, (self.prect.x + k(24), slider.centery - font.get_height() // 2), TEXT)
            track = pygame.Rect(slider.x, slider.centery - k(3), slider.width, max(2, k(6)))
            pygame.draw.rect(s, FRAME, track)
            thumb = track.x + round((track.width - 1) * value / 100)
            pygame.draw.line(s, color, track.midleft, (thumb, track.centery), max(2, k(4)))
            pygame.draw.circle(s, color, (thumb, track.centery), max(3, k(7)))
            font = fonts["small"]
            label(s, font, f"{value}%", (slider.right + k(12), slider.centery - font.get_height() // 2), color)

    def _cycle(self, key, opts):
        st = self.app.profile.settings
        cur = st.get(key, opts[0])
        i = opts.index(cur) if cur in opts else 0
        st[key] = opts[(i + 1) % len(opts)]
        self.app.profile.save()
        if key == "language":
            from .i18n import set_lang
            set_lang(st[key])
        if key in ("resolution", "fullscreen"):
            self.app.apply_display()

    def _toggle(self, key):
        st = self.app.profile.settings
        st[key] = not st.get(key, True)
        self.app.profile.save()
        if key == "fullscreen":
            self.app.apply_display()

    def draw(self):
        s = self.s
        k, F = self.k, self.fonts
        if self.sound_panel and hasattr(self, "sound_rows"):
            self._draw_sound()
            return
        self.title("BEÁLLÍTÁSOK")
        r = self.prect
        panel(s, r, 215)
        lw = r.w - k(24 + 270 + 44)
        bh = self.step - k(8)
        for i, (lab, _, _) in enumerate(self.rows):
            f, txt = fit_font(F, ["med", "small", "tiny"], lab, lw)
            y = r.y + k(16) + i * self.step + bh / 2 - f.get_height() / 2
            label(s, f, txt, (r.x + k(24), y), TEXT)
        label(s, F["tiny"], "A beállítások a profilhoz mentődnek.", (r.x + k(24), r.bottom - k(34)), DIM, r.w - k(48))


class ModsMenu(Screen):
    """Mod-választó (a 0 A.D. mod-ablakának mintájára): fent az elérhető, lent az engedélyezett modok
    betöltési sorrendben; oldalt a kijelölt mod adatai. Dupla kattintás / Engedélyezés–Letiltás mozgat a két
    lista között, a Fel/Le gomb a sorrendet állítja. „Mentés és alkalmazás”: a beállítás a saves/mods.json-ba
    kerül, és a játék azonnal újratölti az adatokat a modokkal."""
    BG = "options"
    ROW = 34

    def __init__(self, app):
        super().__init__(app)
        from . import modmgr as MM
        self.MM = MM
        self.all = {m["id"]: m for m in MM.list_mods()}
        self.enabled = list(MM.enabled_list())
        self.saved = list(self.enabled)
        self.sel = (("on", self.enabled[0]) if self.enabled else
                    (("off", next(iter(self.all))) if self.all else None))
        self.filter = TextInput((0, 0, 10, 10), "", 30)
        self.focus_filter = False
        self.scroll = {"off": 0, "on": 0}
        self.msg = ""
        self.msg_col = TEXT
        self._last_click = (None, 0.0)

    # ---- adatok ----
    def disabled(self):
        return [i for i in self.all if i not in self.enabled]

    def visible(self, which):
        ids = self.enabled if which == "on" else self.disabled()
        q = self.filter.text.strip().lower()
        if not q:
            return ids
        return [i for i in ids if q in (self.all[i]["name"] + " " + self.all[i]["label"] + " " + i + " " +
                                        self.all[i]["description"]).lower()]

    def toggle(self, mid=None):
        mid = mid or (self.sel[1] if self.sel else None)
        if mid is None:
            return
        if mid in self.enabled:
            self.enabled.remove(mid)
            self.sel = ("off", mid)
        elif not self.all[mid]["error"]:
            self.enabled.append(mid)
            self.sel = ("on", mid)
        self.msg = ""

    def move(self, d):
        if not self.sel or self.sel[1] not in self.enabled:
            return
        i = self.enabled.index(self.sel[1])
        j = max(0, min(len(self.enabled) - 1, i + d))
        self.enabled[i], self.enabled[j] = self.enabled[j], self.enabled[i]

    def reset(self):
        self.enabled = []
        self.msg = ""

    def auto(self):
        self.enabled = self.MM.auto_order(self.enabled, self.all)

    def apply(self):
        if self.MM.problems(self.enabled, self.all):
            return None
        from . import data as gamedata
        try:
            data = gamedata.load(enabled_mods=list(self.enabled))
        except Exception as e:  # hibás mod: marad a régi állapot
            self.msg, self.msg_col = f"A modok betöltése nem sikerült: {e}", (255, 130, 110)
            return None
        self.MM.save_enabled(self.enabled)
        self.app.data = data
        self.app.bg = Background(data, self.s.get_size())
        self.saved = list(self.enabled)
        return ("back",)

    # ---- elrendezés ----
    def geom(self):
        W, H = self.s.get_size()
        k = self.k
        lw = min(k(700), int(W * 0.58))
        top = self.top + k(46)
        bottom = H - k(150)
        half = (bottom - top - k(40)) // 2
        off = pygame.Rect(k(36), top + k(26), lw, half - k(26))
        on = pygame.Rect(k(36), off.bottom + k(40), lw, bottom - off.bottom - k(40))
        side = pygame.Rect(off.right + k(20), self.top + k(10), W - off.right - k(56), bottom - self.top - k(10))
        return off, on, side

    def row_rect(self, box, i):
        k = self.k
        return pygame.Rect(box.x + k(4), box.y + k(30) + i * k(self.ROW), box.w - k(8), k(self.ROW) - k(2))

    def rows_fit(self, box):
        return max(1, (box.h - self.k(34)) // self.k(self.ROW))

    def layout(self):
        k = self.k
        W, H = self.s.get_size()
        off, on, side = self.geom()
        self.filter.rect = pygame.Rect(k(36) + k(90), self.top + k(4), min(k(320), off.w - k(90)), k(34))
        self.buttons = []
        for which, box in (("off", off), ("on", on)):
            ids = self.visible(which)
            n = self.rows_fit(box)
            self.scroll[which] = max(0, min(self.scroll[which], max(0, len(ids) - n)))
            for r, mid in enumerate(ids[self.scroll[which]:self.scroll[which] + n]):
                self.buttons.append(Button(self.row_rect(box, r), "", (lambda w=which, m=mid: self.click(w, m)),
                                           invisible=True))
        sel_on = bool(self.sel and self.sel[1] in self.enabled)
        bh, y = k(42), H - k(136)
        x = k(36)
        self.buttons.append(Button((x, y, k(190), bh), "Letiltás" if sel_on else "Engedélyezés", self.toggle,
                                   enabled=bool(self.sel), small=True))
        x += k(200)
        self.buttons.append(Button((x, y, k(90), bh), "Fel", lambda: self.move(-1), enabled=sel_on, small=True))
        x += k(100)
        self.buttons.append(Button((x, y, k(90), bh), "Le", lambda: self.move(1), enabled=sel_on, small=True))
        x += k(100)
        self.buttons.append(Button((x, y, k(170), bh), "Rendezés", self.auto,
                                   enabled=len(self.enabled) > 1, small=True))
        x += k(180)
        self.buttons.append(Button((x, y, k(150), bh), "Mind ki", self.reset, enabled=bool(self.enabled),
                                   small=True))
        probs = self.MM.problems(self.enabled, self.all)
        changed = self.enabled != self.saved
        self.buttons.append(self.main_button("Mentés és alkalmazás", self.apply, enabled=not probs and changed,
                                             width=300))
        self.buttons.append(self.back_button())

    def click(self, which, mid):
        now = time.time()
        if self._last_click[0] == mid and now - self._last_click[1] < 0.35:  # dupla kattintás: át a másik listába
            self.toggle(mid)
            self._last_click = (None, 0.0)
            return
        self._last_click = (mid, now)
        self.sel = (which, mid)
        self.focus_filter = False

    @property
    def modal_input(self):
        return self.focus_filter

    def handle(self, ev):
        if ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            self.focus_filter = self.filter.rect.collidepoint(ev.pos)
        elif ev.type == pygame.MOUSEWHEEL:
            off, on, _ = self.geom()
            pos = pygame.mouse.get_pos()
            for which, box in (("off", off), ("on", on)):
                if box.collidepoint(pos):
                    self.scroll[which] = max(0, self.scroll[which] - ev.y)
        elif ev.type == pygame.KEYDOWN:
            if self.focus_filter:
                if ev.key in (pygame.K_ESCAPE, pygame.K_RETURN):
                    self.focus_filter = False
                else:
                    self.filter.handle(ev)
            elif ev.key == pygame.K_SPACE:
                self.toggle()
            elif ev.key in (pygame.K_UP, pygame.K_DOWN) and self.sel:
                which = self.sel[0]
                ids = self.visible(which)
                if self.sel[1] in ids:
                    i = ids.index(self.sel[1]) + (-1 if ev.key == pygame.K_UP else 1)
                    if 0 <= i < len(ids):
                        self.sel = (which, ids[i])

    # ---- rajz ----
    def draw_list(self, box, which, title):
        s, k, F = self.s, self.k, self.fonts
        panel(s, box, 215)
        label(s, F["small"], title, (box.x + k(10), box.y - k(26)), GOLD, box.w)
        cols = [(k(10), "Név"), (int(box.w * 0.46), "Verzió"), (int(box.w * 0.60), "Címke"),
                (int(box.w * 0.78), "Függőség")]
        for cx, t in cols:
            label(s, F["tiny"], t, (box.x + cx, box.y + k(6)), DIM, k(140))
        pygame.draw.line(s, FRAME_DARK, (box.x + k(4), box.y + k(27)), (box.right - k(4), box.y + k(27)))
        ids = self.visible(which)
        n = self.rows_fit(box)
        mouse = pygame.mouse.get_pos()
        for r, mid in enumerate(ids[self.scroll[which]:self.scroll[which] + n]):
            m = self.all[mid]
            rr = self.row_rect(box, r)
            selected = bool(self.sel and self.sel[1] == mid)
            if selected:
                s.blit(MenuItem.bar(rr.w, rr.h), rr)
            elif rr.collidepoint(mouse):
                pygame.draw.rect(s, FRAME_DARK, rr, 1)
            col = (255, 130, 110) if m["error"] else ((255, 240, 200) if selected else TEXT)
            name = m["name"]
            if which == "on":
                name = f"{self.enabled.index(mid) + 1}. {name}"
            y = rr.centery - F["small"].get_height() / 2
            label(s, F["small"], name, (rr.x + k(6), y), col, cols[1][0] - k(16))
            label(s, F["small"], m["version"] or "–", (box.x + cols[1][0], y), col, cols[2][0] - cols[1][0] - k(8))
            label(s, F["small"], m["label"] or "–", (box.x + cols[2][0], y), col, cols[3][0] - cols[2][0] - k(8))
            deps = ", ".join(m["dependencies"]) or "–"
            label(s, F["small"], deps, (box.x + cols[3][0], y), col, box.w - cols[3][0] - k(10))
        if not ids:
            label(s, F["small"], "Nincs ilyen mod." if self.filter.text else
                  ("Nincs engedélyezett mod – az alapjáték fut." if which == "on" else "Minden mod engedélyezve."),
                  (box.x + k(12), box.y + k(40)), DIM, box.w - k(24))
        if len(ids) > n:
            label(s, F["tiny"], f"{self.scroll[which] + 1}–{min(len(ids), self.scroll[which] + n)} / {len(ids)}",
                  (box.right - k(8), box.y - k(22)), DIM, align="right")

    def draw(self):
        s, k, F = self.s, self.k, self.fonts
        W, H = s.get_size()
        self.title("MODOK")
        off, on, side = self.geom()
        label(s, F["small"], "Szűrés:", (k(36), self.filter.rect.centery - F["small"].get_height() / 2), TEXT)
        self.filter.draw(s, F, self.focus_filter)
        self.draw_list(off, "off", "Elérhető modok")
        self.draw_list(on, "on", "Engedélyezett modok – betöltési sorrend")
        panel(s, side, 215)
        x, y, w = side.x + k(14), side.y + k(12), side.w - k(28)
        if self.sel and self.sel[1] in self.all:
            m = self.all[self.sel[1]]
            label(s, F["big"], m["name"], (x, y), GOLD, w)
            y += F["big"].get_linesize()
            sub = " • ".join(v for v in (m["label"], m["version"] and "v" + m["version"], m["author"]) if v)
            if sub:
                label(s, F["small"], sub, (x, y), DIM, w)
                y += F["small"].get_linesize()
            y += k(6)
            on_ = m["id"] in self.enabled
            label(s, F["small"], "Állapot: " + ("engedélyezve" if on_ else "letiltva"), (x, y),
                  (150, 230, 140) if on_ else DIM, w)
            y += F["small"].get_linesize()
            label(s, F["tiny"], f"Mappa: mods/{m['id']}", (x, y), DIM, w)
            y += F["tiny"].get_linesize()
            label(s, F["tiny"], f"Tartalom: {m['contents']}", (x, y), DIM, w)
            y += F["tiny"].get_linesize() + k(4)
            if m["dependencies"]:
                label(s, F["small"], "Függőségek:", (x, y), TEXT, w)
                y += F["small"].get_linesize()
                for dep in m["dependencies"]:
                    ok = dep in self.enabled
                    have = dep in self.all
                    nm = self.all[dep]["name"] if have else dep
                    label(s, F["tiny"], nm + (" – rendben" if ok else (" – nincs telepítve" if not have else " – nincs engedélyezve")),
                          (x + k(12), y), (150, 230, 140) if ok else (255, 150, 120), w - k(12))
                    y += F["tiny"].get_linesize()
            if m["has_script"]:
                y = wrap(s, F["tiny"], "Figyelem: Python-szkriptet futtat – csak megbízható forrásból telepítsd.",
                         x, y, w, (255, 200, 120))
            y += k(8)
            if m["error"]:
                y = wrap(s, F["small"], m["error"], x, y, w, (255, 130, 110))
            wrap(s, F["small"], m["description"] or "Nincs leírás.", x, y, w, TEXT, max_lines=30)
        else:
            wrap(s, F["small"], "Nincs telepített mod. A modok a játék mods mappájába kerülnek "
                 "(mods/<név>/mod.json); leírás: docs/MODDING.md.", x, y, w, DIM)
        probs = self.MM.problems(self.enabled, self.all)
        yb = H - k(76)
        mw = W - k(250) - k(370)
        if probs:
            label(s, F["small"], "Hiba: " + probs[0] + (f"  (+{len(probs) - 1} további)" if len(probs) > 1 else ""),
                  (k(250), yb + k(14)), (255, 140, 110), mw)
        elif self.msg:
            label(s, F["small"], self.msg, (k(250), yb + k(14)), self.msg_col, mw)
        elif self.enabled != self.saved:
            label(s, F["small"], "Változás van: mentéskor a játék újratölti az adatokat.", (k(250), yb + k(14)),
                  DIM, mw)


class StoriesMenu(Screen):
    """Történetek: a világ, a népek és a novellák olvasója (stories/konyvtar.json). Bal oldalt a kötetek és fejezetek, jobbra az olvasó lap görgethető szöveggel és a fejezetek
    színesítő képeivel (assets/stories/<id>.png; amíg nincs meg, keretezett hely a képaláírással)."""
    BG = "options"
    LIB = os.path.join(ROOT, "stories", "konyvtar.json")

    def __init__(self, app):
        super().__init__(app)
        try:
            with open(self.LIB, encoding="utf-8") as f:
                self.lib = json.load(f)
        except (OSError, ValueError):
            self.lib = {"books": []}
        self.books = self.lib.get("books", [])
        self.chap = {c["id"]: (b, c) for b in self.books for c in b["chapters"]}
        self.order = [c["id"] for b in self.books for c in b["chapters"]]
        last = self.app.profile.settings.get("story_last")
        self.cur = last if last in self.chap else (self.order[0] if self.order else None)
        self.open = {self.chap[self.cur][0]["id"]} if self.cur else set()
        self.scroll = 0.0
        self.tree_scroll = 0
        self._lay = None        # (kulcs, sorok, teljes magasság)
        self._imgs = {}
        self._drag = None
        self.novellas = {md["id"]: md for _, ms in C.list_novellas() for md in ms}

    # ---- betűk ----
    def body_fonts(self):
        key = round(self.u, 2)
        c = getattr(self, "_bf", None)
        if c and c[0] == key:
            return c[1]
        from .fonts import load_font

        def mk(size, display=False, italic=False):
            font = load_font(pygame, max(10, int(size * self.u)), display=display)
            font.set_italic(italic)
            return font

        f = {"p": mk(21), "pi": mk(21, italic=True), "h1": mk(34, display=True),
             "h2": mk(26, display=True), "h3": mk(22, display=True),
             "cap": mk(15, italic=True), "small": mk(16)}
        self._bf = (key, f)
        return f

    # ---- elrendezés ----
    def geom(self):
        W, H = self.s.get_size()
        k = self.k
        tw = min(k(340), int(W * 0.3))
        tree = pygame.Rect(k(24), self.top - k(20), tw, H - self.top - k(90))
        page = pygame.Rect(tree.right + k(16), self.top - k(20), W - tree.right - k(40), H - self.top - k(90))
        return tree, page

    def tree_rows(self):
        rows = []
        for b in self.books:
            rows.append(("book", b))
            if b["id"] in self.open:
                grp = None
                for c in b["chapters"]:
                    if b["id"] == "novellak" and c.get("sub") != grp:  # a novellák népenként csoportosítva
                        grp = c.get("sub")
                        rows.append(("group", {"title": grp}))
                    rows.append(("chap", c))
        return rows

    def layout(self):
        k = self.k
        W, H = self.s.get_size()
        tree, page = self.geom()
        self.buttons = []
        rh = k(30)
        rows = self.tree_rows()
        vis = max(1, (tree.h - k(12)) // rh)
        self.tree_scroll = max(0, min(self.tree_scroll, len(rows) - vis))
        for i, (kind, it) in enumerate(rows[self.tree_scroll:self.tree_scroll + vis]):
            r = pygame.Rect(tree.x + k(6), tree.y + k(6) + i * rh, tree.w - k(12), rh - k(2))
            if kind == "group":
                continue
            fn = (lambda b=it: self.toggle(b["id"])) if kind == "book" else (lambda c=it: self.go(c["id"]))
            self.buttons.append(Button(r, "", fn, invisible=True))
        idx = self.order.index(self.cur) if self.cur in self.order else -1
        bw = k(230)
        self.buttons.append(Button((page.x, H - k(76), bw, k(50)), "Előző fejezet",
                                   lambda: self.go(self.order[idx - 1]), enabled=idx > 0, small=True,
                                   key=pygame.K_LEFT))
        self.buttons.append(Button((page.right - bw, H - k(76), bw, k(50)), "Következő fejezet",
                                   lambda: self.go(self.order[idx + 1]), enabled=0 <= idx < len(self.order) - 1,
                                   small=True, key=pygame.K_RIGHT))
        if self.cur in self.novellas:  # a novellához tartozó önálló pálya
            self.buttons.append(Button((page.right - k(230), k(22), k(230), k(46)), "Játszd le",
                                       lambda: ("start_novella", self.novellas[self.cur]), small=True))
        self.buttons.append(self.back_button())

    def toggle(self, bid):
        if bid in self.open:
            self.open.discard(bid)
        else:
            self.open.add(bid)

    def go(self, cid):
        self.cur = cid
        self.scroll = 0.0
        self._lay = None
        self.open.add(self.chap[cid][0]["id"])
        st = self.app.profile.settings
        st["story_last"] = cid
        try:
            self.app.profile.save()
        except Exception:
            pass

    # ---- szöveg tördelése kiemelésekkel ----
    @staticmethod
    def spans(text):
        """**kiemelt** és *dőlt* részek: [(szó, stílus, szóköz előtte)] – a stílus: 0 sima, 1 kiemelt, 2 dőlt.
        Egy szó több stílusú darabból is állhat (pl. „**onik**,”): a darabok szóköz nélkül tapadnak."""
        out = []
        style = 0
        space = True
        for part in re.split(r"(\*\*|\*)", text):
            if part == "**":
                style = 0 if style == 1 else 1
                continue
            if part == "*":
                style = 0 if style == 2 else 2
                continue
            for i, w in enumerate(part.split(" ")):
                if i > 0:
                    space = True
                if w:
                    out.append((w, style, space and bool(out)))
                    space = False
        return out

    def wrap_spans(self, font, text, width):
        """Sorokra tördelés; egy sor: [(darab, stílus, szélesség, x)]."""
        F = self.body_fonts()
        lines, line, x = [], [], 0
        sp = font.size(" ")[0]
        word = []   # az összetapadó darabok egy szóként törnek

        def place(wd):
            nonlocal line, x
            ww = sum(w for _, _, w, _ in wd)
            gap = sp if line else 0
            if line and x + gap + ww > width:
                lines.append(line)
                line, x, gap = [], 0, 0
            x += gap
            for t, st, w, _ in wd:
                line.append((t, st, w, x))
                x += w
        for t, st, before in self.spans(text):
            f = F["pi"] if (st == 2 and font is F["p"]) else font
            w = f.size(t)[0]
            if before and word:
                place(word)
                word = []
            word.append((t, st, w, 0))
        if word:
            place(word)
        if line:
            lines.append(line)
        return lines

    def build(self, width):
        """A fejezet sorokra bontva: [(y, fajta, adat)] és a teljes magasság (a szélesség változásáig tárolva)."""
        key = (self.cur, width, round(self.u, 2))
        if self._lay and self._lay[0] == key:
            return self._lay[1], self._lay[2]
        F = self.body_fonts()
        k = self.k
        b, c = self.chap[self.cur]
        rows = []
        y = 0

        def text(font, txt, x0, w, kind, col_main):
            nonlocal y
            for ln in self.wrap_spans(font, txt, w):
                rows.append((y, kind, (font, ln, x0, col_main)))
                y += font.get_linesize()
        rows.append((y, "title", (F["h1"], c["title"])))
        y += F["h1"].get_linesize()
        if c.get("sub"):
            rows.append((y, "sub", (F["small"], c["sub"])))
            y += F["small"].get_linesize()
        y += k(14)
        for bl in c["blocks"]:
            t = bl["t"]
            if t in ("h1", "h2", "h3"):
                f = F["h2"] if t in ("h1", "h2") else F["h3"]
                y += k(14)
                text(f, bl["x"], 0, width, "head", GOLD)
                y += k(6)
            elif t == "p":
                text(F["p"], bl["x"], 0, width, "text", TEXT)
                y += k(10)
            elif t == "li":
                rows.append((y, "bullet", (F["p"],)))
                text(F["p"], bl["x"], k(22), width - k(22), "text", TEXT)
                y += k(4)
            elif t == "quote":
                y0 = y
                text(F["p"], bl["x"], k(22), width - k(30), "text", DIM)
                rows.append((y0, "qbar", (y - y0,)))
                y += k(10)
            elif t == "table":
                for i, row in enumerate(bl["rows"]):
                    cells = [x for x in row if x]
                    if not cells:
                        continue
                    if len(cells) > 1:
                        txt = "**" + cells[0] + "** – " + " – ".join(cells[1:]) if i else "**" + " – ".join(cells) + "**"
                    else:
                        txt = cells[0]
                    text(F["p"], txt, k(10), width - k(10), "text", TEXT if i else GOLD)
                    y += k(4)
                y += k(8)
            elif t == "hr":
                y += k(10)
                rows.append((y, "hr", ()))
                y += k(16)
            elif t == "img":
                iw = min(width, k(820))
                ih = iw * 9 // 16
                y += k(10)
                rows.append((y, "img", (bl["id"], bl.get("cap", ""), iw, ih)))
                y += ih + k(6)
                if bl.get("cap"):
                    rows.append((y, "cap", (F["cap"], bl["cap"], iw)))
                    y += F["cap"].get_linesize()
                y += k(14)
        y += k(40)
        self._lay = (key, rows, y)
        return rows, y

    def image(self, iid, w, h):
        key = (iid, w, h)
        if key not in self._imgs:
            p = os.path.join(ROOT, "assets", "stories", iid + ".png")
            im = None
            if os.path.isfile(p):
                try:
                    im = pygame.transform.smoothscale(pygame.image.load(p).convert(), (w, h))
                except pygame.error:
                    im = None
            self._imgs[key] = im
        return self._imgs[key]

    # ---- események ----
    def handle(self, ev):
        tree, page = self.geom()
        if ev.type == pygame.MOUSEWHEEL:
            pos = pygame.mouse.get_pos()
            if tree.collidepoint(pos):
                self.tree_scroll = max(0, self.tree_scroll - ev.y)
            else:
                self.scroll -= ev.y * self.k(60)
        elif ev.type == pygame.KEYDOWN:
            step = {pygame.K_DOWN: 40, pygame.K_UP: -40, pygame.K_PAGEDOWN: page.h - 60,
                    pygame.K_PAGEUP: -(page.h - 60), pygame.K_SPACE: page.h - 60}.get(ev.key)
            if step:
                self.scroll += step
            elif ev.key == pygame.K_HOME:
                self.scroll = 0
            elif ev.key == pygame.K_END:
                self.scroll = 1e9
        elif ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            sb = getattr(self, "_sbar", None)
            if sb and sb.collidepoint(ev.pos):
                self._drag = ev.pos[1]
        elif ev.type == pygame.MOUSEBUTTONUP and ev.button == 1:
            self._drag = None
        elif ev.type == pygame.MOUSEMOTION and self._drag is not None:
            _, total = self.build(page.w - self.k(70))
            dy = ev.pos[1] - self._drag
            self._drag = ev.pos[1]
            self.scroll += dy * total / max(1, page.h)

    # ---- rajz ----
    def draw_tree(self, tree):
        s, k = self.s, self.k
        F = self.fonts
        panel(s, tree, 215)
        rh = k(30)
        rows = self.tree_rows()
        vis = max(1, (tree.h - k(12)) // rh)
        mouse = pygame.mouse.get_pos()
        for i, (kind, it) in enumerate(rows[self.tree_scroll:self.tree_scroll + vis]):
            r = pygame.Rect(tree.x + k(6), tree.y + k(6) + i * rh, tree.w - k(12), rh - k(2))
            if kind == "group":
                label(s, F["tiny"], it["title"] or "", (r.x + k(12), r.centery - F["tiny"].get_height() / 2), DIM,
                      r.w - k(16), shadow=False)
                pygame.draw.line(s, FRAME_DARK, (r.x + k(12), r.bottom - 1), (r.right - k(8), r.bottom - 1))
                continue
            sel = kind == "chap" and it["id"] == self.cur
            if sel:
                s.blit(MenuItem.bar(r.w, r.h), r)
            elif r.collidepoint(mouse):
                pygame.draw.rect(s, FRAME_DARK, r, 1)
            if kind == "book":
                mark = "– " if it["id"] in self.open else "+ "
                label(s, F["small"], mark + it["title"], (r.x + k(4), r.centery - F["small"].get_height() / 2), GOLD,
                      r.w - k(8))
            else:
                ind = k(28) if any(kk == "group" for kk, _ in rows) and self.chap.get(it["id"], ({}, {}))[0].get("id") == "novellak" else k(20)
                label(s, F["tiny"], it["title"], (r.x + ind, r.centery - F["tiny"].get_height() / 2),
                      (255, 240, 200) if sel else TEXT, r.w - ind - k(6))
        if len(rows) > vis:
            h = tree.h * vis / len(rows)
            y = tree.y + (tree.h - h) * self.tree_scroll / max(1, len(rows) - vis)
            pygame.draw.rect(s, FRAME, (tree.right - k(5), y, k(3), h))

    def draw(self):
        s, k = self.s, self.k
        self.title("TÖRTÉNETEK")
        tree, page = self.geom()
        self.draw_tree(tree)
        panel(s, page, 225)
        if not self.cur:
            wrap(s, self.fonts["med"], "Még nincs beemelt történet.",
                 page.x + k(24), page.y + k(24), page.w - k(48), DIM)
            return
        inner = page.inflate(-k(60), -k(30))
        rows, total = self.build(inner.w - k(10))
        self.scroll = max(0.0, min(self.scroll, max(0, total - inner.h)))
        top = int(self.scroll)
        clip = s.get_clip()
        s.set_clip(inner)
        for y, kind, d in rows:
            yy = inner.y + y - top
            if yy > inner.bottom:
                break
            if kind == "img":
                if yy + d[3] < inner.y:
                    continue
            elif yy + k(40) < inner.y:
                continue
            x = inner.x
            if kind == "title":
                label(s, d[0], d[1], (x, yy), GOLD, inner.w)
            elif kind == "sub":
                label(s, d[0], d[1], (x, yy), DIM, inner.w, shadow=False)
            elif kind in ("text", "head"):
                font, ln, x0, col = d
                BF = self.body_fonts()
                for w, st, ww, wx in ln:
                    c = col if kind == "head" else ((255, 222, 150) if st == 1 else ((214, 226, 236) if st == 2 else col))
                    f = BF["pi"] if (st == 2 and font is BF["p"]) else font
                    if kind == "text" and st == 0:
                        s.blit(f.render(w, True, (10, 8, 12)), (x + x0 + wx + 1, yy + 1))
                    s.blit(f.render(w, True, c), (x + x0 + wx, yy))
            elif kind == "bullet":
                pygame.draw.circle(s, GOLD, (x + k(9), yy + d[0].get_linesize() // 2), max(2, k(3)))
            elif kind == "qbar":
                pygame.draw.rect(s, FRAME, (x + k(6), yy, k(3), d[0]))
            elif kind == "hr":
                ornament(s, inner.centerx, yy, inner.w * 0.5, self.u)
            elif kind == "img":
                iid, cap, iw, ih = d
                r = pygame.Rect(inner.centerx - iw // 2, yy, iw, ih)
                im = self.image(iid, iw, ih)
                if im is not None:
                    s.blit(im, r)
                else:  # a kép még nincs meg: keretezett hely
                    ph = pygame.Surface(r.size, pygame.SRCALPHA)
                    ph.fill((60, 46, 30, 150))
                    s.blit(ph, r)
                    label(s, self.fonts["small"], "A kép még készül", (r.centerx, r.centery - k(14)), DIM,
                          r.w - k(20), align="center")
                    label(s, self.fonts["tiny"], f"assets/stories/{iid}.png", (r.centerx, r.centery + k(10)),
                          (130, 120, 100), r.w - k(20), align="center", shadow=False)
                pygame.draw.rect(s, FRAME, r.inflate(4, 4), 2)
                pygame.draw.rect(s, FRAME_DARK, r.inflate(8, 8), 1)
            elif kind == "cap":
                label(s, d[0], d[1], (inner.centerx, yy), DIM, d[2], align="center", shadow=False)
        s.set_clip(clip)
        # görgetősáv
        if total > inner.h:
            bar = pygame.Rect(page.right - k(16), page.y + k(10), k(6), page.h - k(20))
            h = max(k(30), bar.h * inner.h / total)
            y = bar.y + (bar.h - h) * self.scroll / max(1, total - inner.h)
            pygame.draw.rect(s, FRAME_DARK, bar)
            self._sbar = pygame.Rect(bar.x - k(4), y, bar.w + k(8), h)
            pygame.draw.rect(s, GOLD, (bar.x, y, bar.w, h))
        else:
            self._sbar = None
        b, c = self.chap[self.cur]
        label(s, self.fonts["tiny"], f"{b['title']} – {b.get('sub', '')}", (page.centerx, s.get_height() - k(60)),
              DIM, page.w - k(520), align="center", shadow=False)


class CreditsMenu(Screen):
    BG = "options"
    def layout(self):
        self.buttons = [self.back_button()]

    def draw(self):
        s = self.s
        W, H = s.get_size()
        k, F = self.k, self.fonts
        self.title("KÉSZÍTŐK")
        lines = ["TOTU: Sky Rift – A kölcsönkapott öröklét", "",
                 "Kreátor: Mesehordo", "© 2026 Mesehordo – a világ és a történet minden joga fenntartva", "",
                 "Motor: Python + pygame-ce  •  Sprite-generátor: numpy + Pillow",
                 "Ihlet: Warcraft II, StarCraft, Armies of Exigo, Command & Conquer", "",
                 "Világ és történet: docs/EGSZAKADAS_vilag_es_tortenet_v01.md",
                 "Minden tartalom moddolható – lásd docs/MODDING.md"]
        lh = k(34)
        pw = min(k(1000), W - k(72))
        r = pygame.Rect(W / 2 - pw / 2, self.top + k(10), pw, len(lines) * lh + k(40))
        panel(s, r, 200)
        y = r.y + k(20)
        for ln in lines:
            if ln:
                f, txt = fit_font(F, ["med", "small", "tiny"], ln, pw - k(40))
                label(s, f, txt, (W / 2, y), GOLD if ln.startswith("TOTU: Sky Rift") else TEXT, align="center")
            y += lh


class MusicMenu(Screen):
    """Zeneválasztó játék közben: a küldetés saját számai és az általános számok; kattintásra az szól,
    utána a véletlen sorrend folytatódik."""

    def __init__(self, app, snapshot):
        super().__init__(app)
        from . import audio as AU
        self.A = AU.get(app.profile.settings)
        self.snapshot = snapshot
        self.scroll = 0

    def layout(self):
        W, H = self.s.get_size()
        k = self.k
        tr = self.A.all_tracks()
        self.rows = max(1, (H - self.top - k(170)) // k(44))
        self.scroll = max(0, min(self.scroll, max(0, len(tr) - self.rows)))
        self.buttons = []
        for i, (name, rel) in enumerate(tr[self.scroll:self.scroll + self.rows]):
            cur = rel == self.A.current
            self.buttons.append(Button((W / 2 - k(260), self.top + k(20) + i * k(44), k(520), k(38)),
                                       ("» " if cur else "") + name, (lambda r=rel: self.A.choose(r)), small=True))
        self.buttons.append(Button((W / 2 - k(260), H - k(140), k(250), k(46)), "Véletlen következő",
                                   self.A._next_game_track, small=True))
        self.buttons.append(self.back_button("Vissza"))

    def handle(self, ev):
        if ev.type == pygame.MOUSEWHEEL:
            self.scroll = max(0, self.scroll - ev.y)

    def run(self):
        bg = self.app.bg
        snap = self.snapshot

        class Dim:
            def draw(self, s, key=None):
                s.blit(snap, (0, 0))
                d = pygame.Surface(s.get_size(), pygame.SRCALPHA)
                d.fill((0, 0, 0, 170))
                s.blit(d, (0, 0))
        self.app.bg = Dim()
        try:
            return super().run()
        finally:
            self.app.bg = bg

    def draw(self):
        self.title("ZENE")
        F = self.fonts
        W, H = self.s.get_size()
        if not self.A.all_tracks():
            wrap(self.s, F["med"], "Még nincs zene. A számok helye: assets/music (küldetésenként: "
                 "assets/music/kuldetesek/<küldetés>/).", W / 2 - self.k(260), self.top + self.k(30), self.k(520), DIM)
        cur = self.A.current or "–"
        label(self.s, F["small"], "Most szól: " + os.path.splitext(os.path.basename(cur))[0],
              (W / 2, H - self.k(180)), GOLD, self.k(520), align="center")


class PauseMenu(Screen):
    """Játék közbeni menü az aktuális képkocka fölött."""

    def __init__(self, app, snapshot, campaign_mode=False):
        super().__init__(app)
        self.snapshot = snapshot
        self.campaign_mode = campaign_mode

    def layout(self):
        W, H = self.s.get_size()
        k = self.k
        items = [("Folytatás", ("resume",)), ("Mentés", ("save",)), ("Betöltés", ("load",)),
                 ("Zene", ("music",)), ("Beállítások", ("settings",)), ("Újrakezdés", ("restart",)), ("Kilépés a menübe", ("menu",)), ("Kilépés a játékból", ("quit",))]
        y0 = max(self.top + k(30), H * 0.28)
        step = min(k(56), (H - y0 - k(20)) // len(items))
        self.buttons = [Button((W / 2 - k(170), y0 + i * step, k(340), step - k(10)), lab, (lambda a=act: a))
                        for i, (lab, act) in enumerate(items)]

    def run(self):
        bg = self.app.bg
        snap = self.snapshot

        class Dim:
            def draw(self, s, key=None):
                s.blit(snap, (0, 0))
                d = pygame.Surface(s.get_size(), pygame.SRCALPHA)
                d.fill((0, 0, 0, 150))
                s.blit(d, (0, 0))
        self.app.bg = Dim()
        try:
            r = super().run()
        finally:
            self.app.bg = bg
        return ("resume",) if r == ("back",) else r

    def draw(self):
        self.title("SZÜNET", max(self.k(14), self.s.get_height() * 0.12 - self.k(30)))
