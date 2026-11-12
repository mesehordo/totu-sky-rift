"""Hősök fejlődése és RPG-elemek (WC3-szerű): tapasztalat, szintek, tanulható képességek,
auraszintek, tárgyak és leltár, semleges szörnytáborok zsákmánnyal.

A hős egy ``hero: true`` definíciójú egység. Szintenként a ``growth`` szerint erősödik és
kap egy képességpontot; a ``hero_abilities`` képességei 3 szintig taníthatók (a 2. szint
3., a 3. szint 5. hősszinttől), a végső (``ultimate``) 6. szinttől.
"""
import math
import random

from . import dmath
from .world import TILE

XP_LEVELS = [0, 200, 500, 900, 1400, 2000, 2700, 3500, 4400, 5400]
MAX_LEVEL = len(XP_LEVELS)
XP_RANGE = 12 * TILE
SLOTS = 6
STAT_KEYS = ("hp", "damage", "armor", "energy", "regen", "speed_pct", "energy_regen")

_merged = {}


def ability(game, ab_id, level=1):
    """A képesség adatai egy adott szinten (az alapadatok + a ``levels`` megfelelő eleme)."""
    key = (ab_id, level)
    m = _merged.get(key)
    if m is None or m.get("_src") is not game.data.abilities.get(ab_id):
        base = game.data.abilities[ab_id]
        m = {k: v for k, v in base.items() if k != "levels"}
        lv = base.get("levels")
        if lv:
            over = lv[max(0, min(level, len(lv)) - 1)]
            for k, v in over.items():
                if k == "modifiers":
                    m["modifiers"] = dict(m.get("modifiers", {}), **v)
                else:
                    m[k] = v
        m["_src"] = base
        _merged[key] = m
    return m


def max_ability_level(game, ab_id):
    return len(game.data.abilities[ab_id].get("levels") or [{}])


def required_hero_level(game, ab_id, next_level):
    if game.data.abilities[ab_id].get("ultimate"):
        return 6
    return 2 * next_level - 1


def can_learn(game, u, ab_id):
    if not u.hero or u.skill_points <= 0 or ab_id not in u.defn.get("hero_abilities", ()):
        return False
    cur = u.learned.get(ab_id, 0)
    if cur >= max_ability_level(game, ab_id):
        return False
    return u.level >= required_hero_level(game, ab_id, cur + 1)


def learn(game, u, ab_id):
    if not can_learn(game, u, ab_id):
        return False
    u.learned[ab_id] = u.learned.get(ab_id, 0) + 1
    u.skill_points -= 1
    return True


# ---------------------------------------------------------------------------
# statisztikák
def recalc(game, u, equipment_change=False):
    """A szintnövekedés, az örök bónuszok és a viselt tárgyak összesítése ``u.bonus``-ba."""
    if not u.hero:
        return
    items = game.data.items
    b = {k: 0.0 for k in STAT_KEYS}
    for k, v in u.defn.get("growth", {}).items():
        b[k] = b.get(k, 0) + v * (u.level - 1)
    for k, v in u.perm.items():
        b[k] = b.get(k, 0) + v
    for iid in u.inventory:
        it = items.get(iid) if iid else None
        if it and it.get("kind") == "equip":
            for k, v in it.get("bonus", {}).items():
                b[k] = b.get(k, 0) + v
    old_hp, old_en = u.max_hp, u.max_energy
    hp_ratio = u.hp / old_hp if old_hp > 0 else 0.0
    energy_ratio = u.energy / old_en if old_en > 0 else 0.0
    u.bonus = b
    u.max_hp = u.base_hp + b["hp"]
    u.hp = max(1.0, min(u.max_hp, u.hp + (u.max_hp - old_hp)))
    u.max_energy = u.defn.get("energy", 0) + b["energy"]
    u.energy = max(0.0, min(u.max_energy, u.energy + (u.max_energy - old_en)))
    if equipment_change:
        # Preserve ratios without a 1-HP floor: otherwise repeated swaps heal.
        u.hp = min(u.max_hp, max(0.0, hp_ratio * u.max_hp))
        u.energy = min(u.max_energy, max(0.0, energy_ratio * u.max_energy))


def xp_value(e):
    """Mennyi tapasztalatot ér egy legyőzött egység vagy épület."""
    if "xp" in e.defn:
        return e.defn["xp"]
    if getattr(e, "hero", False):
        return 100 + 60 * e.level
    cost = sum(e.defn.get("cost", {}).values()) / max(1, e.defn.get("count", 1))
    v = 8 + cost * 0.18
    return v * (0.5 if e.is_building else 1.0)


