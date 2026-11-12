"""Tárgy-ikonok (WC3-szerű leltár-ikonok): kis 3D modell + keretezett, sötét háttér.

A modell ugyanazzal a renderelővel készül, mint az egységek, majd egy 48×48-as,
ritkaság szerint színezett keretbe kerül.
"""
import math

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

from tools.spritegen.engine import Model, render

RARITY = {"common": (150, 140, 120), "rare": (80, 140, 230), "artifact": (236, 170, 60)}


def _bottle(m, col):
    m.ellipsoid((0, 0, 5), (4.6, 4.6, 4.8), col, spec=0.9, tex=0.04)
    m.ellipsoid((-1.6, -2.6, 6.6), (1.0, 0.6, 1.4), (240, 250, 255), glow=True)
    m.cylinder((0, 0, 9), (0, 0, 12.5), 1.5, (190, 210, 220), spec=0.8)
    m.cylinder((0, 0, 12.5), (0, 0, 14.2), 1.8, (130, 90, 50))


def _book(m, col):
    m.box((0, 0, 1.8), (6, 4.6, 1.8), col, tex=0.2, mat="fur")
    m.box((0.6, 0, 1.8), (5.6, 4.2, 1.4), (236, 226, 196))
    m.box((-5.6, 0, 1.8), (0.6, 4.8, 2.0), (120, 90, 40), spec=0.6)
    m.ellipsoid((0, 0, 3.8), (1.6, 1.6, 0.4), (230, 190, 80), spec=0.9)


def build_icon(key):
    m = Model(key)
    if key == "potion_red":
        _bottle(m, (220, 40, 50))
    elif key == "potion_blue":
        _bottle(m, (60, 120, 240))
    elif key == "tome":
        _book(m, (110, 70, 140))
    elif key == "tome_red":
        _book(m, (170, 40, 40))
    elif key == "tome_green":
        _book(m, (50, 130, 60))
    elif key == "boots":
        for y in (-3, 3):
            m.capsule((0, y, 2), (0, y, 9), 2.2, (120, 80, 50), mat="fur")
            m.ellipsoid((2.6, y, 1.4), (4, 2.2, 1.6), (100, 66, 40))
            m.cylinder((0, y, 8), (0, y, 9.4), 2.5, (190, 170, 120))
    elif key in ("ring", "ring_green"):
        for k in range(14):
            a = k / 14 * 2 * math.pi
            m.ellipsoid((math.cos(a) * 5, 0, 5 + math.sin(a) * 5), (1.3, 1.3, 1.3), (230, 190, 80), spec=1.0)
        m.ellipsoid((0, 0, 10.6), (2.2, 2.2, 2.0), (80, 220, 110) if key == "ring_green" else (200, 210, 230),
                    spec=1.0, glow=key == "ring_green")
    elif key == "claws":
        m.ellipsoid((0, 0, 4), (4.5, 3.6, 3), (110, 80, 60), mat="fur")
        for k in range(3):
            y = (k - 1) * 2.4
            m.capsule((3, y, 4.5), (9, y, 7.5), 0.9, (220, 220, 230), r1=0.15, spec=1.0)
    elif key == "amulet":
        for k in range(12):
            a = k / 12 * math.pi
            m.ellipsoid((math.cos(a) * 6, 0, 8 + math.sin(a) * 5), (0.6,) * 3, (200, 170, 90))
        m.ellipsoid((0, 0, 6), (3.4, 1.6, 3.4), (230, 190, 80), spec=1.0)
        m.ellipsoid((0, -0.8, 6), (2, 1.2, 2), (220, 40, 60), glow=True)
    elif key == "gem":
        m.capsule((0, 0, 1), (0, 0, 11), 4, (110, 200, 250), r1=0.6, spec=1.0, glow=True)
    elif key == "shield":
        m.cylinder((0, -0.8, 6), (0, 0.8, 6), 6.5, (140, 100, 60), stripe=(0, 2.0))
        m.cylinder((0, -1.1, 6), (0, -0.6, 6), 2.2, (200, 200, 210), spec=1.0)
        for k in range(10):
            a = k / 10 * 2 * math.pi
            m.ellipsoid((math.cos(a) * 6, -0.8, 6 + math.sin(a) * 6), (0.9,) * 3, (180, 180, 190), spec=0.8)
    elif key == "sword":
        m.box((0, 0, 9), (0.6, 1.4, 7), (150, 210, 240), spec=1.0, glow=True)
        m.box((0, 0, 1.8), (0.9, 4.2, 0.8), (220, 180, 70), spec=0.9)
        m.capsule((0, 0, -3), (0, 0, 1), 0.9, (90, 60, 40))
        m.ellipsoid((0, 0, -3.6), (1.3,) * 3, (220, 180, 70), spec=0.9)
    elif key == "cloak":
        m.ellipsoid((0, 0, 6), (4.6, 2.4, 7), (50, 50, 70), tex=0.3)
        m.ellipsoid((0, -1, 11.6), (3, 2, 2), (60, 60, 82))
        m.ellipsoid((0, -2.2, 10.4), (1, 0.6, 1), (230, 190, 80), spec=1)
    elif key == "shard":
        for k, (dx, h) in enumerate(((0, 13), (-3, 8), (3, 9))):
            m.capsule((dx, 0, 0), (dx * 1.3, 0, h), 2.2 - k * 0.4, (250, 220, 140), r1=0.3, spec=1.0, glow=True)
    else:
        m.ellipsoid((0, 0, 5), (4, 4, 4), (200, 200, 200))
    m.compile()
    return m


