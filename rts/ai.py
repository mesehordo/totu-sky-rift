"""Adatvezérelt gépi ellenfél. Ugyanazokat a Game.cmd_* parancsokat használja, mint a játékos."""
import math
import random

from . import dmath
from . import heroes
from .world import B_BUILDING, B_RESOURCE, TILE


class AI:
    def __init__(self, game, player):
        self.g = game
        self.p = player
        self.cfg = player.faction.get("ai", {})
        self.t = random.random()
        self.last_decision = game.time
        self.attack_supply = self.cfg.get("attack_supply", 16)
        self.attack_t = 0.0
        self.seen_units, self.seen_blds = {}, {}
        self.enemy_value = 0.0
        self.scouted = False
        self.state = "build"  # build -> gather -> attack -> (retreat) -> build
        self.state_t = 0.0
        self.hold_until = 0.0  # kampány: eddig nem indít támadást
        self.extra_t = 0.0
        self.creep_t = 60.0
        self.reserve = {}  # a hősre félretett nyersanyag
        self.rng = random.Random(game.seed + player.id * 7919)
        self.period = 1.0
        diff = getattr(player, "difficulty", "normal")
        if diff == "easy":  # könnyű: később támad, nagyobb sereggel, lassabban dönt
            self.hold_until = 360
            self.attack_supply = int(self.attack_supply * 1.4)
            self.period = 1.6
        elif diff == "hard":  # nehéz: hamarabb támad, gyorsabban dönt, kezdő nyersanyag-előny
            self.attack_supply = int(self.attack_supply * 0.8)
            self.period = 0.7
            for k in player.res:
                player.res[k] += 200

    # ------------------------------------------------------------------
    def update(self, dt):
        self.t -= dt
        if self.t > 0 or self.p.defeated:
            return
        self.t = self.period
        g, p = self.g, self.p
        elapsed = g.time - getattr(self, "last_decision", g.time - self.period)
        self.last_decision = g.time
        self.my_units = [u for u in g.units if u.owner is p]
        self.my_blds = [b for b in g.buildings if b.owner is p]
        self.workers = [u for u in self.my_units if u.is_worker]
        self.army = [u for u in self.my_units if not u.is_worker and "livestock" not in u.tags]
        hqs = [b for b in self.my_blds if b.defn["id"] == p.faction["hq"]]
        self.base = hqs[0] if hqs else (self.my_blds[0] if self.my_blds else None)
        if self.base is None:
            return
        self.manage_workers(elapsed)
        self.manage_supply()
        self.manage_build(elapsed)
        self.manage_production()
        self.manage_research()
        self.manage_ally()
        self.manage_heroes(elapsed)
        self.manage_army(elapsed)

    # ------------------------------------------------------------------
    def pending(self, bid):
        """Meglévő + épülő + úton lévő épületek száma."""
        n = sum(1 for b in self.my_blds if b.defn["id"] == bid)
        for u in self.workers:
            o = u.order
            if o and o["type"] == "build" and o["bdef"]["id"] == bid:
                n += 1
        return n

    def manage_workers(self, elapsed):
        g, p = self.g, self.p
        ratio = self.cfg.get("gather_ratio", {"wood": 0.5, "stone": 0.32, "meteor": 0.18})
        ratio = {k: v for k, v in ratio.items() if v > 0}
        counts = {k: 0 for k in ratio}
        gatherers = {k: [] for k in ratio}
        for u in self.workers:
            o = u.order
            if o and o["type"] == "gather" and o["res"].kind not in counts and not u.carry_amt:
                u.set_order(None)  # ennek a fajnak nem kell ez a nyersanyag
                continue
            if o and o["type"] == "gather" and o["res"].kind in counts:
                counts[o["res"].kind] += self.share(u)
                gatherers[o["res"].kind].append(u)
        n = max(1, sum(self.share(u) for u in self.workers if u.defn.get("gathers") != []))

        # a készlettől függő súly: ami hiányzik, arra több munkás jut
        def factor(k):
            v = p.res.get(k, 0)
            return 1.8 if v < 150 else 1.0 if v < 600 else 0.5 if v < 1500 else 0.15

        w = {k: ratio[k] * factor(k) + (0.25 if self.reserve.get(k, 0) > p.res.get(k, 0) else 0) for k in ratio}
        tot = sum(w.values()) or 1
        desired = {k: w[k] / tot * n for k in ratio}

        def deficit(k):
            return desired[k] - counts[k]

        for u in self.workers:
            if u.order is not None or u.queue:
                continue
            for kind in sorted(ratio, key=deficit, reverse=True):
                if not u.can_gather(kind):
                    continue
                r = g.nearest_resource(kind, self.base.x, self.base.y, 30 * TILE)
                if r is not None:
                    u.set_order({"type": "gather", "res": r})
                    counts[kind] += self.share(u)
                    break
        # lassú átcsoportosítás: 3 mp-enként egy munkás a legtúlterheltebbről a leghiányosabbra
        self.rebalance_t = getattr(self, "rebalance_t", 0) + elapsed
        if ratio and self.rebalance_t >= 3:
            self.rebalance_t = 0
            over = min(ratio, key=deficit)
            under = max(ratio, key=deficit)
            if deficit(under) > 1 and deficit(over) < -1:
                movers = [u for u in gatherers[over] if not u.carry_amt and u.can_gather(under)]
                r = g.nearest_resource(under, self.base.x, self.base.y, 30 * TILE)
                if movers and r is not None:
                    movers[0].set_order({"type": "gather", "res": r})
        target = self.cfg.get("workers", 18)
        worker_id = p.faction["worker"]
        split = p.faction.get("gatherers")
        if split:  # külön építő és gyűjtők (Vasbirodalom, Ooni): azt képzi, amiből hiány van
            self.train_split(split, ratio, deficit, target)
            return
        if len(self.workers) < target:
            for b in g.producers(p, worker_id):
                if b.defn.get("parallel"):
                    if b.larva > (1 if len(self.workers) > 12 else 0):
                        g.cmd_train(p, worker_id, b)
                elif not b.queue:
                    g.cmd_train(p, worker_id, b)

    def share(self, u):
        """Egy gyűjtő hány „munkásnyi” bevételt ér (Arató: 3)."""
        return self.cfg.get("gatherer_share", {}).get(u.defn["id"], 1)

    def train_split(self, split, ratio, deficit, target):
        g, p = self.g, self.p
        builder_id = p.faction["worker"]
        builders = [u for u in self.workers if u.can_build]
        queued = [it["id"] for b in self.my_blds for it in b.queue]
        have = sum(self.share(u) for u in self.workers) + sum(self.cfg.get("gatherer_share", {}).get(q, 1)
                                                               for q in queued if g.data.defs.get(q, {}).get("worker"))
        uid = None
        if len(builders) + queued.count(builder_id) < self.cfg.get("builders", 3):
            uid = builder_id
        elif have < target:
            if ratio:
                kind = max(ratio, key=deficit)
                uid = split.get(kind, builder_id)
        if uid is None or uid in queued:
            return
        for b in g.producers(p, uid):
            if not b.queue:
                g.cmd_train(p, uid, b)
                return

    def manage_supply(self):
        g, p = self.g, self.p
        cap_max = g.data.game["max_supply"]
        if p.supply_cap >= cap_max:
            return
        su = p.faction.get("supply_unit")
        if su:  # croftian / wullof: a létszámot egységek hordozzák (wullof: emuk, karámonként legfeljebb 5)
            udef = g.data.defs[su]
            queued = sum(1 for b in self.my_blds for it in b.queue if it["id"] == su)
            if p.supply_cap - p.supply_used <= 6 + 2 * (p.supply_cap > 40) and queued < 1 + (p.supply_cap > 60):
                left = g.unit_limit_left(p, udef)
                if left is not None and left <= 0:
                    pen = udef["per_building"]["building"]
                    if self.pending(pen) <= sum(1 for b in self.my_blds if b.defn["id"] == pen and b.complete):
                        self.build(pen)
                elif p.can_afford(udef["cost"]):
                    g.cmd_train(p, su)
            return
        sid = p.faction["supply_building"]
        building_now = sum(1 for b in self.my_blds if b.defn["id"] == sid and not b.complete)
        building_now += sum(1 for u in self.workers if u.order and u.order["type"] == "build"
                            and u.order["bdef"]["id"] == sid)
        prod = sum(1 for b in self.my_blds if b.defn.get("produces") and b.complete)
        margin = min(14, 2 + 2 * prod)
        if p.supply_cap - p.supply_used <= margin and building_now < 1 + (p.supply_cap > 60):
            self.build(sid)

    def manage_build(self, elapsed):
        g, p = self.g, self.p
        need = {}
        for bid in self.cfg.get("build_order", []):
            need[bid] = need.get(bid, 0) + 1
            if self.pending(bid) < need[bid]:
                if g.missing_requirements(p, g.data.defs[bid]):
                    return
                self.build(bid)
                return
        # a build order után: felesleges nyersanyagból újabb termelőépület
        self.extra_t += elapsed
        if self.extra_t > 25 and all(v > 500 for k, v in p.res.items() if k not in ("meteor", "faith")):
            self.extra_t = 0
            prods = [bid for bid in self.cfg.get("build_order", []) if g.data.defs[bid].get("produces")]
            if prods:
                self.build(self.rng.choice(prods))

    def build(self, bid):
        g, p = self.g, self.p
        bdef = g.data.defs[bid]
        if not p.can_afford(bdef["cost"]) or not self.workers:
            return False
        spot = self.find_spot(bdef)
        if spot is None and p.faction.get("mechanic") == "lantern" and not bdef.get("lantern"):
            lid = self.cfg.get("lantern")  # Ooni: nincs fényes hely – új lámpással terjeszkedünk
            if lid:
                return self.build(lid)
        if spot is None:
            # micélia: ha nincs hely a hálózatban, egy spórafonallal terjeszkedünk
            node = self.network_node()
            if node and bid != node:
                return self.build(node)
            return False
        builders = [u for u in self.workers if u.can_build]
        cands = [u for u in builders if u.carry_amt == 0 and (u.order is None or u.order["type"] == "gather")]
        if not cands:
            cands = [u for u in builders if u.order is None or u.order["type"] == "gather"]
        if not cands:
            return False
        cx, cy = (spot[0] + 1) * TILE, (spot[1] + 1) * TILE
        w = min(cands, key=lambda u: dmath.hypot(u.x - cx, u.y - cy))
        if p.faction.get("pond") and not bdef.get("no_pond"):  # Huk'ucha: előbb tavacskát ás
            bw, bh = bdef["size"]
            n = p.rules.get("pond_size", p.faction["pond"].get("size", 2))
            xs = sorted({spot[0], spot[0] + max(0, bw - n)} | set(range(spot[0], spot[0] + max(1, bw - n + 1), n)))
            ys = sorted({spot[1], spot[1] + max(0, bh - n)} | set(range(spot[1], spot[1] + max(1, bh - n + 1), n)))
        queued = False
        if p.faction.get("pond") and not bdef.get("no_pond"):  # a Tóásó előbb kiássa, utána épít (sorban)
            for fy in ys:
                for fx in xs:
                    if g.can_lay_pond(p, fx, fy, check_explored=False)[0]:
                        queued = g.cmd_dig_pond(p, fx, fy, w, queue=queued) or queued
            ok, _ = g.can_place(p, bdef, spot[0], spot[1], check_explored=False)
            if not ok:
                return False
        if p.faction.get("foundation") and not bdef.get("foundation_free"):  # Vasbirodalom: a Fabrikátor előbb alapoz
            bw, bh = bdef["size"]
            big = "imp_nagy_alapozas" in p.researched and bw >= 3 and bh >= 3 and \
                g.can_lay_foundation(p, spot[0], spot[1], check_explored=False, n=4)[0] and \
                p.can_afford(g.data.defs["imp_nagy_alapozas"]["cost"])
            if big:
                queued = g.cmd_found(p, spot[0], spot[1], w, "imp_nagy_alapozas")
            else:
                xs = sorted({spot[0], spot[0] + max(0, bw - 2)} | set(range(spot[0], spot[0] + bw - 1, 2)))
                ys = sorted({spot[1], spot[1] + max(0, bh - 2)} | set(range(spot[1], spot[1] + bh - 1, 2)))
                for fy in ys:
                    for fx in xs:
                        if g.can_lay_foundation(p, fx, fy, check_explored=False)[0] and \
                                p.can_afford(g.data.defs["imp_alapozas"]["cost"]):
                            queued = g.cmd_found(p, fx, fy, w, "imp_alapozas", queue=queued) or queued
        return g.cmd_build(p, bdef, spot[0], spot[1], w, queue=queued)

    def network_node(self):
        if self.p.faction.get("mechanic") != "network":
            return None
        for bid, d in self.p.faction["buildings"].items():
            if d.get("link_radius") and d.get("size") == [1, 1]:
                return bid
        return None

    def find_spot(self, bdef):
        g, p = self.g, self.p
        m = g.map
        w, h = bdef["size"]
        bx, by = int(self.base.x // TILE), int(self.base.y // TILE)
        towers = bool(bdef.get("attack"))
        # tornyokat az ellenség felé, a többit körben
        if towers:
            ex, ey = self.enemy_center()
            ang = dmath.atan2(ey - self.base.y, ex - self.base.x)
            bx += int(dmath.cos(ang) * 6)
            by += int(dmath.sin(ang) * 6)
        centers = [(bx, by)]
        if p.faction.get("mechanic") == "power" and not bdef.get("no_power"):
            centers = [(int(b.x // TILE), int(b.y // TILE)) for b in self.my_blds
                       if b.complete and b.defn.get("power_radius")]
            self.rng.shuffle(centers)
        if p.faction.get("mechanic") == "lantern" and not bdef.get("no_light"):  # Ooni: lámpásfényben
            centers = [(int(b.x // TILE), int(b.y // TILE)) for b in self.my_blds
                       if b.defn.get("light_radius") and (b.complete or b.defn.get("lantern"))]
            self.rng.shuffle(centers)
        for cx, cy in centers:
            for r in range(2 if len(centers) > 1 or centers[0] != (bx, by) else 3, 20):
                cands = []
                for dy in range(-r, r + 1):
                    for dx in range(-r, r + 1):
                        if max(abs(dx), abs(dy)) != r:
                            continue
                        tx, ty = cx + dx - w // 2, cy + dy - h // 2
                        if self.spot_ok(bdef, tx, ty):
                            cands.append((tx, ty))
                if cands:
                    return self.rng.choice(cands)
        return None

    def spot_ok(self, bdef, tx, ty):
        g, p = self.g, self.p
        m = g.map
        w, h = bdef["size"]
        ok, _ = g.can_place(p, bdef, tx, ty, check_explored=False, ignore_layers=bool(p.faction.get("pond")))
        if not ok:
            return False
        # hagyjunk utat: 1 csempe szabad sáv épületek körül, 2 a nyersanyagok felé
        for y in range(ty - 1, ty + h + 1):
            for x in range(tx - 1, tx + w + 1):
                if not m.inb(x, y):
                    return False
                if m.block[y * m.w + x] in (B_RESOURCE, B_BUILDING):
                    return False
        return True

    def enemy_center(self):
        """A legközelebbi még élő ellenséges játékos kezdőhelye."""
        foes = [q for q in self.g.players if self.p.hostile(q) and not q.defeated]
        if not foes:
            return self.p.start_xy
        sx, sy = self.p.start_xy
        return min(foes, key=lambda q: dmath.hypot(q.start_xy[0] - sx, q.start_xy[1] - sy)).start_xy

    def manage_production(self):
        g, p = self.g, self.p
        weights = self.cfg.get("army", {})
        if not weights:
            return
        for b in self.my_blds:
            if not b.complete or not b.powered:
                continue
            prods = [uid for uid in b.defn.get("produces", ()) if uid in weights]
            if not prods:
                continue
            if b.defn.get("parallel"):
                if b.larva <= 0 or len(self.workers) < 8:
                    continue
            elif len(b.queue) >= 2:
                continue
            avail = [uid for uid in prods if not g.missing_requirements(p, g.data.defs[uid])]
            if not avail:
                continue
            # a drágább egységek súlyát növeljük, ha van rá pénz
            choices = []
            for uid in avail:
                wgt = weights[uid]
                cost = g.data.defs[uid]["cost"]
                need = {k: v + self.reserve.get(k, 0) for k, v in cost.items()}
                if p.can_afford(need if self.reserve else cost):
                    choices.append((uid, wgt))
            if not choices:
                continue
            total = sum(w for _, w in choices)
            r = self.rng.uniform(0, total)
            for uid, wgt in choices:
                r -= wgt
                if r <= 0:
                    grp = uid + "_falka"  # Wullof: a kutatás után falkában képez
                    gd = g.data.defs.get(grp)
                    if gd and grp in b.defn.get("produces", ()) and not g.missing_requirements(p, gd) \
                            and p.can_afford(gd["cost"]):
                        uid = grp
                    g.cmd_train(p, uid, b)
                    break

    def manage_ally(self):
        """Diplomácia csarnoka: ha kész és van rá pénz, szövetséget köt (faj szerinti kedvenc nép)."""
        g, p = self.g, self.p
        if getattr(p, "ally", None) is None:
            if g.ally_ready(p) and p.can_afford(p.faction.get("ally_cost", {})):
                g.ai_choose_ally(p)
        elif not getattr(self, "_ally_on", False):  # betöltött mentés / átvett játékos
            self.on_ally(p.ally)

    def on_ally(self, aid):
        """A szövetséges nép épületei, egységei és fejlesztései a gép terveibe."""
        if getattr(self, "_ally_on", False):
            return
        self._ally_on = True
        a = self.g.data.factions.get(aid, {}).get("ai", {})
        self.cfg = dict(self.cfg)
        self.cfg["build_order"] = list(self.cfg.get("build_order", [])) + a.get("build", [])
        army = dict(self.cfg.get("army", {}))
        army.update(a.get("army", {}))
        self.cfg["army"] = army
        self.cfg["research_order"] = list(self.cfg.get("research_order", [])) + a.get("research", [])

    def on_path(self, pid):
        """A választott irány saját építési sorrendje és seregösszetétele."""
        pa = self.p.faction.get("ai", {}).get("paths", {}).get(pid, {})
        self.cfg = dict(self.cfg)
        self.cfg["build_order"] = list(self.cfg.get("build_order", [])) + pa.get("build", [])
        army = dict(self.cfg.get("army", {}))
        army.update(pa.get("army", {}))
        self.cfg["army"] = army
        self.cfg["research_order"] = list(self.cfg.get("research_order", [])) + pa.get("research", [])

    def manage_research(self):
        g, p = self.g, self.p
        if len(self.army) < 6:
            return
        for rid in self.cfg.get("research_order", []):
            if rid in p.researched or rid in p.researching or g.research_blocked(p, rid):
                continue
            r = g.data.researches[rid]
            blds = [b for b in self.my_blds if b.complete and rid in b.defn.get("researches", ()) and not b.queue]
            if blds and p.can_afford(r["cost"]):
                g.cmd_research(p, rid)
            return

    # ------------------------------------------------------------------
    # hírszerzés: csak azt tudja, amit a saját egységei láttak (+ az ellenfél kezdőhelyét)
    def value(self, e):
        c = e.defn.get("cost", {})
        v = sum(c.values()) + c.get("meteor", 0) * 0.5
        if e.is_unit:
            v /= e.defn.get("count", 1)
        frac = (e.hp + e.shield) / max(1, e.max_hp + e.max_shield)
        return v * (0.4 + 0.6 * frac)

    def update_intel(self):
        g, p = self.g, self.p
        now = g.time
        for e in g.units:
            if p.hostile(e.owner) and e.alive and not e.is_worker and not e.owner.neutral and g.is_visible(p, e):
                self.seen_units[e.id] = (self.value(e), now, e)
        for b in g.buildings:
            if p.hostile(b.owner) and b.alive and g.is_visible(p, b):
                self.seen_blds[b.id] = b
        self.seen_units = {k: v for k, v in self.seen_units.items() if v[2].alive and now - v[1] < 90}
        self.seen_blds = {k: b for k, b in self.seen_blds.items() if b.alive}
        self.enemy_value = sum(v[0] for v in self.seen_units.values())
        # a látott védőépületek (tornyok, Szívgombák) is a védelem részei
        self.enemy_value += sum(self.value(b) * 0.9 for b in self.seen_blds.values() if b.attack and b.complete)

    def scout(self):
        g = self.g
        if self.scouted or g.time < 100:
            return
        candidates = [u for u in self.workers if u.alive and u.inside is None and u.speed_base() > 0]
        if not candidates:
            return
        u = self.rng.choice(candidates)
        ex, ey = self.enemy_center()
        if g.cmd_move([u], ex, ey):
            self.scouted = True
            g.cmd_move([u], self.base.x, self.base.y, queue=True)

    def local_strength(self, x, y, r, mine):
        g, p = self.g, self.p
        tot = 0.0
        for e in g.query(x, y, r):
            if not e.alive or e.is_worker or e.owner.neutral or dmath.hypot(e.x - x, e.y - y) > r:
                continue
            if (e.owner is p) == mine and (mine or g.is_visible(p, e)):
                tot += self.value(e)
        if not mine:
            for b in g.buildings:
                if p.hostile(b.owner) and b.attack and b.alive and dmath.hypot(b.x - x, b.y - y) < r and g.is_visible(p, b):
                    tot += self.value(b) * 1.5
        return tot

    def manage_army(self, dt):
        g, p = self.g, self.p
        self.update_intel()
        self.scout()
        self.special_mechanics()
        self.state_t += dt
        army = [u for u in self.army if u.attack or u.defn.get("abilities")]
        my_value = sum(self.value(u) for u in army)
        supply = sum(u.supply() for u in army)

        # 1) védekezés: ellenség a bázis közelében
        threats = [e for e in g.units if p.hostile(e.owner) and e.alive and g.is_visible(p, e)
                   and not (e.owner.neutral and e.order is None)
                   and any(abs(e.x - b.x) < 13 * TILE and abs(e.y - b.y) < 13 * TILE for b in self.my_blds)]
        if threats:
            tx = sum(e.x for e in threats) / len(threats)
            ty = sum(e.y for e in threats) / len(threats)
            tval = sum(self.value(e) for e in threats if not e.is_worker)
            recall = self.state != "attack" or tval > 0.35 * my_value
            defenders = [u for u in army if recall or dmath.hypot(u.x - tx, u.y - ty) < 30 * TILE]
            free = [u for u in defenders
                    if u.order is None or (u.order["type"] in ("move", "amove") and not u.order.get("sub"))]
            if free:
                g.cmd_move(free, tx, ty, attack=True)
            if recall and self.state == "attack":
                self.state, self.state_t = "build", 0
            # kis betörés ellen a munkások is fegyvert fognak, ha nincs sereg a közelben
            near_army = sum(1 for u in army if dmath.hypot(u.x - tx, u.y - ty) < 12 * TILE)
            if near_army == 0 and 0 < tval <= 250:
                for u in self.workers:
                    if dmath.hypot(u.x - tx, u.y - ty) < 9 * TILE and (u.order is None or u.order["type"] == "gather"):
                        tgt = min(threats, key=lambda e: dmath.hypot(e.x - u.x, e.y - u.y))
                        if u.can_hit(tgt):
                            g.cmd_attack([u], tgt)
            return

        gx, gy = self.rally_point()
        max_sup = g.data.game["max_supply"]
        if self.state == "build":
            idle = [u for u in army if u.order is None and dmath.hypot(u.x - gx, u.y - gy) > 7 * TILE]
            if idle:
                g.cmd_move(idle, gx, gy, attack=True)
            # amit nem láttunk, azt is számoljuk: idővel növekvő becsült ellenséges sereg
            est = max(self.enemy_value, 110 * g.time / 60 - 150)
            strong = my_value >= 1.3 * est + 150
            if g.time >= self.hold_until and supply >= self.attack_supply and (strong or p.supply_used >= max_sup - 10):
                self.state, self.state_t = "gather", 0
                g.cmd_move(army, gx, gy, attack=True)
        elif self.state == "gather":
            near = sum(1 for u in army if dmath.hypot(u.x - gx, u.y - gy) < 9 * TILE)
            if near >= 0.75 * len(army) or self.state_t > 25:
                self.state, self.state_t = "attack", 0
                self.attack_t = 0
                self.attack_supply = min(100, self.attack_supply + 8)
        elif self.state == "attack":
            if len(army) < 3:
                self.retreat(army, gx, gy)
                return
            cx = sum(u.x for u in army) / len(army)
            cy = sum(u.y for u in army) / len(army)
            mine = self.local_strength(cx, cy, 12 * TILE, True)
            theirs = self.local_strength(cx, cy, 14 * TILE, False)
            if theirs > 1.6 * mine + 100:
                self.retreat(army, gx, gy)
                return
            self.attack_t -= dt
            if self.attack_t <= 0:
                self.attack_t = 5
                tgt = self.attack_target()
                # összetartás: a lemaradók a csapat közepéhez zárkóznak fel, a többiek a célra mennek
                idle = [u for u in army if u.order is None or (u.order["type"] == "amove" and not u.order.get("sub")
                        and dmath.hypot(u.order["x"] - tgt[0], u.order["y"] - tgt[1]) > 3 * TILE)]
                far = [u for u in idle if dmath.hypot(u.x - cx, u.y - cy) > 14 * TILE]
                front = [u for u in idle if u not in far]
                if far:
                    g.cmd_move(far, cx, cy, attack=True)
                if front:
                    g.cmd_move(front, tgt[0], tgt[1], attack=True)
        elif self.state == "retreat":
            if self.state_t > 12:
                self.state, self.state_t = "build", 0

    # ------------------------------------------------------------------
    # hősök: kiképzés, képességtanulás, italok, tárgyak, szörnyirtás
    def manage_heroes(self, elapsed):
        g, p = self.g, self.p
        altars = [b for b in self.my_blds if b.complete and b.defn.get("altar")
                  and any(g.data.defs.get(uid, {}).get("hero") for uid in b.defn.get("produces", ()))]
        mine = [u for u in self.my_units if u.hero]
        self.reserve = {}
        if altars and len(self.army) >= 3:
            alt = altars[0]
            hero_ids = [uid for uid in alt.defn.get("produces", ()) if g.data.defs.get(uid, {}).get("hero")]
            want = 1 if g.time < 600 else 2 if g.time < 1100 else 3
            order = hero_ids[self.p.id % len(hero_ids):] + hero_ids[:self.p.id % len(hero_ids)]
            for uid in order:
                if g.hero_count(p) >= want or g.hero_busy(p, uid):
                    continue
                cost = g.data.defs[uid]["cost"]
                if p.can_afford(cost) and len(alt.queue) < 2:
                    g.cmd_train(p, uid, alt)
                else:
                    self.reserve = dict(cost)
                break
        for h in mine:
            heroes.auto_learn(g, h)
            if h.hp < h.max_hp * 0.35:
                for i, iid in enumerate(h.inventory):
                    if iid and g.data.items[iid].get("heal"):
                        g.cmd_use_item(h, i)
                        break
            for i, iid in enumerate(h.inventory):
                it = g.data.items.get(iid) if iid else None
                if it and (it.get("xp") or it.get("perm")):
                    g.cmd_use_item(h, i)
            if (h.order is None or h.order["type"] in ("move", "amove") and not h.order.get("sub")) and heroes.free_slot(h) >= 0:
                near = [gi for gi in g.items if gi.alive and dmath.hypot(gi.x - h.x, gi.y - h.y) < 10 * TILE]
                if near:
                    g.cmd_pickup([h], min(near, key=lambda gi: dmath.hypot(gi.x - h.x, gi.y - h.y)))
        # szörnyirtás: a hős és egy kis csapat kitakarítja a közeli, legyőzhető tábort
        self.creep_t -= elapsed
        if not mine or not g.camps or self.state != "build" or self.creep_t > 0:
            return
        h = mine[0]
        squad = [u for u in self.army if not u.hero and u.attack and u.order is None][:8]
        if len(squad) < 4:
            return
        lvl_ok = 1 + (h.level + len(squad) // 3) // 2
        camps = [c for c in g.camps if not c["done"] and c["level"] <= lvl_ok
                 and dmath.hypot(c["x"] - self.base.x, c["y"] - self.base.y) < 34 * TILE]
        if not camps:
            return
        c = min(camps, key=lambda c: dmath.hypot(c["x"] - self.base.x, c["y"] - self.base.y))
        self.creep_t = 45.0
        g.cmd_move([h] + squad, c["x"], c["y"], attack=True)

    def manage_mines(self):
        """Bányára épülő épületek (elf Napfúró, croftian Fertőzött tárna): építés a bázis körüli bányákra, és munkások
        beküldése."""
        g, p = self.g, self.p
        mid = self.cfg.get("mine_building")
        if not mid or self.base is None:
            return
        bdef = g.data.defs[mid]
        mines = [b for b in self.my_blds if b.defn["id"] == mid]
        building = any(u.order and u.order["type"] == "build" and u.order["bdef"]["id"] == mid for u in self.workers)
        consume = bdef["mine"].get("consume")
        early = consume or g.time > 150  # a croftian ebből él; az elf előbb hord
        if early and not building and p.can_afford(bdef["cost"]):
            nodes = [r for r in g.map.resources if r.kind in bdef["on_resource"] and r.alive
                     and getattr(r, "mine", None) is None
                     and dmath.hypot(r.x - self.base.x, r.y - self.base.y) < 22 * TILE]
            builders = [u for u in self.workers if u.can_build and u.inside is None
                        and (u.order is None or u.order["type"] == "gather")]
            if nodes and builders and len(mines) < self.cfg.get("mine_max", 4 if consume else 3):
                r = min(nodes, key=lambda r: (r.kind != "stone", dmath.hypot(r.x - self.base.x, r.y - self.base.y)))
                w = min(builders, key=lambda u: dmath.hypot(u.x - r.x, u.y - r.y))
                g.cmd_build(p, bdef, r.tx, r.ty, w)
        for b in mines:
            if not b.complete or getattr(b, "res", None) is None:
                continue
            slots = bdef["mine"]["slots"] - (b.mine_n if consume else len(b.miners))
            coming = sum(1 for u in self.workers if u.order and u.order["type"] == "enter_mine" and u.order["b"] is b)
            need = slots - coming
            if need <= 0:
                continue
            free = [u for u in self.workers if u.inside is None and u.can_gather(b.res.kind)
                    and (u.order is None or (u.order["type"] == "gather" and not u.carry_amt))]
            if consume:  # croftian: maradjon 3 szabad szolga az építéshez
                spare = [u for u in self.workers if u.inside is None and (u.order is None)]
                free = spare[3:] if len(spare) > 3 else []
            for u in sorted(free, key=lambda u: dmath.hypot(u.x - b.x, u.y - b.y))[:need]:
                u.set_order({"type": "enter_mine", "b": b})

    def manage_points(self):
        """Égi uralom: tétlen egységek a legközelebbi szabad / ellenséges Hasadékkőre; a saját kőre őrállás."""
        g, p = self.g, self.p
        pts = getattr(g, "points", None)
        if not pts or self.base is None:
            return
        self.pt_t = getattr(self, "pt_t", 0) + 1
        if self.pt_t < 3:
            return
        self.pt_t = 0
        idle = [u for u in self.army if u.order is None and u.inside is None and not u.flying]
        targets = [pt for pt in pts if pt.owner is None or
                   (p.hostile(pt.owner) and (pt.guard is None or not pt.guard.alive))]
        if idle and targets and len(idle) >= 3:
            pt = min(targets, key=lambda q: dmath.hypot(q.x - self.base.x, q.y - self.base.y))
            grp = sorted(idle, key=lambda u: dmath.hypot(u.x - pt.x, u.y - pt.y))[:3]
            g.cmd_move(grp, pt.x, pt.y)
        gid = next((bid for bid, b in p.faction["buildings"].items() if b.get("on_point")), None)
        if gid:
            bdef = g.data.defs[gid]
            for pt in pts:
                if pt.owner is p and (pt.guard is None or not pt.guard.alive) and p.can_afford(bdef["cost"]):
                    if any(u.order and u.order["type"] == "build" and u.order["bdef"]["id"] == gid for u in self.workers):
                        break
                    ws = [u for u in self.workers if u.can_build and u.inside is None
                          and (u.order is None or u.order["type"] == "gather")]
                    if ws and g.can_place(p, bdef, pt.tx, pt.ty)[0]:
                        w = min(ws, key=lambda u: dmath.hypot(u.x - pt.x, u.y - pt.y))
                        g.cmd_build(p, bdef, pt.tx, pt.ty, w)
                    break

    def manage_spikes(self):
        """Croftian: 1,5 perctől tüskesort rak a bázis és az ellenség közé (a bázistól ~7 csempére), legfeljebb 8-at."""
        g, p = self.g, self.p
        if not self.cfg.get("spikes") or self.base is None or g.time < 90:
            return
        self._spk = getattr(self, "_spk", 0) + 1
        if self._spk % 8:
            return
        if any(u.order and u.order.get("type") == "spike" for u in self.workers):
            return
        bdef = g.data.defs.get("cro_merges_tuskek")
        if bdef is None or g.spike_count(p) >= 8 * p.rules.get("spike_size", 1) ** 2 \
                or not p.can_afford({k: v + 40 for k, v in bdef["cost"].items()}):
            return
        foes = [q for q in g.players if p.hostile(q) and not getattr(q, "neutral", False) and not q.defeated]
        if not foes:
            return
        ex, ey = foes[0].start_xy
        bx, by = self.base.x, self.base.y
        d = dmath.hypot(ex - bx, ey - by) or 1
        ux, uy = (ex - bx) / d, (ey - by) / d
        n = p.rules.get("spike_size", 1)
        k = g.spike_count(p) // (n * n)
        side = ((k + 1) // 2) * (1 if k % 2 else -1)  # középről kifelé: 0, +1, -1, +2, -2 …
        cx = bx / TILE + ux * 7 - uy * side * n
        cy = by / TILE + uy * 7 + ux * side * n
        tx, ty = int(cx), int(cy)
        if not g.can_lay_spikes(p, tx, ty, n)[0]:
            return
        ws = [u for u in self.workers if u.can_build and u.inside is None and u.order is None]
        if ws:
            w = min(ws, key=lambda u: dmath.hypot(u.x - cx * TILE, u.y - cy * TILE))
            g.cmd_lay_spikes(p, tx, ty, w)

    def manage_gpowers(self):
        """Ö19: az isteni csapás a legsűrűbb látható ellenséges csoportra (legalább 5 egység), a riadó a bázis
        védelmére, az idézés és a szél a saját sereg csatájába."""
        g, p = self.g, self.p
        self._gcall = getattr(self, "_gcall", 0) + 1
        if self._gcall % 6 or self.base is None or not self.base.alive:
            return
        hq = self.base
        for ab_id in hq.defn.get("powers", ()):
            ab = g.data.abilities.get(ab_id, {})
            if ab.get("type") != "gpower" or getattr(hq, "power_ready", {}).get(ab_id, 0) > g.time:
                continue
            cost = g.gpower_cost(ab)
            if not p.can_afford({k: v + (40 if k == "faith" else 0) for k, v in cost.items()}):
                continue
            if ab.get("hq_hp") and hq.hp < hq.max_hp * 0.7:
                continue
            if not ab.get("target"):  # riadó: ha a bázist megtámadják
                if g.count_enemies(hq, 12 * TILE) >= 6:
                    g.cmd_gpower(p, hq, ab_id)
                continue
            r = ab.get("radius", 2.5) * TILE
            foes = [u for u in g.units if u.alive and p.hostile(u.owner) and not getattr(u.owner, "neutral", False)
                    and u.is_unit and g.is_visible(p, u)]
            if len(foes) < 5:
                continue
            best, bn = None, 0
            for u in foes[:60]:
                n = sum(1 for v in foes if dmath.sq(v.x - u.x) + dmath.sq(v.y - u.y) <= r * r)
                if n > bn:
                    best, bn = u, n
            if best is not None and bn >= 5:
                g.cmd_gpower(p, hq, ab_id, best.x, best.y)

    def manage_faith(self):
        """Ö17: Áhítat / Tóének – a középjátéktól 2–4 munkás fohászkodik; az épület-képességeket okosan használja."""
        g, p = self.g, self.p
        fa = p.faction.get("faith")
        if not fa:
            return
        self._fcall = getattr(self, "_fcall", 0) + 1
        if self._fcall % 4:
            return
        shrines = [b for b in self.my_blds if b.complete and b.alive and b.defn.get("pray_slots")]
        ws = [w for w in self.workers if w.alive and w.inside is None]
        praying = [w for w in ws if w.order and w.order.get("type") == "pray"]
        want = 0 if not shrines or len(ws) < 10 or g.time < 180 else (2 if len(ws) < 16 else 4)
        if len(praying) < want:
            b = shrines[0]
            cands = sorted((w for w in ws if w.order and w.order.get("type") == "gather" and not w.carry_amt),
                           key=lambda w: (dmath.hypot(w.x - b.x, w.y - b.y), w.id))
            for w in cands[:want - len(praying)]:
                g.cmd_smart([w], b.x, b.y, b)
        elif len(praying) > want:
            for w in sorted(praying, key=lambda w: w.id)[:len(praying) - want]:
                w.set_order(None)
        for b in self.my_blds:
            for ab_id in (b.defn.get("powers", ()) if b.complete else ()):
                ab = g.data.abilities[ab_id]
                if p.res.get("faith", 0) < ab.get("faith", 0) + 15 or getattr(b, "power_ready", {}).get(ab_id, 0) > g.time:
                    continue
                eff = ab.get("effect")
                if eff == "mine_boost" and b.res is not None and (b.miners or b.mine_n):
                    g.cmd_bpower(p, b, ab_id)
                elif eff == "hasten" and len(b.queue) >= 2:
                    g.cmd_bpower(p, b, ab_id)
                elif eff == "flood":
                    m = g.map
                    hurt = sum(1 for u in self.army if u.alive and u.hp < u.max_hp * 0.6
                               and g.pond[int(u.y // TILE) * m.w + int(u.x // TILE)] == p.id + 1)
                    if hurt >= 4:
                        g.cmd_bpower(p, b, ab_id)

    def special_mechanics(self):
        g, p = self.g, self.p
        self.manage_mines()
        self.manage_points()
        self.manage_faith()
        self.manage_gpowers()
        self.manage_spikes()
        # mutáció (croftian): a tétlen Karmosok egy része páncélbogárrá alakul
        for ab_id, ab in g.data.abilities.items():
            if ab.get("type") != "morph":
                continue
            cands = [u for u in self.army if u.has_ability(ab_id) and u.order is None and u.morph is None]
            if len(cands) >= 4 and p.can_afford({k: v * 2 for k, v in ab.get("cost", {}).items()}):
                g.cmd_morph(cands[:1], ab_id)
        # helyőrség (ursan): ha nincs sereg a közelben, a munkások behúzódnak; utána kijönnek
        gar = [b for b in self.my_blds if b.defn.get("garrison") and b.complete]
        if gar:
            danger = any(g.count_enemies(b, 9 * TILE) >= 2 for b in gar)
            if danger and len([u for u in self.army if dmath.hypot(u.x - self.base.x, u.y - self.base.y) < 14 * TILE]) < 3:
                hq = self.base if self.base.defn.get("garrison") else gar[0]
                ws = [w for w in self.workers if w.inside is None and dmath.hypot(w.x - hq.x, w.y - hq.y) < 12 * TILE]
                if ws and len(hq.garrison) < hq.defn["garrison"]:
                    g.cmd_garrison(ws[:hq.defn["garrison"] - len(hq.garrison)], hq)
            elif not danger:
                for b in gar:
                    if b.garrison and g.count_enemies(b, 12 * TILE) == 0:
                        ws = [u for u in b.garrison if u.is_worker]
                        g.unload(b)
                        for w in ws:
                            r = g.nearest_resource("stone" if p.res.get("stone", 0) < 200 else "wood", w.x, w.y, 20 * TILE)
                            if r is not None:
                                w.set_order({"type": "gather", "res": r})
        # felültetés (wullof): a tétlen bölényekre a közeli gyalogos harcosok felülnek
        for ab_id, ab in g.data.abilities.items():
            if ab.get("type") != "mount":
                continue
            for u in [u for u in self.army if u.has_ability(ab_id) and u.order is None]:
                riders = [r for r in self.army if r.alive and r.defn["id"] in ab["riders"] and r.inside is None
                          and (r.order is None or r.order.get("auto") or r.order.get("type") == "move")
                          and dmath.hypot(r.x - u.x, r.y - u.y) < 14 * TILE]  # harcolót nem rángat ki
                if not riders:
                    continue
                r = min(riders, key=lambda r: dmath.hypot(r.x - u.x, r.y - u.y))
                if dmath.hypot(r.x - u.x, r.y - u.y) <= ab.get("range", 3) * TILE:
                    g.cmd_mount([u], ab_id)
                elif r.order is None or r.order.get("type") != "move":
                    g.cmd_move([r], u.x, u.y)
        # összeolvasztás (micélia)
        for ab_id, ab in g.data.abilities.items():
            if ab.get("type") != "merge":
                continue
            idle = [u for u in self.army if u.has_ability(ab_id) and (u.order is None or u.order["type"] == "amove")]
            if len(idle) >= ab.get("count", 3) + 1:
                g.cmd_merge(idle, ab_id)
        # félbehagyott építkezés: ha senki sem dolgozik rajta, egy munkás befejezi
        if g.builder_style(p) in ("stay", "stay_single", "enter"):
            for b in self.my_blds:
                if b.complete or getattr(b, "self_build", False):
                    continue
                busy = any(w.order and w.order.get("b") is b for w in self.workers)
                busy |= any(w.order and w.order.get("type") == "build" and w.order["bdef"]["id"] == b.defn["id"]
                            for w in self.workers)
                if busy:
                    continue
                cand = [w for w in self.workers if w.inside is None and (w.order is None or
                        (w.order["type"] == "gather" and not w.carry_amt))]
                if cand:
                    w = min(cand, key=lambda w: dmath.hypot(w.x - b.x, w.y - b.y))
                    resume = {"type": "gather", "res": w.order["res"]} if w.order else None
                    w.set_order({"type": "construct", "b": b})
                    if resume:
                        w.queue.append(resume)
        # gépjavítás (vasbirodalom): sérült gépek a bázis közelében
        if p.faction.get("repair"):
            hurt = [u for u in self.army if "mech" in u.tags and u.hp < u.max_hp * 0.7 and u.order is None
                    and dmath.hypot(u.x - self.base.x, u.y - self.base.y) < 20 * TILE]
            for u in hurt[:2]:
                fixers = [w for w in self.workers if w.order and w.order["type"] == "gather" and not w.carry_amt]
                if not fixers:
                    break
                w = min(fixers, key=lambda w: dmath.hypot(w.x - u.x, w.y - u.y))
                resume = {"type": "gather", "res": w.order["res"]}
                w.set_order({"type": "repair", "b": u})
                w.queue.append(resume)

    def retreat(self, army, gx, gy):
        self.g.cmd_move(army, gx, gy)
        self.state, self.state_t = "retreat", 0
        self.attack_supply = max(self.cfg.get("attack_supply", 16), self.attack_supply - 4)

    def rally_point(self):
        ex, ey = self.enemy_center()
        ang = dmath.atan2(ey - self.base.y, ex - self.base.x)
        return self.base.x + dmath.cos(ang) * 8 * TILE, self.base.y + dmath.sin(ang) * 8 * TILE

    def attack_target(self):
        best, bestd = None, 1e18
        for b in self.seen_blds.values():
            d = dmath.hypot(b.x - self.base.x, b.y - self.base.y)
            if d < bestd:
                best, bestd = b, d
        if best is not None:
            return best.x, best.y
        return self.enemy_center()