def grant_kill_xp(game, victim, src):
    if src is None or not src.owner.hostile(victim.owner) or getattr(victim, "lifetime", None) is not None:
        return
    p = src.owner
    heroes = [u for u in game.units if u.alive and u.hero and u.level < MAX_LEVEL and u.owner is p
              and dmath.hypot(u.x - victim.x, u.y - victim.y) <= XP_RANGE]
    if not heroes and getattr(src, "hero", False) and src.alive and src.level < MAX_LEVEL:
        heroes = [src]
    if not heroes:
        return
    amt = xp_value(victim) / len(heroes)
    for h in heroes:
        add_xp(game, h, amt)


def add_xp(game, u, amt):
    if not u.hero or u.level >= MAX_LEVEL:
        return
    u.xp += amt
    while u.level < MAX_LEVEL and u.xp >= XP_LEVELS[u.level]:
        u.level += 1
        u.skill_points += 1
        recalc(game, u)
        u.hp = min(u.max_hp, u.hp + u.max_hp * 0.25)
        game.add_fx("levelup", u.x, u.y, 1.2, (255, 220, 110))
        game.notify(u.owner, f"{u.name} elérte a(z) {u.level}. szintet!", (255, 220, 110))
    if u.level >= MAX_LEVEL:
        u.xp = XP_LEVELS[-1]


def xp_progress(u):
    if u.level >= MAX_LEVEL:
        return 1.0
    lo, hi = XP_LEVELS[u.level - 1], XP_LEVELS[u.level]
    return max(0.0, min(1.0, (u.xp - lo) / (hi - lo)))


# ---------------------------------------------------------------------------
# állapot mentése / visszaállítása (újraélesztés, hadjárat-átvitel)
def snapshot(u):
    return {"level": u.level, "xp": u.xp, "skill_points": u.skill_points, "learned": dict(u.learned),
            "inventory": list(u.inventory), "perm": dict(u.perm)}


def restore(game, u, st):
    if not st:
        return
    u.level = max(1, min(MAX_LEVEL, int(st.get("level", 1))))
    u.xp = float(st.get("xp", XP_LEVELS[u.level - 1]))
    u.learned = {k: int(v) for k, v in st.get("learned", {}).items() if k in u.defn.get("hero_abilities", ())}
    spent = sum(u.learned.values())
    u.skill_points = max(0, int(st.get("skill_points", u.level - spent)))
    inv = list(st.get("inventory", []))[:SLOTS]
    u.inventory = [i if i in game.data.items else None for i in inv] + [None] * (SLOTS - len(inv))
    u.perm = dict(st.get("perm", {}))
    recalc(game, u)
    u.hp, u.energy = u.max_hp, min(u.max_energy, max(u.energy, 50.0))


def set_min_level(game, u, level):
    """Kampány: a hős legalább ezen a szinten kezd (a pontokat a gép vagy a játékos osztja el)."""
    while u.level < min(level, MAX_LEVEL):
        add_xp(game, u, XP_LEVELS[u.level] - u.xp)


def auto_learn(game, u):
    """Gépi ellenfél (és kampány-kezdés): a végsőt előnyben részesítve, körkörösen tanul."""
    abs_ = u.defn.get("hero_abilities", [])
    while u.skill_points > 0:
        cands = [a for a in abs_ if can_learn(game, u, a)]
        if not cands:
            return
        ult = [a for a in cands if game.data.abilities[a].get("ultimate")]
        pick = ult[0] if ult else min(cands, key=lambda a: (u.learned.get(a, 0), abs_.index(a)))
        learn(game, u, pick)


# ---------------------------------------------------------------------------
# tárgyak
class GroundItem:
    is_unit = False
    is_building = False
    is_resource = False
    is_item = True

    def __init__(self, game, item_id, x, y):
        self.id = game.new_id()
        self.item = item_id
        self.x, self.y = x, y
        self.alive = True
        self.t = 0.0

    @property
    def name(self):
        return self.defn["name"]


