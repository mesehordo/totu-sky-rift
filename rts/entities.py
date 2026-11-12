"""Egységek, épületek, lövedékek és területi hatások – állapotgépek."""
import math
import random

from . import dmath
from . import heroes

BURROW = {"beasas"}  # beásás-képességek (rejtett, mozdulatlan)
from .pathfinding import _clear_line, fits
from .world import TILE

STEER_TURNS = tuple((dmath.cos(a), dmath.sin(a)) for a in (0.45, 0.9, 1.35, 1.8, 2.4, math.pi))

REACH = 22  # ennyi px-en belül "odaért" egy épülethez/nyersanyaghoz

def summon_duration(owner, duration):
    if duration is None or owner.faction["id"] == "croftian":
        return duration
    return duration * owner.rules.get("summon_research_mult", 1.0)


def in_resource_shelter(entity):
    order = getattr(entity, "order", None)
    return bool(order and order["type"] == "gather" and order.get("state") == "work"
                and order["res"].alive and order["res"].kind == "stone")


def is_sheltered(entity):
    return getattr(entity, "inside", None) is not None or in_resource_shelter(entity)


def rect_dist(x, y, rect):
    rx, ry, rw, rh = rect
    dx = max(rx - x, 0, x - (rx + rw))
    dy = max(ry - y, 0, y - (ry + rh))
    return dmath.hypot(dx, dy)


def gap(a, b):
    """Két entitás széle közti távolság pixelben."""
    ar = getattr(a, "rect", None) if not a.is_unit else None
    br = getattr(b, "rect", None) if not b.is_unit else None
    if ar and br:
        dx = max(br[0] - (ar[0] + ar[2]), ar[0] - (br[0] + br[2]), 0)
        dy = max(br[1] - (ar[1] + ar[3]), ar[1] - (br[1] + br[3]), 0)
        return dmath.hypot(dx, dy)
    if ar:
        return rect_dist(b.x, b.y, ar) - b.radius
    if br:
        return rect_dist(a.x, a.y, br) - a.radius
    return dmath.hypot(a.x - b.x, a.y - b.y) - a.radius - b.radius


_NO_BONUS = {}


