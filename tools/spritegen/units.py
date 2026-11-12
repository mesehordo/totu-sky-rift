"""Egységmodellek (csontváz + primitívek) és animációs pózok – 2. változat.

Rig-típusok: biped, rider, quad, theropod, flyer, insect, serpent, artillery,
horror, floater, puffball. Ismert egységazonosítóhoz saját, részletes modell
készül; ismeretlenhez (pl. mod egységéhez) a definíció "shape" mezője alapján
egy általános modell.
"""
import math

import numpy as np

from .engine import Model

# --- paletták -------------------------------------------------------------
ELF = dict(emblem="sun", skin=(152, 160, 176), skin2=(120, 128, 146), cloth=(206, 194, 160), jade=(40, 196, 156),
           gold=(232, 186, 64), obsidian=(44, 40, 56), feather=(52, 168, 210), feather2=(226, 92, 52),
           leather=(128, 92, 60), hair=(30, 30, 40), lizard=(92, 132, 82), lizard2=(156, 176, 112),
           metal=(190, 196, 206), wood=(120, 84, 50))
URS = dict(emblem="paw", fur=(146, 98, 58), fur2=(200, 158, 106), snout=(214, 186, 140), metal=(156, 162, 172),
           wood=(118, 80, 46), cloth=(176, 152, 110), bone=(232, 226, 206), leather=(96, 64, 38),
           wool=(112, 76, 46), tiger=(210, 142, 58), skin=(146, 98, 58), hair=(60, 40, 26))
CRO = dict(emblem="eye", shell=(112, 66, 138), shell2=(70, 40, 92), flesh=(204, 122, 162), glow=(150, 255, 120),
           bone=(228, 216, 186), eye=(255, 230, 90))
WUL = dict(emblem="moon", skin=(198, 146, 82), skin2=(236, 214, 170), fur=(198, 146, 82), hair=(70, 46, 30), leather=(150, 104, 62),
           cloth=(186, 160, 120), red=(196, 52, 36), turq=(54, 176, 170), feather=(236, 236, 226),
           feather2=(40, 36, 36), wood=(126, 88, 52), bone=(232, 224, 200), stone=(120, 116, 112),
           bison=(86, 58, 38), bison2=(126, 92, 60), metal=(150, 140, 130), gold=(200, 160, 70),
           emu=(170, 146, 116), emu2=(126, 106, 88), emuneck=(126, 166, 196), ochre=(190, 88, 46), ochre2=(214, 150, 72),
           white=(236, 230, 214), termite=(184, 102, 60), termite2=(146, 78, 46))
MYC = dict(emblem="spore", stalk=(226, 214, 186), stalk2=(190, 176, 146), cap=(176, 72, 58), cap2=(214, 196, 160),
           spot=(244, 236, 214), gill=(140, 104, 92), glow=(80, 230, 205), purple=(140, 96, 170),
           wood=(110, 84, 60), leather=(110, 84, 60), skin=(226, 214, 186), cloth=(160, 140, 120),
           metal=(150, 150, 140), hair=(90, 70, 60))
IMP = dict(emblem="eagle", metal=(166, 156, 140), metal2=(112, 104, 96), bronze=(186, 140, 72), iron=(84, 84, 92),
           red=(170, 40, 34), soul=(120, 255, 190), leather=(98, 70, 44), wood=(110, 80, 50),
           skin=(166, 156, 140), cloth=(170, 40, 34), hair=(60, 60, 60), gold=(220, 176, 70))