def render_icon(key, rarity="common", size=48):
    m = build_icon(key)
    P = m.P
    ext = max(float(np.ptp(P[:, 0])), float(np.ptp(P[:, 1])), float(np.ptp(P[:, 2])))
    f = 30.0 / max(ext, 1)
    zc = float(P[:, 2].max() + P[:, 2].min()) / 2
    im, _ = render(m, {"_scale": (f, f, f)}, 35, (128, 128), (64, int(64 + zc * f * 0.8)), shadow=False)
    bb = im.getbbox()
    if bb:
        im = im.crop(bb)
    inner = size - 10
    if im.width > inner or im.height > inner:
        sc = min(inner / im.width, inner / im.height)
        im = im.resize((max(1, round(im.width * sc)), max(1, round(im.height * sc))), Image.LANCZOS)
    rc = np.array(RARITY.get(rarity, RARITY["common"]), np.float32)
    yy, xx = np.mgrid[0:size, 0:size]
    r = np.hypot(xx - size / 2, yy - size / 2) / (size / 2)
    out = np.zeros((size, size, 3), np.float32)
    out[:] = (np.clip(50 - r * 30, 12, 60)[..., None] * np.array([0.9, 0.85, 1.05]))
    # ritkaság-glória a tárgy körül
    pos = ((size - im.width) // 2, (size - im.height) // 2)
    full = Image.new("L", (size, size), 0)
    full.paste(im.split()[3], pos)
    g = np.asarray(full.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.GaussianBlur(3)), np.float32) / 255 * 0.55
    out = out * (1 - g[..., None]) + rc * g[..., None]
    bg = Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), "RGB").convert("RGBA")
    bg.alpha_composite(im, pos)
    d = ImageDraw.Draw(bg)
    rc = tuple(int(c) for c in rc)
    d.rectangle((0, 0, size - 1, size - 1), outline=(10, 8, 6))
    d.rectangle((1, 1, size - 2, size - 2), outline=rc)
    d.line((2, 2, size - 3, 2), fill=tuple(min(255, c + 60) for c in rc))
    d.rectangle((2, 3, size - 3, size - 3), outline=tuple(c // 2 for c in rc))
    return bg