class Entity:
    is_unit = False
    is_building = False
    is_resource = False
    hero = False
    bonus = _NO_BONUS     # hősök: szint + tárgyak + örök bónuszok (heroes.recalc)
    aura_mods = None      # hős-aurák pillanatnyi hatása (heroes.update_auras)

    def __init__(self, game, owner, defn, x, y):
        self.id = game.new_id()
        self.owner = owner
        self.defn = defn
        self.x, self.y = x, y
        self.alive = True
        self.tags = set(defn.get("tags", []))
        if defn["kind"] == "building":
            self.tags.add("building")
        self.max_hp = owner.stat(defn, "hp")
        self.hp = self.max_hp
        self.max_shield = owner.stat(defn, "shield")
        self.shield = self.max_shield
        self.max_energy = owner.stat(defn, "energy")
        self.energy = min(50.0, self.max_energy)
        self.last_hit = -100.0
        self.attack = defn.get("attack")
        self.cooldown = 0.0
        self.flying = defn.get("flying", False)
        self.scan_t = random.random() * 0.4
        self.target = None
        self.flash = 0.0
        self.last_attack = -100.0  # animációhoz
        self.last_cast = -100.0

    @property
    def name(self):
        return getattr(self, "custom_name", None) or self.defn["name"]

    def mod(self, game, key):
        return 1.0 if key.endswith("_mult") else 0.0

    guard_armor = 0    # Ooni kapuőr nianok által adott páncél (épület)
    aura_armor = 0     # Vasbirodalom Ferrox-szobra: a körzetében álló épületek páncélja

    @property
    def armor(self):
        return self.owner.stat(self.defn, "armor") + self.guard_armor + self.aura_armor

    def sight(self):
        return self.owner.stat(self.defn, "sight", self.defn.get("sight", 7))

    def can_hit(self, other):
        if not self.attack or other is None or not other.alive or other.is_resource:
            return False
        if is_sheltered(other):  # épületben vagy bányában nem célozható
            return False
        t = self.attack.get("targets", ["ground"])
        return ("air" in t) if other.flying else ("ground" in t)

    _range_add = 0

    def atk_range(self):
        if not self.attack:
            return 0
        dep = self.defn.get("deploy")
        extra = dep.get("range_add", 0) if dep and getattr(self, "deployed", False) else 0
        return (self.attack["range"] + self.owner.stat(self.defn, "range", 0) + self._range_add + extra) * TILE

    def in_range(self, target):
        return gap(self, target) <= self.atk_range() + 3

    def attack_damage(self, game):
        dmg = (self.owner.stat(self.defn, "damage", self.attack["damage"]) + self.bonus.get("damage", 0)) \
            * self.mod(game, "damage_mult")
        dmg *= game.pack_mult(self)
        rg = game.rule_tags(self.owner, "rage", self) if self.is_unit else None
        if rg:  # vérszomj: minél sebesültebb, annál erősebb
            dmg *= 1 + rg["max"] * (1 - self.hp / self.max_hp)
        return dmg

    def try_attack(self, game, target):
        if self.cooldown > 0:
            return False
        atk = self.attack
        self.cooldown = atk["cooldown"]
        self.last_attack = game.time
        game.sfx("attack", self)
        dmg = self.attack_damage(game)
        self.target = target
        if self.is_unit:
            self.angle = dmath.atan2(target.y - self.y, target.x - self.x)
        if atk.get("projectile"):
            game.projectiles.append(Projectile(self, target, dmg, atk))
        else:
            game.hit(self, target, dmg, atk, ranged=False)
            game.add_fx("slash", target.x, target.y, 0.2, self.owner.color)
        return True

    rift = False          # Égkő-fény: meteorit vagy Égkő közelében áll
    near_reactor = False  # Vasbirodalom: Lélekreaktor közelében áll (Reaktorpajzs)

    def shield_can_regen(self, game):
        zone = self.defn.get("shield_zone") or self.owner.faction.get("shield_zone")
        if zone == "reactor":  # Reaktorpajzs: csak reaktor, meteorit vagy Égkő mellett
            return self.rift or self.near_reactor
        if zone == "pond":     # Gyöngyház-pajzs: csak saját tavacskán
            m = game.map
            tx, ty = int(self.x // TILE), int(self.y // TILE)
            return m.inb(tx, ty) and game.pond[ty * m.w + tx] == self.owner.id + 1
        return True

    def rift_rule(self, game, key, default=1.0):
        """A Égkő-fény faji erőssége (Szürke elfek erős, a többiek általános)."""
        if not self.rift:
            return default
        fr = self.owner.faction.get("rift", {})
        if key in fr:
            return fr[key]
        return game.data.game.get("rift", {}).get(key, default)

    def spell_mult(self, game):
        m = self.rift_rule(game, "spell_mult", 1.0)
        lm = self.owner.rules.get("lantern_magic")
        if lm and any(b.owner is self.owner and b.alive and b.complete and b.defn.get("light_radius")
                      and dmath.hypot(b.x - self.x, b.y - self.y) <= game.light_r(b) * TILE for b in game.buildings):
            m *= lm  # Lámpás-mágia
        return m

    def regen(self, game, dt):
        g = game.data.game
        if self.max_shield and self.shield < self.max_shield and \
                game.time - self.last_hit > self.defn.get("shield_delay", g["shield_regen_delay"]) and \
                self.shield_can_regen(game):
            rate = g["shield_regen_rate"] * self.rift_rule(game, "shield_regen_mult", 1.0)
            self.shield = min(self.max_shield, self.shield + rate * dt)
        r = self.owner.faction.get("regen", 0) + self.owner.stat(self.defn, "regen", 0) + self.bonus.get("regen", 0)
        if self.aura_mods:
            r += self.aura_mods.get("regen_add", 0)
        if r and self.hp < self.max_hp and (not self.is_building or self.complete):
            if game.in_own_field(self):
                r *= g["creep_regen_mult"] * self.owner.rules.get("creep_regen_mult", 1.0)
            self.hp = min(self.max_hp, self.hp + r * dt)
        if self.max_energy and self.energy < self.max_energy:
            er = (g["energy_regen"] + self.bonus.get("energy_regen", 0)) * self.rift_rule(game, "energy_mult", 1.0)
            erm = self.owner.rules.get("energy_regen_ids")
            if erm and self.defn["id"] in erm:  # Napszemű papok
                er *= erm[self.defn["id"]]
            self.energy = min(self.max_energy, self.energy + er * dt)
        if self.flash > 0:
            self.flash -= dt


class Unit(Entity):
    now = 0.0   # a játékidő (a Game.update állítja): az időfüggő állapotokhoz, pl. a némításhoz
    is_unit = True
    wait_t = 0.0  # torlódási várakozás; a set_order nullázza – alapérték a parancs nélküli útkéréshez és régi mentéshez

    def __init__(self, game, owner, defn, x, y):
        super().__init__(game, owner, defn, x, y)
        self.radius = defn.get("radius", 0.4) * TILE
        self.order = None
        self.queue = []
        self.path = []
        self.path_pending = False
        self.path_goal = None
        self.angle = random.uniform(-math.pi, math.pi)
        self.carry_kind = None
        self.carry_amt = 0
        self.last_res = None
        self.toggles = set()
        self.buffs = {}
        self.ab_ready = {}
        self.slow_until = 0.0
        self.slow_mult = 1.0
        self.is_worker = defn.get("worker", False)
        self.moving = False
        self.stuck_t = 0.0
        self.last_pos = (x, y)
        self.repaths = 0
        self.repath_t = 0.0
        self.cast_t = random.random()
        self.anim = random.random() * 10
        lifetime = defn.get("lifetime")
        self.lifetime = lifetime if "anchor" in self.tags else summon_duration(owner, lifetime)
        self.packed = None                     # wullof vándorszekér: a felszedett épület definíciója
        self.stun_until = 0.0
        self.poison = None      # méreg: (forrás, sebzés/mp, lejárat)
        self.burn = None        # égés (tűz elemű találat): (forrás, sebzés/mp, lejárat)
        self.frozen_until = 0.0  # fagyás (jég elemű találat) – a lassulás mellé, a megjelenítésnek
        self.shock_cd = 0.0     # sokk (villám): a következő bénulás legkorábban ekkor
        self.wet_until = 0.0    # átázott (víz): nem gyullad meg, a villám kétszer hosszabb sokkot ad
        self.rooted_until = 0.0  # gúzsba kötve (gyökér): nem mozdul, de támadhat
        self.asleep_until = 0.0  # alszik (álom): nem cselekszik, az első sebzésre felébred
        self.feared_until = 0.0  # megrémült: menekül a fear_from pont elől
        self.fear_from = None
        self.blind_until = 0.0  # elvakult: távolsági támadásainak egy része mellémegy
        self.silenced_until = 0.0  # elnémítva: nem varázsolhat
        self.marked_until = 0.0  # megjelölt: több sebzést kap, a jelölő a köd alól is látja
        self.marked_by = None
        self.sunder_until = 0.0  # páncéltörés
        self.sunder_armor = 0
        self.rider_id = None    # bölénylovas: a felültetett gyalogos egység azonosítója
        self.inside = None      # ursan helyőrség: az épület, amelyben az egység van
        self.morph = None       # croftian gubó: (cél-egység, hátralévő idő)
        self.hero = bool(defn.get("hero"))
        if self.hero:
            self.level, self.xp, self.skill_points = 1, 0.0, 1
            self.learned = {}                  # képesség -> megtanult szint
            self.inventory = [None] * 6
            self.perm = {}                     # tómokból kapott örök bónuszok
            self.base_hp = self.max_hp
            self.bonus = {}
            st = owner.hero_states.get(defn["id"])
            if st:  # újraélesztés vagy hadjárati átvitel
                heroes.restore(game, self, st)

    camp = None   # semleges szörny: a tábora
    home = None
    spores = 0    # micélia: spóratöltet (ölésből és a hálózatból)
    meals = 0     # micélia: elfogyasztott ellenségek az osztódásig
    hidden = False
    origin = None  # lélekhorgony: az eredeti gép azonosítója
    paid = False

    # ---- képesség-költségek: energia (elfek), életerő (croftian), spóra (micélia), újratöltés (ursan, wullof)
    def ab_ok(self, ab):
        if getattr(self, "silenced_until", 0.0) > Unit.now and ab.get("type") not in (
                "passive", "aura", "autoheal"):           # elnémítva: nem varázsolhat
            return False
        if self.energy < ab.get("energy", 0):
            return False
        if ab.get("faith") and self.owner.res.get("faith", 0) < ab["faith"]:  # Ö17: Áhítat / Tóének
            return False
        if ab.get("hp_cost") and self.hp <= ab["hp_cost"] + 5:
            return False
        if ab.get("spores") and self.spores < ab["spores"]:
            return False
        return True

    def ab_pay(self, ab):
        self.energy -= ab.get("energy", 0)
        if ab.get("faith"):
            self.owner.res["faith"] = self.owner.res.get("faith", 0) - ab["faith"]
        if ab.get("hp_cost"):
            self.hp -= ab["hp_cost"]
        if ab.get("spores"):
            self.spores -= ab["spores"]

    @property
    def burrowed(self):
        return bool(self.toggles) and any(t in BURROW for t in self.toggles)

    def passive(self, game, key):
        """A passzív képességek egy mezőjének értéke (pl. split_kills, detect), vagy None."""
        for a in self.abilities:
            v = game.data.abilities.get(a, {}).get(key)
            if v:
                return v
        return None

    # ---- módosítók / statok -------------------------------------------
    def mod(self, game, key):
        mult = key.endswith("_mult")
        v = 1.0 if mult else 0.0
        active = sorted(self.toggles) + [a for a, until in self.buffs.items() if until > game.time]
        for ab in active:
            m = self.ab(game, ab).get("modifiers", {})
            if key in m:
                v = v * m[key] if mult else v + m[key]
        am = self.aura_mods
        if am and key in am:
            v = v * am[key] if mult else v + am[key]
        if getattr(self, "broken", False):  # Harci kedv elfogyott: megtört
            bm = self.owner.faction.get("morale", {}).get("broken", {})
            if key in bm:
                v = v * bm[key] if mult else v + bm[key]
        return v

    def ab(self, game, ab_id):
        """A képesség adatai a (hősnél megtanult) szinten."""
        if self.hero and ab_id in self.learned:
            return heroes.ability(game, ab_id, self.learned[ab_id])
        return game.data.abilities.get(ab_id, {})

    @property
    def abilities(self):
        base = self.defn.get("abilities", [])
        req = self.defn.get("ability_research")
        if req:  # kutatáshoz kötött képességek (pl. a Groomi köpései)
            base = [a for a in base if a not in req or req[a] in self.owner.researched]
        apath = self.defn.get("ability_path")
        if apath:  # fejlődési irányhoz kötött képességek (pl. croftian átváltozás csak a Páncél-irányban)
            base = [a for a in base if a not in apath or apath[a] == self.owner.path]
        extra = getattr(self, "extra_abilities", None)  # küldetésben kapott (a történet adja, nem a fajta)
        if extra:
            base = list(base) + [x for x in extra if x not in base]
        if self.hero and self.learned:
            return list(base) + [a for a in self.defn.get("hero_abilities", []) if a in self.learned]
        return base

    @property
    def armor(self):
        return self.owner.stat(self.defn, "armor") + self._armor_add + self.bonus.get("armor", 0)

    _armor_add = 0

    def speed_base(self):
        return self.defn.get("speed", 2.5)

    # ---- gyűjtés és építés (Ötletdoboz Ö12/Ö13: külön építő és gyűjtő) -----
    def can_gather(self, kind):
        g = self.defn.get("gathers")
        return self.is_worker and (g is None or kind in g)

    @property
    def can_build(self):
        return self.is_worker and self.defn.get("builds", True)

    def carry_cap(self, game, kind):
        base = self.defn.get("carry", {}).get(kind, game.data.game["resources"][kind]["carry"])
        pct = self.owner.stat(self.defn, "carry_pct", 0)
        return max(1, int(round(base * (1 + pct / 100))))

    def speed(self, game):
        pct = self.owner.stat(self.defn, "speed_pct", 0) + self.bonus.get("speed_pct", 0)
        s = self.defn.get("speed", 2.5) * (1 + pct / 100)
        s *= self.mod(game, "speed_mult")
        if self.slow_until > game.time:
            s *= self.slow_mult
        if getattr(self, "rooted_until", 0.0) > game.time:   # gúzsba kötve: nem mozdul (támadni tud)
            return 0.0
        if not self.flying and game.on_own_creep(self):
            s *= game.data.game["creep_speed_mult"]
        if not self.flying:
            s *= game.pond_speed(self)
        s *= game.pack_speed(self)
        return s * TILE

    summon_hero = None

    @property
    def summon_seconds(self):
        if self.lifetime is None or "anchor" in self.tags or self.owner.faction["id"] == "croftian":
            return None
        return max(0, math.ceil(self.lifetime))

    def supply(self):
        if self.summon_hero is not None:
            return 0
        return self.defn.get("supply", 1) / self.defn.get("count", 1)

    # ---- parancsok -----------------------------------------------------
    def refund_build_orders(self):
        for pending in (self.order, *self.queue):
            if pending and "reserved_build_cost" in pending:
                self.owner.refund(pending.pop("reserved_build_cost"))

    def _resume_build_queue(self, order):
        queued = self.queue
        self.queue = []
        self.set_order(order)
        self.queue = queued
        if order is None:
            self.finish()

    def set_order(self, order, queue=False):
        if queue and (self.order or self.queue):
            self.queue.append(order)
            return
        self.refund_build_orders()
        self.queue = []
        self.order = order
        self.arrived_move_serial = None
        self.wait_t = 0.0
        self.repaths = 0
        self.yield_until = 0.0
        self.path = []
        self.path_goal = None
        self.path_pending = False

    def finish(self):
        self.order = None
        self.yield_until = 0.0
        self.path = []
        self.path_goal = None
        self.path_pending = False
        if self.queue:
            self.order = self.queue.pop(0)

    def set_path(self, game, gx, gy):
        self.path_goal = (gx, gy)
        self.stuck_t = 0
        self.last_pos = (self.x, self.y)
        if self.flying and not self.repaths:
            self.path = [(gx, gy)]
            self.path_pending = False
            return
        self.path = []
        self.path_pending = True
        game.request_path(self)

    def follow(self, game, dt):
        """Haladás az útvonalon. True, ha megérkezett."""
        if getattr(self, "deployed", False) and self.path:  # Felállítás: indulás előtt össze kell csukni
            self.pack_t = getattr(self, "pack_t", 0.0) + dt
            if self.pack_t < self.defn.get("deploy", {}).get("pack", 1):
                return False
            self.deployed, self.pack_t = False, 0.0
        if self.path_pending:
            return False
        if not self.path:
            until = getattr(self, "yield_until", 0.0)
            if until:
                if game.time >= until:
                    self.yield_until = 0.0
                    self.set_path(game, *self.path_goal)
                return False
            return True
        order = self.order
        if order and order.get("traffic_rank") and order.get("type") in ("move", "amove"):
            if "traffic_at" not in order:
                # A nagy egységek szűk átjáróhoz kisebb hullámokban indulnak.
                points = [(self.x, self.y)] + self.path
                tight = any(not _clear_line(game.map, *a, *b, self.radius + TILE * .8)
                            for a, b in zip(points, points[1:]))
                delay = min(5, order["traffic_rank"]) * (self.radius * 2 + TILE) / max(1, self.speed(game)) if tight else 0
                order["traffic_at"] = game.time + delay
            if game.time < order["traffic_at"]:
                return False
        px, py = self.path[0]
        dx, dy = px - self.x, py - self.y
        d = dmath.hypot(dx, dy)
        sp = self.speed(game)
        if sp <= 0:  # pl. ostrom-módban
            return False
        step = sp * dt
        if d > 0.5:
            self.angle = dmath.atan2(dy, dx)
        self.moving = True
        self.last_move = game.time
        nx, ny = (px, py) if d <= step else (self.x + dx / d * step, self.y + dy / d * step)
        if not self.flying:
            tile = (int(nx // TILE), int(ny // TILE))
            current_tile = (int(self.x // TILE), int(self.y // TILE))
            if not fits(game.map, nx, ny, self.radius) and (tile != current_tile or fits(game.map, self.x, self.y, self.radius)):
                self.repaths = min(8, self.repaths + 1)
                self.set_path(game, *(self.path_goal or self.path[-1]))
                return False
        blk = self._blocker(game, nx, ny)
        wt = getattr(self, "wait_t", 0.0)
        if blk is not None:  # egy másik egység áll az útban: kikerülés, kivárás vagy (a végcélnál) megérkezés
            self.wait_t = wt + dt
            if self.wait_t > 2 and self.order and self.order.get("auto") and self.order["type"] == "move":
                self.path = []
                return True
            if not self._steer(game, dx / max(d, 1e-6), dy / max(d, 1e-6), step, blk):   # nem fér el: vár (nem tolja a másikat)
                self.moving = False
        elif d <= step:
            self.x, self.y = px, py
            self.path.pop(0)
            self.wait_t = max(0.0, wt - dt * 0.5)
        else:
            self.x, self.y = nx, ny
            self.wait_t = max(0.0, wt - dt * 0.5)
        self.stuck_t += dt
        if self.stuck_t >= 0.8 + min(4, self.repaths) * 0.15:
            ox, oy = self.last_pos
            self.last_pos = (self.x, self.y)
            self.stuck_t = 0
            moved = dmath.hypot(self.x - ox, self.y - oy)
            progress = dmath.hypot(px - ox, py - oy) - dmath.hypot(px - self.x, py - self.y)
            if self.path and (moved < sp * 0.15 or (self.wait_t > 0.6 and progress < sp * 0.12)):
                self.yield_until = 0.0
                self.repaths = min(8, self.repaths + 1)
                self.set_path(game, *(self.path_goal or self.path[-1]))
            elif moved >= sp * 0.15:
                self.repaths = 0
        return not self.path and not self.path_pending and not getattr(self, "yield_until", 0.0)

    # ---- ütközés: nincs átfedés, nincs tologatás ---------------------------
    def _passes_through(self, v):
        """Gyűjtő munkások egymáson átmehetnek (mint a StarCraftban), különben torlódnának a bányánál."""
        if not (self.is_worker and v.is_worker):
            return False
        a, b = self.order, v.order
        return bool(a and b and a.get("type") in ("gather", "return") and b.get("type") in ("gather", "return"))

    def _blocker(self, game, nx, ny, candidates=None):
        """Az az egység, amelybe az (nx, ny) pontra lépve belecsúsznánk (csak a közeledés számít, a távolodás nem)."""
        best, bd = None, 1e9
        if candidates is None:
            candidates = game.collision_candidates(nx, ny, self.radius)
        for v in candidates:
            m = self.radius + v.radius
            ddx, ddy = v.x - nx, v.y - ny
            if abs(ddx) >= m or abs(ddy) >= m:
                continue
            if v is self or not v.alive or v.flying != self.flying or v.inside is not None or v.burrowed or self._passes_through(v):
                continue
            dn = dmath.hypot(ddx, ddy)
            if dn < m and (dn < bd or dn == bd and v.id < best.id):
                if dn < dmath.hypot(v.x - self.x, v.y - self.y) - 0.01:
                    best, bd = v, dn
        return best

    def _free(self, game, x, y, candidates=None):
        m = game.map
        r = self.radius
        legal = (r <= x <= m.w * TILE - r and r <= y <= m.h * TILE - r) if self.flying else fits(m, x, y, r)
        return legal and self._blocker(game, x, y, candidates) is None

    def _steer(self, game, ux, uy, step, blk):
        """Jobbra tartó kitérés, nagyobb szögekkel, ha az egyenes út tartósan foglalt."""
        blk_still = not blk.path and not blk.path_pending
        if (not self.flying and self.wait_t > 0.5 and not getattr(self, "yield_until", 0.0)
                and self.id > blk.id and blk.path):
            bx, by = blk.path[0]
            opposing = ux * (bx - blk.x) + uy * (by - blk.y) < -1
            large_jam = self.wait_t > 1.2 and self.radius > TILE / 2 and blk.wait_t > 0.6
            side_clearance = self.radius + blk.radius
            m = game.map
            constrained = not (fits(m, self.x - uy * side_clearance, self.y + ux * side_clearance, self.radius)
                               and fits(m, self.x + uy * side_clearance, self.y - ux * side_clearance, self.radius))
            if opposing and constrained and self._yield_corridor(game, ux, uy, blk):
                return True
            if large_jam and self._yield_corridor(game, blk.x - self.x, blk.y - self.y, blk):
                return True
        side = -1.0
        reach = self.radius + step
        nearby = [v for v in game.collision_candidates(self.x, self.y, reach)
                  if abs(v.x - self.x) < reach + v.radius and abs(v.y - self.y) < reach + v.radius]
        turns = STEER_TURNS[:4]
        if self.wait_t > 0.65 and blk_still:
            turns = STEER_TURNS[:-1]
        for ca, sa in turns:
            for sign in (side, -side):
                turn = sa * sign
                vx, vy = ux * ca - uy * turn, ux * turn + uy * ca
                tx, ty = self.x + vx * step, self.y + vy * step
                if self._free(game, tx, ty, nearby):
                    self.x, self.y = tx, ty
                    self.angle = dmath.atan2(vy, vx)
                    return True
        # az álló, tétlen saját egység félreáll (nem tolják el: maga lép odébb)
        if (self.wait_t > 2.4 and blk_still and blk.owner is self.owner and blk.order is None and blk.morph is None
                and blk.speed_base() > 0 and not getattr(blk, "deployed", False)):
            serial = self.order.get("move_serial") if self.order else None
            if serial is None or serial != getattr(blk, "arrived_move_serial", None):
                blk.make_way(game, ux, uy, self)
        return False

    def _yield_corridor(self, game, ux, uy, blocker):
        # Szembejövő társak közül csak az egyik tolat ki a szűk átjáróból.
        distance = self.radius + blocker.radius + 4
        # A csempefolyosó tengelye stabil akkor is, ha az előző kitérés fél lépéssel eltolt.
        if abs(ux) >= abs(uy):
            ux, uy = math.copysign(1, ux), 0
        else:
            ux, uy = 0, math.copysign(1, uy)
        m = game.map
        for tiles in range(1, 33):
            retreat = tiles * TILE / 2
            x, y = self.x - ux * retreat, self.y - uy * retreat
            if not _clear_line(m, self.x, self.y, x, y, self.radius):
                break
            for side in (-1, 1):
                px, py = x - uy * distance * side, y + ux * distance * side
                if self._free(game, px, py) and _clear_line(m, x, y, px, py, self.radius):
                    self.path = [(x, y), (px, py)]
                    self.yield_until = game.time + (retreat + distance * 2) / max(1, self.speed(game)) + 0.8
                    self.last_pos = (self.x, self.y)
                    self.stuck_t = self.wait_t = 0.0
                    return True
        return False

    def make_way(self, game, ux, uy, other):
        """Félreállás: a mozgó társ útjából oldalra lép, a szabadabb irányba (rövid lépés-parancs)."""
        if getattr(self, "_yield_t", -9.0) > game.time - 1.0:
            return
        self._yield_t = game.time
        dist = (self.radius + other.radius) * 1.3
        for sgn in (1.0, -1.0):
            px, py = -uy * sgn, ux * sgn
            for f in (1.0, 1.6):
                tx = self.x + px * dist * f + ux * dist * 0.3
                ty = self.y + py * dist * f + uy * dist * 0.3
                if fits(game.map, tx, ty, self.radius) and self._blocker(game, tx, ty) is None:
                    self.set_order({"type": "move", "x": tx, "y": ty, "auto": True})
                    return

    def move_to(self, game, x, y, dt):
        """Kényelmi: oda megy; True, ha odaért."""
        if self.path_goal != (x, y):
            self.set_path(game, x, y)
            return False
        if self.path_pending:
            return False
        return self.follow(game, dt)

    # ---- frissítés -----------------------------------------------------
    def update(self, game, dt):
        sa = getattr(self, "slow_after", None)
        if sa is not None and game.time >= sa[0]:
            self.slow_after = None
            self.slow_until, self.slow_mult = game.time + sa[1], 0.6
        ab = getattr(self, "after_buff", None)
        if ab is not None and self.buffs.get(ab[0], 0) <= game.time:
            self.after_buff = None
            if self.order is None or self.order.get("auto"):
                self.set_order(ab[1])
        if isinstance(self.inside, Unit) and self.origin is not None:
            carrier = self.inside
            if (not carrier.alive or carrier.inside is not None or carrier.morph is not None
                    or not game.anchor_order(carrier, self)):
                game.release_anchor(self, carrier)
            if self.inside is not None:
                return  # a Lélekhorgony élettartama hordozás közben szünetel
        if self.inside is not None:  # helyőrségben: csak gyógyul
            self.regen(game, dt)
            return
        if self.morph is not None:
            self.regen(game, dt)
            into, left = self.morph
            left -= dt
            if left <= 0:
                game.finish_morph(self, into)
            else:
                self.morph = (into, left)
            return
        self.regen(game, dt)
        self._armor_add = self.mod(game, "armor_add")
        self._range_add = self.mod(game, "range_add")
        if self.lifetime is not None:
            self.lifetime -= dt
            if self.lifetime <= 0:
                game.kill(self, None, corpse=False)
                return
        if self.cooldown > 0:
            self.cooldown -= dt
        sheltered = in_resource_shelter(self)
        if self.poison is not None:  # méreg: folyamatos, páncélt figyelmen kívül hagyó sebzés
            src, dps, until = self.poison
            if game.time >= until:
                self.poison = None
            elif not sheltered:
                self.hp -= dps * dt
                if self.hp <= 0:
                    game.kill(self, src if src is not None and src.alive else None)
                    return
        if getattr(self, "burn", None) is not None:  # égés: mint a méreg, a tűz elemű találat után
            src, dps, until = self.burn
            if game.time >= until:
                self.burn = None
            elif not sheltered:
                self.hp -= dps * dt
                if self.hp <= 0:
                    game.kill(self, src if src is not None and src.alive else None)
                    return
        self.moving = False
        self.anim += dt
        if self.stun_until > game.time or getattr(self, "asleep_until", 0.0) > game.time:
            return
        if getattr(self, "feared_until", 0.0) > game.time and self.fear_from is not None:
            self._fleeing = True
            self.move_to(game, self.fear_from[0], self.fear_from[1], dt)
            return
        if getattr(self, "_fleeing", False):
            self._fleeing = False
            self.path = []
            self.path_goal = None
            self.path_pending = False
        if self.origin is not None and self.hp >= self.max_hp - 0.5:
            game.rebody(self)
            return
        if self.burrowed:
            if self.owner.is_ai:
                for ab_id in sorted(self.toggles & BURROW):
                    self._ai_cast(game, ab_id, self.ab(game, ab_id))
            return
        self.autocast(game, dt)
        if not self.alive:  # pl. az önrobbantó atka felrobbant
            return
        o = self.order
        if o is None:
            if self.queue:
                self.order = self.queue.pop(0)
            else:
                self._idle(game, dt)
            return
        getattr(self, "_o_" + o["type"])(game, dt, o)

    def _scan(self, game, dt, radius):
        self.scan_t -= dt
        if self.scan_t > 0:
            return None
        self.scan_t = 0.4
        return game.find_enemy(self, radius)

    def _idle(self, game, dt):
        self.target = None
        if not self.attack or self.is_worker:
            return
        if self.camp is not None:  # semleges szörny: csak a tábora közelében támad
            hx, hy = self.home
            if dmath.hypot(self.x - hx, self.y - hy) > TILE * 1.5:
                self.set_order({"type": "move", "x": hx, "y": hy, "leash": True})
                return
            e = self._scan(game, dt, 5 * TILE)
            if e:
                self.order = {"type": "attack", "target": e, "auto": True, "home": self.home}
                for o in self.camp["units"]:  # az egész tábor felriad
                    if o is not self and o.alive and o.order is None:
                        o.order = {"type": "attack", "target": e, "auto": True, "home": o.home}
            return
        e = self._scan(game, dt, self.sight() * TILE)
        if e:
            self.order = {"type": "attack", "target": e, "auto": True, "home": (self.x, self.y)}

    def engage(self, game, dt, target):
        """Üldöz/támad. Visszatér: True, ha támadótávon belül van."""
        if self.in_range(target):
            self.path = []
            self.path_pending = False
            self.angle = dmath.atan2(target.y - self.y, target.x - self.x)
            dep = self.defn.get("deploy")
            if dep and not getattr(self, "deployed", False):  # Felállítás: lövés előtt fel kell állni
                self.deploy_t = getattr(self, "deploy_t", 0.0) + dt
                if self.deploy_t >= dep.get("time", 2):
                    self.deployed, self.deploy_t = True, 0.0
                    game.add_fx("dust", self.x, self.y, 0.4, (160, 140, 110))
                return True
            self.try_attack(game, target)
            return True
        self.deploy_t = 0.0
        self.repath_t -= dt
        if (not self.path and not self.path_pending) or self.repath_t <= 0:
            if self.repath_t <= 0 or not self.path:
                self.repath_t = 0.8
                self.set_path(game, target.x, target.y)
        self.follow(game, dt)
        return False

    def _finish_move(self, game, order):
        if dmath.hypot(self.x - order["x"], self.y - order["y"]) > 1:
            if not (order.get("leash") or order.get("auto")):
                game.notify(self.owner, f"{self.defn['name']}: a mozgás célpontja nem érhető el.")
        self.finish()
        self.arrived_move_serial = order.get("move_serial")

    def _o_move(self, game, dt, o):
        if o.get("leash"):  # visszatérő szörny: gyorsan regenerálódik, nem támad
            self.hp = min(self.max_hp, self.hp + self.max_hp * 0.1 * dt)
        if not o.get("started") or self.path_goal is None:
            o["started"] = True
            self.set_path(game, o["x"], o["y"])
            return
        if self.follow(game, dt):
            self._finish_move(game, o)

    def _o_amove(self, game, dt, o):
        sub = o.get("sub")
        if sub is not None and (not sub.alive or gap(self, sub) > self.sight() * TILE * 1.6):
            sub = o["sub"] = None
            o["started"] = False
        if sub is None:
            e = self._scan(game, dt, self.sight() * TILE)
            if e:
                o["sub"] = e
                return
            if not o.get("started") or self.path_goal is None:
                o["started"] = True
                self.set_path(game, o["x"], o["y"])
                return
            if self.follow(game, dt):
                self._finish_move(game, o)
            return
        self.engage(game, dt, sub)

    def _o_attack(self, game, dt, o):
        t = o["target"]
        if not self.can_hit(t) or not (t.defn.get("spike_tile") or self.owner.hostile(t.owner)):
            self.finish()
            return
        if o.get("auto"):
            hx, hy = o["home"]
            if dmath.hypot(self.x - hx, self.y - hy) > (7 if self.camp is not None else 9) * TILE:
                self.set_order({"type": "move", "x": hx, "y": hy, "leash": self.camp is not None})
                return
        self.engage(game, dt, t)

    def _o_hold(self, game, dt, o):
        self.path = []
        if not self.attack:
            return
        t = o.get("sub")
        if not self.can_hit(t) or not self.owner.hostile(t.owner) or not game.is_visible(self.owner, t) or not self.in_range(t):
            o["sub"] = None
            e = self._scan(game, dt, self.atk_range() + self.radius + 2 * TILE)
            if e and self.in_range(e):
                o["sub"] = e
            return
        self.angle = dmath.atan2(t.y - self.y, t.x - self.x)
        self.try_attack(game, t)

    # ---- gazdaság --------------------------------------------------------
    def _o_gather(self, game, dt, o):
        p = self.owner
        rdefs = game.data.game["resources"]
        r = o["res"]
        st = o.get("state", "go")
        if st == "go":
            if self.carry_amt > 0 and self.carry_kind != r.kind:
                o["state"] = "return"
                return
            if not self.can_gather(r.kind):  # ez a munkás ezt nem gyűjti (pl. Favágó oni aranyat)
                self.finish()
                return
            if not r.alive:
                nr = game.nearest_resource(r.kind, r.x, r.y, 12 * TILE)
                if nr is None:
                    self.finish()
                    return
                o["res"] = r = nr
                self.path = []
            self.last_res = r
            if gap(self, r) <= REACH:
                o["state"] = "work"
                # nagyobb rakomány: arányosan tovább rakodik (az előnye a kevesebb fordulóban van)
                o["timer"] = rdefs[r.kind]["gather_time"] * self.carry_cap(game, r.kind) / rdefs[r.kind]["carry"]
                if r.kind == "wood" and self.defn.get("tap_wood"):
                    o["timer"] = self.defn["tap_wood"].get("time", 10)
                self.path = []
                self.path_pending = False
                return
            if self.move_to(game, r.x, r.y, dt):
                # elérhetetlen vagy foglalt: keress másikat
                o["fails"] = o.get("fails", 0) + 1
                if o["fails"] > 3:
                    nr = game.nearest_resource(r.kind, self.x, self.y, 14 * TILE, exclude=r)
                    if nr is None:
                        self.finish()
                        return
                    o["res"] = nr
                    o["fails"] = 0
                self.path_goal = None
        elif st == "work":
            if not r.alive:
                o["state"] = "go"
                return
            self.angle = dmath.atan2(r.y - self.y, r.x - self.x)
            tap = self.defn.get("tap_wood") if r.kind == "wood" else None
            if tap:  # Kéregcsapolás: egy fán egy csapoló; a fa lassan fogy, a fa hordás nélkül jön
                tp = getattr(r, "tapper", None)
                if tp is not None and tp is not self and tp.alive and tp.order and tp.order.get("res") is r:
                    nr = game.free_tap_tree(self, r)
                    if nr is not None:
                        nr.tapper = self  # lefoglalja, hogy más ne induljon ugyanoda
                        o["res"], o["state"] = nr, "go"
                        self.path = []
                    else:  # nincs szabad fa a közelben: vár
                        self.finish()
                    return
                r.tapper = self
                o["timer"] -= dt
                if o["timer"] <= 0:
                    amt = tap.get("amount", 8)
                    p.res["wood"] = p.res.get("wood", 0) + amt
                    p.gathered["wood"] = p.gathered.get("wood", 0) + amt
                    r.amount -= amt * tap.get("drain", 0.5)
                    o["timer"] = tap.get("time", 10)
                    if r.amount <= 0:
                        game.map.remove_resource(r)
                        o["state"] = "go"
                return
            o["timer"] -= dt
            if o["timer"] <= 0:
                cap = self.carry_cap(game, r.kind)
                take = min(cap, r.amount)
                r.amount -= take
                self.carry_kind = r.kind
                self.carry_amt = take
                if r.amount <= 0:
                    game.map.remove_resource(r)
                o["state"] = "return"
                self.path_goal = None
        elif st == "return":
            if self.carry_amt <= 0:
                o["state"] = "go"
                return
            failed = o.setdefault("failed_dropoffs", [])
            drop = game.nearest_dropoff(p, self.carry_kind, self.x, self.y, exclude=failed)
            if drop is None:
                if failed and game.time >= o.get("retry_dropoffs_at", 0):
                    failed.clear()
                return
            if gap(self, drop) <= REACH:
                game.income(p, self.carry_kind, self.carry_amt)
                fi = self.defn.get("find_items")
                if fi and self.carry_kind in ("stone", "meteor") and game.rand() < fi.get("chance", 0.02):
                    game.found_item(self, drop, fi)
                self.carry_amt = 0
                self.carry_kind = None
                o.pop("failed_dropoffs", None)
                o["state"] = "go"
                self.path_goal = None
                self.path = []
                return
            if self.move_to(game, drop.x, drop.y, dt):
                self.path_goal = None
                if gap(self, drop) > REACH:
                    failed.append(drop.id)
                    o["retry_dropoffs_at"] = game.time + 2.0

    def _o_spike(self, game, dt, o):
        """Croftian: mérgező tüskék lerakása (a Csápszolga a mező mellett dolgozik, nem olvad bele)."""
        n = o["n"]
        rect = (o["tx"] * TILE, o["ty"] * TILE, n * TILE, n * TILE)
        if rect_dist(self.x, self.y, rect) <= self.radius + TILE * 0.8:
            self.path = []
            o["t"] += dt
            self.angle = dmath.atan2(rect[1] + rect[3] / 2 - self.y, rect[0] + rect[2] / 2 - self.x)
            if o["t"] >= o.get("time", 2):
                if not game.finish_spikes(self.owner, o["tx"], o["ty"], n):
                    self.owner.refund(o["cost"])
                if self.queue:
                    self.finish()
                else:
                    self.set_order(o.get("resume"))
            return
        if self.move_to(game, rect[0] + rect[2] / 2, rect[1] + rect[3] / 2, dt):
            o["fails"] = o.get("fails", 0) + 1
            self.path_goal = None
            if o["fails"] > 3:
                self.owner.refund(o["cost"])
                self.finish()

    def _o_dig(self, game, dt, o):
        """Huk’ucha: tavacska ásása (a Tóásó a terület mellett dolgozik)."""
        n = o["n"]
        rect = (o["tx"] * TILE, o["ty"] * TILE, n * TILE, n * TILE)
        if rect_dist(self.x, self.y, rect) <= self.radius + TILE * 0.8:
            self.path = []
            site = game.worksite(self.owner, "dig", o)
            if site is None:
                self.finish()
                return
            site["t"] += dt
            site["idle"] = 0.0
            o["t"] = site["t"]
            self.angle = dmath.atan2(rect[1] + rect[3] / 2 - self.y, rect[0] + rect[2] / 2 - self.x)
            if site["t"] >= game.POND_TIME:
                ok, _ = game.can_lay_pond(self.owner, o["tx"], o["ty"], check_explored=False)
                game.end_worksite(site)
                game.finish_pond(self.owner, o["tx"], o["ty"], n, ok)
                if not ok:
                    self.owner.refund(site["cost"])
                if self.queue:
                    self.finish()
                else:
                    self.set_order(o.get("resume"))
            return
        if self.move_to(game, rect[0] + rect[2] / 2, rect[1] + rect[3] / 2, dt):
            o["fails"] = o.get("fails", 0) + 1
            self.path_goal = None
            if o["fails"] > 3:
                if not o.get("site"):  # még el sem kezdte: a terv megszűnik, az ára visszajár
                    game.finish_pond(self.owner, o["tx"], o["ty"], n, False)
                    self.owner.refund(o["cost"])
                self.finish()

    def _o_found(self, game, dt, o):
        """Ö3: lapkő lerakása (a Fabrikátor a terület mellett dolgozik a megadott ideig)."""
        n = o["n"]
        rect = (o["tx"] * TILE, o["ty"] * TILE, n * TILE, n * TILE)
        if rect_dist(self.x, self.y, rect) <= self.radius + TILE * 0.8:
            self.path = []
            site = game.worksite(self.owner, "found", o)
            if site is None:
                self.finish()
                return
            site["t"] += dt
            site["idle"] = 0.0
            o["t"] = site["t"]
            self.angle = dmath.atan2(rect[1] + rect[3] / 2 - self.y, rect[0] + rect[2] / 2 - self.x)
            bdef = game.data.defs[o["bid"]]
            if site["t"] >= bdef.get("time", 3):
                game.end_worksite(site)
                ok, why = game.can_lay_foundation(self.owner, o["tx"], o["ty"], check_explored=False, n=n)
                if ok:
                    game.finish_foundation(self.owner, o["tx"], o["ty"], n)
                else:
                    self.owner.refund(site["cost"])
                if self.queue:  # sorba tett további lapkő / az épület maga
                    self.finish()
                else:
                    self.set_order(o.get("resume"))
            return
        if self.move_to(game, rect[0] + rect[2] / 2, rect[1] + rect[3] / 2, dt):
            o["fails"] = o.get("fails", 0) + 1
            self.path_goal = None
            if o["fails"] > 3:
                if not o.get("site"):
                    self.owner.refund(o["cost"])
                self.finish()

    def _o_build(self, game, dt, o):
        bdef = o["bdef"]
        tx, ty = o["tx"], o["ty"]
        w, h = bdef["size"]
        rect = (tx * TILE, ty * TILE, w * TILE, h * TILE)
        cx, cy = (tx + w / 2) * TILE, (ty + h / 2) * TILE
        if rect_dist(self.x, self.y, rect) <= self.radius + TILE * 0.8:
            ok, reason = game.can_place(self.owner, bdef, tx, ty)
            if not ok:
                self.owner.refund(o.pop("reserved_build_cost", bdef["cost"]))
                game.notify(self.owner, f"Nem lehet építeni: {reason}", (255, 120, 90))
                self._resume_build_queue(None if self.queue else o.get("resume"))
                return
            b = game.place_building(self.owner, bdef, tx, ty)
            o.pop("reserved_build_cost", None)
            style = game.builder_style(self.owner)
            if style == "consume":
                game.consume_unit(self)
            elif style in ("stay", "stay_single", "enter"):
                self._resume_build_queue({"type": "construct", "b": b, "resume": o.get("resume")})
            else:
                self._resume_build_queue(None if self.queue else o.get("resume"))
            return
        if self.move_to(game, cx, cy, dt):
            o["fails"] = o.get("fails", 0) + 1
            self.path_goal = None
            if o["fails"] > 3:
                self.owner.refund(o.pop("reserved_build_cost", bdef["cost"]))
                game.notify(self.owner, "Az építési hely elérhetetlen.", (255, 120, 90))
                self._resume_build_queue(None if self.queue else o.get("resume"))

    def _o_pray(self, game, dt, o):
        """Ö17: a szentély mellett fohászkodik / énekel (a termelést a Game.update_faith számolja)."""
        b = o["b"]
        if not b.alive or not b.complete:
            self.finish()
            return
        if gap(self, b) <= REACH + TILE * 0.5:
            o["at"] = True
            self.path = []
            self.angle = dmath.atan2(b.y - self.y, b.x - self.x)
            return
        o["at"] = False
        if self.move_to(game, b.x, b.y, dt):
            self.path_goal = None

    def _o_enter_mine(self, game, dt, o):
        b = o["b"]
        if not b.alive:
            self.finish()
            return
        if gap(self, b) <= REACH:
            game.enter_mine(self, b)
            return
        if self.move_to(game, b.x, b.y, dt):
            self.path_goal = None

    def _o_gate(self, game, dt, o):
        b = o["b"]
        if not b.alive:
            self.finish()
            return
        if gap(self, b) <= REACH:
            game.enter_gate(self, b)
            return
        if self.move_to(game, b.x, b.y, dt):
            self.path_goal = None

    def _o_construct(self, game, dt, o):
        b = o["b"]
        if not b.alive or b.complete:
            self._resume_build_queue(None if self.queue else o.get("resume"))
            return
        gp = gap(self, b)
        # ha a munkás megakadt az épület mellett (pl. átlósan a sarkánál), közelről is építhet
        stuck_close = gp <= REACH + TILE * 0.9 and (o.get("close") or (
            not self.path and not self.path_pending and getattr(self, "_stall", 0) > 1.0))
        if gp <= REACH or stuck_close:
            if stuck_close:
                o["close"] = True
            self.path = []
            self._stall = 0
            self.angle = dmath.atan2(b.y - self.y, b.x - self.x)
            if game.builder_style(self.owner) == "enter":  # Ursan: bemászik a félkész épületbe
                if getattr(b, "builder_in", None) is None:
                    game.enter_construction(self, b)
                elif b.builder_in is not self:  # már dolgozik bent valaki
                    self._resume_build_queue(None if self.queue else o.get("resume"))
                return
            b.builders += 1
            return
        ox, oy = self.x, self.y
        if self.move_to(game, b.x, b.y, dt):
            self.path_goal = None
        self._stall = getattr(self, "_stall", 0) + dt if dmath.hypot(self.x - ox, self.y - oy) < 0.05 else 0

    def _o_repair(self, game, dt, o):
        b = o["b"]
        if not b.alive or getattr(b, "inside", None) is not None or b.hp >= b.max_hp or not getattr(b, "complete", True):
            self.finish()
            return
        if gap(self, b) <= REACH:
            self.path = []
            self.angle = dmath.atan2(b.y - self.y, b.x - self.x)
            if getattr(b, "origin", None) is not None and not b.paid:
                cost = {k: int(v * 0.35) for k, v in game.data.defs[b.origin].get("cost", {}).items()}
                if not self.owner.can_afford(cost):
                    game._notify_missing(self.owner, cost)
                    self.finish()
                    return
                self.owner.pay(cost)
                b.paid = True
            b.hp = min(b.max_hp, b.hp + b.max_hp / max(20.0, b.defn.get("time", 30)) * 0.6 * dt)
            return
        if self.move_to(game, b.x, b.y, dt):
            self.path_goal = None

    def _o_enter(self, game, dt, o):
        b = o["b"]
        if not b.alive or not b.complete or b.owner is not self.owner or len(b.garrison) >= b.defn.get("garrison", 0):
            self.finish()
            return
        if rect_dist(self.x, self.y, b.rect) <= self.radius + REACH:
            game.enter_building(self, b)
            return
        if self.move_to(game, b.x, b.y, dt):
            o["fails"] = o.get("fails", 0) + 1
            self.path_goal = None
            if o["fails"] > 3:
                self.finish()

    def _o_pickup(self, game, dt, o):
        gi = o["item"]
        if not gi.alive:
            self.finish()
            return
        if dmath.hypot(gi.x - self.x, gi.y - self.y) <= self.radius + REACH:
            heroes.pick_up(game, self, gi)
            self.finish()
            return
        if self.move_to(game, gi.x, gi.y, dt):
            o["fails"] = o.get("fails", 0) + 1
            self.path_goal = None
            if o["fails"] > 3:
                self.finish()

    # ---- képességek ------------------------------------------------------
    def _o_place_ability(self, game, dt, order):
        ab_id = order["ab"]
        ab = self.ab(game, ab_id)
        x, y = order["x"], order["y"]
        radius = game.placement_radius(ab)
        morph = ab["type"] == "morph"
        if (not self.has_ability(ab_id) or self.wait_t > 6
                or not fits(game.map, x, y, radius)):
            game.notify(self.owner, "A kijelölt hely már nem használható.", (255, 160, 90))
            self.finish()
            return
        distance = dmath.hypot(x - self.x, y - self.y)
        minimum = self.radius + radius
        ready = distance < 0.01 if morph else minimum <= distance <= minimum + REACH
        if ready and _clear_line(game.map, self.x, self.y, x, y, 0):
            if not game.unit_placement_allowed(radius, x, y, ignore=self):
                game.notify(self.owner, 'A kijelölt hely időközben foglalttá vált.', (255, 160, 90))
                self.finish()
                return
            if ab["type"] == "morph":
                game.cmd_morph([self], ab_id)
            elif ab["type"] == "lay_mine":
                game.cmd_lay_mine([self], ab_id, position=(x, y))
            else:
                self.summon(game, ab_id, position=(x, y))
            self.finish()
            return
        if morph:
            if not self.move_to(game, x, y, dt) or dmath.hypot(x-self.x, y-self.y) < 0.01:
                return
        else:
            tried = order.setdefault("tried", [])
            if "approach" not in order:
                step = minimum + 3
                points = [(x + dmath.cos(i * math.tau / 8) * step,
                           y + dmath.sin(i * math.tau / 8) * step) for i in range(8)]
                free = [p for p in points if p not in tried and fits(game.map, *p, self.radius)
                        and _clear_line(game.map, *p, x, y, 0)]
                if free:
                    order["approach"] = min(free, key=lambda p: dmath.hypot(p[0]-self.x, p[1]-self.y))
            if "approach" in order:
                if self.move_to(game, *order["approach"], dt):
                    tried.append(order.pop("approach"))
                return
        game.notify(self.owner, "A kijelölt hely elérhetetlen.", (255, 160, 90))
        self.finish()

    def _o_anchor_rescue(self, game, dt, order):
        anchor = order["anchor"]
        if not anchor.alive or anchor.owner is not self.owner or anchor.inside not in (None, self):
            self.finish()
            return
        if self.wait_t > 6:
            game.notify(self.owner, "A Lélekhorgonyhoz vezető út elakadt.", (255, 160, 90))
            self.finish()
            return
        if anchor.inside is None:
            if gap(self, anchor) <= REACH:
                anchor.inside = self
                self.path = []
                self.path_goal = None
                self.path_pending = False
                return
            if not self.move_to(game, anchor.x, anchor.y, dt):
                return
            if gap(self, anchor) <= REACH:
                return
        else:
            x, y = order["x"], order["y"]
            minimum = self.radius + anchor.radius
            if (dmath.hypot(self.x - x, self.y - y) <= minimum + REACH
                    and game.anchor_drop_allowed(anchor, x, y)):
                anchor.x, anchor.y = x, y
                anchor.inside = None
                self.finish()
                return
            if game.anchor_drop_allowed(anchor, x, y, ignore=self):
                points = [(x + dmath.cos(i * math.tau / 8) * (minimum + 3),
                           y + dmath.sin(i * math.tau / 8) * (minimum + 3)) for i in range(8)]
                tried = order.setdefault("tried", [])
                if "approach" not in order:
                    free = [p for p in points if p not in tried and fits(game.map, *p, self.radius)]
                    if free:
                        order["approach"] = min(free, key=lambda p: dmath.hypot(self.x - p[0], self.y - p[1]))
                if "approach" in order:
                    approach = order["approach"]
                    if not self.move_to(game, *approach, dt):
                        return
                    tried.append(order.pop("approach"))
                    return
        game.notify(self.owner, "A Lélekhorgony mentése meghiúsult.", (255, 160, 90))
        self.finish()

    def _o_cast(self, game, dt, o):
        ab = self.ab(game, o["ab"])
        x, y = o["x"], o["y"]
        if not self.ab_ok(ab) or self.ab_ready.get(o["ab"], 0) > game.time:
            self.set_order(o.get("resume"))
            return
        if ab.get("type") == "blink":  # Fénylépés / Békaugrás: azonnali áthelyezés a pontra
            d = dmath.hypot(x - self.x, y - self.y)
            rng = self.blink_range(ab) * TILE
            if d > rng:  # a hatótáv szélére
                x, y = self.x + (x - self.x) * rng / d, self.y + (y - self.y) * rng / d
            spot = game.landing_position(x, y, self.radius, self.flying,
                                         origin=(self.x, self.y), max_distance=rng, ignore=self)
            if spot is None:
                self.set_order(o.get("resume"))
                return
            game.add_fx("cast", self.x, self.y, 0.5, tuple(ab.get("color", (150, 230, 255))), 22, ab.get("element"))
            self.x, self.y = spot
            self.path = []
            self.path_pending = False
            game.add_fx("impact", self.x, self.y, 0.5, tuple(ab.get("color", (150, 230, 255))), 22, ab.get("element"))
            if ab.get("land_damage"):  # Békaugrás: landoláskor megsebzi a közeli ellenséget
                for spike in game.spikes_in_radius(self.x, self.y, ab.get("land_radius", 1.2) * TILE):
                    game.apply_damage(self, spike, ab["land_damage"], ab, ranged=False, magic=True)
                for e in game.enemies_in_radius(self.owner, self.x, self.y, ab.get("land_radius", 1.2) * TILE,
                                                units_only=True):
                    game.apply_damage(self, e, ab["land_damage"], None, ranged=False, element=ab.get("element"))
            self.ab_pay(ab)
            self.ab_ready[o["ab"]] = game.time + ab.get("cooldown", 10)
            game.sfx("ability_" + o["ab"], self)
            self.last_cast = game.time
            self.set_order(o.get("resume"))
            return
        bolt = ab.get("type") == "bolt"
        t = None
        if bolt:
            t = o.get("target")
            if (t is None or not t.alive or is_sheltered(t)
                    or not (t.defn.get("spike_tile") or self.owner.hostile(t.owner))):
                t = o["target"] = game.find_enemy_near(self, x, y, 1.6 * TILE)
            if t is None:
                game.notify(self.owner, "Nincs célpont.", (255, 160, 90))
                self.set_order(o.get("resume"))
                return
            x, y = o["x"], o["y"] = t.x, t.y
        cast_range = ab.get("range", 6) * TILE + (t.radius if bolt else 0)
        if dmath.hypot(x - self.x, y - self.y) <= cast_range:
            self.path = []
            self.path_pending = False
            if bolt:
                spell = {"damage": ab.get("damage", 50) * self.spell_mult(game), "projectile": ab.get("projectile"),
                         "spell": True,
                         "slow": ab.get("slow"), "slow_dur": ab.get("duration", 1.5), "targets": ["ground", "air"],
                         "bounces": ab.get("bounces", 0), "splash": ab.get("splash"), "poison": ab.get("poison"),
                         "proj_speed": ab.get("proj_speed"), "element": ab.get("element"),
                         "color": ab.get("color")}
                game.add_fx("cast", self.x, self.y, 0.45, tuple(ab.get("color", (220, 200, 255))), 20, ab.get("element"))
                if spell["proj_speed"] is None:
                    spell.pop("proj_speed")
                if spell["projectile"]:
                    game.projectiles.append(Projectile(self, t, spell["damage"], spell))
                else:
                    game.hit(self, t, spell["damage"], spell, ranged=False)
                    game.add_fx("impact", t.x, t.y, 0.5, tuple(ab.get("color", (255, 255, 200))), 18, ab.get("element"))
            elif ab.get("type") == "raise":
                if not game.raise_corpse(self, ab, x, y):
                    game.notify(self.owner, "Nincs feltámasztható tetem a közelben.", (255, 160, 90))
                    self.set_order(o.get("resume"))
                    return
            elif not bolt:
                game.effects.append(AreaEffect(game, self, ab, x, y))
                game.add_fx("cast", self.x, self.y, 0.6, tuple(ab.get("color", (220, 200, 255))), 24, ab.get("element"))
            if ab.get("cooldown"):
                self.ab_ready[o["ab"]] = game.time + ab["cooldown"]
            self.ab_pay(ab)
            self.last_cast = game.time
            self.angle = dmath.atan2(y - self.y, x - self.x)
            game.sfx("ability_" + o["ab"], self)
            self.set_order(o.get("resume"))
            return
        if self.move_to(game, x, y, dt) and dmath.hypot(x - self.x, y - self.y) > cast_range:
            game.notify(self.owner, "A képesség célpontja nem érhető el.", (255, 160, 90))
            self.set_order(o.get("resume"))

    def blink_range(self, ab):
        rng = ab.get("range", 6)
        for rid, r in ab.get("range_research", {}).items():
            if rid in self.owner.researched:
                rng = max(rng, r)
        return rng

    def has_ability(self, ab_id):
        return ab_id in self.abilities

    def activate_buff(self, game, ab_id):
        ab = self.ab(game, ab_id)
        if self.ab_ready.get(ab_id, 0) > game.time or not self.ab_ok(ab):
            return False
        self.ab_pay(ab)
        self.buffs[ab_id] = game.time + ab.get("duration", 4)
        game.sfx("ability_" + ab_id, self)
        self.ab_ready[ab_id] = game.time + ab.get("cooldown", 10)
        if ab.get("after_slow"):  # Gőztúlnyomás: utána lehűl a kazán
            self.slow_after = (self.buffs[ab_id], ab["after_slow"])
        game.add_fx("cast", self.x, self.y, 0.7, tuple(ab.get("color", (255, 230, 150))), 24, ab.get("element"))
        if ab.get("drop_work") and self.order and self.order["type"] in ("gather", "construct", "repair"):
            self.after_buff = (ab_id, dict(self.order))  # Fegyverbe!: a riadó után visszamegy dolgozni
            self.set_order(None)
        return True

    def autocast(self, game, dt):
        abils = self.abilities
        if not abils:
            return
        self.cast_t -= dt
        if self.cast_t > 0:
            return
        self.cast_t = 0.5
        for ab_id in abils:
            ab = self.ab(game, ab_id)
            typ = ab.get("type")
            if typ == "autoheal":
                if game.time < self.ab_ready.get(ab_id, 0) or not self.ab_ok(ab):
                    continue
                t = game.most_damaged_ally(self, ab.get("range", 5) * TILE, ab.get("heal_tags", ("organic",)))
                if t:
                    t.hp = min(t.max_hp, t.hp + ab.get("amount", 10))
                    self.ab_pay(ab)
                    self.ab_ready[ab_id] = game.time + ab.get("cooldown", 1)
                    self.last_cast = game.time
                    game.add_fx("heal", t.x, t.y, 0.5, (120, 255, 140))
            elif self.owner.is_ai:
                self._ai_cast(game, ab_id, ab)
                if not self.alive:
                    return

    def _o_unpack(self, game, dt, o):
        """Wullof vándorszekér: a célhelyen újra felállítja a tábort."""
        bdef = self.packed
        w, h = bdef["size"]
        tx, ty = o["tx"], o["ty"]
        rect = (tx * TILE, ty * TILE, w * TILE, h * TILE)
        if rect_dist(self.x, self.y, rect) <= self.radius + TILE * 0.8:
            ok, reason = game.can_place(self.owner, bdef, tx, ty, check_req=False)
            if not ok:
                game.notify(self.owner, f"Nem lehet letelepíteni: {reason}", (255, 120, 90))
                self.finish()
                return
            game.unpack(self, tx, ty)
            return
        if self.move_to(game, (tx + w / 2) * TILE, (ty + h / 2) * TILE, dt):
            o["fails"] = o.get("fails", 0) + 1
            self.path_goal = None
            if o["fails"] > 3:
                self.finish()

    def cast_nova(self, game, ab_id):
        """Azonnali kör alakú csapás a hős körül: sebzés + bénítás/lassítás."""
        ab = self.ab(game, ab_id)
        if not self.ab_ok(ab) or self.ab_ready.get(ab_id, 0) > game.time:
            return False
        self.ab_pay(ab)
        self.ab_ready[ab_id] = game.time + ab.get("cooldown", 10)
        self.last_cast = game.time
        r = ab.get("radius", 2.5) * TILE
        if ab.get("heal"):  # Nap kegyelme: a saját élőlények azonnal gyógyulnak
            for e in game.query(self.x, self.y, r + 32):
                if e.is_unit and e.alive and e.owner is self.owner and "organic" in e.tags \
                        and dmath.hypot(e.x - self.x, e.y - self.y) <= r:
                    e.hp = min(e.max_hp, e.hp + ab["heal"])
        for e in game.enemies_in_radius(self.owner, self.x, self.y, r, units_only=True) if ab.get("damage", 50) else ():
            if not e.is_unit:
                continue
            game.apply_damage(self, e, ab.get("damage", 50) * self.spell_mult(game), None, ranged=False, magic=True,
                              element=ab.get("element"))
            if ab.get("stun") and ab.get("element") not in ("fear", "sleep"):
                e.stun_until = max(e.stun_until, game.time + ab["stun"])
            elif ab.get("slow") and not ab.get("stun"):
                e.slow_until, e.slow_mult = game.time + ab.get("duration", 2), ab["slow"]
        spike_damage = ab.get("building_damage", ab.get("damage", 50) * self.spell_mult(game))
        if spike_damage:
            for spike in game.spikes_in_radius(self.x, self.y, r):
                game.apply_damage(self, spike, spike_damage, ab, ranged=False, magic=True)
        if ab.get("building_damage"):  # Mészbombás manó: az épületeket is rombolja
            for b in game.enemies_in_radius(self.owner, self.x, self.y, r):
                if b.is_building:
                    game.apply_damage(self, b, ab["building_damage"], None, ranged=False, magic=True)
        game.add_fx("cast", self.x, self.y, 0.5, tuple(ab.get("color", (255, 220, 140))), 24, ab.get("element"))
        game.add_fx("enova", self.x, self.y, 0.7, tuple(ab.get("color", (255, 220, 140))), r, ab.get("element"))
        game.sfx("ability_" + ab_id, self)
        if ab.get("self_destruct"):  # robbanó atka: a robbanás megöli
            game.kill(self, None, corpse=False)
        return True

    def summon(self, game, ab_id, position=None):
        ab = self.ab(game, ab_id)
        if not self.ab_ok(ab) or self.ab_ready.get(ab_id, 0) > game.time:
            return False
        udef = game.data.defs[ab["unit"]]
        radius = udef.get("radius", 0.4) * TILE
        flying = udef.get("flying", False)
        count = ab.get("count", 1)
        permanent = self.hero and self.defn.get("faction") == "croftian"
        if permanent:
            hero_id = self.defn["id"]  # újraélesztés után is ugyanahhoz a hőshöz tartoznak
            remaining = 4 - game.hero_summon_count(self.owner, hero_id)
            if remaining <= 0:
                game.notify(self.owner, "Ez a hős már négy élő egységet idézett.", (255, 160, 90))
                return False
            count = min(count, remaining)
        spots = [(*position, radius)] if position is not None else []
        for k in range(count if position is None else 0):
            a = self.angle + (k - 0.5) * 1.2
            pos = game.landing_position(self.x + dmath.cos(a) * 24, self.y + dmath.sin(a) * 24,
                                        radius, flying, reserved=spots)
            if pos is None:
                return False
            spots.append((*pos, radius))
        self.ab_pay(ab)
        self.ab_ready[ab_id] = game.time + ab.get("cooldown", 1)
        self.last_cast = game.time
        game.sfx("ability_" + ab_id, self)
        for x, y, _ in spots:
            u = Unit(game, self.owner, udef, x, y)
            if permanent:
                u.lifetime = None
                u.summon_hero = hero_id
            else:
                duration = ab.get("lifetime", udef.get("lifetime", 30))
                u.lifetime = summon_duration(self.owner, duration * self.owner.rules.get("summon_life_mult", 1.0))
            game.units.append(u)
            game.add_fx("impact", u.x, u.y, 0.6, tuple(ab.get("color", (160, 200, 255))), 16, ab.get("element"))
        game.add_fx("cast", self.x, self.y, 0.7, tuple(ab.get("color", (160, 200, 255))), 26, ab.get("element"))
        return True

    def _ai_cast(self, game, ab_id, ab):
        typ = ab.get("type")
        if typ == "chant":  # wullof: egy ének mindig szóljon
            if not any(game.data.abilities.get(t, {}).get("type") == "chant" for t in self.toggles):
                game.cmd_toggle([self], ab_id)
            return
        if typ == "toggle" and ab.get("burrow"):  # croftian: sebesülten, ellenség nélkül beássa magát gyógyulni
            near = game.count_enemies(self, 7 * TILE)
            if ab_id not in self.toggles and self.hp < self.max_hp * 0.4 and near == 0 and self.order is None:
                self.toggles.add(ab_id)
            elif ab_id in self.toggles and (self.hp >= self.max_hp * 0.95 or near):
                self.toggles.discard(ab_id)
            return
        if typ == "blink":  # elf: a távolabbi célpont mellé lép
            o = self.order
            t = o and (o.get("target") if o["type"] == "attack" else o.get("sub"))
            rng = self.blink_range(ab)
            if t is not None and t.alive and self.ab_ok(ab) and self.ab_ready.get(ab_id, 0) <= game.time \
                    and min(3.0, rng * 0.5) * TILE < gap(self, t) < rng * TILE:
                self.set_order({"type": "cast", "ab": ab_id, "x": t.x, "y": t.y, "resume": o})
            return
        if typ == "passive":
            return
        if typ == "overdrive":  # Vasbirodalom: csak épen, nyugalomban, és ha van mit gyorsítani
            o = self.order
            b = o.get("b") if o and o.get("type") == "construct" else None
            if b is not None and b.alive and not b.complete and b.build_time - b.progress > 12 \
                    and self.hp > self.max_hp * 0.8 and self.ab_ready.get(ab_id, 0) <= game.time \
                    and game.count_enemies(self, 8 * TILE) == 0 and gap(self, b) <= REACH + TILE:
                game.cmd_overdrive([self], ab_id)
            return
        if typ == "ward":  # Ooni Lámpáspap: harcban, ha elég társ áll körülötte
            if self.ab_ok(ab) and self.ab_ready.get(ab_id, 0) <= game.time and game.count_enemies(self, 7 * TILE) >= 2:
                game.cmd_ward([self], ab_id)
            return
        if typ == "lay_mine":  # Huk’ucha Ikrarakó: nyugalomban, a bázis körül rakja le az ikrát
            if self.ab_ok(ab) and self.ab_ready.get(ab_id, 0) <= game.time and game.count_enemies(self, 8 * TILE) == 0 \
                    and (self.order is None or self.order["type"] == "move"):
                game.cmd_lay_mine([self], ab_id)
            return
        if typ == "nova" and ab.get("building_damage") and self.ab_ok(ab):
            if any(b.is_building for b in game.enemies_in_radius(self.owner, self.x, self.y, ab.get("radius", 2) * TILE)):
                self.cast_nova(game, ab_id)
                return
        if typ == "nova" and ab.get("heal") and not ab.get("damage"):
            if self.ab_ok(ab) and self.ab_ready.get(ab_id, 0) <= game.time:
                r = ab.get("radius", 3) * TILE
                hurt = [u for u in game.query(self.x, self.y, r) if u.alive and u.is_unit and u.owner is self.owner
                        and u.max_hp - u.hp >= ab["heal"] * 0.6]
                if len(hurt) >= 4:
                    self.cast_nova(game, ab_id)
            return
        if typ == "nova":
            if self.ab_ok(ab) and self.ab_ready.get(ab_id, 0) <= game.time:
                need = 4 if ab.get("ultimate") else 2
                if game.count_enemies(self, ab.get("radius", 2.5) * TILE) >= need:
                    self.cast_nova(game, ab_id)
            return
        if typ == "bolt":
            if not self.ab_ok(ab) or self.ab_ready.get(ab_id, 0) > game.time \
                    or (self.order and self.order["type"] == "cast"):
                return
            rng = ab.get("range", 6) * TILE
            cands = [e for e in game.query(self.x, self.y, rng) if e.alive and self.owner.hostile(e.owner)
                     and e.attack and dmath.hypot(e.x - self.x, e.y - self.y) <= rng]
            if cands:
                t = max(cands, key=lambda e: (e.hero, e.max_hp))
                self.set_order({"type": "cast", "ab": ab_id, "x": t.x, "y": t.y, "target": t, "resume": self.order})
            return
        if typ == "area" and ab.get("heal"):
            if not self.ab_ok(ab) or self.ab_ready.get(ab_id, 0) > game.time:
                return
            r = ab.get("radius", 3) * TILE
            hurt = [u for u in game.query(self.x, self.y, r) if u.alive and u.owner is self.owner
                    and u.hp < u.max_hp * 0.6]
            if len(hurt) >= 4 and not (self.order and self.order["type"] == "cast"):
                self.set_order({"type": "cast", "ab": ab_id, "x": self.x, "y": self.y, "resume": self.order})
            return
        if typ in ("summon", "area", "self_buff") and self.ab_ready.get(ab_id, 0) > game.time:
            return
        if typ == "summon":
            if game.count_enemies(self, 9 * TILE) >= 2:
                self.summon(game, ab_id)
            return
        if typ == "raise":
            if self.ab_ok(ab) and not (self.order and self.order["type"] == "cast"):
                c = game.find_corpse(self.x, self.y, (ab.get("range", 6) + 3) * TILE)
                if c is not None:
                    self.set_order({"type": "cast", "ab": ab_id, "x": c[2], "y": c[3], "resume": self.order})
            return
        if typ == "toggle" and ab.get("modifiers", {}).get("range_add"):
            # ostrom-mód: ha ellenség van a (megnövelt) lőtávon belül, telepít; ha rég nincs, felszed
            near = game.count_enemies(self, self.atk_range() + 3 * TILE)
            if near and ab_id not in self.toggles:
                self.toggles.add(ab_id)
            elif not near and ab_id in self.toggles and game.count_enemies(self, (self.sight() + 4) * TILE) == 0:
                self.toggles.discard(ab_id)
            return
        if typ == "area":
            if not self.ab_ok(ab) or (self.order and self.order["type"] == "cast"):
                return
            spot = game.find_cluster(self, (ab.get("range", 8) + 2) * TILE, ab.get("radius", 2) * TILE, 3)
            if spot:
                self.set_order({"type": "cast", "ab": ab_id, "x": spot[0], "y": spot[1], "resume": self.order})
        elif typ == "toggle":
            near = game.count_enemies(self, 8 * TILE, ranged_only=True)
            if near >= 2 and ab_id not in self.toggles:
                self.toggles.add(ab_id)
            elif near == 0 and ab_id in self.toggles:
                self.toggles.discard(ab_id)
        elif typ == "self_buff":
            o = self.order
            t = o and (o.get("target") if o["type"] == "attack" else o.get("sub"))
            if t and t.alive and gap(self, t) > 2 * TILE:
                self.activate_buff(game, ab_id)


class Building(Entity):
    is_building = True

    def __init__(self, game, owner, defn, tx, ty, complete=False):
        w, h = defn["size"]
        super().__init__(game, owner, defn, (tx + w / 2) * TILE, (ty + h / 2) * TILE)
        self.tx, self.ty, self.w, self.h = tx, ty, w, h
        self.rect = (tx * TILE, ty * TILE, w * TILE, h * TILE)
        self.radius = max(w, h) * TILE / 2
        self.build_time = defn.get("time", 30)
        self.complete = complete
        self.progress = self.build_time if complete else 0.0
        if not complete:
            self.hp = self.max_hp * 0.1
            self.shield = self.max_shield * 0.1
        self.queue = []
        self.rally = None
        self.powered = True
        self.larva = defn.get("larva", 0) if complete else 0
        self.larva_t = 0.0
        self.builders = 0
        self.creep_r = defn.get("creep_radius", 2.0) if complete else 2.0
        self.connected = True  # micélia-hálózat
        self.creep_t = 0.0
        self.garrison = []     # ursan helyőrség: a bent lévő egységek
        self.builder_in = None  # Ursan: az építés idejére bemászott munkás
        self.res = None         # bányára épült épület (Napfúró, Fertőzött tárna): a bánya
        self.miners = []        # Napfúró: a bent dolgozó munkások
        self.mine_n = 0         # Fertőzött tárna: a beolvasztott szolgák száma
        self.mine_acc = 0.0
        self.partner = None     # Torii-kapu: a pár másik fele
        self.gate_q = []        # Torii-kapu: az átkelésre váró egységek
        self.gate_t = 0
        self.gate_cd = 0

    def update(self, game, dt):
        self.regen(game, dt)
        if not self.complete:
            rate = game.build_rate(self)
            self.builders = 0
            if rate:
                inc = rate * dt
                self.progress += inc
                self.hp = min(self.max_hp, self.hp + self.max_hp * 0.9 * inc / self.build_time)
                if self.max_shield:
                    self.shield = min(self.max_shield, self.shield + self.max_shield * 0.9 * inc / self.build_time)
            if self.progress >= self.build_time:
                self.complete = True
                self.larva = self.defn.get("larva", 0)
                game.on_building_complete(self)
                if getattr(self, "builder_in", None) is not None:
                    game.leave_construction(self)
            return
        if self.defn.get("mine"):
            game.update_mine(self, dt)
        if self.defn.get("torii") and self.gate_q:
            game.update_gate(self)
        if self.cooldown > 0:
            self.cooldown -= dt
        if self.defn.get("garrison"):  # a bent lévők lövései erősítik (vagy fegyverrel látják el) az épületet
            n = len(self.garrison)
            base = self.defn.get("attack")
            if n:
                atk = dict(base) if base else {"damage": 5, "range": 6, "cooldown": 1.3,
                                               "targets": ["ground", "air"], "projectile": "arrow"}
                atk["damage"] = atk["damage"] + self.owner.rules.get("garrison_dmg", 4) * n
                atk["cooldown"] = max(0.5, atk["cooldown"] * (1 - 0.06 * n))
                self.attack = atk
            else:
                self.attack = base
        if self.attack:
            t = self.target
            if (not self.can_hit(t) or not self.owner.hostile(t.owner)
                    or not self.in_range(t) or not game.is_visible(self.owner, t)):
                self.target = t = None
                self.scan_t -= dt
                if self.scan_t <= 0:
                    self.scan_t = 0.4
                    self.target = t = game.find_enemy(self, self.atk_range() + 3 * TILE)
                    if t is not None and not self.in_range(t):
                        self.target = t = None
                else:
                    t = None
            if t is not None and self.powered:
                self.try_attack(game, t)
        maxl = self.defn.get("larva", 0) + (self.owner.rules.get("larva_add", 0) if self.defn.get("larva") else 0)
        if maxl:
            used = sum(1 for q in self.queue if q["kind"] == "unit")
            if self.larva + used < maxl:
                self.larva_t += dt
                if self.larva_t >= self.defn.get("larva_time", 10) / self.owner.rules.get("larva_mult", 1.0):
                    self.larva_t = 0
                    self.larva += 1
            else:
                self.larva_t = 0
        if not self.queue or not self.powered:
            return
        items = self.queue if self.defn.get("parallel") else self.queue[:1]
        hm = game.boost_mult(self, "hasten")  # Gyöngyérés
        for item in list(items):
            item["progress"] = min(item["time"], item["progress"] + dt * hm)
            if item["progress"] >= item["time"] and game.finish_item(self, item):
                self.queue.remove(item)


class Projectile:
    def __init__(self, src, target, dmg, atk, start=None, hit_ids=()):
        self.src = src
        self.owner = src.owner
        self.x, self.y = src.x, src.y
        if src.is_unit and src.flying:
            self.y -= 14
        if start is not None:  # pattanó lövedék (bumeráng): az előző célponttól indul
            self.x, self.y = start
        self.hit_ids = set(hit_ids)
        self.target = target
        self.tx, self.ty = target.x, target.y
        self.dmg = dmg
        self.atk = atk
        self.kind = atk.get("projectile", "arrow")
        self.speed = atk.get("proj_speed", 11 if self.kind == "boulder" else 15) * TILE
        self.alive = True
        self.sx, self.sy = self.x, self.y

    def update(self, game, dt):
        targetable = self.target.alive and not is_sheltered(self.target)
        if targetable:
            self.tx, self.ty = self.target.x, self.target.y - (14 if self.target.flying else 0)
        dx, dy = self.tx - self.x, self.ty - self.y
        d = dmath.hypot(dx, dy)
        step = self.speed * dt
        if d <= step:
            self.alive = False
            if targetable:
                game.hit(self.src, self.target, self.dmg, self.atk, ranged=True)
                if self.atk.get("bounces", 0) > 0:  # bumeráng: ívben továbbrepül a következő közeli ellenségre
                    self.hit_ids.add(self.target.id)
                    nxt, bd = None, 3.5 * TILE
                    for e in game.query(self.tx, self.ty, bd + 32):
                        if e.alive and e.id not in self.hit_ids and self.owner.hostile(e.owner) and e.is_unit                                 and game.seen_by(self.owner, e):  # rejtett/beásott egységre nem pattan
                            dd = dmath.hypot(e.x - self.tx, e.y - self.ty)
                            if dd <= bd:
                                nxt, bd = e, dd
                    if nxt is not None:
                        atk = dict(self.atk, bounces=self.atk["bounces"] - 1)
                        game.projectiles.append(Projectile(self.src, nxt, int(self.dmg * 0.75), atk,
                                                           start=(self.tx, self.ty), hit_ids=self.hit_ids))
            elif self.atk.get("splash"):
                game.splash(self.src, self.tx, self.ty, self.dmg, self.atk, exclude=None)
            if self.atk.get("spell"):
                game.add_fx("impact", self.tx, self.ty, 0.5, tuple(self.atk.get("color") or (230, 210, 255)), 18,
                            self.atk.get("element"))
            elif self.atk.get("element"):
                game.add_fx("impact", self.tx, self.ty, 0.3, (255, 230, 150), 9, self.atk.get("element"))
            else:
                game.add_fx("hit", self.tx, self.ty, 0.25, (255, 230, 150) if self.kind != "acid" else (150, 255, 100))
        else:
            self.x += dx / d * step
            self.y += dy / d * step


class AreaEffect:
    def __init__(self, game, src, ab, x, y):
        self.owner = src.owner
        self.src = src
        self.ab = ab
        self.x, self.y = x, y
        self.radius = ab.get("radius", 2) * TILE
        self.t = ab.get("duration", 4)
        self.dur = self.t
        self.tick = 0.0
        self.alive = True
        self.color = tuple(ab.get("color", (255, 200, 80)))

    def update(self, game, dt):
        self.t -= dt
        self.tick -= dt
        if self.tick <= 0:
            self.tick = 0.5
            if self.ab.get("heal"):
                for e in game.query(self.x, self.y, self.radius + 32):
                    if e.alive and e.owner is self.owner and dmath.hypot(e.x - self.x, e.y - self.y) <= self.radius:
                        e.hp = min(e.max_hp, e.hp + self.ab["heal"] * 0.5)
            if self.ab.get("dps"):
                damage = self.ab["dps"] * 0.5 * self.src.spell_mult(game)
                for spike in game.spikes_in_radius(self.x, self.y, self.radius):
                    game.apply_damage(self.src, spike, damage, self.ab, ranged=False, magic=True)
            if self.ab.get("buildings") and self.ab.get("dps"):  # Ordan villáma: az épületeket is sújtja
                for e in game.enemies_in_radius(self.owner, self.x, self.y, self.radius):
                    if e.is_building:
                        game.apply_damage(self.src, e, self.ab["dps"] * 0.5, None, ranged=False, magic=True)
            if self.ab.get("element") == "wind":
                push = (game.data.element_fx.get("wind") or {}).get("push", 0.45) * TILE
                for e in game.enemies_in_radius(self.owner, self.x, self.y, self.radius, units_only=True):
                    if e.flying or e.hero and e.defn.get("size_class") == "huge":
                        continue
                    dx, dy = e.x - self.x, e.y - self.y
                    d = dmath.hypot(dx, dy) or 1.0
                    nx, ny = e.x + dx / d * push, e.y + dy / d * push
                    if e._free(game, nx, ny):
                        e.x, e.y = nx, ny
                        if e.path_goal is not None:
                            e.set_path(game, *e.path_goal)
                        game.add_fx("ring", e.x, e.y, 0.2, (220, 225, 210))
            if self.ab.get("element") in ("root", "sleep", "fear", "blind", "water", "silence", "mark",
                                          "sunder") and not self.ab.get("dps"):
                for e in game.enemies_in_radius(self.owner, self.x, self.y, self.radius, units_only=True):
                    game.apply_element(self.src, e, self.ab["element"], 0)   # sebzés nélküli irányító mező
            for e in game.enemies_in_radius(self.owner, self.x, self.y, self.radius, units_only=True):
                if self.ab.get("dps"):
                    game.apply_damage(self.src, e, self.ab["dps"] * 0.5 * self.src.spell_mult(game), None, ranged=False,
                                      magic=True, element=self.ab.get("element"))
                if self.ab.get("slow"):
                    e.slow_until = game.time + 0.7
                    e.slow_mult = self.ab["slow"]
        if self.t <= 0:
            self.alive = False