# ============================================================================
# BIPED – közös testépítő
def biped(name, s, pal, species="elf", weapon=None, offhand=None, head=None, armor=None, bulky=1.0,
          robe=False, cape=False, tail=False, staff_glow=None, gear=()):
    m = Model(name)
    w = bulky
    hip, sh = 10 * s, 18.5 * s
    mech = species in ("mech", "ghost")
    m.bone("torso", "root", (0, 0, hip))
    m.bone("head", "torso", (0, 0, sh + 1 * s))
    m.bone("arm_r", "torso", (0, 5 * s * w, sh - 0.5 * s))
    m.bone("arm_l", "torso", (0, -5 * s * w, sh - 0.5 * s))
    m.bone("leg_r", "root", (0, 2.3 * s * w, hip))
    m.bone("leg_l", "root", (0, -2.3 * s * w, hip))
    skin = pal["skin"]
    fur = "fur" if species in ("bear", "dingo") else None
    # ---- lábak
    for side, b in ((1, "leg_r"), (-1, "leg_l")):
        y = side * 2.3 * s * w
        if species == "ghost":
            continue
        if mech:
            m.capsule((0, y, hip), (0.9 * s, y, 5.4 * s), 1.5 * s * w, pal["metal2"], b, spec=0.5)
            m.ellipsoid((0.9 * s, y, 5.4 * s), (1.3 * s, 1.3 * s, 1.3 * s), pal["bronze"], b, spec=0.7)
            m.capsule((0.9 * s, y, 5.4 * s), (0, y, 1.4 * s), 1.1 * s * w, pal["iron"], b, spec=0.5)
            m.capsule((-0.6 * s, y, 8.5 * s), (-0.8 * s, y, 3 * s), 0.45 * s, pal["bronze"], b, spec=0.8)
            m.box((0.8 * s, y, 0.7 * s), (2.2 * s, 1.3 * s * w, 0.7 * s), pal["metal2"], b, spec=0.4)
        elif species == "mushroom":
            m.capsule((0, y, hip), (0.3 * s, y * 1.15, 1.4 * s), 1.9 * s * w, pal["stalk2"], b, r1=1.5 * s, tex=0.1)
            for k in range(3):
                a = (k - 1) * 0.7
                m.capsule((0.3 * s, y * 1.15, 1.2 * s), (0.3 * s + math.cos(a) * 2.4 * s, y * 1.15 + math.sin(a) * 1.4 * s,
                          0.3 * s), 0.6 * s, pal["stalk2"], b, r1=0.25 * s)
        else:
            leg_col = pal.get("leather", skin) if species == "elf" else (pal.get("leather", skin) if species == "dingo"
                                                                         else skin)
            m.capsule((0, y, hip), (0.9 * s, y, 5.4 * s), 1.6 * s * w, skin if fur else leg_col, b, r1=1.35 * s * w,
                      mat=fur)
            m.capsule((0.9 * s, y, 5.4 * s), (0.1 * s, y, 1.4 * s), 1.3 * s * w, leg_col, b, r1=1.1 * s * w, mat=fur)
            boot = pal.get("leather", (70, 50, 36))
            m.ellipsoid((0.9 * s, y, 0.9 * s), (2.0 * s, 1.25 * s * w, 0.95 * s), shade(boot, 0.75), b)
            if species == "elf":  # jáde lábvért-szalag
                m.ellipsoid((0.95 * s, y, 4.2 * s), (1.2 * s, 1.35 * s * w, 0.5 * s), pal["jade"], b, spec=0.4)
    # ---- törzs
    if robe:
        m.capsule((0, 0, hip + 6 * s), (0, 0, 1.0 * s), 3.8 * s * w, pal["cloth"], "torso", r1=5.6 * s * w,
                  team=True, tex=0.1)
        m.capsule((0.2 * s, 0, hip + 6 * s), (0.6 * s, 0, 1.0 * s), 1.0 * s, shade(pal["cloth"], 0.7), "torso",
                  r1=1.6 * s, tex=0.05)
    if species == "ghost":
        m.capsule((0, 0, hip + 3 * s), (-2 * s, 0, 1.5 * s), 3.6 * s * w, pal["soul"], "torso", r1=0.6 * s, glow=True)
    if species == "mushroom":
        m.capsule((0, 0, hip - 1 * s), (0, 0, sh + 1 * s), 3.6 * s * w, pal["stalk"], "torso", r1=3.0 * s * w, tex=0.1)
        for k in range(5):  # micélium-szálak és apró gombák a testen
            a = k * 1.3
            m.capsule((math.cos(a) * 3.2 * s * w, math.sin(a) * 3.2 * s * w, hip + k * 1.4 * s),
                      (math.cos(a + 0.4) * 3.4 * s * w, math.sin(a + 0.4) * 3.4 * s * w, hip + k * 1.4 * s + 2 * s),
                      0.22 * s, pal["stalk2"], "torso", tex=0)
        for k in range(3):
            a = 2.0 + k * 1.1
            m.ellipsoid((math.cos(a) * 3.3 * s, math.sin(a) * 3.3 * s, hip + 4 * s + k * 1.6 * s), (0.7 * s, 0.7 * s, 0.35 * s),
                        pal["cap"], "torso", spec=0.3)
        m.ellipsoid((0, 0, hip + 1 * s), (3.9 * s * w, 4.1 * s * w, 2.0 * s), pal["stalk2"], "torso", tex=0.15)
    elif mech:
        m.box((0, 0, hip + 5.5 * s), (2.8 * s * w, 3.9 * s * w, 4.2 * s), pal["metal"], "torso", spec=0.6, mat="rivets")
        m.box((1.0 * s, 0, hip + 6.2 * s), (2.2 * s * w, 3.2 * s * w, 3.0 * s), pal["bronze"], "torso", spec=0.8)
        m.ellipsoid((3.0 * s, 0, hip + 6.6 * s), (0.7 * s, 1.0 * s, 1.0 * s), pal["soul"], "torso", glow=True)
        m.box((-0.2 * s, 0, hip + 1.2 * s), (2.4 * s * w, 3.4 * s * w, 1.4 * s), pal["iron"], "torso", spec=0.4)
        for k in range(4):  # pteruges – bőrszalag-szoknya
            y = (k - 1.5) * 1.7 * s * w
            m.box((1.6 * s, y, hip - 0.8 * s), (0.5 * s, 0.75 * s, 1.6 * s), pal["red"], "torso", team=True)
        m.cylinder((-2.8 * s, 1.4 * s, hip + 5 * s), (-2.8 * s, 1.4 * s, hip + 9 * s), 0.8 * s, pal["iron"], "torso",
                   spec=0.4)
        m.cylinder((-2.8 * s, -1.4 * s, hip + 5 * s), (-2.8 * s, -1.4 * s, hip + 9 * s), 0.8 * s, pal["iron"], "torso",
                   spec=0.4)
    elif species != "ghost":
        m.ellipsoid((0, 0, hip + 5.6 * s), (3.2 * s * w, 4.5 * s * w, 4.6 * s), skin, "torso", mat=fur)
        m.ellipsoid((0.3 * s, 0, hip + 2.2 * s), (2.7 * s * w, 3.6 * s * w, 2.6 * s), skin, "torso", mat=fur)
        if species == "dingo":
            m.ellipsoid((1.4 * s, 0, hip + 5.2 * s), (2.0 * s * w, 2.8 * s * w, 3.4 * s), pal["skin2"], "torso", mat="fur")
        if species == "bear":
            m.ellipsoid((1.3 * s, 0, hip + 4 * s), (2.4 * s * w, 3.4 * s * w, 3.3 * s), pal["fur2"], "torso", mat="fur")
    # páncélzat / ruházat
    if armor == "metal" and not mech:
        m.ellipsoid((0.6 * s, 0, hip + 6 * s), (3.0 * s * w, 4.3 * s * w, 3.8 * s), pal["metal"], "torso", spec=0.8)
        for side in (-1, 1):
            m.ellipsoid((0, side * 4.6 * s * w, sh - 0.2 * s), (2.2 * s, 1.9 * s, 1.5 * s), pal["metal"], "torso",
                        spec=0.8)
    elif armor == "leather":
        m.ellipsoid((0.5 * s, 0, hip + 5.4 * s), (3.0 * s * w, 4.4 * s * w, 3.6 * s), pal["leather"], "torso", tex=0.12)
    elif armor == "fur":
        m.ellipsoid((-0.4 * s, 0, sh - 0.8 * s), (3.2 * s * w, 5.4 * s * w, 2.0 * s), pal.get("fur2", skin), "torso",
                    mat="fur")
    elif armor == "jade":
        m.ellipsoid((1.8 * s, 0, hip + 6.6 * s), (1.4 * s, 3.8 * s * w, 1.3 * s), pal["jade"], "torso", spec=0.5)
        m.ellipsoid((2.3 * s, 0, hip + 6.8 * s), (0.7 * s, 1.1 * s, 0.7 * s), pal["gold"], "torso", spec=0.9)
    if not mech and species not in ("mushroom", "ghost"):
        # öv csattal + ágyékkötő (csapatszín)
        m.ellipsoid((0.2 * s, 0, hip + 0.6 * s), (3.0 * s * w, 3.9 * s * w, 0.8 * s), pal.get("leather", (90, 60, 40)),
                    "torso")
        m.box((3.0 * s, 0, hip + 0.6 * s), (0.4 * s, 0.9 * s, 0.6 * s), pal.get("gold", pal.get("metal")), "torso",
              spec=0.8)
        if not robe:
            m.box((2.2 * s, 0, hip - 1.6 * s), (0.6 * s, 1.6 * s * w, 2.0 * s), (200, 200, 200), "torso", team=True)
            m.box((2.5 * s, 0, hip - 3.2 * s), (0.4 * s, 1.7 * s * w, 0.35 * s), pal.get("gold", (200, 170, 80)), "torso",
                  spec=0.7)  # díszszegély
            m.box((-2.0 * s, 0, hip - 1.4 * s), (0.6 * s, 1.8 * s * w, 1.8 * s), (200, 200, 200), "torso", team=True)
        for side in (-1, 1):  # erszények az övön
            m.ellipsoid((1.4 * s, side * 3.0 * s * w, hip + 0.1 * s), (0.9 * s, 0.7 * s, 1.0 * s),
                        shade(pal.get("leather", (110, 80, 50)), 0.9), "torso", tex=0.12)
        # vállszíj (átlósan a mellkason)
        m.capsule((1.6 * s, 3.6 * s * w, hip + 7.6 * s), (2.0 * s, -3.4 * s * w, hip + 1.4 * s), 0.45 * s,
                  shade(pal.get("leather", (110, 80, 50)), 0.75), "torso")
    if cape:
        m.ellipsoid((-2.6 * s, 0, hip + 3 * s), (0.9 * s, 4.6 * s * w, 7.4 * s), (200, 200, 200), "torso", team=True,
                    tex=0.1)
    if tail:
        m.bone("tail", "torso", (-2.6 * s, 0, hip + 0.8 * s))
        m.capsule((-2.6 * s, 0, hip + 0.8 * s), (-7 * s, 0, hip - 3 * s), 1.4 * s, pal["fur"], "tail", r1=0.8 * s,
                  mat="fur")
        m.ellipsoid((-7.2 * s, 0, hip - 3.3 * s), (1.0 * s, 0.9 * s, 0.9 * s), pal["skin2"], "tail")
    # ---- fej
    hz = sh + 3.8 * s
    if species == "bear":
        m.ellipsoid((2.2 * s, 0, hz - 2.4 * s), (1.6 * s, 2.0 * s, 1.6 * s), pal.get("fur2", skin), "head", mat="fur")  # szakáll
        for side in (-1, 1):
            m.capsule((2.6 * s, side * 0.8 * s, hz - 3.2 * s), (2.8 * s, side * 0.9 * s, hz - 4.6 * s), 0.45 * s,
                      pal.get("fur2", skin), "head", r1=0.25 * s)  # fonott szakállvég
            m.ellipsoid((2.8 * s, side * 0.9 * s, hz - 4.0 * s), (0.4 * s,) * 3, pal.get("metal", (160, 160, 170)),
                        "head", spec=0.9, tex=0)
        m.ellipsoid((0.3 * s, 0, hz), (3.2 * s, 3.4 * s, 3.2 * s), skin, "head", mat="fur")
        m.ellipsoid((3.0 * s, 0, hz - 0.9 * s), (1.9 * s, 1.6 * s, 1.4 * s), pal["snout"], "head")
        m.ellipsoid((4.7 * s, 0, hz - 0.5 * s), (0.6 * s, 0.8 * s, 0.6 * s), (28, 20, 20), "head", spec=0.6, tex=0)
        for side in (-1, 1):
            m.ellipsoid((-0.4 * s, side * 2.6 * s, hz + 2.5 * s), (1.0 * s, 0.8 * s, 1.0 * s), skin, "head")
            m.ellipsoid((2.2 * s, side * 1.3 * s, hz + 0.7 * s), (0.45 * s, 0.4 * s, 0.4 * s), (15, 10, 8), "head",
                        tex=0, spec=0.5)
    elif species == "dingo":
        for k in range(7):  # türkiz-csont gyöngysor
            a = math.radians(-70 + k * 23)
            m.ellipsoid((1.6 * s + math.cos(a) * 0.6 * s, math.sin(a) * 2.4 * s, sh + 0.6 * s - abs(math.sin(a)) * 0.8 * s),
                        (0.45 * s,) * 3, pal["turq"] if k % 2 else pal["bone"], "torso", spec=0.5, tex=0)
        m.ellipsoid((0.2 * s, 0, hz), (2.6 * s, 2.5 * s, 2.6 * s), skin, "head", mat="fur")
        m.capsule((1.6 * s, 0, hz - 0.4 * s), (5.6 * s, 0, hz - 1.0 * s), 1.4 * s, skin, "head", r1=0.75 * s)
        m.ellipsoid((3.6 * s, 0, hz - 1.5 * s), (2.0 * s, 1.1 * s, 0.7 * s), pal["skin2"], "head")
        m.ellipsoid((5.9 * s, 0, hz - 0.9 * s), (0.55 * s, 0.6 * s, 0.5 * s), (24, 20, 20), "head", tex=0, spec=0.6)
        for side in (-1, 1):
            m.capsule((-0.2 * s, side * 1.6 * s, hz + 1.6 * s), (-0.8 * s, side * 2.2 * s, hz + 5.4 * s), 1.1 * s,
                      skin, "head", r1=0.2 * s)
            m.ellipsoid((-0.3 * s, side * 1.75 * s, hz + 3 * s), (0.4 * s, 0.3 * s, 1.1 * s), pal["skin2"], "head")
            m.ellipsoid((2.2 * s, side * 1.2 * s, hz + 0.4 * s), (0.45 * s, 0.4 * s, 0.4 * s), (235, 170, 40), "head",
                        tex=0, glow=True)
            m.capsule((1.4 * s, side * 1.9 * s, hz - 0.2 * s), (3.2 * s, side * 1.2 * s, hz - 0.6 * s), 0.3 * s,
                      pal["red"], "head", tex=0)  # harci festés
    elif species == "elf":
        m.ellipsoid((1.7 * s, 0, sh + 0.8 * s), (0.8 * s, 2.6 * s, 0.6 * s), pal["gold"], "torso", spec=0.9)  # nyakék
        m.ellipsoid((2.3 * s, 0, sh + 0.4 * s), (0.5 * s, 0.7 * s, 0.7 * s), pal["jade"], "torso", spec=0.6)
        m.ellipsoid((0.5 * s, 0, hz), (2.5 * s, 2.3 * s, 2.9 * s), skin, "head")
        m.ellipsoid((2.6 * s, 0, hz - 0.4 * s), (0.7 * s, 0.6 * s, 1.0 * s), skin, "head")
        m.ellipsoid((-0.3 * s, 0, hz + 1.2 * s), (2.3 * s, 2.4 * s, 2.0 * s), pal["hair"], "head", tex=0.1)
        for side in (-1, 1):
            m.capsule((0.2 * s, side * 2.1 * s, hz + 0.3 * s), (-1.8 * s, side * 3.8 * s, hz + 2.0 * s), 0.55 * s, skin,
                      "head", r1=0.15 * s)
            m.ellipsoid((2.4 * s, side * 0.95 * s, hz + 0.5 * s), (0.38 * s, 0.38 * s, 0.38 * s), (40, 200, 160), "head",
                        tex=0, glow=True)
            m.capsule((2.0 * s, side * 1.3 * s, hz - 0.4 * s), (1.0 * s, side * 2.0 * s, hz - 1.2 * s), 0.25 * s,
                      pal["jade"], "head", tex=0)
    elif species == "mushroom":
        m.ellipsoid((0, 0, sh + 2.0 * s), (5.6 * s * w, 5.6 * s * w, 1.8 * s), pal["gill"], "head", stripe=(0, 0.9 * s))
        m.ellipsoid((0, 0, sh + 3.2 * s), (6.0 * s * w, 6.0 * s * w, 3.2 * s), pal["cap"], "head", spec=0.25, tex=0.08)
        rx, rz = 6.0 * s * w, 3.2 * s
        for k in range(8):
            a = k * 2.4
            r = (1.0 + (k % 3) * 1.6) * s * w
            z = sh + 3.2 * s + rz * math.sqrt(max(0.0, 1 - (r / rx) ** 2))
            m.ellipsoid((math.cos(a) * r, math.sin(a) * r, z), (1.0 * s, 1.0 * s, 0.45 * s), pal["spot"], "head", tex=0)
        for side in (-1, 1):
            m.ellipsoid((2.9 * s, side * 1.2 * s, sh - 0.6 * s), (0.5 * s, 0.6 * s, 0.7 * s), (40, 28, 26), "torso",
                        tex=0)
            m.ellipsoid((3.1 * s, side * 1.2 * s, sh - 0.4 * s), (0.2 * s, 0.25 * s, 0.25 * s), pal["glow"], "torso",
                        glow=True)
    elif mech or species == "ghost":
        hc = pal["soul"] if species == "ghost" else pal["metal"]
        m.ellipsoid((0.4 * s, 0, hz), (2.6 * s, 2.4 * s, 2.8 * s), hc, "head", spec=0.8, glow=species == "ghost")
        m.box((1.8 * s, 0, hz - 1.4 * s), (1.0 * s, 1.8 * s, 1.0 * s), pal["bronze"] if species != "ghost" else hc,
              "head", spec=0.8, glow=species == "ghost")
        m.box((2.85 * s, 0, hz + 0.1 * s), (0.2 * s, 1.5 * s, 0.35 * s), pal["soul"], "head", glow=True)
        m.box((2.85 * s, 0, hz - 0.6 * s), (0.2 * s, 0.35 * s, 0.8 * s), pal["soul"], "head", glow=True)
    # fejdíszek
    if head == "feathers":
        m.capsule((0.2 * s, 0, hz + 2.2 * s), (0.4 * s, 0, hz + 2.8 * s), 1.6 * s, pal["gold"], "head", r1=1.1 * s,
                  spec=0.8)
        for i, a in enumerate((-70, -45, -20, 0, 20, 45, 70)):
            ang = math.radians(a)
            tip = (-3.5 * s, math.sin(ang) * 5.5 * s, hz + 7.2 * s - abs(a) / 70 * 2.4 * s)
            m.capsule((0, 0, hz + 3 * s), tip, 0.95 * s, (pal["feather"], pal["jade"], pal.get("feather2", pal["feather"]))[i % 3],
                      "head", r1=0.35 * s, team=(i == 3))
    elif head == "plume":
        for i, a in enumerate((-25, 0, 25)):
            ang = math.radians(a)
            m.capsule((-0.5 * s, 0, hz + 2.4 * s), (-3 * s, math.sin(ang) * 3 * s, hz + 5.2 * s), 0.8 * s,
                      pal["feather"] if i != 1 else pal["jade"], "head", r1=0.3 * s, team=(i == 1))
    elif head == "horns":
        m.ellipsoid((0.2 * s, 0, hz + 1.5 * s), (3.0 * s, 3.2 * s, 1.9 * s), pal["metal"], "head", spec=0.9)
        m.box((2.4 * s, 0, hz + 0.6 * s), (0.4 * s, 0.4 * s, 1.6 * s), pal["metal"], "head", spec=0.9)
        for side in (-1, 1):
            m.capsule((0, side * 2.8 * s, hz + 2 * s), (-0.5 * s, side * 4.6 * s, hz + 5.4 * s), 0.95 * s, pal["bone"],
                      "head", r1=0.25 * s)
    elif head == "hood":
        m.ellipsoid((-0.4 * s, 0, hz + 0.8 * s), (3.2 * s, 3.6 * s, 3.4 * s), pal["cloth"], "head", team=True, tex=0.1)
    elif head == "headband":  # wullof: fejpánt sastollal
        m.ellipsoid((0.2 * s, 0, hz + 0.9 * s), (2.7 * s, 2.6 * s, 0.6 * s), pal["red"], "head", team=True)
        for i in range(2):
            m.capsule((-1.6 * s, (i - 0.5) * 1.2 * s, hz + 1 * s), (-3.6 * s, (i - 0.5) * 2.2 * s, hz + 5 * s), 0.6 * s,
                      pal["feather"], "head", r1=0.25 * s)
            m.capsule((-3.4 * s, (i - 0.5) * 2.1 * s, hz + 4.6 * s), (-3.8 * s, (i - 0.5) * 2.3 * s, hz + 5.5 * s),
                      0.4 * s, pal["feather2"], "head", r1=0.15 * s)
    elif head == "emufeathers":  # wullof sámán: emutoll-fejpánt, fehér pöttyfestés az arcon
        m.ellipsoid((0.2 * s, 0, hz + 0.9 * s), (2.7 * s, 2.6 * s, 0.6 * s), pal["ochre"], "head", team=False)
        for i in range(7):
            a = math.radians(-75 + i * 25)
            m.capsule((-0.4 * s, math.sin(a) * 2.4 * s, hz + 1.4 * s),
                      (-1.6 * s, math.sin(a) * 3.6 * s, hz + 4.6 * s - abs(math.sin(a)) * 1.0 * s), 0.9 * s,
                      pal["emu"] if i % 2 else pal["emu2"], "head", r1=0.25 * s, mat="fur")
        for k in range(6):
            m.ellipsoid((2.0 * s + (k % 2) * 0.4 * s, (k // 2 - 1) * 1.2 * s, hz + 0.3 * s - (k % 2) * 0.7 * s),
                        (0.28 * s,) * 3, pal["white"], "head", tex=0)
    elif head == "warbonnet":  # wullof sámán: tollkorona
        m.ellipsoid((0.2 * s, 0, hz + 1.0 * s), (2.8 * s, 2.7 * s, 0.7 * s), pal["turq"], "head", team=True)
        for i in range(9):
            a = math.radians(-100 + i * 25)
            tip = (-1.5 * s + math.cos(a) * 0.5 * s, math.sin(a) * 5 * s, hz + 6 * s + math.cos(a) * 1.2 * s)
            m.capsule((0.0, math.sin(a) * 2.6 * s, hz + 1.4 * s), tip, 0.7 * s, pal["feather"], "head", r1=0.3 * s)
            m.ellipsoid(tip, (0.4 * s, 0.4 * s, 0.5 * s), pal["feather2"], "head")
    elif head == "crest":  # római taréj
        m.capsule((1.6 * s, 0, hz + 2.6 * s), (-2.4 * s, 0, hz + 2.6 * s), 1.1 * s, pal["red"], "head", team=True,
                  tex=0.15)
        for k in range(5):
            m.ellipsoid((1.4 * s - k * 0.9 * s, 0, hz + 3.6 * s), (0.5 * s, 0.6 * s, 0.8 * s), pal["red"], "head",
                        team=True)
    # ---- karok
    for side, b in ((1, "arm_r"), (-1, "arm_l")):
        y = side * 5 * s * w
        if species == "ghost":
            m.capsule((0, y, sh - 0.5 * s), (1.2 * s, y + side * 0.4 * s, sh - 6.8 * s), 1.1 * s * w, pal["soul"], b,
                      r1=0.7 * s, glow=True)
            continue
        acol = pal["metal2"] if mech else skin
        m.capsule((0, y, sh - 0.5 * s), (0.5 * s, y + side * 0.3 * s, sh - 3.8 * s), 1.4 * s * w, acol, b, r1=1.2 * s * w,
                  mat=fur, spec=0.5 if mech else 0)
        m.capsule((0.5 * s, y + side * 0.3 * s, sh - 3.8 * s), (1.0 * s, y + side * 0.5 * s, sh - 7.0 * s), 1.2 * s * w,
                  acol, b, r1=1.0 * s * w, mat=fur, spec=0.5 if mech else 0)
        if mech:
            m.ellipsoid((0, y, sh - 0.2 * s), (2.0 * s, 1.8 * s, 1.6 * s), pal["bronze"], b, spec=0.9)
            m.ellipsoid((0.5 * s, y + side * 0.3 * s, sh - 3.8 * s), (1.0 * s, 1.0 * s, 1.0 * s), pal["iron"], b,
                        spec=0.6)
        elif species in ("elf", "dingo"):
            m.ellipsoid((0.8 * s, y + side * 0.4 * s, sh - 5.6 * s), (1.2 * s, 1.25 * s * w, 0.9 * s),
                        pal.get("gold", pal["leather"]) if species == "elf" else pal["leather"], b, spec=0.6)
        m.ellipsoid((1.1 * s, y + side * 0.5 * s, sh - 7.6 * s), (1.2 * s, 1.1 * s, 1.1 * s),
                    pal["iron"] if mech else skin, b, spec=0.4 if mech else 0)
    hand_r = (1.1 * s, 5.5 * s * w, sh - 7.6 * s)
    hand_l = (1.1 * s, -5.5 * s * w, sh - 7.6 * s)
    _weapon(m, s, w, sh, pal, weapon, hand_r, staff_glow)
    _offhand(m, s, w, sh, pal, offhand, hand_l)
    _gear(m, s, w, hip, sh, hz, pal, gear, species)
    m.rig = "biped"
    m.attack_style = {"bow": "bow", "staff": "cast", "spear": "throw", "crossbow": "bow", "tube": "bow",
                      "boomerang": "throw", "woomera": "throw",
                      "soulstaff": "cast"}.get(weapon, "melee")
    m.height = hz + 4 * s
    return m


def shade(c, f):
    return tuple(max(0, min(255, int(v * f))) for v in c)


def _gear(m, s, w, hip, sh, hz, pal, gear, species):
    """Sziluettet adó felszerelés: háti tárgyak, vállvért, palást, nyaklánc, festés.
    A test eleje +X, a háta −X; a jobb kéz +Y oldalon."""
    leather = pal.get("leather", (96, 64, 38))
    wood = pal.get("wood", (118, 80, 46))
    bone = pal.get("bone", (232, 226, 206))
    metal = pal.get("metal", (156, 162, 172))
    bx = -3.6 * s * w  # a hát felülete (a bunda fölött)
    for g in gear:
        if g == "quiver":  # tegez nyilakkal, ferdén a háton
            m.capsule((bx - 0.6 * s, 2.0 * s, hip + 1.5 * s), (bx - 1.2 * s, -1.6 * s, sh + 3.0 * s), 1.15 * s, leather,
                      "torso", r1=1.0 * s, tex=0.12)
            for k in range(4):
                o = (k - 1.5) * 0.55 * s
                m.capsule((bx - 1.2 * s, -1.6 * s + o, sh + 3.0 * s), (bx - 1.6 * s, -2.4 * s + o, sh + 5.6 * s), 0.18 * s,
                          wood, "torso", tex=0)
                m.capsule((bx - 1.6 * s, -2.4 * s + o, sh + 5.2 * s), (bx - 1.7 * s, -2.6 * s + o, sh + 6.4 * s), 0.45 * s,
                          pal.get("feather", (230, 230, 220)), "torso", r1=0.15 * s, tex=0)
        elif g == "harpoons":  # szigonyköteg kötéllel
            for k in range(3):
                o = (k - 1) * 0.9 * s
                p0 = (bx - 0.8 * s, 2.6 * s + o, hip - 1.0 * s)
                p1 = (bx - 1.8 * s, -2.8 * s + o, sh + 10.5 * s)
                m.capsule(p0, p1, 0.4 * s, wood, "torso", tex=0.05)
                m.capsule(p1, (bx - 2.1 * s, -3.4 * s + o, sh + 13.0 * s), 0.85 * s, bone, "torso", r1=0.1 * s, spec=0.3)
                for side in (-1, 1):  # szakáll (visszahorog)
                    m.capsule((bx - 1.9 * s, -3.0 * s + o, sh + 11.4 * s),
                              (bx - 1.9 * s, -3.0 * s + o + side * 0.9 * s, sh + 10.4 * s), 0.3 * s, bone, "torso",
                              r1=0.08 * s)
            m.capsule((bx - 0.6 * s, 3.0 * s, sh - 1 * s), (bx - 0.6 * s, -3.0 * s, sh - 1 * s), 0.35 * s, leather, "torso")
        elif g == "basket":  # háti kosár fával és kővel
            c = (bx - 2.4 * s, 0, hip + 6.0 * s)
            m.capsule((c[0], 0, c[2] - 3.4 * s), (c[0] - 0.6 * s, 0, c[2] + 3.4 * s), 2.6 * s, shade(wood, 1.35), "torso",
                      r1=3.3 * s, ends=False, mat="thatch")
            m.capsule((c[0], 0, c[2] - 3.4 * s), (c[0], 0, c[2] - 3.0 * s), 2.7 * s, wood, "torso", ends=False)
            for k in range(3):
                m.capsule((c[0] - 0.6 * s, (k - 1) * 1.5 * s, c[2] + 3.0 * s),
                          (c[0] - 1.4 * s, (k - 1) * 2.0 * s, c[2] + 7.0 * s), 0.75 * s, shade(wood, 0.9), "torso",
                          tex=0.1, mat="planks")
            m.ellipsoid((c[0] - 0.3 * s, 1.6 * s, c[2] + 3.6 * s), (1.4 * s, 1.4 * s, 1.1 * s), (150, 148, 140), "torso",
                        mat="stones")
        elif g == "axe_back":  # tartalék fejsze a háton
            m.capsule((bx - 0.7 * s, -2.4 * s, hip), (bx - 0.9 * s, 2.4 * s, sh + 4 * s), 0.35 * s, wood, "torso")
            m.box((bx - 0.9 * s, 2.8 * s, sh + 3.4 * s), (0.25 * s, 1.0 * s, 1.5 * s), metal, "torso", spec=0.8)
        elif g == "pelt":  # állatbőr-palást a vállon, fejjel
            fc = pal.get("fur2", pal.get("bison2", (150, 110, 70)))
            m.ellipsoid((bx + 0.6 * s, 0, sh - 2.2 * s), (1.3 * s, 5.6 * s * w, 5.0 * s), shade(fc, 0.8), "torso",
                        mat="fur")
            m.ellipsoid((bx + 1.2 * s, 0, sh + 1.6 * s), (1.8 * s, 4.6 * s * w, 1.6 * s), fc, "torso", mat="fur")
            m.ellipsoid((bx + 0.2 * s, 0, sh + 2.8 * s), (1.6 * s, 1.5 * s, 1.2 * s), shade(fc, 0.9), "torso", mat="fur")
            for side in (-1, 1):
                m.ellipsoid((bx + 0.6 * s, side * 1.1 * s, sh + 3.8 * s), (0.5 * s, 0.4 * s, 0.6 * s), shade(fc, 0.85),
                            "torso")
        elif g == "pauldron":  # nagy vállvért a bal (pajzsos) vállon
            col = metal if species in ("mech", "elf") else bone
            m.ellipsoid((0, -5.6 * s * w, sh + 0.9 * s), (3.0 * s, 2.7 * s, 1.9 * s), col, "arm_l",
                        spec=0.7 if col is metal else 0.2, mat="scales" if col is bone else None)
            for k in range(3):
                m.capsule((-0.6 * s + k * 0.9 * s, -6.6 * s * w, sh + 1.0 * s), (-0.4 * s + k * 1.1 * s, -7.4 * s * w,
                          sh + 2.6 * s), 0.45 * s, shade(col, 0.9), "arm_l", r1=0.1 * s)
        elif g == "necklace":  # fog- és csontnyaklánc
            for k in range(9):
                a = math.radians(-80 + k * 20)
                p = (1.9 * s + math.cos(a) * 0.9 * s, math.sin(a) * 3.0 * s * w, sh + 0.4 * s - abs(math.sin(a)) * 1.2 * s)
                m.capsule(p, (p[0] + 0.3 * s, p[1], p[2] - 0.9 * s), 0.32 * s, bone, "torso", r1=0.1 * s, tex=0)
        elif g == "paint":  # harci festés: csíkok a mellkason és a karon
            pc = pal.get("paint", (60, 110, 200))
            for k in range(3):
                z = hip + 4.5 * s + k * 1.3 * s
                m.capsule((3.0 * s * w, -2.6 * s * w, z), (3.1 * s * w, 2.6 * s * w, z - 0.8 * s), 0.32 * s, pc, "torso",
                          tex=0)
            for side, b in ((1, "arm_r"), (-1, "arm_l")):
                m.capsule((0.9 * s, side * 6.3 * s * w, sh - 2.5 * s), (1.2 * s, side * 6.4 * s * w, sh - 4.2 * s),
                          0.35 * s, pc, b, tex=0)
        elif g == "runes":  # izzó rúnakövek az övről lógva
            for k in range(4):
                a = math.radians(-60 + k * 40)
                p = (math.cos(a) * 3.4 * s * w, math.sin(a) * 3.6 * s * w, hip - 0.6 * s)
                m.capsule((p[0], p[1], hip + 0.4 * s), p, 0.12 * s, leather, "torso", tex=0)
                m.box((p[0], p[1], p[2] - 0.7 * s), (0.5 * s, 0.5 * s, 0.7 * s), (120, 120, 128), "torso")
                m.box((p[0] + 0.45 * s, p[1], p[2] - 0.7 * s), (0.1 * s, 0.25 * s, 0.4 * s),
                      pal.get("rune", (120, 200, 255)), "torso", glow=True)
        elif g == "totem_back":  # kis totem / rúnabot a háton, a fej fölé nyúlva
            m.capsule((bx - 1.0 * s, 0, hip - 1 * s), (bx - 1.6 * s, 0, hz + 6 * s), 0.45 * s, wood, "torso")
            for k, col in enumerate((pal.get("red", (180, 60, 40)), pal.get("turq", (60, 170, 170)), bone)):
                m.box((bx - 1.5 * s, 0, hz + 1.5 * s + k * 1.4 * s), (0.7 * s, 0.9 * s, 0.65 * s), col, "torso")
            for side in (-1, 1):
                m.capsule((bx - 1.6 * s, side * 0.6 * s, hz + 5.6 * s), (bx - 2.2 * s, side * 2.6 * s, hz + 7 * s),
                          0.4 * s, pal.get("feather", (236, 236, 226)), "torso", r1=0.15 * s)
        elif g == "banner_back":  # háti csapatzászló
            m.capsule((bx - 0.8 * s, 0, hip), (bx - 0.8 * s, 0, hz + 11 * s), 0.4 * s, wood, "torso")
            m.box((bx - 0.8 * s, 2.6 * s, hz + 7.5 * s), (0.25 * s, 2.6 * s, 3.6 * s), (200, 200, 200), "torso", team=True)
            m.emblem((bx - 0.5 * s, 2.6 * s, hz + 7.5 * s), 2.0 * s, 2.8 * s, 0, pal.get("emblem", "paw"),
                     (236, 232, 220), "torso", sign=1, tex=0.03)
        elif g == "skull":  # trófeakoponya az övön
            m.ellipsoid((2.0 * s, 3.4 * s * w, hip - 0.8 * s), (0.9 * s, 0.8 * s, 1.0 * s), bone, "torso", tex=0.05)
            for side in (-1, 1):
                m.ellipsoid((2.8 * s, 3.4 * s * w + side * 0.35 * s, hip - 0.6 * s), (0.2 * s,) * 3, (30, 24, 20),
                            "torso", tex=0)
        elif g == "spores":  # micélia spórazsákok a hátán
            for k in range(3):
                m.ellipsoid((bx - 0.8 * s, (k - 1) * 2.0 * s, hip + 4 * s + (k % 2) * 1.6 * s),
                            (1.2 * s, 1.1 * s, 1.4 * s), pal.get("purple", (140, 96, 170)), "torso", spec=0.3)
                m.ellipsoid((bx - 1.6 * s, (k - 1) * 2.0 * s, hip + 4.4 * s + (k % 2) * 1.6 * s), (0.35 * s,) * 3,
                            pal.get("glow", (80, 230, 205)), "torso", glow=True)
        elif g == "lantern":  # lélekfény-lámpás a háton (Vasbirodalom)
            iron = pal.get("iron", metal)
            m.capsule((bx - 0.8 * s, 0, hip), (bx - 1.4 * s, 0, hz + 5 * s), 0.35 * s, iron, "torso")
            m.capsule((bx - 1.4 * s, 0, hz + 5 * s), (bx - 0.2 * s, 0, hz + 6 * s), 0.3 * s, iron, "torso")
            m.ellipsoid((bx, 0, hz + 4.6 * s), (0.9 * s, 0.9 * s, 1.2 * s), pal.get("soul", (120, 255, 190)), "torso",
                        glow=True)


def _weapon(m, s, w, sh, pal, weapon, hand, glow):
    y = hand[1]
    if weapon == "club":  # obszidián macuahuitl
        m.capsule(hand, (9.5 * s, y, sh - 6.4 * s), 1.0 * s, pal["obsidian"], "arm_r", spec=0.4)
        for k in range(5):
            x = 3.8 * s + k * 1.3 * s
            for zz in (-1.15, 1.15):
                m.ellipsoid((x, y, sh - 7.0 * s + zz * s * 0.9), (0.45 * s, 0.35 * s, 0.6 * s), (220, 226, 236), "arm_r",
                            tex=0, spec=1.0)
    elif weapon == "axe":
        m.capsule(hand, (8.4 * s, y, sh - 6 * s), 0.7 * s, pal["wood"], "arm_r", mat="planks")
        m.ellipsoid((7.8 * s, y, sh - 4.3 * s), (1.8 * s, 0.45 * s, 2.4 * s), pal["metal"], "arm_r", spec=1.0)
    elif weapon == "twin_axe":
        for b, hy in (("arm_r", y), ("arm_l", -y)):
            h = (hand[0], hy, hand[2])
            m.capsule(h, (7.4 * s, hy, sh - 6 * s), 0.6 * s, pal["wood"], b)
            m.ellipsoid((7 * s, hy, sh - 4.6 * s), (1.6 * s, 0.4 * s, 2.1 * s), pal["metal"], b, spec=1.0)
    elif weapon == "bow":
        cx, cy, cz = 2.6 * s, -5.5 * s * w, sh - 7.6 * s
        pts = [(cx + 1.6 * s * math.cos(a), cy, cz + 6.6 * s * math.sin(a)) for a in
               [math.radians(t) for t in range(-80, 81, 20)]]
        for a, b2 in zip(pts, pts[1:]):
            m.capsule(a, b2, 0.55 * s, pal.get("leather", pal.get("wood")), "arm_l")
        m.capsule(pts[0], pts[-1], 0.15 * s, (230, 230, 220), "arm_l", tex=0)
        m.capsule((-2.4 * s, 2.0 * s, sh + 1 * s), (-2.4 * s, 2.8 * s, sh - 6 * s), 1.0 * s, pal.get("leather", (90, 60, 40)),
                  "torso")  # tegez
        for k in range(3):
            m.capsule((-2.4 * s, 2.0 * s + k * 0.5 * s, sh + 1 * s), (-2.6 * s, 2.0 * s + k * 0.5 * s, sh + 3 * s), 0.3 * s,
                      (230, 230, 230), "torso", tex=0)
    elif weapon == "boomerang":  # festett bumeráng a kézben (+ kettő az övben)
        bc, bc2 = pal.get("wood", (126, 88, 52)), pal.get("ochre", (190, 88, 46))
        el = (6.0 * s, y + 0.6 * s, sh - 6.2 * s)
        m.capsule(hand, el, 0.75 * s, bc, "arm_r", r1=0.6 * s)
        m.capsule(el, (8.8 * s, y - 1.6 * s, sh - 8.4 * s), 0.6 * s, bc, "arm_r", r1=0.45 * s)
        for k in range(3):  # pöttyfestés
            t = (k + 1) / 4
            m.ellipsoid((hand[0] + (el[0] - hand[0]) * t, hand[1] + (el[1] - hand[1]) * t + 0.3 * s,
                         hand[2] + (el[2] - hand[2]) * t + 0.5 * s), (0.3 * s,) * 3, pal.get("white", (236, 230, 214)),
                        "arm_r", tex=0)
        for k in range(2):  # tartalék bumerángok az övben
            yy = (1.6 + k * 1.3) * s
            m.capsule((1.8 * s, yy, 10 * s + 1.4 * s), (2.6 * s, yy, 10 * s - 1.2 * s), 0.45 * s, bc2, "torso")
            m.capsule((2.6 * s, yy, 10 * s - 1.2 * s), (3.6 * s, yy, 10 * s + 0.6 * s), 0.45 * s, bc2, "torso")
    elif weapon == "woomera":  # dárdavető: hosszú, vékony dárda a vetőfán
        m.capsule((-1 * s, y, sh - 7.6 * s), (4 * s, y, sh - 7.0 * s), 0.7 * s, pal.get("ochre", (190, 88, 46)), "arm_r",
                  r1=0.4 * s)  # a vetőfa
        m.capsule((-5 * s, y, sh - 6.6 * s), (14 * s, y, sh - 5.2 * s), 0.4 * s, pal["wood"], "arm_r")
        m.capsule((14 * s, y, sh - 5.2 * s), (16.4 * s, y, sh - 5.0 * s), 0.75 * s, pal["stone"], "arm_r", r1=0.1 * s)
        for k in range(2):  # tartalék dárdák a háton
            m.capsule((-3.4 * s, -1.4 * s + k * 0.9 * s, 6 * s), (-4.4 * s, 2.0 * s + k * 0.9 * s, sh + 9 * s), 0.3 * s,
                      pal["wood"], "torso")
    elif weapon == "horn":  # harci kürt (szarv), a szájhoz emelve
        m.capsule(hand, (2.6 * s, y * 0.5, sh + 1.6 * s), 0.7 * s, pal.get("bone", (232, 226, 206)), "arm_r", r1=1.2 * s)
        m.capsule((2.6 * s, y * 0.5, sh + 1.6 * s), (4.4 * s, y * 0.3, sh + 3.4 * s), 1.2 * s,
                  pal.get("bone", (232, 226, 206)), "arm_r", r1=1.6 * s)
        m.ellipsoid((1.4 * s, y * 0.8, sh - 2 * s), (0.5 * s, 0.6 * s, 0.4 * s), pal.get("metal", (160, 160, 170)),
                    "arm_r", spec=0.9)
    elif weapon == "lance":  # wullof kőhegyű lándzsa tollakkal
        m.capsule((-4 * s, y, sh - 8.5 * s), (12 * s, y, sh - 6.2 * s), 0.55 * s, pal["wood"], "arm_r")
        m.capsule((12 * s, y, sh - 6.2 * s), (15 * s, y, sh - 5.8 * s), 1.0 * s, pal["stone"], "arm_r", r1=0.1 * s)
        m.capsule((10.5 * s, y, sh - 6.4 * s), (10.0 * s, y, sh - 9 * s), 0.4 * s, pal["feather"], "arm_r", r1=0.15 * s)
        m.capsule((10.0 * s, y, sh - 6.4 * s), (9.4 * s, y, sh - 8.6 * s), 0.4 * s, pal["red"], "arm_r", r1=0.15 * s)
    elif weapon == "spear":
        m.capsule((-3 * s, y, sh - 8 * s), (11 * s, y, sh - 6 * s), 0.55 * s, pal["wood"], "arm_r")
        m.capsule((11 * s, y, sh - 6 * s), (14.5 * s, y, sh - 5.6 * s), 0.9 * s, pal["metal"], "arm_r", r1=0.1 * s,
                  spec=1.0)
    elif weapon in ("staff", "soulstaff"):
        m.capsule((1.1 * s, y, sh - 14 * s), (1.1 * s, y, sh + 4 * s), 0.6 * s, pal.get("wood", (100, 80, 60)), "arm_r")
        gc = glow or pal.get("jade", (200, 200, 120))
        if weapon == "soulstaff":
            m.box((1.1 * s, y, sh + 5 * s), (1.2 * s, 1.2 * s, 1.6 * s), pal["bronze"], "arm_r", spec=0.9)
        m.ellipsoid((1.1 * s, y, sh + 5.4 * s), (1.4 * s, 1.4 * s, 1.4 * s), gc, "arm_r", glow=True)
        for k in range(3):
            a = k * 2.1
            m.capsule((1.1 * s, y, sh + 3.6 * s), (1.1 * s + math.cos(a) * 1.8 * s, y + math.sin(a) * 1.8 * s,
                      sh + 6.8 * s), 0.25 * s, pal.get("gold", (200, 170, 80)), "arm_r", spec=0.8)
    elif weapon == "tool":
        m.capsule(hand, (6 * s, y, sh - 6 * s), 0.55 * s, (120, 90, 50), "arm_r")
        m.capsule((6 * s, y, sh - 4.2 * s), (6 * s, y, sh - 7.6 * s), 0.7 * s, (160, 160, 166), "arm_r", r1=0.25 * s,
                  spec=0.8)
        m.ellipsoid((-2.4 * s, 0, 15 * s), (1.9 * s, 2.8 * s, 2.8 * s), (150, 118, 76), "torso", mat="planks")  # puttony
    elif weapon == "gladius":
        m.box((4.6 * s, y, sh - 7.2 * s), (3.0 * s, 0.25 * s, 0.6 * s), pal["metal"], "arm_r", spec=1.0)
        m.box((1.6 * s, y, sh - 7.2 * s), (0.3 * s, 0.4 * s, 1.2 * s), pal["bronze"], "arm_r", spec=0.9)
    elif weapon == "crossbow":  # vállra szerelt íjpuska
        m.box((3.4 * s, y - 1.0 * s, sh - 6.4 * s), (3.4 * s, 0.6 * s, 0.6 * s), pal["wood"], "arm_r")
        m.capsule((6.4 * s, y - 4 * s, sh - 6.4 * s), (6.4 * s, y + 2 * s, sh - 6.4 * s), 0.45 * s, pal["iron"], "arm_r",
                  spec=0.8)
        m.box((0.4 * s, 4.8 * s * w, sh + 1.2 * s), (2.2 * s, 1.0 * s, 0.9 * s), pal["bronze"], "torso", spec=0.8)
    elif weapon == "tube":  # gomba spóracső
        m.capsule((-1 * s, y, sh - 7 * s), (8 * s, y, sh - 6 * s), 1.2 * s, pal["purple"], "arm_r", r1=0.9 * s)
        m.ellipsoid((8.2 * s, y, sh - 6 * s), (0.7 * s, 1.0 * s, 1.0 * s), pal["glow"], "arm_r", glow=True)
        m.ellipsoid((-1 * s, y, sh - 6 * s), (1.6 * s, 1.6 * s, 1.6 * s), pal["purple"], "arm_r")
    elif weapon == "mushclub":
        m.capsule(hand, (7 * s, y, sh - 5 * s), 0.9 * s, pal["wood"], "arm_r")
        m.ellipsoid((7.4 * s, y, sh - 4.6 * s), (2.0 * s, 1.6 * s, 1.6 * s), pal["stalk2"], "arm_r", tex=0.2)


def _offhand(m, s, w, sh, pal, offhand, hand):
    y = -6.7 * s * w
    if offhand == "round_shield":
        m.ellipsoid((2.3 * s, y, sh - 5 * s), (1.0 * s, 4.9 * s, 5.3 * s), pal["wood"], "arm_l", mat="planks")
        m.ellipsoid((2.9 * s, y, sh - 5 * s), (0.6 * s, 4.1 * s, 4.5 * s), (200, 200, 200), "arm_l", team=True)
        m.ellipsoid((3.4 * s, y, sh - 5 * s), (0.7 * s, 1.1 * s, 1.1 * s), pal["metal"], "arm_l", spec=1.0)
        m.ellipsoid((2.5 * s, y, sh - 5 * s), (1.0 * s, 5.0 * s, 0.4 * s), pal["metal"], "arm_l", spec=0.8)
        m.emblem((3.56 * s, y, sh - 5 * s), 3.1 * s, 3.3 * s, 0, pal.get("emblem", "paw"), (238, 234, 222), "arm_l",
                 tex=0.04)
    elif offhand == "plank_shield":  # ursan: nagy, lapos, négyszögletes deszkapajzs festett mancscímerrel
        zc = sh - 4.6 * s
        m.box((2.4 * s, y, zc), (0.55 * s, 4.2 * s, 6.4 * s), pal["wood"], "arm_l", stripe=(1, 1.7 * s), tex=0.22)
        for dz in (-3.9 * s, 3.9 * s):  # keresztlécek, kötözött szélek
            m.box((2.0 * s, y, zc + dz), (0.35 * s, 4.3 * s, 0.6 * s), shade(pal["wood"], 0.7), "arm_l", tex=0.15)
        for dy in (-4.1 * s, 4.1 * s):
            m.box((2.5 * s, y + dy, zc), (0.62 * s, 0.35 * s, 6.5 * s), shade(pal["wood"], 0.6), "arm_l", tex=0.15)
        m.emblem((2.98 * s, y, zc + 0.3 * s), 3.2 * s, 3.6 * s, 0, "paw", (200, 200, 200), "arm_l", team=True, tex=0.08)
    elif offhand == "parry_shield":  # wullof: keskeny, hosszú, festett hárítópajzs (okker-fehér csíkok)
        zc = sh - 5.0 * s
        m.ellipsoid((2.4 * s, y, zc), (0.8 * s, 1.9 * s, 6.4 * s), pal.get("ochre", (190, 88, 46)), "arm_l", tex=0.15)
        for k in range(5):
            m.ellipsoid((3.0 * s, y, zc - 4.0 * s + k * 2.0 * s), (0.4 * s, 1.3 * s, 0.35 * s),
                        pal.get("white", (236, 230, 214)) if k % 2 == 0 else (200, 200, 200), "arm_l", tex=0,
                        team=(k % 2 == 1))
    elif offhand == "scutum":
        m.box((2.6 * s, y, sh - 5.4 * s), (0.6 * s, 3.6 * s, 5.6 * s), (200, 200, 200), "arm_l", team=True)
        m.box((3.0 * s, y, sh - 5.4 * s), (0.3 * s, 3.7 * s, 0.5 * s), pal["bronze"], "arm_l", spec=0.9)
        m.ellipsoid((3.3 * s, y, sh - 5.4 * s), (0.6 * s, 1.2 * s, 1.2 * s), pal["bronze"], "arm_l", spec=1.0)
        m.emblem((3.25 * s, y, sh - 4.2 * s), 3.0 * s, 3.2 * s, 0, pal.get("emblem", "eagle"), (232, 196, 90), "arm_l",
                 spec=0.8)
    elif offhand == "cap_shield":  # gomba: kalap-pajzs
        m.ellipsoid((2.6 * s, y, sh - 4.6 * s), (1.4 * s, 4.8 * s, 4.8 * s), pal["cap"], "arm_l", spec=0.3)
        m.ellipsoid((3.4 * s, y, sh - 4.0 * s), (0.6 * s, 1.0 * s, 1.0 * s), pal["spot"], "arm_l", tex=0)
        m.ellipsoid((3.3 * s, y + 2 * s, sh - 6 * s), (0.6 * s, 0.8 * s, 0.8 * s), pal["spot"], "arm_l", tex=0)
        m.ellipsoid((2.2 * s, y, sh - 4.6 * s), (0.8 * s, 4.0 * s, 4.0 * s), (200, 200, 200), "arm_l", team=True)


# ============================================================================
def quadruped(m, s, kind, pal, length=1.0, height=1.0, saddle=True):
    """Négylábú: lizard (tollas gyík), tiger, bison, dog, wolf, mammoth. Csontok: head, leg_*."""
    L = 9 * s * length
    hb = 9 * s * height
    if kind == "ashwolf":
        kind = "dog"
        pal = dict(pal, _ash=True)
    body = {"lizard": pal.get("lizard"), "tiger": pal.get("tiger"), "bison": pal.get("bison2"), "dog": (196, 146, 82),
            "wolf": (150, 220, 230), "mammoth": pal.get("wool")}[kind]
    belly = {"lizard": pal.get("lizard2"), "tiger": (238, 214, 172), "bison": pal.get("bison"), "dog": (236, 214, 170),
             "wolf": (210, 245, 250), "mammoth": (90, 60, 36)}[kind]
    if pal.get("_ash"):
        body, belly = (96, 92, 90), (150, 144, 136)
    glow = kind == "wolf"
    mat = {"lizard": "scales", "tiger": "fur", "bison": "fur", "dog": "fur", "wolf": None, "mammoth": "fur"}[kind]
    m.bone("head", "root", (L * 0.9, 0, hb + 1 * s))
    for nm, x, y in (("leg_fl", L * 0.62, 2.8 * s), ("leg_fr", L * 0.62, -2.8 * s),
                     ("leg_bl", -L * 0.62, 2.8 * s), ("leg_br", -L * 0.62, -2.8 * s)):
        m.bone(nm, "root", (x, y, hb - 1 * s))
        th = 1.9 * s if kind in ("mammoth", "bison") else 1.5 * s
        knee = (x + (0.8 if nm[-2] == "f" else -0.9) * s, y, hb * 0.5)
        m.capsule((x, y, hb - 1 * s), knee, th * 1.2, body, nm, r1=th, mat=mat, glow=glow)
        m.capsule(knee, (x + 0.4 * s, y, 1.0 * s), th, belly if kind == "dog" else body, nm, r1=th * 0.75, mat=mat,
                  glow=glow)
        foot = (60, 48, 36) if kind not in ("bison", "mammoth") else (50, 40, 32)
        m.ellipsoid((x + 0.9 * s, y, 0.7 * s), (1.6 * s, th * 0.9, 0.8 * s), foot if not glow else body, nm, glow=glow)
    m.ellipsoid((0, 0, hb + 0.5 * s), (L, 4.2 * s, 4.0 * s * height), body, "root", mat=mat, glow=glow)
    m.ellipsoid((0, 0, hb - 1.6 * s), (L * 0.8, 3.4 * s, 2.4 * s), belly, "root", mat=mat, glow=glow)
    if kind == "bison":
        m.ellipsoid((L * 0.45, 0, hb + 3.4 * s), (L * 0.55, 4.6 * s, 4.4 * s), pal["bison"], "root", mat="fur")
        m.ellipsoid((L * 0.95, 0, hb + 0.2 * s), (3.4 * s, 3.6 * s, 4.2 * s), pal["bison"], "head", mat="fur")
        m.ellipsoid((L * 1.25, 0, hb - 0.6 * s), (2.2 * s, 2.2 * s, 2.6 * s), pal["bison"], "head", mat="fur")
        m.ellipsoid((L * 1.42, 0, hb - 1.8 * s), (1.2 * s, 1.6 * s, 1.0 * s), (40, 30, 24), "head")
        for side in (-1, 1):
            m.capsule((L * 1.15, side * 2.2 * s, hb + 1.4 * s), (L * 1.1, side * 4.2 * s, hb + 1.8 * s), 0.8 * s,
                      pal["bone"], "head", r1=0.5 * s)
            m.capsule((L * 1.1, side * 4.2 * s, hb + 1.8 * s), (L * 1.2, side * 4.4 * s, hb + 3.6 * s), 0.5 * s,
                      pal["bone"], "head", r1=0.12 * s)
            m.ellipsoid((L * 1.32, side * 1.6 * s, hb + 0.4 * s), (0.4 * s, 0.4 * s, 0.4 * s), (20, 14, 10), "head", tex=0)
    elif kind != "mammoth":
        hc = body
        m.capsule((L * 0.8, 0, hb + 1 * s), (L * 1.2, 0, hb + 4 * s), 2.2 * s, hc, "head", mat=mat, glow=glow)
        if kind in ("dog", "wolf"):
            m.ellipsoid((L * 1.3, 0, hb + 4.6 * s), (2.4 * s, 2.0 * s, 2.0 * s), hc, "head", mat=mat, glow=glow)
            m.capsule((L * 1.4, 0, hb + 4.3 * s), (L * 1.4 + 3.6 * s, 0, hb + 3.6 * s), 1.1 * s, hc, "head", r1=0.6 * s,
                      glow=glow)
            for side in (-1, 1):
                m.capsule((L * 1.25, side * 1.2 * s, hb + 6 * s), (L * 1.15, side * 1.6 * s, hb + 9 * s), 0.8 * s, hc,
                          "head", r1=0.15 * s, glow=glow)
        else:
            m.ellipsoid((L * 1.35, 0, hb + 4.6 * s), (3.2 * s, 2.2 * s, 2.0 * s), hc, "head", mat=mat)
            m.ellipsoid((L * 1.35 + 1.5 * s, 0, hb + 3.9 * s), (2.0 * s, 1.5 * s, 1.0 * s), belly, "head")
        for side in (-1, 1):
            m.ellipsoid((L * 1.42, side * 1.4 * s, hb + 5.4 * s), (0.5 * s, 0.4 * s, 0.5 * s),
                        (235, 200, 40) if not glow else (255, 255, 255), "head", tex=0, glow=True)
    if kind == "tiger":
        for side in (-1, 1):
            m.capsule((L * 1.35 + 2 * s, side * 0.9 * s, hb + 3.4 * s), (L * 1.35 + 2.4 * s, side * 0.9 * s, hb + 0.4 * s),
                      0.55 * s, (246, 240, 226), "head", r1=0.1 * s, tex=0, spec=0.5)
            m.ellipsoid((L * 1.2, side * 1.6 * s, hb + 6.2 * s), (0.7 * s, 0.5 * s, 0.8 * s), body, "head")
        for i in range(5):
            x = L * 0.7 - i * L * 0.35
            m.ellipsoid((x, 0, hb + 0.5 * s), (0.6 * s, 4.3 * s, 4.1 * s), (54, 34, 22), "root", tex=0)
        m.capsule((-L, 0, hb + 0.5 * s), (-L - 6 * s, 0, hb - 2 * s), 1.2 * s, body, "root", r1=0.5 * s, mat="fur")
    if kind == "lizard":  # farkatlan, tollas taréjjal
        for i in range(6):
            x = L * 0.7 - i * L * 0.3
            m.capsule((x, 0, hb + 3.6 * s), (x - 2.5 * s, 0, hb + 7.6 * s - i * 0.3 * s), 0.8 * s,
                      (pal["feather"], pal["jade"], pal["feather2"])[i % 3], "root", r1=0.2 * s, team=(i % 3 == 1))
        m.capsule((L * 1.3, 0, hb + 6 * s), (L * 1.0, 0, hb + 9.5 * s), 0.7 * s, pal["feather2"], "head", r1=0.2 * s,
                  team=True)
        m.ellipsoid((-L * 0.95, 0, hb + 0.6 * s), (2.2 * s, 3.4 * s, 3.0 * s), body, "root", mat="scales")
    if kind == "wolf":
        m.capsule((-L, 0, hb + 0.5 * s), (-L - 6 * s, 0, hb + 2.5 * s), 1.4 * s, body, "root", r1=0.4 * s, glow=True)
    if kind == "dog":
        m.capsule((-L, 0, hb + 0.5 * s), (-L - 5 * s, 0, hb - 1.5 * s), 1.2 * s, body, "root", r1=0.5 * s, mat="fur")
    if saddle:
        m.ellipsoid((0, 0, hb + 3.6 * s), (3.8 * s, 4.5 * s, 1.0 * s), (200, 200, 200), "root", team=True)
        m.ellipsoid((0, 0, hb + 3.0 * s), (4.2 * s, 4.8 * s, 0.6 * s), pal.get("leather", (100, 70, 40)), "root")
    m.mount_top = hb + 4.2 * s
    m.body_len = L
    return m


def rider_on(m, s, pal, species, weapon, top):
    m.bone("rtorso", "root", (0, 0, top))
    m.bone("rarm_r", "rtorso", (0, 3.6 * s, top + 6 * s))
    m.bone("rarm_l", "rtorso", (0, -3.6 * s, top + 6 * s))
    skin = pal["skin"]
    fur = "fur" if species in ("bear", "dingo") else None
    m.ellipsoid((0, 0, top + 3.4 * s), (2.6 * s, 3.4 * s, 4.0 * s), skin, "rtorso", mat=fur)
    if species == "elf":
        m.ellipsoid((0.6 * s, 0, top + 3.6 * s), (2.3 * s, 3.3 * s, 2.8 * s), pal["jade"], "rtorso", spec=0.4)
    elif species != "bear":
        m.ellipsoid((0.4 * s, 0, top + 3.8 * s), (2.6 * s, 3.6 * s, 3.0 * s), pal.get("metal", (150, 150, 150)), "rtorso",
                    spec=0.8)
    if species == "bear":  # az ursanok legfeljebb ágyékkötőt viselnek
        m.ellipsoid((0.6 * s, 0, top + 4.2 * s), (2.2 * s, 3.0 * s, 2.6 * s), pal["fur2"], "rtorso", mat="fur")
        m.box((1.6 * s, 0, top + 0.2 * s), (0.5 * s, 1.6 * s, 1.4 * s), (200, 200, 200), "rtorso", team=True)
    else:
        m.ellipsoid((-1.4 * s, 0, top + 3.2 * s), (1.0 * s, 3.4 * s, 4.0 * s), (200, 200, 200), "rtorso", team=True)
    hz = top + 9.4 * s
    if species == "elf":
        m.ellipsoid((0.5 * s, 0, hz), (2.2 * s, 2.1 * s, 2.4 * s), skin, "rtorso")
        m.ellipsoid((-0.3 * s, 0, hz + 1 * s), (2.0 * s, 2.1 * s, 1.7 * s), pal["hair"], "rtorso")
        for i, a in enumerate((-35, -10, 10, 35)):
            m.capsule((0, 0, hz + 1.8 * s), (-3.2 * s, math.sin(math.radians(a)) * 3.4 * s, hz + 5 * s), 0.7 * s,
                      (pal["feather"], pal["jade"], pal["feather2"], pal["feather"])[i], "rtorso", r1=0.2 * s,
                      team=(i == 1))
    elif species == "dingo":
        m.ellipsoid((0.3 * s, 0, hz), (2.2 * s, 2.1 * s, 2.2 * s), skin, "rtorso", mat="fur")
        m.capsule((1.4 * s, 0, hz - 0.3 * s), (4.6 * s, 0, hz - 0.8 * s), 1.1 * s, skin, "rtorso", r1=0.6 * s)
        for side in (-1, 1):
            m.capsule((0, side * 1.3 * s, hz + 1.4 * s), (-0.5 * s, side * 1.8 * s, hz + 4.4 * s), 0.9 * s, skin, "rtorso",
                      r1=0.2 * s)
        m.ellipsoid((0.2 * s, 0, hz + 0.8 * s), (2.3 * s, 2.2 * s, 0.5 * s), pal["red"], "rtorso", team=True)
    else:
        m.ellipsoid((0.3 * s, 0, hz), (2.5 * s, 2.6 * s, 2.5 * s), skin, "rtorso", mat="fur")
        m.ellipsoid((2.5 * s, 0, hz - 0.6 * s), (1.4 * s, 1.2 * s, 1.0 * s), pal["snout"], "rtorso")
        m.ellipsoid((3.8 * s, 0, hz - 0.3 * s), (0.5 * s, 0.6 * s, 0.5 * s), (28, 20, 20), "rtorso", tex=0, spec=0.6)
        for side in (-1, 1):  # kerek medvefülek
            m.ellipsoid((-0.3 * s, side * 2.0 * s, hz + 2.0 * s), (0.8 * s, 0.7 * s, 0.8 * s), skin, "rtorso")
    for side, b in ((1, "rarm_r"), (-1, "rarm_l")):
        y = side * 3.6 * s
        m.capsule((0, y, top + 6 * s), (0.8 * s, y, top + 0.8 * s), 1.1 * s, skin, b, mat=fur)
        m.ellipsoid((0.9 * s, y, top + 0.4 * s), (0.9 * s, 0.9 * s, 0.9 * s), skin, b)
    yy = 3.6 * s
    if weapon == "club":
        m.capsule((0.9 * s, yy, top + 0.6 * s), (7.4 * s, yy, top + 1.6 * s), 0.8 * s, pal["obsidian"], "rarm_r")
        for k in range(4):
            m.ellipsoid((3.4 * s + k * 1.2 * s, yy, top + 1.6 * s), (0.4 * s, 0.3 * s, 0.9 * s), (220, 226, 236),
                        "rarm_r", tex=0, spec=1.0)
    elif weapon == "lance":
        m.capsule((-3 * s, yy, top + 0.4 * s), (11 * s, yy, top + 2 * s), 0.5 * s, pal["wood"], "rarm_r")
        m.capsule((11 * s, yy, top + 2 * s), (13.6 * s, yy, top + 2.3 * s), 0.9 * s, pal["stone"], "rarm_r", r1=0.1 * s)
        m.capsule((9 * s, yy, top + 1.8 * s), (8.6 * s, yy, top - 0.8 * s), 0.35 * s, pal["feather"], "rarm_r")
    else:
        m.capsule((0.9 * s, yy, top + 0.6 * s), (7.4 * s, yy, top + 2 * s), 0.6 * s, pal["wood"], "rarm_r")
        m.ellipsoid((7 * s, yy, top + 3.2 * s), (1.5 * s, 0.45 * s, 1.9 * s), pal["metal"], "rarm_r", spec=1.0)


def emu_mount(m, s, pal, size=1.0):
    """Óriásemu lovasállatnak: bozontos tolltest, hosszú kékes nyak, két erős pikkelyes láb.
    A lábak a 'leg_fl' / 'leg_bl' csontokon vannak, így a négylábú járásanimáció váltakozva mozgatja őket."""
    k = size
    hip = 10 * s * k
    m.bone("head", "root", (3.6 * s * k, 0, hip + 3 * s * k))
    for nm, y in (("leg_fl", 1.8 * s * k), ("leg_bl", -1.8 * s * k)):
        m.bone(nm, "root", (0, y, hip))
        knee = (-1.2 * s * k, y, hip * 0.5)  # az emu „térde” hátrafelé hajlik
        m.capsule((0.2 * s * k, y, hip), knee, 1.5 * s * k, pal["emu2"], nm, r1=0.9 * s * k, mat="fur")
        m.capsule(knee, (0.4 * s * k, y, 0.8 * s * k), 0.75 * s * k, (120, 120, 120), nm, r1=0.6 * s * k, mat="scales")
        for t in range(3):
            m.capsule((0.4 * s * k, y, 0.6 * s * k), (2.4 * s * k, y + (t - 1) * 0.9 * s * k, 0.3 * s * k), 0.35 * s * k,
                      (96, 96, 96), nm, r1=0.15 * s * k)
    # bozontos, lelógó tollazat
    m.ellipsoid((-0.6 * s * k, 0, hip + 2.4 * s * k), (5.6 * s * k, 4.0 * s * k, 3.8 * s * k), pal["emu"], "root",
                mat="fur")
    m.ellipsoid((-3.6 * s * k, 0, hip + 1.4 * s * k), (2.8 * s * k, 3.4 * s * k, 3.0 * s * k), pal["emu2"], "root",
                mat="fur")  # far
    m.ellipsoid((-0.2 * s * k, 0, hip + 0.2 * s * k), (4.8 * s * k, 3.6 * s * k, 2.4 * s * k), pal["emu2"], "root",
                mat="fur")
    # nyak és fej
    nk0, nk1 = (4.0 * s * k, 0, hip + 3.0 * s * k), (8.2 * s * k, 0, hip + 13.0 * s * k)
    m.capsule(nk0, (6.6 * s * k, 0, hip + 8.0 * s * k), 1.3 * s * k, pal["emuneck"], "head", r1=1.0 * s * k)
    m.capsule((6.6 * s * k, 0, hip + 8.0 * s * k), nk1, 1.0 * s * k, pal["emuneck"], "head", r1=0.85 * s * k)
    m.ellipsoid((4.4 * s * k, 0, hip + 4.0 * s * k), (2.0 * s * k, 2.2 * s * k, 2.2 * s * k), pal["emu2"], "head",
                mat="fur")
    hx, hz = nk1[0] + 0.5 * s * k, nk1[2] + 0.6 * s * k
    m.ellipsoid((hx, 0, hz), (1.7 * s * k, 1.3 * s * k, 1.3 * s * k), pal["emu2"], "head", mat="fur")
    m.capsule((hx + 1.2 * s * k, 0, hz - 0.2 * s * k), (hx + 3.2 * s * k, 0, hz - 0.7 * s * k), 0.5 * s * k, (60, 56, 52),
              "head", r1=0.2 * s * k)
    for side in (-1, 1):
        m.ellipsoid((hx + 0.6 * s * k, side * 1.0 * s * k, hz + 0.4 * s * k), (0.4 * s * k,) * 3, (230, 130, 40), "head",
                    tex=0, glow=True)
    # nyereg: festett bőrtakaró csapatszínnel
    top = hip + 6.0 * s * k
    m.ellipsoid((-0.8 * s * k, 0, top - 0.4 * s * k), (3.0 * s * k, 3.6 * s * k, 0.8 * s * k), (200, 200, 200), "root",
                team=True)
    for t in range(4):
        m.ellipsoid((-2.2 * s * k + t * 0.9 * s * k, 3.4 * s * k, top - 1.4 * s * k), (0.35 * s * k,) * 3,
                    pal["white"], "root", tex=0)
    m.mount_top = top
    return m


def emu_unit(name, s, pal):
    """Tenyészemu (létszámot adó jószág): nyereg nélkül, csak egy csapatszínű bokaszalaggal."""
    m = Model(name)
    emu_mount(m, s, pal, size=0.85)
    m.parts = [pt for pt in m.parts if not pt[4]]  # a nyeregtakaró (csapatszín) nem kell
    for side in (-1, 1):
        m.ellipsoid((-0.2 * s, side * 1.5 * s, 4.2 * s), (0.7 * s, 0.7 * s, 0.5 * s), (200, 200, 200), "leg_fl"
                    if side > 0 else "leg_bl", team=True, tex=0)
    m.rig = "quad"
    m.attack_style = "melee"
    m.height = 10 * s * 0.85 + 13 * s
    return m


def groomi(name, s, pal):
    """Groomi: piros terrormadár – nagy, horgas csőrű fej, rövid szárnycsökevények, erős lábak, tollbóbita."""
    m = Model(name)
    red, red2, dark = (200, 52, 40), (150, 34, 30), (70, 40, 34)
    hip = 9 * s
    m.bone("head", "root", (3 * s, 0, hip + 5 * s))
    m.bone("jaw", "head", (8.5 * s, 0, hip + 9.5 * s))
    m.bone("leg_r", "root", (0, 2.2 * s, hip))
    m.bone("leg_l", "root", (0, -2.2 * s, hip))
    for side, b in ((1, "leg_r"), (-1, "leg_l")):
        y = side * 2.2 * s
        m.capsule((0, y, hip), (-1.6 * s, y, hip * 0.5), 1.9 * s, red2, b, r1=1.0 * s, mat="fur")
        m.capsule((-1.6 * s, y, hip * 0.5), (0.6 * s, y, 0.8 * s), 0.7 * s, (226, 170, 60), b, r1=0.5 * s,
                  mat="scales")
        for k in range(3):
            m.capsule((0.6 * s, y, 0.6 * s), (2.6 * s, y + (k - 1) * 0.9 * s, 0.3 * s), 0.35 * s, (60, 50, 40), b,
                      r1=0.12 * s)
    # törzs: előre dőlt, tömör test, vastag farok-tollazat
    m.ellipsoid((0, 0, hip + 2.4 * s), (4.6 * s, 3.2 * s, 3.4 * s), red, "root", mat="fur")
    m.ellipsoid((0.6 * s, 0, hip + 0.8 * s), (3.8 * s, 2.6 * s, 2.2 * s), (236, 150, 90), "root", mat="fur")  # has
    for k in range(5):
        m.capsule((-3.6 * s, (k - 2) * 0.8 * s, hip + 2.6 * s), (-7.4 * s, (k - 2) * 1.5 * s, hip + 4.4 * s + (k % 2) * s),
                  0.8 * s, dark if k % 2 else red2, "root", r1=0.25 * s)
    for side in (-1, 1):  # szárnycsökevény
        m.ellipsoid((-0.4 * s, side * 3.0 * s, hip + 3.2 * s), (2.4 * s, 0.6 * s, 1.2 * s), red2, "root", mat="fur")
    m.box((-1 * s, 0, hip + 5.5 * s), (1.8 * s, 1.6 * s, 0.4 * s), (200, 200, 200), "root", team=True)
    # nyak és fej
    m.capsule((3 * s, 0, hip + 4 * s), (6 * s, 0, hip + 9 * s), 1.6 * s, red, "head", r1=1.3 * s, mat="fur")
    m.ellipsoid((7 * s, 0, hip + 9.6 * s), (2.4 * s, 1.7 * s, 1.9 * s), red, "head", mat="fur")
    m.capsule((8.4 * s, 0, hip + 10 * s), (11.6 * s, 0, hip + 9.4 * s), 1.2 * s, (236, 200, 90), "head", r1=0.5 * s)
    m.capsule((11.6 * s, 0, hip + 9.4 * s), (12.2 * s, 0, hip + 8.2 * s), 0.5 * s, (236, 200, 90), "head", r1=0.15 * s)
    m.capsule((8.4 * s, 0, hip + 9.0 * s), (11.0 * s, 0, hip + 8.6 * s), 0.7 * s, (210, 170, 70), "jaw", r1=0.3 * s)
    for side in (-1, 1):
        m.ellipsoid((7.8 * s, side * 1.3 * s, hip + 10.4 * s), (0.45 * s,) * 3, (255, 230, 70), "head", tex=0,
                    glow=True)
    for k in range(5):  # bóbita
        m.capsule((6.4 * s, (k - 2) * 0.4 * s, hip + 11 * s), (4.4 * s, (k - 2) * 0.9 * s, hip + 13.6 * s - abs(k - 2) * 0.5 * s),
                  0.5 * s, (250, 120, 40) if k % 2 else red, "head", r1=0.15 * s)
    m.rig = "theropod"
    m.attack_style = "bite"
    m.height = hip + 14 * s
    return m


def rider(name, s, pal, kind):
    m = Model(name)
    if kind == "elf":
        quadruped(m, s, "lizard", pal, length=1.1, height=0.82)
        rider_on(m, s, pal, "elf", "club", m.mount_top)
    elif kind == "ursan":
        quadruped(m, s, "tiger", pal, length=1.0, height=0.85)
        rider_on(m, s, pal, "bear", "axe", m.mount_top)
    else:  # wullof: bölény
        quadruped(m, s, "bison", pal, length=1.05, height=1.0)
        rider_on(m, s, pal, "dingo", "lance", m.mount_top)
    m.rig = "rider"
    m.attack_style = "melee"
    m.height = m.mount_top + 13 * s
    return m


def beast(name, s, kind, pal):
    """Nyereg nélküli négylábú (pl. idézett kardfogú tigris)."""
    m = Model(name)
    quadruped(m, s, kind, pal, length=1.0, height=0.9, saddle=False)
    m.rig = "quad"
    m.attack_style = "melee"
    m.height = m.mount_top + 6 * s
    return m


def cocoon(name, s, pal):
    """Croftian gubó: lüktető, erezett tojásburok."""
    m = Model(name)
    m.ellipsoid((0, 0, 5 * s), (4.2 * s, 4.0 * s, 5.4 * s), pal["flesh"], "root", tex=0.2)
    for k in range(7):
        a = k * 0.9
        m.capsule((math.cos(a) * 3.8 * s, math.sin(a) * 3.6 * s, 1 * s),
                  (math.cos(a + 0.5) * 3.2 * s, math.sin(a + 0.5) * 3.0 * s, 9 * s), 0.5 * s, pal["shell2"], "root")
    m.ellipsoid((0, 0, 9.6 * s), (1.6 * s, 1.6 * s, 1.0 * s), pal["glow"], "root", glow=True)
    m.ellipsoid((0, 0, 0.8 * s), (5.2 * s, 5.0 * s, 1.2 * s), (200, 200, 200), "root", team=True)
    m.rig = "static"
    m.attack_style = "none"
    m.height = 11 * s
    return m


def spirit_wolf(name, s, pal):
    m = Model(name)
    quadruped(m, s, "wolf", pal, length=0.9, height=0.85, saddle=False)
    m.rig = "quad"
    m.attack_style = "melee"
    m.height = m.mount_top + 6 * s
    return m


def travois(name, s, pal):
    """Vándorszekér: teherhordó dingó, mögötte tipirudakon vonszolt csomag."""
    m = Model(name)
    quadruped(m, s, "dog", pal, length=0.9, height=0.8, saddle=False)
    hb = 9 * s * 0.8
    for side in (-1, 1):
        m.capsule((2 * s, side * 2.6 * s, hb + 2 * s), (-16 * s, side * 4.4 * s, 0.4 * s), 0.55 * s, pal["wood"], "root")
    m.box((-10 * s, 0, 3 * s), (4 * s, 3.4 * s, 1.0 * s), pal["wood"], "root", mat="planks")
    m.ellipsoid((-10 * s, 0, 5.4 * s), (4.4 * s, 3.4 * s, 2.6 * s), pal["cloth"], "root", tex=0.15)
    m.ellipsoid((-10 * s, 0, 6.6 * s), (3.0 * s, 2.6 * s, 1.4 * s), (200, 200, 200), "root", team=True)
    for k in range(4):
        m.capsule((-13 * s + k * 2 * s, -2 * s, 5 * s), (-10 * s + k * 2 * s, 2 * s, 9 * s), 0.35 * s, pal["wood"], "root")
    m.rig = "quad"
    m.attack_style = "melee"
    m.height = 14 * s
    return m


def mammoth(name, s, pal):
    m = Model(name)
    quadruped(m, s, "mammoth", pal, length=1.15, height=1.25, saddle=False)
    hb = 9 * s * 1.25
    L = 9 * s * 1.15
    m.ellipsoid((L * 0.95, 0, hb + 2 * s), (4.6 * s, 4.0 * s, 4.2 * s), pal["wool"], "head", mat="fur")
    m.ellipsoid((L * 0.35, 0, hb + 3.4 * s), (L * 0.5, 4.4 * s, 3.4 * s), pal["wool"], "root", mat="fur")
    m.bone("trunk", "head", (L * 1.25, 0, hb))
    for i in range(5):
        z0 = hb - i * 2.0 * s
        m.capsule((L * 1.25 + i * 0.4 * s, 0, z0), (L * 1.25 + (i + 1) * 0.4 * s, 0, z0 - 2.0 * s),
                  1.6 * s - i * 0.22 * s, (100, 70, 45), "trunk", stripe=(2, 0.8 * s))
    for side in (-1, 1):
        m.capsule((L * 1.2, side * 2.2 * s, hb - 1 * s), (L * 1.55, side * 3.4 * s, hb - 3.5 * s), 0.9 * s, pal["bone"],
                  "head", tex=0, spec=0.4)
        m.capsule((L * 1.55, side * 3.4 * s, hb - 3.5 * s), (L * 1.8, side * 2.4 * s, hb + 0.6 * s), 0.7 * s,
                  pal["bone"], "head", r1=0.25 * s, tex=0, spec=0.4)
        m.ellipsoid((L * 0.85, side * 4.2 * s, hb + 2.6 * s), (2.4 * s, 0.6 * s, 3.0 * s), pal["wool"], "head", mat="fur")
        m.ellipsoid((L * 1.18, side * 2.6 * s, hb + 3.2 * s), (0.45 * s, 0.4 * s, 0.4 * s), (20, 14, 10), "head", tex=0)
    top = hb + 5.0 * s
    m.box((0, 0, top), (5 * s, 4.4 * s, 0.7 * s), pal["wood"], "root", mat="planks")
    m.box((0, 0, top - 1.4 * s), (5.4 * s, 4.8 * s, 0.6 * s), (200, 200, 200), "root", team=True)
    for x in (-4 * s, 4 * s):
        for y in (-3.8 * s, 3.8 * s):
            m.capsule((x, y, top), (x, y, top + 3 * s), 0.5 * s, pal["wood"], "root")
    for y in (-3.8 * s, 3.8 * s):
        m.capsule((-4 * s, y, top + 2.6 * s), (4 * s, y, top + 2.6 * s), 0.4 * s, pal["wood"], "root")
    m.bone("thrower", "root", (-2 * s, 0, top + 1 * s))
    m.capsule((-2 * s, 0, top + 1 * s), (-7 * s, 0, top + 7 * s), 0.6 * s, pal["wood"], "thrower")
    m.ellipsoid((-7.5 * s, 0, top + 8 * s), (1.4 * s, 1.4 * s, 1.4 * s), (140, 135, 130), "thrower", tex=0.2)
    m.ellipsoid((2.5 * s, 0, top + 2.8 * s), (1.6 * s, 2.0 * s, 2.2 * s), pal["fur"], "root", mat="fur")
    m.ellipsoid((2.6 * s, 0, top + 5.2 * s), (1.5 * s, 1.5 * s, 1.5 * s), pal["fur"], "root", mat="fur")
    m.ellipsoid((2.5 * s, 0, top + 6.2 * s), (1.6 * s, 1.6 * s, 0.8 * s), pal["metal"], "root", spec=0.9)
    m.rig = "quad"
    m.attack_style = "siege"
    m.height = top + 10 * s
    return m


def theropod(name, s, pal):
    """Tollaskoronás rex: két lábon járó óriásdínó, hátán íjásszal."""
    m = Model(name)
    hip = 11 * s
    m.bone("head", "root", (5 * s, 0, hip + 5 * s))
    m.bone("jaw", "head", (11 * s, 0, hip + 7 * s))
    m.bone("leg_r", "root", (0, 3 * s, hip))
    m.bone("leg_l", "root", (0, -3 * s, hip))
    body = (92, 122, 94)
    for side, b in ((1, "leg_r"), (-1, "leg_l")):
        y = side * 3 * s
        m.capsule((0, y, hip), (1 * s, y, hip - 5 * s), 2.7 * s, body, b, r1=2.0 * s, mat="scales")
        m.capsule((1 * s, y, hip - 5 * s), (-0.5 * s, y, 1.2 * s), 1.6 * s, body, b, r1=1.2 * s, mat="scales")
        for k in range(3):
            m.capsule((-0.5 * s, y, 1 * s), (2.4 * s, y + (k - 1) * 1.0 * s, 0.4 * s), 0.5 * s, (60, 50, 40), b,
                      r1=0.15 * s)
    m.ellipsoid((0, 0, hip + 2.4 * s), (7.5 * s, 4.4 * s, 4.6 * s), body, "root", mat="scales")
    m.ellipsoid((1 * s, 0, hip + 0.5 * s), (6 * s, 3.4 * s, 3 * s), pal["lizard2"], "root")
    m.capsule((-6 * s, 0, hip + 2.5 * s), (-15 * s, 0, hip + 1 * s), 3 * s, body, "root", r1=0.6 * s, mat="scales")
    for k in range(6):
        x = 3 * s - k * 3 * s
        m.capsule((x, 0, hip + 6.4 * s - k * 0.3 * s), (x - 2 * s, 0, hip + 8.6 * s - k * 0.4 * s), 0.7 * s,
                  (pal["feather"], pal["feather2"])[k % 2], "root", r1=0.2 * s)
    m.capsule((5 * s, 0, hip + 4 * s), (9 * s, 0, hip + 7.5 * s), 2.8 * s, body, "head", mat="scales")
    m.ellipsoid((11 * s, 0, hip + 8 * s), (4.2 * s, 2.6 * s, 2.6 * s), body, "head", mat="scales")
    m.ellipsoid((13.5 * s, 0, hip + 6.6 * s), (2.6 * s, 2.0 * s, 0.9 * s), (210, 200, 180), "jaw")
    for k in range(5):
        m.capsule((11.5 * s + k * 0.8 * s, 1.2 * s, hip + 7.2 * s), (11.6 * s + k * 0.8 * s, 1.1 * s, hip + 6.4 * s),
                  0.25 * s, (250, 248, 240), "head", r1=0.05 * s, tex=0)
    for side in (-1, 1):
        m.ellipsoid((11.6 * s, side * 1.9 * s, hip + 9 * s), (0.6 * s, 0.4 * s, 0.6 * s), (250, 200, 40), "head",
                    tex=0, glow=True)
        m.capsule((5 * s, side * 3.2 * s, hip + 1 * s), (7 * s, side * 3 * s, hip - 1.5 * s), 0.7 * s, body, "root",
                  r1=0.4 * s)
    for i, a in enumerate((-70, -40, -15, 15, 40, 70)):
        ang = math.radians(a)
        m.capsule((10 * s, 0, hip + 10 * s), (7 * s, math.sin(ang) * 5 * s, hip + 15 * s - abs(a) / 70 * 3 * s),
                  1.0 * s, (pal["feather"], pal["jade"], pal["feather2"])[i % 3], "head", r1=0.3 * s,
                  team=(i % 3 == 2))
    m.ellipsoid((10 * s, 0, hip + 10.4 * s), (1.4 * s, 2.4 * s, 0.8 * s), pal["gold"], "head", spec=1.0)
    top = hip + 6.6 * s
    m.box((-1 * s, 0, top), (3.4 * s, 3.4 * s, 0.6 * s), (200, 200, 200), "root", team=True)
    m.ellipsoid((-1 * s, 0, top + 3 * s), (1.8 * s, 2.4 * s, 2.8 * s), pal["skin"], "root")
    m.ellipsoid((-0.6 * s, 0, top + 6.6 * s), (1.6 * s, 1.5 * s, 1.7 * s), pal["skin"], "root")
    m.ellipsoid((-1.0 * s, 0, top + 7.4 * s), (1.5 * s, 1.6 * s, 1.2 * s), pal["hair"], "root")
    m.capsule((-0.6 * s, 0, top + 8 * s), (-3 * s, 0, top + 10.5 * s), 0.6 * s, pal["jade"], "root", r1=0.2 * s)
    m.rig = "theropod"
    m.attack_style = "bite"
    m.height = top + 11 * s
    return m


def flyer(name, s, kind, pal):
    m = Model(name)
    alt = 16 * s
    m.bone("wing_l", "root", (0, -2 * s, alt + 1 * s))
    m.bone("wing_r", "root", (0, 2 * s, alt + 1 * s))
    m.bone("head", "root", (4 * s, 0, alt + 1 * s))
    body, wing, team_part = {
        "quetzal": ((70, 140, 150), pal.get("feather"), True),
        "raven": ((38, 38, 48), (28, 28, 38), True),
        "rem": (pal.get("shell"), (150, 92, 162), True),
        "thunderbird": ((236, 236, 228), (226, 226, 216), True),
        "aquila": (pal.get("metal"), pal.get("metal2"), True),
    }[kind]
    mech = kind == "aquila"
    m.ellipsoid((0, 0, alt), (5 * s, 2.4 * s, 2.2 * s), body, "root", mat=None if mech else "fur", spec=0.7 if mech else 0)
    if kind == "quetzal":
        m.capsule((4 * s, 0, alt + 0.5 * s), (8 * s, 0, alt + 2.5 * s), 1.2 * s, body, "head")
        m.ellipsoid((9 * s, 0, alt + 2.8 * s), (2.2 * s, 1.2 * s, 1.2 * s), (90, 160, 160), "head")
        m.capsule((10 * s, 0, alt + 2.5 * s), (14 * s, 0, alt + 2 * s), 0.7 * s, pal["gold"], "head", r1=0.1 * s, spec=0.9)
        m.capsule((8.5 * s, 0, alt + 3.5 * s), (5 * s, 0, alt + 7 * s), 0.9 * s, pal["jade"], "head", r1=0.2 * s, team=True)
        for i in range(3):
            m.capsule((-4 * s, 0, alt), (-9 * s, (i - 1) * 2.2 * s, alt - 0.5 * s), 0.8 * s,
                      (pal["feather"], pal["jade"], pal["feather"])[i], "root", r1=0.3 * s, team=(i == 1))
    elif kind in ("raven", "thunderbird"):
        hc = (44, 44, 54) if kind == "raven" else (240, 240, 232)
        m.ellipsoid((4.8 * s, 0, alt + 1 * s), (2.3 * s, 1.9 * s, 1.9 * s), hc, "head", mat="fur")
        beak = (70, 70, 75) if kind == "raven" else (64, 60, 62)
        m.capsule((6.4 * s, 0, alt + 1 * s), (9.4 * s, 0, alt + 0.2 * s), 0.85 * s, beak, "head", r1=0.15 * s, spec=0.6)
        for side in (-1, 1):
            m.ellipsoid((5.6 * s, side * 1.1 * s, alt + 1.8 * s), (0.4 * s, 0.4 * s, 0.4 * s), (240, 220, 80), "head",
                        tex=0, glow=True)
        m.ellipsoid((-6 * s, 0, alt), (3.6 * s, 2.6 * s, 0.6 * s), body, "root", stripe=(1, 1.2 * s))
        m.ellipsoid((0, 0, alt + 1.6 * s), (2.4 * s, 2.0 * s, 0.8 * s), (200, 200, 200), "root", team=True)
        if kind == "thunderbird":  # villámminták
            for side in (-1, 1):
                for k in range(3):
                    m.capsule((1 * s, side * (2 + k * 3) * s, alt + 1.7 * s), (-1.4 * s, side * (3.2 + k * 3) * s,
                              alt + 1.7 * s), 0.5 * s, (150, 210, 255), "wing_l" if side < 0 else "wing_r", glow=True)
            for i in range(6):  # sárga kakadu-bóbita
                a = math.radians(-40 + i * 16)
                m.capsule((4.6 * s, 0, alt + 2.4 * s), (2.0 * s + math.cos(a) * 1.5 * s, math.sin(a) * 1.6 * s,
                          alt + 7.4 * s - abs(i - 2.5) * 0.5 * s), 0.55 * s, (250, 214, 60), "head", r1=0.15 * s)
    elif kind == "aquila":
        m.box((4.6 * s, 0, alt + 1 * s), (1.8 * s, 1.4 * s, 1.4 * s), pal["bronze"], "head", spec=0.9)
        m.capsule((6.2 * s, 0, alt + 1 * s), (8.6 * s, 0, alt + 0.2 * s), 0.8 * s, pal["gold"], "head", r1=0.1 * s,
                  spec=1.0)
        m.box((5.9 * s, 0, alt + 1.6 * s), (0.3 * s, 1.2 * s, 0.3 * s), pal["soul"], "head", glow=True)
        m.cylinder((-5 * s, 0, alt), (-7.5 * s, 0, alt), 1.4 * s, pal["iron"], "root", spec=0.5)
        m.ellipsoid((-7.8 * s, 0, alt), (0.4 * s, 1.0 * s, 1.0 * s), (255, 160, 60), "root", glow=True)
        m.box((0, 0, alt + 2 * s), (2.6 * s, 1.4 * s, 0.4 * s), (200, 200, 200), "root", team=True)
    else:  # szárnyas rém
        m.ellipsoid((4.6 * s, 0, alt + 0.6 * s), (2.6 * s, 2 * s, 1.8 * s), pal["shell2"], "head", spec=0.5)
        for side in (-1, 1):
            m.capsule((6 * s, side * 0.8 * s, alt), (9 * s, side * 1.6 * s, alt - 1.8 * s), 0.5 * s, pal["bone"],
                      "head", r1=0.1 * s, tex=0)
            m.ellipsoid((5.8 * s, side * 1.2 * s, alt + 1.6 * s), (0.5 * s, 0.5 * s, 0.5 * s), pal["glow"], "head",
                        glow=True)
        m.capsule((-4 * s, 0, alt), (-11 * s, 0, alt + 1 * s), 1.4 * s, body, "root", r1=0.3 * s, spec=0.4)
        m.ellipsoid((-1 * s, 0, alt + 1.8 * s), (3 * s, 1.6 * s, 0.8 * s), (200, 200, 200), "root", team=True)
    for side, b in ((-1, "wing_l"), (1, "wing_r")):
        span = 14 * s if kind != "thunderbird" else 17 * s
        m.capsule((0, side * 2 * s, alt + 1 * s), (1 * s, side * span, alt + 1.5 * s), 0.6 * s,
                  pal["bone"] if kind == "rem" else (pal["bronze"] if mech else wing), b, r1=0.3 * s,
                  spec=0.8 if mech else 0)
        for k in range(6):
            y = side * (3 * s + k * (span - 3 * s) / 6)
            col = wing
            if kind == "quetzal" and k % 2:
                col = pal["jade"]
            if kind == "thunderbird":
                col = ((240, 240, 232), (250, 222, 110), (226, 226, 216))[k % 3]
            m.ellipsoid((-2 * s - k * 0.3 * s, y, alt + 1 * s), (3.4 * s - k * 0.3 * s, (span - 3 * s) / 10, 0.35 * s),
                        col, b, team=(k == 5 and kind in ("quetzal", "thunderbird")), spec=0.7 if mech else 0,
                        stripe=(0, 0.9 * s) if not mech else None)
    m.rig = "flyer"
    m.attack_style = "dive"
    m.altitude = alt
    m.height = alt + 8 * s
    return m


def insect(name, s, pal, scythes=True, worker=False, mech=False):
    m = Model(name)
    hb = 5 * s
    shell = pal["metal"] if mech else pal["shell"]
    shell2 = pal["iron"] if mech else pal["shell2"]
    sp = 0.8 if mech else 0.45
    m.bone("head", "root", (4 * s, 0, hb))
    m.bone("arm_r", "root", (3 * s, 2 * s, hb + 1 * s))
    m.bone("arm_l", "root", (3 * s, -2 * s, hb + 1 * s))
    for i, x in enumerate((2.5 * s, 0, -2.5 * s)):
        for side, nm in ((1, f"leg_r{i}"), (-1, f"leg_l{i}")):
            if worker and i == 0 and not mech:
                continue
            m.bone(nm, "root", (x, side * 1.6 * s, hb))
            m.capsule((x, side * 1.6 * s, hb), (x + 0.5 * s, side * 4.5 * s, hb + 2 * s), 0.65 * s, shell2, nm, spec=sp)
            m.ellipsoid((x + 0.5 * s, side * 4.5 * s, hb + 2 * s), (0.7 * s,) * 3, shell, nm, spec=sp)
            m.capsule((x + 0.5 * s, side * 4.5 * s, hb + 2 * s), (x + 1.2 * s, side * 5.8 * s, 0.4 * s), 0.5 * s,
                      shell2, nm, r1=0.15 * s, spec=sp)
    for k in range(3):  # szelvényezett potroh
        m.ellipsoid((-2.2 * s - k * 1.8 * s, 0, hb + 1.2 * s - k * 0.2 * s), (2.0 * s, 3.2 * s - k * 0.5 * s, 2.6 * s - k * 0.3 * s),
                    shell if k % 2 == 0 else shell2, "root", spec=sp, mat="rivets" if mech else None)
    m.ellipsoid((-3.5 * s, 0, hb + 2.8 * s), (3.0 * s, 2.0 * s, 1.2 * s), (200, 200, 200), "root", team=True)
    m.ellipsoid((1.2 * s, 0, hb + 0.9 * s), (2.8 * s, 2.4 * s, 2.2 * s), shell, "root", spec=sp)
    m.ellipsoid((4.4 * s, 0, hb + 1 * s), (2.2 * s, 2.0 * s, 1.8 * s), shell2, "head", spec=sp)
    for side in (-1, 1):
        ec = pal["soul"] if mech else pal["eye"]
        m.ellipsoid((5.4 * s, side * 1.1 * s, hb + 1.8 * s), (0.5 * s, 0.5 * s, 0.5 * s), ec, "head", glow=True)
        m.ellipsoid((5.2 * s, side * 0.6 * s, hb + 2.3 * s), (0.3 * s, 0.3 * s, 0.3 * s), ec, "head", glow=True)
        if not mech:
            m.capsule((6 * s, side * 0.8 * s, hb + 0.2 * s), (7.6 * s, side * 0.2 * s, hb - 0.3 * s), 0.45 * s,
                      pal["bone"], "head", r1=0.1 * s, tex=0)
            m.capsule((5.2 * s, side * 0.7 * s, hb + 2.4 * s), (8 * s, side * 2.2 * s, hb + 5.0 * s), 0.18 * s,
                      shell2, "head", r1=0.08 * s)  # csápok
    if not mech:
        for spine in range(4):
            x = 0.5 * s - spine * 2.0 * s
            m.capsule((x, 0, hb + 3.2 * s), (x - 1.5 * s, 0, hb + 6 * s), 0.6 * s, pal["bone"], "root", r1=0.1 * s, tex=0)
    for side, b in ((1, "arm_r"), (-1, "arm_l")):
        if scythes:
            m.capsule((3 * s, side * 2 * s, hb + 1 * s), (5 * s, side * 3 * s, hb + 5 * s), 0.6 * s, shell2, b, spec=sp)
            m.capsule((5 * s, side * 3 * s, hb + 5 * s), (9 * s, side * 2.6 * s, hb + 2 * s), 0.7 * s, pal["bone"], b,
                      r1=0.1 * s, tex=0, spec=0.5)
        elif mech:
            m.capsule((3 * s, side * 2 * s, hb + 1 * s), (6.5 * s, side * 2.6 * s, hb + 2 * s), 0.5 * s, pal["bronze"], b,
                      spec=0.9)
            m.ellipsoid((6.8 * s, side * 2.6 * s, hb + 2 * s), (0.8 * s, 0.6 * s, 0.6 * s), pal["iron"], b, spec=0.8)
        else:
            m.capsule((3 * s, side * 2 * s, hb + 1 * s), (6.5 * s, side * 2.4 * s, hb + 0.2 * s), 0.6 * s, pal["flesh"],
                      b, r1=0.3 * s)
    if worker:
        if mech:
            m.cylinder((-3 * s, 0, hb + 3.6 * s), (-3 * s, 0, hb + 6.4 * s), 1.4 * s, pal["bronze"], "root", spec=0.9)
            m.ellipsoid((-3 * s, 0, hb + 6.8 * s), (0.9 * s,) * 3, pal["soul"], "root", glow=True)
        else:
            m.ellipsoid((-4 * s, 0, hb + 3.4 * s), (2.4 * s, 2.0 * s, 1.8 * s), pal["flesh"], "root", tex=0.15)
    m.rig = "insect"
    m.attack_style = "melee"
    m.height = hb + 7 * s
    return m


def serpent(name, s, pal):
    m = Model(name)
    m.bone("torso", "root", (0, 0, 3 * s))
    m.bone("head", "torso", (2 * s, 0, 15 * s))
    m.bone("arm_r", "torso", (1.5 * s, 2.6 * s, 11 * s))
    m.bone("arm_l", "torso", (1.5 * s, -2.6 * s, 11 * s))
    pts = [(-9 * s, 0, 1.6 * s), (-5 * s, 2 * s, 1.8 * s), (-1 * s, 0, 2.6 * s)]
    for a, b in zip(pts, pts[1:]):
        m.capsule(a, b, 2.4 * s, pal["shell2"], "root", r1=2.6 * s, spec=0.4, stripe=(0, 1.4 * s))
    up = [(-1 * s, 0, 2.6 * s), (0, 0, 7 * s), (1 * s, 0, 11 * s), (2 * s, 0, 14.5 * s)]
    for a, b in zip(up, up[1:]):
        m.capsule(a, b, 2.8 * s, pal["shell"], "torso", r1=2.4 * s, spec=0.45, stripe=(2, 1.3 * s))
    m.ellipsoid((1.8 * s, 0, 9 * s), (2.2 * s, 2.0 * s, 4.4 * s), pal["flesh"], "torso", stripe=(2, 0.9 * s))
    m.ellipsoid((-1.4 * s, 0, 10 * s), (2.4 * s, 3.0 * s, 4.6 * s), (200, 200, 200), "torso", team=True)
    for k in range(6):
        z = 3.5 * s + k * 2.1 * s
        for side in (-0.6, 0.6):
            m.capsule((-1.6 * s, side * s, z), (-5 * s, side * 2.2 * s, z + 1.6 * s), 0.6 * s, pal["bone"], "torso",
                      r1=0.1 * s, tex=0)
    m.ellipsoid((4 * s, 0, 16 * s), (3.2 * s, 2.4 * s, 2.0 * s), pal["shell"], "head", spec=0.5)
    m.ellipsoid((6.4 * s, 0, 15.4 * s), (1.4 * s, 1.4 * s, 1.0 * s), pal["flesh"], "head")
    for side in (-1, 1):
        m.capsule((3 * s, side * 1.6 * s, 17 * s), (0, side * 3.4 * s, 19.5 * s), 0.6 * s, pal["bone"], "head",
                  r1=0.1 * s, tex=0)
        m.ellipsoid((5 * s, side * 1.4 * s, 16.8 * s), (0.5 * s, 0.4 * s, 0.4 * s), pal["eye"], "head", glow=True)
    for side, b in ((1, "arm_r"), (-1, "arm_l")):
        m.capsule((1.5 * s, side * 2.6 * s, 11 * s), (4.5 * s, side * 3.6 * s, 8.5 * s), 0.6 * s, pal["shell2"], b)
        m.capsule((4.5 * s, side * 3.6 * s, 8.5 * s), (8 * s, side * 3 * s, 10.5 * s), 0.6 * s, pal["bone"], b,
                  r1=0.1 * s, tex=0)
    m.rig = "serpent"
    m.attack_style = "spit"
    m.height = 21 * s
    return m


def artillery(name, s, pal):
    m = Model(name)
    hb = 6 * s
    for i, (x, y) in enumerate(((4 * s, 4 * s), (4 * s, -4 * s), (-4 * s, 4 * s), (-4 * s, -4 * s))):
        nm = f"leg{i}"
        m.bone(nm, "root", (x, y * 0.6, hb))
        m.capsule((x, y * 0.6, hb), (x * 1.3, y * 1.2, hb * 0.6), 1.1 * s, pal["shell2"], nm, spec=0.4)
        m.capsule((x * 1.3, y * 1.2, hb * 0.6), (x * 1.2, y * 1.35, 0.4 * s), 0.9 * s, pal["shell2"], nm, r1=0.4 * s)
    m.ellipsoid((0, 0, hb + 2 * s), (7 * s, 5 * s, 3.6 * s), pal["shell"], "root", spec=0.45, stripe=(0, 1.8 * s))
    m.ellipsoid((-1 * s, 0, hb + 4 * s), (5 * s, 4 * s, 2 * s), (200, 200, 200), "root", team=True)
    m.bone("sac", "root", (-2 * s, 0, hb + 5 * s))
    m.ellipsoid((-2.5 * s, 0, hb + 7.4 * s), (4.4 * s, 3.8 * s, 3.8 * s), pal["glow"], "sac", glow=True, tex=0.15)
    for k in range(4):
        a = k * 1.6
        m.capsule((-2.5 * s, 0, hb + 4 * s), (-2.5 * s + math.cos(a) * 4 * s, math.sin(a) * 3.6 * s, hb + 9 * s), 0.35 * s,
                  pal["flesh"], "sac")
    m.bone("tube", "root", (4 * s, 0, hb + 4 * s))
    m.capsule((4 * s, 0, hb + 4 * s), (12 * s, 0, hb + 8 * s), 1.8 * s, pal["shell2"], "tube", r1=1.2 * s,
              stripe=(0, 1.2 * s), spec=0.4)
    m.ellipsoid((12.4 * s, 0, hb + 8.2 * s), (1.0 * s, 1.4 * s, 1.4 * s), pal["glow"], "tube", glow=True)
    m.ellipsoid((6 * s, 0, hb + 1 * s), (2 * s, 2.6 * s, 1.8 * s), pal["shell2"], "root")
    m.rig = "artillery"
    m.attack_style = "siege"
    m.height = hb + 12 * s
    return m


def horror(name, s, pal):
    m = Model(name)
    hb = 7 * s
    m.ellipsoid((0, 0, hb + 3 * s), (7 * s, 6.4 * s, 6.4 * s), pal["shell"], "root", spec=0.45, mat="scales")
    m.ellipsoid((-1 * s, 0, hb + 7 * s), (5.4 * s, 5 * s, 2.6 * s), (200, 200, 200), "root", team=True)
    m.ellipsoid((2 * s, 0, hb + 1 * s), (5.6 * s, 5.4 * s, 4 * s), pal["flesh"], "root", stripe=(2, 1.1 * s))
    for i in range(7):
        a = math.radians(-60 + i * 20)
        m.ellipsoid((6 * s, math.sin(a) * 3.8 * s, hb + 5 * s + math.cos(a * 2) * 1.2 * s), (0.9 * s,) * 3, pal["eye"],
                    "root", glow=True)
    m.ellipsoid((7 * s, 0, hb + 1.5 * s), (1.2 * s, 3 * s, 1.4 * s), (40, 10, 30), "root", tex=0)
    for k in range(6):
        m.capsule((7.4 * s, (k - 2.5) * 0.9 * s, hb + 2.6 * s), (7.6 * s, (k - 2.5) * 0.9 * s, hb + 1.4 * s), 0.3 * s,
                  pal["bone"], "root", r1=0.08 * s, tex=0)
    for i in range(8):
        a = i / 8 * 2 * math.pi
        nm = f"t{i}"
        bx, by = math.cos(a) * 5 * s, math.sin(a) * 5 * s
        m.bone(nm, "root", (bx, by, hb))
        pts = [(bx, by, hb), (bx * 1.6, by * 1.6, hb - 3 * s), (bx * 2.2, by * 2.2, 1 * s), (bx * 2.8, by * 2.8, 0.6 * s)]
        for k, (p0, p1) in enumerate(zip(pts, pts[1:])):
            m.capsule(p0, p1, (1.8 - k * 0.5) * s, pal["shell2"] if k < 2 else pal["flesh"], nm, r1=(1.4 - k * 0.45) * s,
                      stripe=(2, 0.8 * s))
    for k in range(4):
        a = math.radians(-60 + k * 40)
        m.capsule((-3 * s, math.sin(a) * 4 * s, hb + 7 * s), (-6 * s, math.sin(a) * 6 * s, hb + 12 * s), 1 * s,
                  pal["bone"], "root", r1=0.15 * s, tex=0)
    m.rig = "horror"
    m.attack_style = "slam"
    m.height = hb + 13 * s
    return m


def floater(name, s, pal, kind="brain"):
    m = Model(name)
    alt = 9 * s if kind == "brain" else 13 * s
    if kind == "brain":
        m.ellipsoid((0, 0, alt + 4 * s), (5 * s, 4.4 * s, 4 * s), pal["flesh"], "root", tex=0.18)
        for k in range(4):
            m.ellipsoid((-2 * s + k * 1.6 * s, 0, alt + 7.2 * s), (0.9 * s, 4.0 * s, 0.6 * s), (170, 90, 130), "root")
        m.capsule((0, -4.2 * s, alt + 4 * s), (0, 4.2 * s, alt + 4 * s), 0.4 * s, (150, 70, 110), "root")
        m.ellipsoid((0, 0, alt + 1.6 * s), (4.4 * s, 4 * s, 1.6 * s), (200, 200, 200), "root", team=True)
        for side in (-1, 1):
            m.ellipsoid((4.2 * s, side * 1.6 * s, alt + 4 * s), (0.9 * s, 0.9 * s, 0.9 * s), pal["glow"], "root",
                        glow=True)
        tcol, tcol2 = pal["shell"], pal["shell2"]
    else:  # lebegő pöfeteg
        m.ellipsoid((0, 0, alt + 3 * s), (6 * s, 6 * s, 5.4 * s), pal["cap2"], "root", tex=0.12)
        for k in range(9):
            a = k * 2.3
            r = (1.5 + (k % 3) * 1.6) * s
            m.ellipsoid((math.cos(a) * r, math.sin(a) * r, alt + 8 * s - r * 0.3), (0.8 * s, 0.8 * s, 0.5 * s),
                        pal["spot"], "root", tex=0)
        m.ellipsoid((0, 0, alt - 1 * s), (3.4 * s, 3.4 * s, 1.4 * s), (200, 200, 200), "root", team=True)
        m.ellipsoid((5.4 * s, 0, alt + 3.4 * s), (0.9 * s, 1.6 * s, 1.0 * s), (60, 40, 40), "root", tex=0)
        for side in (-1, 1):
            m.ellipsoid((5.0 * s, side * 2.2 * s, alt + 5 * s), (0.6 * s, 0.6 * s, 0.6 * s), pal["glow"], "root",
                        glow=True)
        tcol, tcol2 = pal["stalk2"], pal["gill"]
    for i in range(6):
        a = i / 6 * 2 * math.pi
        nm = f"t{i}"
        bx, by = math.cos(a) * 2.6 * s, math.sin(a) * 2.6 * s
        m.bone(nm, "root", (bx, by, alt + 1 * s))
        m.capsule((bx, by, alt + 1 * s), (bx * 1.6, by * 1.6, alt - 4 * s), 0.9 * s, tcol, nm, r1=0.5 * s)
        m.capsule((bx * 1.6, by * 1.6, alt - 4 * s), (bx * 1.3, by * 1.3, alt - 8 * s), 0.5 * s, tcol2, nm, r1=0.15 * s)
    m.rig = "floater"
    m.attack_style = "cast"
    m.altitude = alt
    m.height = alt + 9 * s
    return m


def puffball(name, s, pal, big=False):
    """Pöfeteg: kerek pöfetegtest apró gyökérlábakon, arccal."""
    m = Model(name)
    R = 5 * s
    m.bone("leg_r", "root", (0, 1.8 * s, 2.4 * s))
    m.bone("leg_l", "root", (0, -1.8 * s, 2.4 * s))
    for side, b in ((1, "leg_r"), (-1, "leg_l")):
        m.capsule((0, side * 1.8 * s, 2.4 * s), (0.6 * s, side * 2.0 * s, 0.4 * s), 1.1 * s, pal["stalk2"], b, r1=0.7 * s)
    m.ellipsoid((0, 0, R + 1.5 * s), (R, R, R * 0.92), pal["cap2"], "root", tex=0.14)
    m.ellipsoid((0, 0, R * 1.5 + 1.5 * s), (R * 0.55, R * 0.55, R * 0.4), (200, 200, 200), "root", team=True)
    for k in range(12):
        a = k * 2.4
        z = 2 + (k % 4) * 0.22
        m.ellipsoid((math.cos(a) * R * 0.7, math.sin(a) * R * 0.7, R * z * 0.62 + 1.5 * s), (0.7 * s, 0.7 * s, 0.6 * s),
                    pal["spot"], "root", tex=0)
    m.ellipsoid((R * 0.92, 0, R * 1.1), (0.8 * s, 1.6 * s, 0.8 * s), (50, 30, 28), "root", tex=0)
    for side in (-1, 1):
        m.ellipsoid((R * 0.85, side * 1.6 * s, R * 1.5), (0.6 * s, 0.6 * s, 0.7 * s), (30, 20, 20), "root", tex=0)
        m.ellipsoid((R * 0.92, side * 1.6 * s, R * 1.55), (0.25 * s, 0.25 * s, 0.25 * s), pal["glow"], "root", glow=True)
        m.capsule((R * 0.2, side * R * 0.95, R * 1.1), (R * 0.9, side * R * 1.3, R * 0.7), 0.7 * s, pal["stalk2"],
                  "root", r1=0.4 * s)
    if big:
        for k in range(6):
            a = k * 1.05
            m.ellipsoid((math.cos(a) * R * 0.8, math.sin(a) * R * 0.8, R * 0.6), (1.6 * s, 1.6 * s, 1.4 * s),
                        pal["cap"], "root", spec=0.3)
    m.rig = "puffball"
    m.attack_style = "melee"
    m.height = R * 2 + 2 * s
    return m


def termite_giant(name, s, pal):
    """Termeszvár-óriás: vörös földből rakott, tornyos termeszvár, ami felállt és jár."""
    m = biped(name, s, dict(pal, skin=pal["termite2"], leather=pal["termite2"]), species="golem", weapon=None, bulky=1.4)
    hip, sh = 10 * s, 18.5 * s
    rng = np.random.default_rng(7)
    m.capsule((0, 0, hip - 1 * s), (-0.4 * s, 0, sh + 4 * s), 5.2 * s, pal["termite"], "torso", r1=3.6 * s, mat="stones")
    for k in range(9):  # kiálló földtornyocskák (a termeszvár „kéményei”)
        a = rng.uniform(0, 2 * math.pi)
        z = hip + rng.uniform(1, 9) * s
        r0 = 4.6 * s - (z - hip) * 0.12
        base = (math.cos(a) * r0 * 0.8, math.sin(a) * r0 * 0.9, z)
        tip = (base[0] * 1.15 - 0.6 * s, base[1] * 1.15, z + rng.uniform(3, 6) * s)
        m.capsule(base, tip, rng.uniform(1.2, 1.8) * s, pal["termite"] if k % 2 else pal["termite2"], "torso",
                  r1=0.4 * s, mat="stones")
    for k in range(3):  # a fej: magas, csúcsos torony
        m.capsule((0.3 * s, 0, sh + 2 * s + k * 2.6 * s), (0.1 * s - k * 0.4 * s, 0, sh + 5 * s + k * 2.6 * s),
                  (3.0 - k * 0.8) * s, pal["termite"], "head", r1=(2.2 - k * 0.6) * s, mat="stones")
    for side in (-1, 1):  # izzó szemek a földben
        m.ellipsoid((2.8 * s, side * 1.3 * s, sh + 3.4 * s), (0.5 * s, 0.6 * s, 0.5 * s), (255, 200, 90), "head",
                    glow=True)
    for k in range(4):  # okker-fehér pöttyfestés a testen
        m.ellipsoid((4.6 * s, (k - 1.5) * 1.4 * s, hip + 5 * s + (k % 2) * 1.2 * s), (0.35 * s,) * 3, pal["white"],
                    "torso", tex=0)
    m.box((4.4 * s, 0, hip + 1.2 * s), (0.5 * s, 2.0 * s, 1.0 * s), (200, 200, 200), "torso", team=True)
    m.attack_style = "melee"
    return m


def golem(name, s, pal):
    """Totemóriás: festett, faragott totemoszlop-tagokból álló élő óriás."""
    m = biped(name, s, dict(pal, skin=pal["wood"]), species="golem", weapon=None, bulky=1.35)
    # a biped testét faragott tömbökkel fedjük be
    hip, sh = 10 * s, 18.5 * s
    cols = [pal["red"], pal["turq"], (40, 36, 36), pal["wood"]]
    for k in range(4):
        z = hip - 1 * s + k * 3.2 * s
        m.box((0, 0, z), (3.4 * s, 4.6 * s, 1.5 * s), cols[k % 4] if k % 2 else pal["wood"], "torso", mat="planks")
        m.ellipsoid((3.4 * s, 0, z + 0.4 * s), (0.6 * s, 1.6 * s, 0.9 * s), (240, 236, 220), "torso", tex=0)
        m.ellipsoid((3.8 * s, 0, z + 0.4 * s), (0.4 * s, 0.7 * s, 0.5 * s), (20, 18, 16), "torso", tex=0)
    m.box((0.3 * s, 0, sh + 3.6 * s), (3.0 * s, 3.4 * s, 2.6 * s), pal["wood"], "head", mat="planks")
    m.capsule((3.0 * s, 0, sh + 3.6 * s), (6.4 * s, 0, sh + 2.4 * s), 1.2 * s, pal["gold"], "head", r1=0.2 * s)
    for side in (-1, 1):
        m.ellipsoid((3.3 * s, side * 1.6 * s, sh + 4.6 * s), (0.5 * s, 0.7 * s, 0.7 * s), (255, 220, 120), "head",
                    glow=True)
        m.ellipsoid((-0.5 * s, side * 6.4 * s, sh + 3 * s), (2.6 * s, 3.6 * s, 0.6 * s), pal["red"], "head", team=True)
    m.attack_style = "melee"
    return m


# ============================================================================
def heroic(m, head=1.2, wide=1.14, tall=0.93):
    """v4 – a festett csomag zömök, hősies arányai: nagyobb fej, szélesebb, kicsit alacsonyabb test."""
    hb = m.bones.get("head")
    out = []
    for part in m.parts:
        pts, nrm = part[0], part[1]
        if hb is not None and part[3] == "head":
            pv = hb[1]
            pts = pv + (pts - pv) * head
        sc = np.array([wide, wide, tall])
        pts = pts * sc
        nrm = nrm / sc
        nrm = nrm / np.linalg.norm(nrm, axis=1, keepdims=True)
        out.append((pts, nrm) + tuple(part[2:]))
    m.parts = out
    m.bones = {k: (par, np.asarray(pv, float) * np.array([wide, wide, tall])) for k, (par, pv) in m.bones.items()}
    return m


def build_unit(defn):
    m = _build_unit(defn)
    rig = getattr(m, "rig", "")
    if rig in ("biped", "rider"):
        heroic(m)
    else:
        heroic(m, head=1.0, wide=1.1, tall=0.96)
    return m


def _build_unit(defn):
    """Definícióból modell. Ismert azonosítóknál saját modell, egyébként a shape alapján."""
    uid = defn["id"]
    s = defn.get("radius", 0.4) * 32 / 7.8
    fac = defn.get("faction")
    shape = defn.get("shape", "warrior")
    E, U, C, Wl, My, Im = ELF, URS, CRO, WUL, MYC, IMP
    table = {
        "elf_kofarago": lambda: biped(uid, s, E, "elf", "tool", head="plume", armor="leather", gear=("basket",)),
        "elf_tollas_harcos": lambda: biped(uid, s, E, "elf", "club", head="feathers", armor="jade", cape=True,
                                           gear=("pauldron",)),
        "elf_jade_ijasz": lambda: biped(uid, s, E, "elf", "bow", head="plume", armor="leather", gear=("quiver",)),
        "elf_raptor_lovas": lambda: rider(uid, s * 0.9, E, "elf"),
        "elf_napszoro": lambda: biped(uid, s, E, "elf", "staff", robe=True, staff_glow=(255, 214, 90), head="feathers",
                                      armor="jade"),
        "elf_quetzal": lambda: flyer(uid, s * 0.85, "quetzal", E),
        "elf_tollas_rex": lambda: theropod(uid, s * 0.95, E),
        "urs_telepes": lambda: biped(uid, s * 0.92, U, "bear", "tool", bulky=1.15, armor="leather"),
        "urs_pajzshordozo": lambda: biped(uid, s * 0.9, U, "bear", "axe", offhand="round_shield", bulky=1.2,
                                          head="horns", armor="metal"),
        "urs_szigonyvezo": lambda: biped(uid, s * 0.9, U, "bear", "spear", bulky=1.1, head="hood", armor="fur"),
        "urs_berserker": lambda: biped(uid, s * 0.93, U, "bear", "twin_axe", bulky=1.25, armor="fur"),
        "urs_runapap": lambda: biped(uid, s * 0.9, U, "bear", "staff", robe=True, staff_glow=(120, 200, 255),
                                     head="hood", armor="fur"),
        "urs_kardfogu": lambda: rider(uid, s * 0.92, U, "ursan"),
        "urs_viharhollo": lambda: flyer(uid, s * 0.85, "raven", U),
        "urs_mammut": lambda: mammoth(uid, s * 0.8, U),
        "cro_csapszolga": lambda: insect(uid, s * 1.05, C, scythes=False, worker=True),
        "cro_karmos": lambda: insect(uid, s * 1.05, C),
        "cro_tuskekopo": lambda: serpent(uid, s * 0.95, C),
        "cro_savkopo": lambda: artillery(uid, s * 0.85, C),
        "cro_agyfalo": lambda: floater(uid, s * 1.0, C),
        "cro_szarnyas_rem": lambda: flyer(uid, s * 0.85, "rem", C),
        "cro_csapos_horror": lambda: horror(uid, s * 0.8, C),
        "wul_gyujtogeto": lambda: biped(uid, s, Wl, "dingo", "tool", head="headband", armor="leather", tail=True,
                                         gear=("basket",)),
        "wul_landzsas": lambda: biped(uid, s, dict(Wl, paint=(236, 230, 214)), "dingo", "lance", head="headband",
                                       armor="leather", tail=True, offhand="parry_shield", gear=("paint", "necklace")),
        "wul_ijas_vadasz": lambda: biped(uid, s, dict(Wl, paint=(236, 230, 214)), "dingo", "boomerang",
                                          head="headband", armor="leather", tail=True, gear=("paint", "basket")),
        "wul_holdsaman": lambda: biped(uid, s, Wl, "dingo", "staff", robe=True, staff_glow=(200, 230, 255),
                                       head="emufeathers", tail=True, gear=("totem_back", "necklace")),
        "wul_szellemfarkas": lambda: spirit_wolf(uid, s * 0.95, Wl),
        "wul_bolenylovas": lambda: rider(uid, s * 0.88, Wl, "wullof"),
        "wul_mennydorgesmadar": lambda: flyer(uid, s * 0.82, "thunderbird", Wl),
        "wul_totemorias": lambda: termite_giant(uid, s * 0.75, Wl),
        "wul_vandorszeker": lambda: travois(uid, s * 0.85, Wl),
        "wul_emu": lambda: emu_unit(uid, s * 1.0, Wl),
        # --- fejlődési irányok új egységei
        "wul_csordavezer": lambda: beast(uid, s * 0.95, "bison", dict(Wl, bison=(70, 46, 30), bison2=(104, 72, 46))),
        "wul_groomi_fioka": lambda: groomi(uid, s * 1.1, Wl),
        "wul_groomi_anya": lambda: groomi(uid, s * 0.92, Wl),
        "wul_totemfarago": lambda: biped(uid, s, dict(Wl, paint=(236, 230, 214)), "dingo", "tool", head="headband",
                                         armor="leather", tail=True, gear=("totem_back", "paint")),
        "wul_alomjaro": lambda: biped(uid, s, dict(Wl, cloth=(90, 70, 120)), "dingo", "staff", robe=True,
                                      staff_glow=(200, 180, 255), head="emufeathers", tail=True,
                                      gear=("necklace",)),
        "urs_medvealak": lambda: biped(uid, s * 0.85, dict(URS, paint=(190, 40, 30)), "bear", None, bulky=1.6,
                                       gear=("paint", "skull")),
        "urs_kurtos": lambda: biped(uid, s * 0.9, URS, "bear", "horn", bulky=1.15, gear=("banner_back", "necklace")),
        "urs_runaorias": lambda: stone_troll(uid, s * 0.72),
        "cro_robbanoatka": lambda: insect(uid, s * 1.1, dict(CRO, shell=(90, 150, 60), shell2=(50, 96, 40)),
                                          scythes=False),
        "cro_vadaszkarmos": lambda: insect(uid, s * 1.05, dict(CRO, shell=(150, 60, 84), shell2=(96, 36, 56))),
        "cro_ostromtaraj": lambda: artillery(uid, s * 0.85, dict(CRO, shell=(96, 54, 118), shell2=(60, 34, 80))),
        "cro_asoferegg": lambda: serpent(uid, s * 0.95, dict(CRO, shell=(150, 104, 70), shell2=(100, 66, 44))),
        "myc_tetemgomba": lambda: biped(uid, s, dict(MYC, cap=(150, 156, 120), stalk=(200, 200, 180)), "mushroom",
                                        "mushclub"),
        "myc_fagomba_orias": lambda: biped(uid, s * 0.8, dict(MYC, cap=(120, 88, 58), stalk=(170, 150, 120)),
                                           "mushroom", "mushclub", bulky=1.8),
        "myc_alomszovo": lambda: biped(uid, s, dict(MYC, cap=(110, 100, 200)), "mushroom", "staff",
                                       staff_glow=(170, 150, 255), robe=True),
        "elf_fopap": lambda: biped(uid, s, dict(ELF, cloth=(236, 210, 120)), "elf", "staff", robe=True,
                                   staff_glow=(255, 230, 120), head="feathers", armor="jade", cape=True),
        "elf_jaguar": lambda: biped(uid, s, dict(ELF, leather=(200, 150, 70)), "elf", "bow", head="plume",
                                    armor="leather", gear=("quiver",)),
        "elf_csillagjos": lambda: biped(uid, s, dict(ELF, cloth=(50, 52, 96)), "elf", "staff", robe=True,
                                        staff_glow=(150, 200, 255), head="plume"),
        "imp_praetorianus": lambda: biped(uid, s * 0.95, dict(IMP, metal=(206, 176, 100)), "mech", "gladius",
                                          offhand="scutum", head="crest", bulky=1.2, cape=True, gear=("pauldron",)),
        "wul_boleny": lambda: beast(uid, s * 1.05, "bison", Wl),
        "wul_groomi": lambda: groomi(uid, s * 0.95, Wl),
        "myc_sporaszedo": lambda: biped(uid, s, My, "mushroom", "tool", bulky=1.05, gear=("basket",)),
        "myc_pofeteg": lambda: puffball(uid, s * 0.95, My),
        "myc_tuskegomba_lovesz": lambda: biped(uid, s * 0.95, dict(My, cap=(120, 70, 150)), "mushroom", "tube",
                                                gear=("spores",)),
        "myc_kalapos_or": lambda: biped(uid, s * 0.9, dict(My, cap=(150, 104, 60)), "mushroom", "mushclub",
                                        offhand="cap_shield", bulky=1.25),
        "myc_alomsaman": lambda: biped(uid, s * 0.95, dict(My, cap=(80, 160, 170)), "mushroom", "staff",
                                       staff_glow=(120, 255, 220), robe=True),
        "myc_oriaspofeteg": lambda: puffball(uid, s * 0.85, dict(My, cap=(170, 110, 70)), big=True),
        "myc_lebego_pofeteg": lambda: floater(uid, s * 0.8, My, kind="puff"),
        "imp_fabrikator": lambda: insect(uid, s, Im, scythes=False, worker=True, mech=True),
        "imp_legios": lambda: biped(uid, s * 0.92, Im, "mech", "gladius", offhand="scutum", head="crest",
                                     gear=("pauldron",)),
        "imp_ballistarius": lambda: biped(uid, s * 0.92, Im, "mech", "crossbow", head="crest", gear=("quiver",)),
        "imp_szellemidezo": lambda: biped(uid, s * 0.92, dict(Im, cloth=(60, 60, 70)), "mech", "soulstaff", robe=True,
                                          staff_glow=Im["soul"], head="hood", gear=("lantern",)),
        "imp_centurio": lambda: mech_walker(uid, s * 0.75, Im),
        "imp_aquila": lambda: flyer(uid, s * 0.85, "aquila", Im),
        "imp_kolosszus": lambda: mech_walker(uid, s * 0.62, Im, big=True),
        "imp_szellemlegios": lambda: biped(uid, s * 0.92, Im, "ghost", "gladius", offhand=None, head="crest"),
        "imp_szellemkolosszus": lambda: biped(uid, s * 0.8, Im, "ghost", "axe", bulky=1.5, head="crest"),
        "vad_kristalybogar": lambda: crystal_bug(uid, s * 1.0),
        "vad_hamufarkas": lambda: ash_wolf(uid, s * 0.9),
        "vad_eterliderc": lambda: ether_wisp(uid, s * 0.95),
        "vad_kotroll": lambda: stone_troll(uid, s * 0.72),
        "vad_ercgolem": lambda: stone_troll(uid, s * 0.66, ore=True),
    }
    hs = s * 1.12  # a hősök kicsit nagyobbak
    E2 = dict(E, emblem="sun")
    table.update({
        "elf_csillagdardas": lambda: biped(uid, s, E2, "elf", "spear", head="plume", armor="jade"),
        "elf_obszidian_veto": lambda: artillery(uid, s * 0.8, dict(shell=(74, 68, 82), shell2=(46, 42, 52),
                                                                  glow=(70, 230, 200), flesh=(206, 194, 160))),
        "elf_jadeor": lambda: golem(uid, s * 0.72, dict(wood=(58, 160, 124), red=(40, 196, 156), turq=(234, 188, 62),
                                                       gold=(234, 188, 62))),
        "elf_hos_szelai": lambda: biped(uid, hs, E2, "elf", "staff", robe=True, staff_glow=(255, 214, 90),
                                        head="feathers", armor="jade", cape=True),
        "elf_hos_itzcoatl": lambda: biped(uid, hs, E2, "elf", "club", offhand="round_shield", head="feathers",
                                          armor="jade", cape=True, bulky=1.2),
        "elf_hos_xochi": lambda: biped(uid, hs, dict(E2, cloth=(60, 170, 150)), "elf", "staff", robe=True,
                                       staff_glow=(80, 240, 220), head="plume", armor="jade"),
        "urs_telepes": lambda: biped(uid, s * 0.92, U, "bear", "tool", bulky=1.15, gear=("basket", "necklace")),
        "urs_pajzshordozo": lambda: biped(uid, s * 0.9, U, "bear", "axe", offhand="plank_shield", bulky=1.22,
                                          gear=("pauldron", "paint")),
        "urs_szigonyvezo": lambda: biped(uid, s * 0.9, U, "bear", "spear", bulky=1.1, gear=("harpoons", "necklace")),
        "urs_berserker": lambda: biped(uid, s * 0.93, dict(U, paint=(190, 40, 30)), "bear", "twin_axe", bulky=1.25,
                                        gear=("paint", "pelt", "skull")),
        "urs_runapap": lambda: biped(uid, s * 0.9, U, "bear", "staff", staff_glow=(120, 200, 255), bulky=1.1,
                                     gear=("runes", "pelt", "necklace")),
        "urs_dardaor": lambda: biped(uid, s * 0.9, U, "bear", "spear", offhand="plank_shield", bulky=1.18,
                                     gear=("banner_back", "pauldron")),
        "urs_ostromszan": lambda: artillery(uid, s * 0.8, dict(shell=(132, 92, 54), shell2=(92, 60, 34),
                                                               glow=(170, 176, 186), flesh=(206, 180, 140))),
        "urs_kardfogu_tigris": lambda: beast(uid, s, "tiger", U),
        "urs_hos_ragna": lambda: biped(uid, hs * 0.92, dict(U, fur=(96, 70, 52)), "bear", "axe",
                                       offhand="plank_shield", bulky=1.34, gear=("banner_back", "pelt", "pauldron")),
        "urs_hos_hjalmar": lambda: biped(uid, hs * 0.9, dict(U, fur=(214, 210, 200), skin=(214, 210, 200),
                                                             fur2=(236, 232, 224)), "bear", "staff",
                                         staff_glow=(170, 225, 255), bulky=1.15,
                                         gear=("runes", "pelt", "totem_back")),
        "urs_hos_bjarka": lambda: biped(uid, hs * 0.92, dict(U, fur=(150, 96, 52), skin=(150, 96, 52)), "bear",
                                        "spear", bulky=1.3, gear=("harpoons", "paint", "necklace", "skull")),
        "cro_pancelbogar": lambda: insect(uid, s * 1.2, dict(C, shell=(96, 58, 120), shell2=(60, 36, 80))),
        "cro_guboj": lambda: cocoon(uid, s, C),
        "cro_szemhordozo": lambda: floater(uid, s * 0.95, dict(C, flesh=(170, 110, 150))),
        "cro_hos_keth": lambda: insect(uid, hs * 1.15, dict(C, shell=(150, 104, 66), shell2=(98, 66, 40))),
        "cro_hos_vor": lambda: horror(uid, hs * 0.62, dict(C, shell=(130, 60, 120))),
        "cro_hos_ssarha": lambda: floater(uid, hs * 1.05, dict(C, flesh=(200, 120, 210), glow=(220, 140, 255))),
        "wul_tomahawkos": lambda: biped(uid, s, dict(Wl, paint=(214, 150, 72)), "dingo", "woomera", head="headband",
                                         armor="leather", tail=True, gear=("paint", "necklace")),
        "wul_hos_tava": lambda: biped(uid, hs, Wl, "dingo", "boomerang", head="headband", armor="leather", tail=True,
                                      cape=True),
        "wul_hos_varnu": lambda: biped(uid, hs, Wl, "dingo", "staff", robe=True, staff_glow=(210, 230, 255),
                                       head="emufeathers", tail=True, cape=True),
        "wul_hos_kurrin": lambda: rider(uid, hs * 0.9, Wl, "wullof"),
        "myc_rothaszto": lambda: biped(uid, s * 0.95, dict(My, cap=(116, 124, 60)), "mushroom", "mushclub",
                                       bulky=1.15),
        "myc_hos_mor": lambda: biped(uid, hs, dict(My, cap=(90, 150, 200)), "mushroom", "staff", robe=True,
                                     staff_glow=(150, 255, 210), cape=True),
        "myc_hos_gyokeranya": lambda: biped(uid, hs * 0.95, dict(My, cap=(130, 96, 60)), "mushroom", "mushclub",
                                            offhand="cap_shield", bulky=1.5),
        "myc_hos_sporin": lambda: biped(uid, hs, dict(My, cap=(150, 60, 150)), "mushroom", "tube", cape=True),
        "imp_onager": lambda: artillery(uid, s * 0.8, dict(shell=Im["metal"], shell2=Im["iron"], glow=Im["soul"],
                                                          flesh=Im["bronze"])),
        "imp_hos_livia": lambda: biped(uid, hs * 0.92, dict(Im, cloth=(226, 226, 232)), "mech", "soulstaff",
                                       robe=True, staff_glow=Im["soul"], head="hood", cape=True),
        "imp_hos_nera": lambda: biped(uid, hs * 0.88, Im, "mech", "crossbow", head="crest", cape=True),
        "imp_hos_cassius": lambda: biped(uid, hs * 0.95, Im, "mech", "gladius", offhand="scutum", head="crest",
                                         bulky=1.4, cape=True),
    })
    if uid in table:
        return table[uid]()
    pal = {"elves": E, "ursan": U, "croftian": C, "wullof": Wl, "mycel": My, "imperium": Im}.get(fac, U)
    species = {"elves": "elf", "ursan": "bear", "wullof": "dingo", "mycel": "mushroom", "imperium": "mech"}.get(fac, "elf")
    generic = {
        "worker": lambda: biped(uid, s, pal, species, "tool") if fac != "croftian" else insect(uid, s, C, False, True),
        "warrior": lambda: biped(uid, s, pal, species, "axe"),
        "shieldbearer": lambda: biped(uid, s, pal, species, "axe", offhand="round_shield"),
        "archer": lambda: biped(uid, s, pal, species, "bow"),
        "caster": lambda: biped(uid, s, pal, species, "staff", robe=True) if fac != "croftian" else floater(uid, s, C),
        "rider": lambda: rider(uid, s * 0.9, pal, {"elves": "elf", "wullof": "wullof"}.get(fac, "ursan")),
        "beast": lambda: mammoth(uid, s * 0.8, U) if fac != "elves" else theropod(uid, s * 0.95, E),
        "flyer": lambda: flyer(uid, s * 0.85, "raven", U),
        "bug": lambda: insect(uid, s, C),
        "spitter": lambda: serpent(uid, s, C),
        "artillery": lambda: artillery(uid, s * 0.85, C),
        "horror": lambda: horror(uid, s * 0.8, C),
        "golem": lambda: golem(uid, s * 0.75, Wl),
        "puffball": lambda: puffball(uid, s, My),
        "mech": lambda: mech_walker(uid, s * 0.75, Im),
        "ghost": lambda: biped(uid, s, Im, "ghost", "gladius"),
    }
    return generic.get(shape, generic["warrior"])()


# a meteorkristály mindenhol lila (kriptonit-szerű)
CRYS = dict(shell=(70, 66, 110), shell2=(40, 34, 70), bone=(220, 170, 255), eye=(200, 120, 255), flesh=(120, 96, 170),
            glow=(176, 70, 255), stone=(118, 112, 106), stone2=(84, 80, 76), skin=(118, 112, 106), wood=(118, 112, 106),
            leather=(84, 80, 76), red=(118, 112, 106), turq=(176, 70, 255), gold=(150, 150, 140), metal=(150, 150, 150),
            hair=(60, 60, 60))


def _crystals(m, pts, s, bone="root"):
    for (x, y, z, h, ax, ay) in pts:
        m.capsule((x, y, z), (x + ax * s, y + ay * s, z + h * s), 0.9 * s, CRYS["glow"], bone, r1=0.1 * s, glow=True)


def crystal_bug(name, s):
    m = insect(name, s, CRYS)
    hb = 5 * s
    _crystals(m, [(-2 * s, 0, hb + 3 * s, 4, -1, 0), (-4 * s, 1 * s, hb + 2.6 * s, 3, -1, 1), (-4 * s, -1 * s, hb + 2.6 * s, 3, -1, -1),
                  (0, 0, hb + 2.8 * s, 3, 0.5, 0)], s)
    return m


def ash_wolf(name, s):
    m = Model(name)
    quadruped(m, s, "ashwolf", URS, length=1.0, height=0.9, saddle=False)
    hb = 9 * s * 0.9
    for k in range(6):  # felborzolt hamuszőr a hátán
        m.capsule((4 * s - k * 2 * s, 0, hb + 3 * s), (3 * s - k * 2 * s, 0, hb + 5.4 * s), 0.7 * s, (70, 66, 64), "root",
                  r1=0.15 * s)
    for side in (-1, 1):
        m.ellipsoid((9 * 1.42 * s, side * 1.4 * s, hb + 5.4 * s), (0.55 * s,) * 3, (255, 90, 50), "head", glow=True)
    m.rig = "quad"
    m.attack_style = "melee"
    m.height = hb + 10 * s
    return m


def ether_wisp(name, s):
    m = Model(name)
    alt = 11 * s
    m.ellipsoid((0, 0, alt + 3 * s), (3.4 * s,) * 3, (180, 240, 255), "root", glow=True)
    m.ellipsoid((0, 0, alt + 3 * s), (4.6 * s, 4.6 * s, 4.2 * s), (90, 170, 230), "root", tex=0.25, spec=0.8)
    for k in range(5):  # éterörvények
        a = k * 1.25
        m.capsule((math.cos(a) * 3 * s, math.sin(a) * 3 * s, alt + 2 * s + k * 0.6 * s),
                  (math.cos(a + 0.9) * 4.4 * s, math.sin(a + 0.9) * 4.4 * s, alt + 3.4 * s + k * 0.5 * s), 0.35 * s,
                  (200, 250, 255), "root", glow=True)
    for side in (-1, 1):
        m.ellipsoid((3.6 * s, side * 1.3 * s, alt + 4 * s), (0.6 * s,) * 3, (255, 255, 255), "root", glow=True)
    m.ellipsoid((0, 0, alt + 7.6 * s), (1.6 * s, 1.6 * s, 0.6 * s), (200, 200, 200), "root", team=True)
    for i in range(6):
        a = i / 6 * 2 * math.pi
        nm = f"t{i}"
        bx, by = math.cos(a) * 2.4 * s, math.sin(a) * 2.4 * s
        m.bone(nm, "root", (bx, by, alt + 1 * s))
        m.capsule((bx, by, alt + 1 * s), (bx * 1.8, by * 1.8, alt - 5 * s), 0.9 * s, (110, 190, 240), nm, r1=0.15 * s,
                  glow=True)
    m.rig = "floater"
    m.attack_style = "cast"
    m.altitude = alt
    m.height = alt + 9 * s
    return m


def stone_troll(name, s, ore=False):
    pal = dict(CRYS)
    if ore:
        pal.update(skin=(70, 74, 92), wood=(70, 74, 92), stone=(60, 64, 80))
    m = biped(name, s, pal, species="golem", weapon=None, bulky=1.5 if ore else 1.35)
    hip, sh = 10 * s, 18.5 * s
    for k in range(5 if ore else 3):  # kőtömbök a testen
        a = k * 1.3
        m.ellipsoid((math.cos(a) * 2.6 * s, math.sin(a) * 3.6 * s, hip + 2 * s + k * 1.6 * s), (2.2 * s, 2.0 * s, 1.8 * s),
                    pal["stone2"], "torso", tex=0.2)
    m.ellipsoid((0.5 * s, 0, sh + 3.4 * s), (3.0 * s, 3.2 * s, 2.6 * s), pal["stone"], "head", tex=0.2)
    m.capsule((2.8 * s, 0, sh + 2.6 * s), (3.6 * s, 0, sh + 1.6 * s), 1.4 * s, pal["stone2"], "head")
    for side in (-1, 1):
        m.ellipsoid((3.0 * s, side * 1.3 * s, sh + 3.8 * s), (0.5 * s,) * 3, (255, 200, 90) if not ore else CRYS["glow"],
                    "head", glow=True)
        m.capsule((3.4 * s, side * 1.0 * s, sh + 1.6 * s), (3.8 * s, side * 1.4 * s, sh + 3.2 * s), 0.4 * s, (230, 226, 210),
                  "head", r1=0.1 * s, tex=0)  # agyarak
        m.ellipsoid((1.2 * s, side * 6.0 * s, sh - 8.2 * s), (2.0 * s, 1.8 * s, 1.8 * s), pal["stone2"],
                    "arm_r" if side > 0 else "arm_l", tex=0.2)  # ököl
    _crystals(m, [(-2.2 * s, 0, sh + 1 * s, 5 if ore else 4, -1.4, 0), (-2 * s, 2.4 * s, sh, 4, -1, 1.2),
                  (-2 * s, -2.4 * s, sh, 4, -1, -1.2)] + ([(-1 * s, 0, sh + 3 * s, 6, -0.6, 0), (0, 3 * s, hip + 4 * s, 3, 0.4, 1.2),
                                                           (0, -3 * s, hip + 4 * s, 3, 0.4, -1.2)] if ore else []), s, "torso")
    m.attack_style = "melee"
    return m


def mech_walker(name, s, pal, big=False):
    """Centurio-mecha / Kolosszus: kétlábú lépegető, pilótafülkében lélekfény."""
    m = Model(name)
    hip = 13 * s
    m.bone("torso", "root", (0, 0, hip))
    m.bone("leg_r", "root", (0, 4 * s, hip))
    m.bone("leg_l", "root", (0, -4 * s, hip))
    m.bone("arm_r", "torso", (0, 7 * s, hip + 6 * s))
    m.bone("arm_l", "torso", (0, -7 * s, hip + 6 * s))
    m.bone("head", "torso", (0, 0, hip + 10 * s))
    for side, b in ((1, "leg_r"), (-1, "leg_l")):
        y = side * 4 * s
        m.box((0, y, hip - 1 * s), (2.6 * s, 2.0 * s, 2.0 * s), pal["metal2"], b, spec=0.5, mat="rivets")
        m.capsule((0, y, hip - 1 * s), (-2.2 * s, y, hip - 6.5 * s), 1.8 * s, pal["iron"], b, spec=0.6)
        m.ellipsoid((-2.2 * s, y, hip - 6.5 * s), (1.6 * s,) * 3, pal["bronze"], b, spec=0.9)
        m.capsule((-2.2 * s, y, hip - 6.5 * s), (0.8 * s, y, 1.6 * s), 1.5 * s, pal["metal"], b, spec=0.6)
        m.capsule((-1 * s, y, hip - 2 * s), (0, y, 3 * s), 0.5 * s, pal["bronze"], b, spec=1.0)  # dugattyú
        m.box((1.2 * s, y, 0.8 * s), (3.4 * s, 2.2 * s, 0.8 * s), pal["metal2"], b, spec=0.5)
    m.box((0, 0, hip + 5 * s), (5 * s, 6 * s, 5 * s), pal["metal"], "torso", spec=0.6, mat="rivets")
    m.box((2.4 * s, 0, hip + 6 * s), (2.8 * s, 4.4 * s, 3.4 * s), pal["bronze"], "torso", spec=0.9)
    m.box((5.2 * s, 0, hip + 7 * s), (0.3 * s, 2.6 * s, 1.0 * s), pal["soul"], "torso", glow=True)
    m.box((-4.4 * s, 0, hip + 6 * s), (1.4 * s, 5 * s, 3 * s), pal["iron"], "torso", spec=0.4)
    for k in (-1, 1):
        m.cylinder((-5.2 * s, k * 2.6 * s, hip + 8 * s), (-5.2 * s, k * 2.6 * s, hip + 14 * s), 1.0 * s, pal["iron"],
                   "torso", spec=0.4)
    m.box((0, 0, hip + 1.2 * s), (4 * s, 5.4 * s, 1.0 * s), (200, 200, 200), "torso", team=True)
    m.box((0.5 * s, 0, hip + 10.6 * s), (2.6 * s, 2.6 * s, 1.4 * s), pal["metal"], "head", spec=0.8)
    m.capsule((2 * s, 0, hip + 12.6 * s), (-3 * s, 0, hip + 12.6 * s), 1.0 * s, pal["red"], "head", team=True)
    # fegyverek
    if big:
        m.bone("gun", "torso", (0, 0, hip + 11 * s))
        m.box((0, 0, hip + 12 * s), (3.4 * s, 3.0 * s, 2.0 * s), pal["metal2"], "gun", spec=0.6)
        m.cylinder((2 * s, 0, hip + 12.6 * s), (14 * s, 0, hip + 13.6 * s), 1.8 * s, pal["iron"], "gun", spec=0.7)
        m.cylinder((13 * s, 0, hip + 13.5 * s), (14.6 * s, 0, hip + 13.7 * s), 2.3 * s, pal["bronze"], "gun", spec=0.9)
        for side in (-1, 1):  # támasztólábak ostrom-módhoz
            m.capsule((-3 * s, side * 6 * s, hip + 2 * s), (-6 * s, side * 9 * s, 0.5 * s), 0.8 * s, pal["iron"], "torso",
                      spec=0.5)
    for side, b in ((1, "arm_r"), (-1, "arm_l")):
        y = side * 7 * s
        m.ellipsoid((0, y, hip + 6 * s), (2.6 * s, 2.0 * s, 2.4 * s), pal["bronze"], b, spec=0.9)
        if side > 0 and not big:  # forgócsöves gépágyú
            m.cylinder((0, y, hip + 4 * s), (8 * s, y, hip + 4 * s), 1.6 * s, pal["metal2"], b, spec=0.6)
            for k in range(4):
                a = k * math.pi / 2
                m.capsule((7 * s, y + math.cos(a) * 0.9 * s, hip + 4 * s + math.sin(a) * 0.9 * s),
                          (12 * s, y + math.cos(a) * 0.9 * s, hip + 4 * s + math.sin(a) * 0.9 * s), 0.45 * s, pal["iron"],
                          b, spec=0.8)
        else:
            m.capsule((0, y, hip + 5 * s), (2 * s, y, hip - 1 * s), 1.6 * s, pal["iron"], b, spec=0.6)
            m.box((3.4 * s, y, hip - 1.6 * s), (1.6 * s, 0.6 * s, 3.4 * s), pal["metal"], b, spec=0.8)
    m.rig = "biped"
    m.attack_style = "siege" if big else "bow"
    m.height = hip + 15 * s
    return m


# ============================================================================
# ANIMÁCIÓK
def anims_for(m, defn):
    """{név: (képkockaszám, fps, ismétlődik)}"""
    a = {"idle": (4, 5, True), "walk": (6, 10, True), "attack": (6, 12, False), "death": (6, 9, False)}
    if defn.get("worker"):
        a["work"] = (6, 9, True)
    if defn.get("abilities") or defn.get("hero_abilities") or m.attack_style == "cast":
        a["cast"] = (6, 9, False)
    return a


def _sin(i, n, ph=0.0):
    return math.sin(2 * math.pi * i / n + ph)


def _seq(seq, i, n):
    """Kulcsképkocka-sor mintavétele n képkockára (lineáris interpoláció)."""
    t = i / max(1, n - 1) * (len(seq) - 1)
    k = int(t)
    f = t - k
    if k >= len(seq) - 1:
        return seq[-1]
    return seq[k] + (seq[k + 1] - seq[k]) * f


def pose(m, anim, i, n):
    rig = m.rig
    p = {}
    if anim == "death":
        return death_pose(m, i, n)
    if rig == "biped":
        if anim == "idle":
            p["torso"] = (0, 2 * _sin(i, n), 0)
            p["arm_r"] = (0, -4 + 3 * _sin(i, n), 0)
            p["arm_l"] = (0, 4 - 3 * _sin(i, n), 0)
            p["head"] = (0, 0, 6 * _sin(i, n, 1))
            p["tail"] = (0, 0, 15 * _sin(i, n))
            p["_offset"] = (0, 0, 0.3 * _sin(i, n))
        elif anim == "walk":
            a = 34 * _sin(i, n)
            p["leg_r"] = (0, a, 0)
            p["leg_l"] = (0, -a, 0)
            p["arm_r"] = (0, -0.8 * a, 0)
            p["arm_l"] = (0, 0.8 * a, 0)
            p["torso"] = (0, 6, 4 * _sin(i, n))
            p["tail"] = (0, 0, 20 * _sin(i, n, 1))
            p["_offset"] = (0, 0, abs(_sin(i, n)) * 0.9)
        else:
            st = m.attack_style if anim == "attack" else ("melee" if anim == "work" else "cast")
            if st == "bow":
                draw = _seq([0, 0.5, 1, 1, 0.1, 0], i, n)
                p["arm_l"] = (0, -88, 8)
                p["arm_r"] = (0, -80 + 30 * draw, -25 * draw)
                p["torso"] = (0, 0, -12)
            elif st == "throw":
                p["arm_r"] = (0, _seq([-40, -140, -165, -60, -20, -30], i, n), 0)
                p["torso"] = (0, _seq([0, -10, -14, 12, 6, 0], i, n), 0)
            elif st == "cast":
                k = _seq([0.1, 0.6, 1, 1, 0.6, 0.2], i, n)
                p["arm_r"] = (0, -160 * k, 0)
                p["arm_l"] = (0, -140 * k, 0)
                p["torso"] = (0, -10 * k, 0)
                p["head"] = (0, -10 * k, 0)
            elif st == "siege":
                p["torso"] = (0, _seq([0, -6, -10, 8, 3, 0], i, n), 0)
                p["arm_r"] = (0, _seq([0, -20, -30, 10, 0, 0], i, n), 0)
            else:
                if anim == "work":
                    seq = [-30, -110, -145, -40, 10, -10]
                else:
                    seq = [-50, -140, -170, -60, -10, -30]
                p["arm_r"] = (0, _seq(seq, i, n), 0)
                p["torso"] = (0, _seq([0, -8, -12, 18, 8, 2], i, n), _seq([0, -10, -16, 12, 5, 0], i, n))
                p["arm_l"] = (0, 20, 0)
                p["leg_l"] = (0, -10, 0)
                p["leg_r"] = (0, 12, 0)
    elif rig in ("rider", "quad"):
        if anim == "idle":
            p["head"] = (0, 4 * _sin(i, n), 4 * _sin(i, n, 1))
            p["_offset"] = (0, 0, 0.3 * _sin(i, n))
            p["trunk"] = (0, 15 * _sin(i, n), 10 * _sin(i, n, 2))
        elif anim == "walk":
            a = (22 if "trunk" in m.bones else 36) * _sin(i, n)
            p["leg_fl"] = (0, a, 0)
            p["leg_br"] = (0, a, 0)
            p["leg_fr"] = (0, -a, 0)
            p["leg_bl"] = (0, -a, 0)
            p["head"] = (0, 6 * _sin(i, n, 1), 0)
            p["_offset"] = (0, 0, abs(_sin(i, n)) * 1.0)
            p["trunk"] = (0, 20 * _sin(i, n), 0)
            p["rtorso"] = (0, 4 * _sin(i, n, 1), 0)
        else:
            k = _seq([0.2, 0.7, 1.0, 0.4, 0.1, 0], i, n)
            if rig == "rider":
                p["root"] = (0, -10 * k, 0)
                p["head"] = (0, 25 * k, 0)
                p["rarm_r"] = (0, _seq([-40, -150, -170, -30, -10, -20], i, n), 0)
            else:
                p["thrower"] = (0, _seq([0, 50, 70, -60, -20, 0], i, n), 0)
                p["trunk"] = (0, -60 * k, 0)
                p["root"] = (0, -6 * k, 0)
                p["head"] = (0, 20 * k, 0)
    elif rig == "theropod":
        if anim == "idle":
            p["head"] = (0, 5 * _sin(i, n), 6 * _sin(i, n, 1))
            p["_offset"] = (0, 0, 0.4 * _sin(i, n))
        elif anim == "walk":
            a = 30 * _sin(i, n)
            p["leg_r"] = (0, a, 0)
            p["leg_l"] = (0, -a, 0)
            p["head"] = (0, 6 * _sin(i, n, 1), 0)
            p["root"] = (0, 0, 4 * _sin(i, n))
            p["_offset"] = (0, 0, abs(_sin(i, n)) * 1.2)
        else:
            k = _seq([0.2, -0.4, 1.0, 0.6, 0.2, 0], i, n)
            p["head"] = (0, 25 * k, 0)
            p["jaw"] = (0, 30 * max(0, -k) + (25 if 0 < i < n // 2 else 0), 0)
            p["root"] = (0, 8 * k, 0)
    elif rig == "flyer":
        fl = 45 * _sin(i, n)
        sp = 1.0 if anim == "idle" else 1.4
        if anim in ("attack", "cast"):
            fl = _seq([40, 10, -30, -40, 0, 20], i, n)
            p["root"] = (0, _seq([0, 10, 25, 15, 5, 0], i, n), 0)
            p["head"] = (0, _seq([0, 10, 25, 15, 5, 0], i, n), 0)
        p["wing_l"] = (-fl * sp, 0, 0)
        p["wing_r"] = (fl * sp, 0, 0)
        p["_offset"] = (0, 0, 1.5 * _sin(i, n, 1))
    elif rig == "insect":
        if anim == "idle":
            p["head"] = (0, 0, 6 * _sin(i, n))
            p["arm_r"] = (0, 5 * _sin(i, n), 0)
            p["arm_l"] = (0, -5 * _sin(i, n), 0)
        elif anim == "walk":
            for k in range(3):
                ph = 0 if k % 2 == 0 else math.pi
                p[f"leg_r{k}"] = (0, 0, 25 * _sin(i, n, ph))
                p[f"leg_l{k}"] = (0, 0, -25 * _sin(i, n, ph + math.pi))
            p["_offset"] = (0, 0, abs(_sin(i, n)) * 0.6)
            p["arm_r"] = (0, 10 * _sin(i, n), 0)
            p["arm_l"] = (0, -10 * _sin(i, n), 0)
        else:
            k = _seq([0.2, -0.6, 1.0, 0.6, 0.2, 0], i, n)
            k2 = _seq([0.2, 1.0, -0.6, 0.6, 0.2, 0], i, n)
            p["arm_r"] = (0, 55 * k, 0)
            p["arm_l"] = (0, 55 * k2, 0)
            p["head"] = (0, 12 * k, 0)
            p["root"] = (0, 5 * k, 0)
    elif rig == "serpent":
        if anim == "idle":
            p["torso"] = (0, 3 * _sin(i, n), 6 * _sin(i, n, 1))
            p["head"] = (0, 0, 8 * _sin(i, n))
        elif anim == "walk":
            p["torso"] = (8 * _sin(i, n), 6, 10 * _sin(i, n, 1))
            p["root"] = (0, 0, 8 * _sin(i, n))
            p["_offset"] = (0, 0, abs(_sin(i, n)) * 0.5)
        else:
            k = _seq([0, -0.7, -1, 0.8, 0.3, 0], i, n)
            p["torso"] = (0, 18 * k, 0)
            p["head"] = (0, 15 * k, 0)
            p["arm_r"] = (0, -30 * abs(k), 0)
            p["arm_l"] = (0, -30 * abs(k), 0)
    elif rig == "artillery":
        if anim == "idle":
            p["sac"] = (0, 3 * _sin(i, n), 0)
            p["tube"] = (0, 2 * _sin(i, n, 1), 0)
        elif anim == "walk":
            for k in range(4):
                p[f"leg{k}"] = (0, 25 * _sin(i, n, k * math.pi / 2 + (math.pi if k in (1, 2) else 0)), 0)
            p["_offset"] = (0, 0, abs(_sin(i, n)) * 0.7)
        else:
            k = _seq([0, 0.5, 1, -0.6, -0.2, 0], i, n)
            p["tube"] = (0, -12 * k, 0)
            p["sac"] = (0, 8 * k, 0)
            p["root"] = (0, -4 * k, 0)
    elif rig == "horror":
        wig = 1.0 if anim == "idle" else 1.6
        for k in range(8):
            p[f"t{k}"] = (8 * wig * _sin(i, n, k * 0.9), 10 * wig * _sin(i, n, k * 1.3), 0)
        if anim == "walk":
            p["_offset"] = (0, 0, abs(_sin(i, n)) * 1.0)
        if anim in ("attack", "cast"):
            k = _seq([0.3, -0.5, 1.0, 0.5, 0.1, 0], i, n)
            for j in (0, 1, 7):
                p[f"t{j}"] = (0, -60 * k, 0)
            p["root"] = (0, 6 * k, 0)
    elif rig == "floater":
        for k in range(6):
            p[f"t{k}"] = (10 * _sin(i, n, k), 12 * _sin(i, n, k * 1.7), 0)
        p["_offset"] = (0, 0, 1.2 * _sin(i, n))
        if anim in ("attack", "cast"):
            k = _seq([0.2, 0.6, 1.0, 0.8, 0.4, 0.1], i, n)
            for j in range(6):
                p[f"t{j}"] = (0, -50 * k if j in (0, 1, 5) else 15 * k, 0)
            p["_offset"] = (0, 0, 2 * k)
    elif rig == "puffball":
        if anim == "idle":
            sq = 1 + 0.04 * _sin(i, n)
            p["_scale"] = (1 / sq ** 0.5, 1 / sq ** 0.5, sq)
        elif anim == "walk":
            h = abs(_sin(i, n))
            p["_offset"] = (0, 0, h * 3.0)
            sq = 0.9 + 0.15 * h
            p["_scale"] = (1 / sq ** 0.5, 1 / sq ** 0.5, sq)
            p["leg_r"] = (0, 30 * _sin(i, n), 0)
            p["leg_l"] = (0, -30 * _sin(i, n), 0)
        else:
            k = _seq([0, -0.4, 1.0, 0.6, 0.2, 0], i, n)
            p["root"] = (0, 18 * k, 0)
            sq = 1 - 0.12 * max(0, -k) + 0.06 * max(0, k)
            p["_scale"] = (1 / sq ** 0.5, 1 / sq ** 0.5, sq)
    return p


def death_pose(m, i, n):
    """Összeesés: a test oldalra/hátra dől, lesüllyed; a végén tetem."""
    t = min(1.0, i / max(1, n - 3))
    p = {}
    alt = getattr(m, "altitude", 0)
    if m.rig in ("biped", "theropod"):
        p["root"] = (0, -88 * t, 0)
        p["_offset"] = (-2 * t, 0, (m.height * 0.12) * t * (1 - t) * 2 - 0.5 * t)
        if m.rig == "biped":
            p["arm_r"] = (0, -60 * t, 0)
            p["arm_l"] = (0, -70 * t, 0)
            p["leg_r"] = (0, -20 * t, 0)
    elif m.rig in ("rider", "quad", "insect", "artillery", "horror", "serpent"):
        p["root"] = (88 * t, 0, 0)
        p["_offset"] = (0, -m.height * 0.15 * t, -0.2 * t)
        if m.rig == "rider":
            p["rtorso"] = (0, -60 * t, 0)
    elif m.rig == "puffball":
        sq = max(0.2, 1 - t * 0.85)
        p["_scale"] = (1 + 0.5 * t, 1 + 0.5 * t, sq)
    elif m.rig in ("flyer", "floater"):
        p["root"] = (60 * t, 0, 0)
        p["_offset"] = (0, 0, -alt * min(1.0, t * 1.2) + 1.2 * math.sin(math.pi * t))
        if m.rig == "flyer":
            p["wing_l"] = (-50 * (1 - t), 0, 0)
            p["wing_r"] = (50 * (1 - t), 0, 0)
    if i >= n - 2 and m.rig != "puffball":
        p["_scale"] = (1, 1, 0.55 if i == n - 2 else 0.4)
    return p