def drop_item(game, item_id, x, y):
    m = game.map
    x = min(max(x, 0), m.w * TILE - 1)
    y = min(max(y, 0), m.h * TILE - 1)
    if not m.passable(int(x // TILE), int(y // TILE)):
        spot = m.nearest_free(int(x // TILE), int(y // TILE))
        if spot is not None:
            x, y = ((spot[0] + 0.5) * TILE, (spot[1] + 0.5) * TILE)
    gi = GroundItem(game, item_id, x, y)
    gi.defn = game.data.items[item_id]
    game.items.append(gi)
    game.add_fx("ring", x, y, 0.6, (255, 230, 140))
    return gi


def free_slot(u):
    for i, s in enumerate(u.inventory):
        if s is None:
            return i
    return -1


def pick_up(game, u, gi):
    if not gi.alive:
        return False
    i = free_slot(u)
    if i < 0:
        game.notify(u.owner, "Megtelt a leltár.", (255, 160, 90))
        return False
    gi.alive = False
    game.items = [g for g in game.items if g.alive]
    u.inventory[i] = gi.item
    recalc(game, u, equipment_change=True)
    game.notify(u.owner, f"{u.name}: {gi.defn['name']}", (255, 230, 150))
    return True


def use_item(game, u, slot):
    iid = u.inventory[slot] if 0 <= slot < len(u.inventory) else None
    it = game.data.items.get(iid) if iid else None
    if not it or it.get("kind") != "consumable":
        return False
    if it.get("heal"):
        if u.hp >= u.max_hp:
            game.notify(u.owner, "Az életerő már teljes.", (255, 160, 90))
            return False
        u.hp = min(u.max_hp, u.hp + it["heal"])
        game.add_fx("heal", u.x, u.y, 0.6, (120, 255, 140))
    if it.get("energy"):
        if u.energy >= u.max_energy and not (it.get("heal") or it.get("xp") or it.get("perm")):
            game.notify(u.owner, "Az energia már teljes.", (255, 160, 90))
            return False
        u.energy = min(u.max_energy, u.energy + it["energy"])
        game.add_fx("ring", u.x, u.y, 0.5, (120, 170, 255))
    if it.get("xp"):
        add_xp(game, u, it["xp"])
    for k, v in it.get("perm", {}).items():
        u.perm[k] = u.perm.get(k, 0) + v
        game.add_fx("levelup", u.x, u.y, 1.0, (255, 220, 110))
    u.inventory[slot] = None
    recalc(game, u)
    return True


def drop_from_slot(game, u, slot):
    iid = u.inventory[slot] if 0 <= slot < len(u.inventory) else None
    if not iid:
        return False
    u.inventory[slot] = None
    recalc(game, u, equipment_change=True)
    a = random.uniform(0, 2 * math.pi)
    drop_item(game, iid, u.x + dmath.cos(a) * 18, u.y + dmath.sin(a) * 18)
    return True


def roll_loot(game, rarities):
    items = game.data.items
    rarity = random.choice(rarities)
    pool = [k for k, v in items.items() if v.get("rarity") == rarity] or list(items)
    return random.choice(pool) if pool else None


# ---------------------------------------------------------------------------
# auraszintek
def _rect_gap(x, y, rect):
    rx, ry, rw, rh = rect
    dx = max(rx - x, 0, x - (rx + rw))
    dy = max(ry - y, 0, y - (ry + rh))
    return dmath.hypot(dx, dy)


def update_auras(game):
    """Fél másodpercenként: a hős-aurák hatása a közeli szövetségesekre (azonos módosító nem halmozódik)."""
    for u in game.units:
        u.aura_mods = None
    for b in game.buildings:
        b.aura_armor = 0
    game.update_rift()  # Égkő-fény (meteorit, Égkő)
    for b in game.buildings:  # bónuszt adó épületek (Vasbirodalom szobrai): csak amíg állnak, a körzetükben
        au = b.defn.get("aura")
        if not au or not b.complete or not b.alive:
            continue
        r = au.get("radius", 6) * TILE
        tags = set(au.get("tags", ()))
        mods = au.get("modifiers", {})
        for u in game.query(b.x, b.y, r):
            if u.owner is not b.owner or not u.alive or not u.is_unit or dmath.hypot(u.x - b.x, u.y - b.y) > r:
                continue
            if tags and not tags & set(u.tags):
                continue
            am = u.aura_mods = u.aura_mods or {}
            for k, v in mods.items():
                am[k] = max(am.get(k, 1.0 if k.endswith("_mult") else 0.0), v)
        ba = au.get("building_armor")
        if ba:
            for o in game.buildings:
                if o.owner is b.owner and o.alive and _rect_gap(b.x, b.y, o.rect) <= r:
                    o.aura_armor = max(o.aura_armor, ba)
    for b in game.buildings:  # gyógyító épületek (pl. croftian Tápgomba)
        ra = b.defn.get("regen_aura")
        if ra and b.complete:
            r = 6 * TILE
            for u in game.query(b.x, b.y, r):
                if u.owner is b.owner and u.alive and dmath.hypot(u.x - b.x, u.y - b.y) <= r:
                    u.aura_mods = u.aura_mods or {}
                    u.aura_mods["regen_add"] = max(u.aura_mods.get("regen_add", 0), ra)
    for h in game.units:
        if not h.alive or h.inside is not None:
            continue
        srcs = []
        if h.hero and h.learned:
            srcs += [(a, lv) for a, lv in h.learned.items()]
        for a in h.defn.get("abilities", ()):  # egységaurák (pl. rúnakő) és bekapcsolt énekek (wullof)
            t = game.data.abilities.get(a, {}).get("type")
            if t == "aura" or (t == "chant" and a in h.toggles):
                srcs.append((a, 1))
        for ab_id, lv in srcs:
            ab = ability(game, ab_id, lv)
            if ab.get("type") not in ("aura", "chant"):
                continue
            r = ab.get("radius", 7) * TILE
            mods = ab.get("modifiers", {})
            for u in game.query(h.x, h.y, r):
                if u.owner is not h.owner or not u.alive or dmath.hypot(u.x - h.x, u.y - h.y) > r:
                    continue
                am = u.aura_mods
                if am is None:
                    am = u.aura_mods = {}
                for k, v in mods.items():
                    if k.endswith("_mult"):
                        am[k] = max(am.get(k, 1.0), v) if v >= 1 else min(am.get(k, 1.0), v)
                    else:
                        am[k] = max(am.get(k, 0.0), v)


# ---------------------------------------------------------------------------
# semleges szörnytáborok
_CAMP_DATA = None


def camp_units(game, people, lvl):
    """Szövetséges nép tábora (data/camps.json): a nép szerepkörei (közelharcos, pajzsos, lövész, varázsló,
    nagy) a szintenkénti recept szerint; a zsákmány a vadon azonos szintű táboráé."""
    global _CAMP_DATA
    if _CAMP_DATA is None:
        import json
        import os
        from .data import ROOT
        try:
            with open(os.path.join(ROOT, "data", "camps.json"), encoding="utf-8") as f:
                _CAMP_DATA = json.load(f)
        except (OSError, ValueError):
            _CAMP_DATA = {}
    pe = _CAMP_DATA.get("peoples", {}).get(people)
    rec = _CAMP_DATA.get("recipe", {}).get(str(lvl))
    if not pe or not rec:
        return None
    units = {}
    fixed = pe.get("levels", {}).get(str(lvl))  # a vadon táboraihoz hangolt összetétel
    if fixed:
        units = {uid: n for uid, n in fixed.items() if uid in game.data.defs}
    else:
        for role, n in rec.items():
            uid = pe["roles"].get(role)
            if uid and uid in game.data.defs:
                units[uid] = units.get(uid, 0) + n
    nf = game.data.neutral_faction() or {}
    loot = (nf.get("camps", {}).get(str(lvl)) or {}).get("loot", ["common"])
    return {"units": units, "loot": loot, "people": people}


def place_camps(game):
    """Tükrözött tábor-elhelyezés: kezdőpontonként egy könnyű és egy közepes tábor, a felezőkön
    erősebbek, középen a „főnök”-tábor."""
    nf = game.data.neutral_faction()
    if nf is None or not nf.get("camps"):
        return
    from .game import Player
    p = Player(game, 99, nf["name"], nf, nf.get("color", (150, 140, 120)), True)
    p.neutral = True
    game.neutral = p
    m = game.map
    cx, cy = (m.w - 1) / 2, (m.h - 1) / 2
    starts = list(m.starts[:len(game.players)])
    from .mapgen import camp_spots
    spots = camp_spots(starts, cx, cy)
    camps = nf["camps"]
    peoples = dict((getattr(game, "mapdef", None) or {}).get("camps", {}))  # szintenként: melyik nép tábora
    for x, y, lvl in spots:
        who = peoples.get(str(lvl), "vadon")
        info = camp_units(game, who, lvl) if who != "vadon" else camps.get(str(lvl))
        if not info:
            continue
        tx, ty = int(x), int(y)
        if not m.inb(tx, ty):
            continue
        free = m.nearest_free(tx, ty)
        if free is None:
            continue
        if any(dmath.hypot(free[0] - s[0], free[1] - s[1]) < 12 for s in starts):
            continue
        hx, hy = (free[0] + 0.5) * TILE, (free[1] + 0.5) * TILE
        camp = {"x": hx, "y": hy, "level": lvl, "units": [], "loot": list(info.get("loot", ["common"])),
                "done": False}
        k = 0
        from .entities import Unit
        for uid, n in info["units"].items():
            udef = game.data.defs.get(uid)
            if udef is None:
                continue
            for _ in range(n):
                a = k * 2.1
                r = 22 + 8 * (k % 2)
                u = Unit(game, p, udef, hx + dmath.cos(a) * r, hy + dmath.sin(a) * r)
                u.camp = camp
                u.home = (u.x, u.y)
                game.units.append(u)
                camp["units"].append(u)
                k += 1
        game.camps.append(camp)


def update_camps(game):
    for camp in game.camps:
        if camp["done"]:
            continue
        if all(not u.alive for u in camp["units"]):
            camp["done"] = True
            iid = roll_loot(game, camp["loot"])
            if iid:
                drop_item(game, iid, camp["x"], camp["y"])
            if camp["level"] >= 3 and random.random() < 0.6:
                drop_item(game, random.choice(["gyogyfozet", "eterfozet"]), camp["x"] + 20, camp["y"] + 8)
