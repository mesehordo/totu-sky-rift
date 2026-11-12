"""Logó és nyitókép (intro) – a játék saját sprite-jaiból és festett effektekből.

    python tools/make_intro.py
        -> assets/ui/logo.png (1200×300), assets/ui/logo_small.png (600×150),
           assets/ui/bg/intro.png (1920×1080), assets/ui/bg/menu.png (1920×1080, logó nélkül)

Ha később kézzel rajzolt logó / nyitókép kerül ugyanezekre a helyekre, a játék azokat használja.
"""
import json
import math
import os

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPR = os.path.join(ROOT, "assets", "sprites")
UI = os.path.join(ROOT, "assets", "ui")
RNG = np.random.default_rng(1977)
TITLE = "TOTU: Sky Rift"


def font(size):
    for fn in ("georgiab.ttf", "constanb.ttf", "cambriab.ttf", "timesbd.ttf", "arialbd.ttf", "DejaVuSerif-Bold.ttf"):
        try:
            return ImageFont.truetype(fn, size)
        except OSError:
            pass
    return ImageFont.load_default()


def noise(w, h, scale, octaves=4, seed=0):
    """Egyszerű értékzaj (festett textúrákhoz)."""
    rng = np.random.default_rng(seed)
    out = np.zeros((h, w), np.float32)
    amp, tot = 1.0, 0.0
    for o in range(octaves):
        s = max(1, int(scale / 2 ** o))
        g = rng.random((h // s + 2, w // s + 2)).astype(np.float32)
        im = Image.fromarray((g * 255).astype(np.uint8)).resize((w + s * 2, h + s * 2), Image.BICUBIC)
        out += np.asarray(im, np.float32)[:h, :w] / 255 * amp
        tot += amp
        amp *= 0.5
    return out / tot


def glow(img, radius, strength=1.0):
    """Fénylés: elmosott másolat rávetítve (additív)."""
    g = img.filter(ImageFilter.GaussianBlur(radius))
    a = np.asarray(g, np.float32) * strength
    b = np.asarray(img, np.float32)
    return Image.fromarray(np.clip(b + a, 0, 255).astype(np.uint8), img.mode)


# ---------------------------------------------------------------- logó
def make_logo(W=1200, H=300, meteor=True):
    big = 2  # nagyban rajzolunk, a végén kicsinyítünk (simább élek)
    w, h = W * big, H * big
    tmp = Image.new("L", (w, h), 0)
    d = ImageDraw.Draw(tmp)
    spacing = int(6 * big)
    size = int(176 * big)
    while True:  # a felirat férjen el a szélesség 90%-ában
        f = font(size)
        widths = [d.textbbox((0, 0), ch, font=f)[2] - d.textbbox((0, 0), ch, font=f)[0] for ch in TITLE]
        total = sum(widths) + spacing * (len(TITLE) - 1)
        if total <= w * 0.9 or size < 40:
            break
        size = int(size * 0.94)
    x = (w - total) // 2
    bb = d.textbbox((0, 0), TITLE, font=f)
    y = (h - (bb[3] - bb[1])) // 2 - bb[1] + int(8 * big)
    for ch, cw in zip(TITLE, widths):
        off = d.textbbox((0, 0), ch, font=f)[0]
        d.text((x - off, y), ch, font=f, fill=255)
        x += cw + spacing
    mask = tmp
    # kőtextúra
    n1 = noise(w, h, 40 * big, 5, 3)
    n2 = noise(w, h, 6 * big, 3, 4)
    yy = np.linspace(0, 1, h)[:, None]
    base = 0.62 + 0.25 * (1 - yy) + (n1 - 0.5) * 0.35 + (n2 - 0.5) * 0.18
    stone = np.dstack([base * 196, base * 178, base * 150]).clip(0, 255)
    # domborítás: a maszk elmosott gradienséből fény/árnyék (balról-felülről)
    m = np.asarray(mask.filter(ImageFilter.GaussianBlur(3 * big)), np.float32) / 255
    gx = np.roll(m, -2, 1) - np.roll(m, 2, 1)
    gy = np.roll(m, -2, 0) - np.roll(m, 2, 0)
    shade = 1 + (-gx - gy) * 1.6
    stone = (stone * shade[..., None]).clip(0, 255)
    # repedések
    crack = Image.new("L", (w, h), 0)
    cd = ImageDraw.Draw(crack)
    for _ in range(26):
        px, py = RNG.uniform(0, w), RNG.uniform(h * 0.25, h * 0.8)
        ang = RNG.uniform(0, math.pi)
        pts = [(px, py)]
        for _ in range(RNG.integers(3, 7)):
            ang += RNG.uniform(-0.8, 0.8)
            ln = RNG.uniform(8, 26) * big
            px, py = px + math.cos(ang) * ln, py + math.sin(ang) * ln
            pts.append((px, py))
        cd.line(pts, fill=255, width=max(1, int(1.2 * big)))
    crk = np.asarray(crack, np.float32)[..., None] / 255
    stone = stone * (1 - crk * 0.6)
    # az „égszakadás”: izzó, ferde hasadék végig a feliraton
    rift = Image.new("L", (w, h), 0)
    rd = ImageDraw.Draw(rift)
    pts = []
    for i in range(40):
        t = i / 39
        pts.append((t * w, h * (0.30 + 0.38 * t) + RNG.uniform(-9, 9) * big))
    rd.line(pts, fill=255, width=int(5 * big))
    rift_core = np.asarray(rift, np.float32) / 255
    rift_glow = np.asarray(rift.filter(ImageFilter.GaussianBlur(10 * big)), np.float32) / 255
    cyan = np.array([150, 255, 235], np.float32)
    stone = stone * (1 - rift_core[..., None]) + cyan * rift_core[..., None]
    stone = stone + np.array([60, 200, 190], np.float32) * (rift_glow[..., None] * 0.7)
    alpha = np.asarray(mask, np.float32) / 255
    alpha = alpha * (1 - 0.85 * rift_core * (np.asarray(mask, np.float32) / 255))  # a hasadék átvág a betűkön
    letters = np.dstack([stone.clip(0, 255), alpha * 255]).astype(np.uint8)
    letters = Image.fromarray(letters, "RGBA")
    # sötét körvonal és vetett árnyék
    outline = mask.filter(ImageFilter.MaxFilter(int(9 * big) | 1))
    out = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    sh = Image.new("RGBA", (w, h), (10, 6, 16, 0))
    sh.putalpha(outline.filter(ImageFilter.GaussianBlur(8 * big)).point(lambda v: int(v * 0.85)))
    out.alpha_composite(sh, (int(6 * big), int(9 * big)))
    ol = Image.new("RGBA", (w, h), (34, 22, 30, 0))
    ol.putalpha(outline)
    # meteor-csóva a felirat mögött
    tail = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    td = ImageDraw.Draw(tail)
    x0, y0, x1, y1 = w * 0.97, h * 0.05, w * 0.05, h * 0.95
    for i in range(60):
        t = i / 59
        r = (1 - t) * 26 * big + 2
        cx, cy = x0 + (x1 - x0) * t * 0.55, y0 + (y1 - y0) * t * 0.55
        a = int(200 * (1 - t) ** 1.6)
        td.ellipse((cx - r, cy - r * 0.6, cx + r, cy + r * 0.6), fill=(255, 210, 140, a))
    if meteor:
        out.alpha_composite(tail.filter(ImageFilter.GaussianBlur(9 * big)))
        head = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        ImageDraw.Draw(head).ellipse((x0 - 22 * big, y0 - 18 * big, x0 + 22 * big, y0 + 18 * big),
                                     fill=(255, 250, 230, 255))
        out.alpha_composite(head.filter(ImageFilter.GaussianBlur(6 * big)))
    out.alpha_composite(ol)
    out.alpha_composite(letters)
    # izzó szegély a hasadék mentén (a betűkön kívül is)
    rg = Image.new("RGBA", (w, h), (120, 255, 230, 0))
    rg.putalpha(rift.filter(ImageFilter.GaussianBlur(14 * big)).point(lambda v: int(v * 0.55)))
    out.alpha_composite(rg)
    out = out.resize((W, H), Image.LANCZOS)
    al = out.getchannel("A").point(lambda v: 0 if v < 10 else v)  # az elmosás halvány „doboza” ne látsszon
    out.putalpha(al)
    return out


# ---------------------------------------------------------------- nyitókép
def sprite_frame(cat, sid, direction="S", col=0, team=(200, 60, 50)):
    mp = os.path.join(SPR, cat, sid + ".json")
    if not os.path.isfile(mp):
        return None
    m = json.load(open(mp, encoding="utf-8"))
    fw, fh = m["frame_size"]
    row = m.get("directions", ["S"]).index(direction) if direction in m.get("directions", ["S"]) else 0
    im = Image.open(os.path.join(SPR, cat, m["image"])).convert("RGBA").crop((col * fw, row * fh, col * fw + fw,
                                                                             row * fh + fh))
    if m.get("team_mask"):
        tm = Image.open(os.path.join(SPR, cat, m["team_mask"])).convert("RGBA").crop((col * fw, row * fh,
                                                                                     col * fw + fw, row * fh + fh))
        a = np.asarray(im, np.float32)
        t = np.asarray(tm, np.float32)[..., 3:4] / 255
        lum = a[..., :3].mean(axis=2, keepdims=True) / 200
        a[..., :3] = a[..., :3] * (1 - t) + np.array(team, np.float32) * lum * t
        im = Image.fromarray(a.clip(0, 255).astype(np.uint8), "RGBA")
    bb = im.getbbox()
    return im.crop(bb) if bb else im


def lit(im, color, amount):
    """Színes megvilágítás (a kráter fénye) egy sprite-ra."""
    a = np.asarray(im, np.float32)
    a[..., :3] = a[..., :3] * (1 - amount) + np.array(color, np.float32) * (a[..., :3] / 255) * 1.6 * amount
    return Image.fromarray(a.clip(0, 255).astype(np.uint8), "RGBA")


def make_scene(W=1920, H=1080, with_logo=True):
    yy = np.linspace(0, 1, H)[:, None, None]
    top = np.array([10, 8, 30], np.float32)
    mid = np.array([58, 30, 74], np.float32)
    hor = np.array([226, 120, 70], np.float32)
    sky = np.where(yy < 0.5, top + (mid - top) * (yy / 0.5), mid + (hor - mid) * ((yy - 0.5) / 0.22).clip(0, 1))
    sky = np.broadcast_to(sky, (H, W, 3)).copy()
    n = noise(W, H, 260, 4, 11)[..., None]
    sky *= 0.88 + 0.24 * n  # felhős, festett ég
    img = Image.fromarray(sky.clip(0, 255).astype(np.uint8), "RGB").convert("RGBA")
    d = ImageDraw.Draw(img)
    for _ in range(700):  # csillagok
        x, y = RNG.uniform(0, W), RNG.uniform(0, H * 0.55)
        b = int(RNG.uniform(120, 255))
        r = RNG.choice([0.6, 0.8, 1.2, 1.8], p=[0.5, 0.3, 0.15, 0.05])
        d.ellipse((x - r, y - r, x + r, y + r), fill=(b, b, min(255, b + 20), 255))
    # az ég hasadéka: izzó, cikkcakkos fényrés + sarki fény
    rift = Image.new("L", (W, H), 0)
    rd = ImageDraw.Draw(rift)
    pts = [(W * 0.08 + i * W * 0.84 / 30, H * 0.10 + i * H * 0.012 + RNG.uniform(-14, 14)) for i in range(31)]
    rd.line(pts, fill=255, width=4)
    for i in range(0, 30, 3):  # leágazások
        x, y = pts[i]
        rd.line([(x, y), (x + RNG.uniform(-60, 60), y + RNG.uniform(20, 70))], fill=120, width=2)
    rift = rift.filter(ImageFilter.GaussianBlur(1.2))
    aur = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ad = ImageDraw.Draw(aur)
    for i in range(30):
        x, y = pts[i]
        ad.ellipse((x - 120, y - 40, x + 120, y + 140), fill=(60, 220, 190, 26))
    img.alpha_composite(aur.filter(ImageFilter.GaussianBlur(40)))
    rg = Image.new("RGBA", (W, H), (190, 255, 240, 0))
    rg.putalpha(rift.filter(ImageFilter.GaussianBlur(18)).point(lambda v: min(255, v * 3)))
    img.alpha_composite(rg)
    rc = Image.new("RGBA", (W, H), (240, 255, 250, 0))
    rc.putalpha(rift)
    img.alpha_composite(rc)
    # a meteor: fej + hosszú, kettős csóva a kráter felé
    mx0, my0, mx1, my1 = W * 0.86, H * 0.08, W * 0.52, H * 0.60
    tail = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    td = ImageDraw.Draw(tail)
    for i in range(90):
        t = i / 89
        cx, cy = mx0 + (mx1 - mx0) * t * 0.45, my0 + (my1 - my0) * t * 0.45
        r = 8 + 70 * t
        a = int(150 * (1 - t) ** 1.3)
        td.ellipse((cx - r, cy - r * 0.55, cx + r, cy + r * 0.55), fill=(255, 190, 120, a))
    tail = tail.rotate(0)
    img.alpha_composite(tail.filter(ImageFilter.GaussianBlur(16)))
    hd = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(hd).ellipse((mx0 - 26, my0 - 20, mx0 + 26, my0 + 20), fill=(255, 250, 235, 255))
    img.alpha_composite(glow(hd, 30, 1.6))
    # hegyvonulatok (hátulról előre), a meteor felőli peremfénnyel
    for k, (base, amp, col, rim) in enumerate(((0.56, 110, (44, 30, 58), 0.55), (0.66, 80, (30, 22, 40), 0.4),
                                                (0.76, 60, (20, 16, 26), 0.25))):
        layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        pts = [(0, H)]
        x, y = 0, H * base
        while x < W + 40:
            y = min(H * (base + 0.08), max(H * (base - 0.12), y + RNG.uniform(-amp, amp) * 0.4))
            pts.append((x, y))
            x += RNG.uniform(20, 60)
        pts.append((W + 40, H))
        ImageDraw.Draw(layer).polygon(pts, fill=col + (255,))
        edge = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        ImageDraw.Draw(edge).line(pts[1:-1], fill=(255, 170, 110, int(170 * rim)), width=3)
        img.alpha_composite(layer)
        img.alpha_composite(edge.filter(ImageFilter.GaussianBlur(2)))
        fog = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        ImageDraw.Draw(fog).rectangle((0, H * (base + 0.02), W, H), fill=(90, 60, 110, 40))
        img.alpha_composite(fog.filter(ImageFilter.GaussianBlur(30)))
    # a völgy talaja és a kráter
    gnd = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    gd = ImageDraw.Draw(gnd)
    gd.polygon([(0, H * 0.72), (W * 0.2, H * 0.69), (W * 0.45, H * 0.71), (W * 0.7, H * 0.69), (W, H * 0.72),
                (W, H), (0, H)], fill=(255, 255, 255, 255))
    ga = np.asarray(gnd, np.float32)
    gy = np.linspace(0, 1, H)[:, None]
    tex = noise(W, H, 30, 4, 31)
    gcol = np.dstack([46 + 30 * tex - 14 * gy, 38 + 22 * tex - 12 * gy, 34 + 16 * tex - 10 * gy])
    ga[..., :3] = gcol
    img.alpha_composite(Image.fromarray(ga.clip(0, 255).astype(np.uint8), "RGBA"))
    edge = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(edge).line([(0, H * 0.72), (W * 0.2, H * 0.69), (W * 0.45, H * 0.71), (W * 0.7, H * 0.69),
                               (W, H * 0.72)], fill=(255, 170, 120, 120), width=3)
    img.alpha_composite(edge.filter(ImageFilter.GaussianBlur(2)))
    cx, cy = W * 0.52, H * 0.80
    cr = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(cr).ellipse((cx - 210, cy - 52, cx + 210, cy + 52), fill=(70, 56, 50, 255))
    ImageDraw.Draw(cr).ellipse((cx - 190, cy - 44, cx + 190, cy + 46), fill=(20, 16, 18, 255))
    img.alpha_composite(cr)
    cg = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(cg).ellipse((cx - 120, cy - 28, cx + 120, cy + 28), fill=(190, 90, 255, 255))  # lila meteorkő
    img.alpha_composite(glow(cg, 50, 2.2))
    beam = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(beam).polygon([(cx - 40, cy), (cx + 40, cy), (cx + 140, 0), (cx - 140, 0)], fill=(190, 110, 255, 30))
    img.alpha_composite(beam.filter(ImageFilter.GaussianBlur(40)))
    mp = os.path.join(SPR, "resources", "meteor.json")  # a kráterben a játék lila meteorkristálya, nagyban
    if os.path.isfile(mp):
        mm = json.load(open(mp, encoding="utf-8"))
        fw, fh = mm["frame_size"]
        cr_im = Image.open(os.path.join(SPR, "resources", mm["image"])).convert("RGBA").crop((0, 0, fw, fh))
        bb = cr_im.getbbox()
        if bb:
            cr_im = cr_im.crop(bb)
            cr_im = cr_im.resize((cr_im.width * 3, cr_im.height * 3), Image.NEAREST)
            img.alpha_composite(glow(cr_im, 12, 0.9), (int(cx - cr_im.width / 2), int(cy - cr_im.height * 0.78)))
    # a hat faj otthona a kráter körül (a játék épületei), kétszeresre nagyítva, a kráter fényével
    blds = [("elf_napszentely", 0.09, 0.80, (232, 190, 70)), ("urs_nagyterem", 0.24, 0.77, (90, 140, 220)),
            ("wul_tanacssator", 0.37, 0.75, (220, 90, 40)), ("myc_szivgomba", 0.67, 0.75, (80, 225, 205)),
            ("imp_forum", 0.80, 0.77, (200, 60, 50)), ("cro_kaptar", 0.94, 0.80, (150, 255, 120))]
    for bid, fx, fy, team in blds:
        im = sprite_frame("buildings", bid, team=team)
        if im is None:
            continue
        sc = 1.6
        im = im.resize((int(im.width * sc), int(im.height * sc)), Image.NEAREST)
        dist = abs(fx - 0.52)
        im = lit(im, (200, 120, 255), max(0.0, 0.45 - dist * 0.6))
        dark = Image.new("RGBA", im.size, (20, 14, 30, 0))
        dark.putalpha(im.getchannel("A").point(lambda v: int(v * 0.35)))
        im.alpha_composite(dark)
        img.alpha_composite(im, (int(W * fx - im.width / 2), int(H * fy - im.height * 0.92)))
    # előtér: a hat faj hősei háttal, a kráter felé nézve
    heroes = [("elf_hos_szelai", 0.20), ("urs_hos_ragna", 0.32), ("wul_hos_tava", 0.43), ("myc_hos_mor", 0.61),
              ("imp_hos_cassius", 0.72), ("cro_hos_keth", 0.84)]
    for uid, fx in heroes:
        im = sprite_frame("units", uid, direction="N")
        if im is None:
            continue
        sc = 3.4
        im = im.resize((int(im.width * sc), int(im.height * sc)), Image.NEAREST)
        im = lit(im, (200, 120, 255), 0.25)
        dark = Image.new("RGBA", im.size, (14, 10, 22, 0))
        dark.putalpha(im.getchannel("A").point(lambda v: int(v * 0.45)))
        im.alpha_composite(dark)
        rim = Image.new("RGBA", im.size, (215, 160, 255, 0))
        edge = ImageChops.subtract(im.getchannel("A"), im.getchannel("A").transform(
            im.size, Image.AFFINE, (1, 0, 0, 0, 1, 3)))
        rim.putalpha(edge.point(lambda v: int(v * 0.8)))
        im.alpha_composite(rim)
        img.alpha_composite(im, (int(W * fx - im.width / 2), int(H * 0.99 - im.height)))
    # festett hatás: enyhe lágyítás + zaj + vignetta
    img = Image.blend(img, img.filter(ImageFilter.GaussianBlur(1.2)), 0.35)
    a = np.asarray(img, np.float32)
    a[..., :3] *= (0.94 + 0.12 * noise(W, H, 3, 2, 21))[..., None]
    vy, vx = np.mgrid[0:H, 0:W]
    v = 1 - 0.55 * (((vx - W / 2) / (W * 0.62)) ** 2 + ((vy - H * 0.55) / (H * 0.75)) ** 2)
    a[..., :3] *= v.clip(0.25, 1)[..., None]
    img = Image.fromarray(a.clip(0, 255).astype(np.uint8), "RGBA")
    if with_logo:
        logo = main_logo()
        logo = logo.resize((900, round(900 * logo.height / logo.width)), Image.LANCZOS)
        img.alpha_composite(logo, ((W - logo.width) // 2, int(H * 0.09)))
        f = font(26)
        ImageDraw.Draw(img).text((W - 280, H - 48), "Kreátor: Mesehordo", font=f, fill=(220, 210, 200, 200))
    return img.convert("RGB")


LOGO_SRC = os.path.join(UI, "logos", "07_lila_vihar.png")


def main_logo():
    """A fő logó (assets/ui/logos/07_lila_vihar.png) körbevágva, 1200 széles; ha nincs, a rajzolt logó."""
    if not os.path.exists(LOGO_SRC):
        return make_logo()
    im = Image.open(LOGO_SRC).convert("RGBA")
    im = im.crop(im.getbbox())
    return im.resize((1200, round(1200 * im.height / im.width)), Image.LANCZOS)


def main():
    os.makedirs(os.path.join(UI, "bg"), exist_ok=True)
    logo = main_logo()
    logo.save(os.path.join(UI, "logo.png"))
    logo.resize((600, round(600 * logo.height / logo.width)), Image.LANCZOS).save(os.path.join(UI, "logo_small.png"))
    make_scene(with_logo=True).save(os.path.join(UI, "bg", "intro.png"))
    make_scene(with_logo=False).save(os.path.join(UI, "bg", "menu.png"))
    print("logó és nyitókép: assets/ui/logo.png, logo_small.png, bg/intro.png, bg/menu.png")


if __name__ == "__main__":
    main()
