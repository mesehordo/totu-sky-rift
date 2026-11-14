// SPDX-License-Identifier: GPL-3.0-or-later
// Aurethi Birodalom (MECH-IMP-1–3: alapozás, Reaktorpajzs, Lélekhorgony; túlhajtás, horgonymentés; csak az Arató gyűjt
// aranyat és meteoritot) – M7, ADR-008; a régi játék (G 1184–1191, 1382–1435, 2093–2130, 2329–2423; E 157–165,
// 722–730, 1255–1300) és a spec/FAJMECHANIKAK.md 7.3 szerint:
//   - alapozás: a Fabrikátor lapkövet épít (`imp_alapozas` 2 × 2, `imp_nagy_alapozas` 4 × 4); kész lapkő helyén alapozás
//     marad (nem akadály); a teljesen alapozáson álló épület teljes életerővel készül el, alapozás nélkül fél életerővel,
//     és fél életerő fölött 0,2%/s-mal kopik (0,5 s-onként); a `foundation_free` épület alapozás nélkül is teljes;
//     a főépület alapozáson kezd (1 csempés peremmel);
//   - Reaktorpajzs (`shield_zone: reactor`): a pajzs csak Lélekreaktor 6 csempéjén belül vagy Égkő-fényben töltődik;
//   - Lélekhorgony: a `lelekhorgony` képességű, nem hős, nem idézett egység helyén horgony marad (életereje a sablon
//     életereje, legalább 50, 15%-on); teljesen helyreállítva újra az eredeti egység lesz; a `lelekhorgony_mentes`
//     képességű egység elviszi és a pontra leteszi;
//   - Túlhajtás: a Fabrikátor épülő épülete azonnal halad (a hátralévő idő fele; kész épületnél a hiányzó életerő fele),
//     a Fabrikátor a max életereje 40%-át elveszti (páncél nélkül);
//   - gyűjtők (`gatherers`): aranyat és meteoritot csak az Arató; Égkő-fényben a `mech` címkéjűek +1 életerő/s.
#include <algorithm>

#include "map/map.hpp"
#include "map/pathfinder.hpp"
#include "sim/faction_module.hpp"
#include "sim/sim.hpp"

namespace totu::sim {

namespace {

// --- komponensek -------------------------------------------------------------------------------------------------------
struct ImpFoundation {   // lerakott alapozás (csempés téglalap; nem akadály)
  uint8_t owner = 0;
  int16_t tx = 0, ty = 0, w = 0, h = 0;
  void hash(core::StateHash& hh) const { hh.add_u64(owner); hh.add_i64(tx); hh.add_i64(ty); hh.add_i64(w); hh.add_i64(h); }
  void save(core::ByteWriter& wr) const { wr.u8(owner); wr.i32(tx); wr.i32(ty); wr.i32(w); wr.i32(h); }
  static ImpFoundation load(core::ByteReader& r) {
    ImpFoundation f;
    f.owner = r.u8();
    f.tx = static_cast<int16_t>(r.i32()); f.ty = static_cast<int16_t>(r.i32());
    f.w = static_cast<int16_t>(r.i32()); f.h = static_cast<int16_t>(r.i32());
    return f;
  }
};

struct ImpUnfounded {   // alapozás nélkül épült épület: kopik
  uint8_t on = 1;
  void hash(core::StateHash& h) const { h.add_u64(on); }
  void save(core::ByteWriter& w) const { w.u8(on); }
  static ImpUnfounded load(core::ByteReader& r) { ImpUnfounded u; u.on = r.u8(); return u; }
};

struct ImpAnchor {   // Lélekhorgony: az eredeti egység sablonja
  uint16_t origin = kNoDef;
  void hash(core::StateHash& h) const { h.add_u64(origin); }
  void save(core::ByteWriter& w) const { w.u32(origin); }
  static ImpAnchor load(core::ByteReader& r) { ImpAnchor a; a.origin = static_cast<uint16_t>(r.u32()); return a; }
};

struct ImpRescue {   // horgonymentés: a horgonyért megy, felveszi, és a pontra viszi
  EntityId anchor = 0;
  Fixed x, y;
  uint8_t carrying = 0;
  int32_t repath = 0;
  void hash(core::StateHash& h) const {
    h.add_u64(anchor); h.add_i64(x.raw()); h.add_i64(y.raw()); h.add_u64(carrying); h.add_i64(repath);
  }
  void save(core::ByteWriter& w) const { w.u32(anchor); w.i32(x.raw()); w.i32(y.raw()); w.u8(carrying); w.i32(repath); }
  static ImpRescue load(core::ByteReader& r) {
    ImpRescue o;
    o.anchor = r.u32(); o.x = Fixed::from_raw(r.i32()); o.y = Fixed::from_raw(r.i32()); o.carrying = r.u8(); o.repath = r.i32();
    return o;
  }
};

const Fixed kReach = Fixed::from_ratio(22, 32);

Fixed fx_of(const content::Value* v, Fixed def) {
  if (!v) return def;
  const auto f = v->as_fixed();
  return f ? *f : def;
}

const content::Value* faction_obj(Sim& s, uint8_t owner) {
  if (owner < 1 || owner > Sim::kMaxPlayers) return nullptr;
  const FactionDef* f = s.rules()->faction(s.player(owner).faction);
  return f && f->raw ? f->raw->find("Faction") : nullptr;
}

const content::Value* foundation_cfg(Sim& s, uint8_t owner) {
  const content::Value* f = faction_obj(s, owner);
  const content::Value* fd = f ? f->find("foundation") : nullptr;
  return fd && fd->is_object() ? fd : nullptr;
}

bool is_foundation_tile(const EntityDef& d) {   // a lapkő maga (`Legacy.foundation_tile`)
  const content::Value* l = d.raw ? d.raw->find("Legacy") : nullptr;
  const content::Value* t = l ? l->find("foundation_tile") : nullptr;
  return t && t->is_bool() && t->as_bool();
}

// a téglalap minden csempéje a játékos alapozásán áll-e
bool founded(Sim& s, uint8_t owner, int tx, int ty, int w, int h) {
  std::vector<uint8_t> cov(static_cast<size_t>(w * h), 0);
  s.world().pool<ImpFoundation>().each([&](EntityId, const ImpFoundation& f) {
    if (f.owner != owner) return;
    for (int y = std::max<int>(ty, f.ty); y < std::min<int>(ty + h, f.ty + f.h); ++y) {
      for (int x = std::max<int>(tx, f.tx); x < std::min<int>(tx + w, f.tx + f.w); ++x) {
        cov[static_cast<size_t>((y - ty) * w + (x - tx))] = 1;
      }
    }
  });
  return std::all_of(cov.begin(), cov.end(), [](uint8_t c) { return c != 0; });
}

void add_foundation(Sim& s, uint8_t owner, int tx, int ty, int w, int h) {
  const EntityId f = s.world().create();
  s.world().add(f, Position{Fixed::from_int(tx) + Fixed::from_ratio(w, 2), Fixed::from_int(ty) + Fixed::from_ratio(h, 2)});
  ImpFoundation fd;
  fd.owner = owner;
  fd.tx = static_cast<int16_t>(tx);
  fd.ty = static_cast<int16_t>(ty);
  fd.w = static_cast<int16_t>(w);
  fd.h = static_cast<int16_t>(h);
  s.world().add(f, fd);
}

bool has_tag_raw(const EntityDef& d, const content::Value* tags) {
  if (!tags || !tags->is_array()) return false;
  for (const auto& t : tags->items()) {
    if (t.is_string() && d.has_tag(t.text())) return true;
  }
  return false;
}

class Module final : public FactionModule {
 public:
  const char* name() const override { return "imperium"; }

  void register_components(World& w) override {
    w.register_component<ImpFoundation>("ImpFoundation");
    w.register_component<ImpUnfounded>("ImpUnfounded");
    w.register_component<ImpAnchor>("ImpAnchor");
    w.register_component<ImpRescue>("ImpRescue");
  }

  // a főépület alapozáson kezd (1 csempés peremmel)
  void on_match_start(Sim& s) override {
    std::vector<std::pair<uint8_t, Building>> hqs;
    s.world().pool<Building>().each([&](EntityId e, const Building& b) {
      const Health* h = s.world().get<Health>(e);
      if (!h || !foundation_cfg(s, h->owner)) return;
      const FactionDef* f = s.rules()->faction(s.player(h->owner).faction);
      if (f && b.def == f->hq) hqs.emplace_back(h->owner, b);
    });
    for (const auto& [o, b] : hqs) add_foundation(s, o, b.tx - 1, b.ty - 1, b.w + 2, b.h + 2);
  }

  // a kész lapkőből alapozás lesz (nem akadály); a többi épület: alapozás nélkül fél életerővel, kopik
  void on_building_complete(Sim& s, EntityId e) override {
    const Building* b = s.world().get<Building>(e);
    Health* h = s.world().get<Health>(e);
    if (!b || !h) return;
    const content::Value* cfg = foundation_cfg(s, h->owner);
    if (!cfg) return;
    const EntityDef& d = s.rules()->defs[b->def];
    if (is_foundation_tile(d)) {
      const Building bb = *b;
      add_foundation(s, h->owner, bb.tx, bb.ty, bb.w, bb.h);
      if (s.nav_) s.nav_->remove_obstacle({bb.tx, bb.ty, bb.w, bb.h});
      s.world().destroy(e);
      return;
    }
    // alapozás nélkül is teljes életerős (`Placement.foundation_free`, a régi G 2767)
    const content::Value* pl = d.raw ? d.raw->find("Placement") : nullptr;
    const content::Value* ff = pl ? pl->find("foundation_free") : nullptr;
    if (ff && ff->is_bool() && ff->as_bool()) return;
    if (founded(s, h->owner, b->tx, b->ty, b->w, b->h)) return;
    const Fixed unf = fx_of(cfg->find("unfounded_hp"), Fixed::from_ratio(1, 2));
    h->hp = std::min(h->hp, std::max(1, (Fixed::from_int(h->max) * unf).floor_int()));
    s.world().add(e, ImpUnfounded{1});
  }

  // Reaktorpajzs: csak Lélekreaktor 6 csempéjén belül vagy Égkő-fényben
  bool shield_regen_allowed(Sim& s, EntityId e) override {
    const Health* h = s.world().get<Health>(e);
    const content::Value* f = h ? faction_obj(s, h->owner) : nullptr;
    const content::Value* z = f ? f->find("shield_zone") : nullptr;
    if (!z || !z->is_string() || z->text() != "reactor") return true;
    if (s.in_rift(e)) return true;
    const Position* p = s.world().get<Position>(e);
    if (!p) return false;
    bool near = false;
    s.world().pool<Building>().each([&](EntityId be, const Building& b) {
      if (near || !b.complete) return;
      const Health* bh = s.world().get<Health>(be);
      if (!bh || bh->owner != h->owner || bh->hp <= 0) return;
      const content::Value* raw = s.rules()->defs[b.def].raw.get();
      const content::Value* sr = raw ? raw->find("Mechanics") : nullptr;
      sr = sr ? sr->find("shield_reactor") : nullptr;
      if (!sr || !sr->is_bool() || !sr->as_bool()) return;
      const Position& bp = *s.world().get<Position>(be);
      near = core::length(bp.x - p->x, bp.y - p->y) <= Fixed::from_int(6);
    });
    return near;
  }

  // gyűjtők: a `gatherers` szerinti nyersanyagot csak a megnevezett munkás
  bool can_gather(Sim& s, EntityId w, int kind) override {
    const Health* h = s.world().get<Health>(w);
    const Unit* u = s.world().get<Unit>(w);
    const content::Value* f = h ? faction_obj(s, h->owner) : nullptr;
    const content::Value* g = f ? f->find("gatherers") : nullptr;
    if (!u || !g || !g->is_object()) return true;
    const content::Value* who = g->find(resource_id(kind));
    return !who || !who->is_string() || s.rules()->defs[u->def].id == who->text();
  }

  // Égkő-fényben a megjelölt címkéjűek (mech) +1 életerő/s
  Fixed regen_add(Sim& s, EntityId e) override {
    const Health* h = s.world().get<Health>(e);
    const Unit* u = s.world().get<Unit>(e);
    if (!h || !u || !s.in_rift(e)) return Fixed{};
    const content::Value* f = faction_obj(s, h->owner);
    const content::Value* rg = f ? f->find("rift") : nullptr;
    rg = rg ? rg->find("regen") : nullptr;
    if (!rg || !has_tag_raw(s.rules()->defs[u->def], rg->find("tags"))) return Fixed{};
    return fx_of(rg->find("add"), Fixed::from_int(1));
  }

  // Lélekhorgony: a horgonyképes egység helyén horgony marad; a hordozó halálakor a horgony leesik
  void on_death(Sim& s, EntityId victim, EntityId, uint8_t) override {
    if (const ImpRescue* rs = s.world().get<ImpRescue>(victim); rs && rs->carrying) drop_anchor(s, rs->anchor, victim);
    const Unit* u = s.world().get<Unit>(victim);
    const Health* h = s.world().get<Health>(victim);
    if (!u || !h || s.world().get<ImpAnchor>(victim) || s.world().get<Lifetime>(victim) || s.world().get<HeroState>(victim)) return;
    const content::Value* f = faction_obj(s, h->owner);
    const content::Value* aid = f ? f->find("soul_anchor") : nullptr;
    if (!aid || !aid->is_string()) return;
    const uint16_t anchor_def = s.rules()->def(aid->text());
    const int la = s.rules()->ability_index("lelekhorgony");
    const EntityDef& d = s.rules()->defs[u->def];
    if (anchor_def == kNoDef || la < 0 || d.hero || u->def == anchor_def ||
        std::find(d.abilities.begin(), d.abilities.end(), static_cast<uint16_t>(la)) == d.abilities.end()) {
      return;
    }
    const Position p = *s.world().get<Position>(victim);
    const uint8_t owner = h->owner;
    const uint16_t origin = u->def;
    const EntityId a = s.spawn_unit(anchor_def, owner, p.x, p.y);
    if (a == kNoEntity) return;
    Health& ah = *s.world().get<Health>(a);
    ah.max = std::max(50, s.rules()->defs[origin].hp);
    ah.hp = std::max(1, ah.max * 15 / 100);
    s.world().add(a, ImpAnchor{origin});
  }

  // a keret által nem ismert típusok: Túlhajtás (`overdrive`) és horgonymentés (`anchor_rescue`)
  bool use_ability(Sim& s, const std::vector<EntityId>& units, uint16_t ab, Fixed x, Fixed y, EntityId target) override {
    const AbilityDef& a = s.rules()->abilities[ab];
    const content::Value* t = a.spec ? a.spec->find("type") : nullptr;
    const std::string type = t && t->is_string() ? t->text() : "";
    if (type == "overdrive") return overdrive(s, units, ab, a);
    if (type == "anchor_rescue") return rescue(s, units, target, x, y);
    return false;
  }

  void tick(Sim& s) override {
    // kopás: az alapozás nélküli épület fél életerő fölött 0,5 s-onként max × 0,002 × 0,5
    if (s.tick() % 15 == 0) {
      s.world().pool<ImpUnfounded>().each([&](EntityId e, const ImpUnfounded&) {
        Health* h = s.world().get<Health>(e);
        if (!h || h->hp <= 0) return;
        const content::Value* cfg = foundation_cfg(s, h->owner);
        const content::Value* dc = cfg ? cfg->find("decay") : nullptr;
        const Fixed pct = fx_of(dc ? dc->find("pct_per_s") : nullptr, Fixed::from_ratio(2, 1000));
        const Fixed mn = fx_of(dc ? dc->find("min") : nullptr, Fixed::from_ratio(1, 2));
        Vital* vt = s.world().get<Vital>(e);
        const int64_t floor_m = (int64_t{h->max} * 1000 * mn.raw()) >> 16;
        const int64_t loss = (int64_t{h->max} * 1000 * pct.raw() >> 16) / 2;   // 0,5 s
        const int64_t cur = int64_t{h->hp} * 1000 - (vt ? vt->deficit : 0);
        if (cur <= floor_m) return;
        const int64_t eff = std::max(floor_m, cur - loss);
        h->hp = static_cast<int32_t>((eff + 999) / 1000);
        if (vt) vt->deficit = static_cast<int32_t>(int64_t{h->hp} * 1000 - eff);
      });
    }
    // újratestesülés: a teljesen helyreállított horgonyból az eredeti egység
    std::vector<EntityId> full;
    s.world().pool<ImpAnchor>().each([&](EntityId e, const ImpAnchor&) {
      const Health* h = s.world().get<Health>(e);
      if (h && h->hp > 0 && h->hp >= h->max && !s.world().get<Sheltered>(e)) full.push_back(e);
    });
    for (const EntityId e : full) {
      const uint16_t origin = s.world().get<ImpAnchor>(e)->origin;
      const Position p = *s.world().get<Position>(e);
      const uint8_t owner = s.world().get<Health>(e)->owner;
      s.world().pool<ImpAnchor>().remove(e);
      s.kill(e);
      const EntityId u = s.spawn_unit(origin, owner, p.x, p.y);
      if (u != kNoEntity) s.place_at_free(u, p.x, p.y);
    }
    // horgonymentés
    std::vector<EntityId> carriers;
    s.world().pool<ImpRescue>().each([&](EntityId e, const ImpRescue&) { carriers.push_back(e); });
    for (const EntityId e : carriers) step_rescue(s, e);
  }

 private:
  static bool overdrive(Sim& s, const std::vector<EntityId>& units, uint16_t ab, const AbilityDef& a) {
    const Fixed share = fx_of(a.spec->find("share"), Fixed::from_ratio(1, 2));
    const Fixed self_hp = fx_of(a.spec->find("self_hp"), Fixed::from_ratio(2, 5));
    bool done = false;
    for (const EntityId u : units) {
      Caster* c = s.world().get<Caster>(u);
      const Worker* w = s.world().get<Worker>(u);
      if (!c || !w || !s.ability_ready(u, *c, a)) continue;
      EntityId b = w->state == Worker::Construct ? w->site : 0;
      if (!b) {
        if (const RepairOrder* ro = s.world().get<RepairOrder>(u)) b = ro->building;
      }
      Building* bd = b && s.world().alive(b) ? s.world().get<Building>(b) : nullptr;
      Health* bh = bd ? s.world().get<Health>(b) : nullptr;
      if (!bd || !bh || bh->hp <= 0 || s.gap(u, b) > kReach + Fixed::from_int(1)) continue;
      if (!bd->complete) {
        const int64_t total = int64_t{std::max(1, s.rules()->defs[bd->def].time_ticks)} * 1000;
        const int64_t add = ((total - bd->progress) * share.raw()) >> 16;
        bd->progress = static_cast<int32_t>(std::min<int64_t>(total, bd->progress + add));
        bh->hp = static_cast<int32_t>(std::min<int64_t>(bh->max, bh->hp + int64_t{bh->max} * 9 * add / (10 * total)));
      } else {
        bh->hp = std::min(bh->max, bh->hp + (Fixed::from_int(bh->max - bh->hp) * share).floor_int());
      }
      s.ability_spend(*c, ab, a);
      Health& uh = *s.world().get<Health>(u);   // saját sérülés: páncél és pajzs nem fogja fel
      uh.hp -= static_cast<int32_t>((int64_t{uh.max} * self_hp.raw() + 0x8000) >> 16);   // kerekítve
      if (uh.hp <= 0) {   // halál-esemény (horgony) a tick végén, a közös halálsorban
        uh.hp = 0;
        if (std::find(s.dying_.begin(), s.dying_.end(), u) == s.dying_.end()) s.dying_.push_back(u);
      }
      done = true;
    }
    return done;
  }

  static bool rescue(Sim& s, const std::vector<EntityId>& units, EntityId anchor, Fixed x, Fixed y) {
    const ImpAnchor* an = s.world().alive(anchor) ? s.world().get<ImpAnchor>(anchor) : nullptr;
    if (!an || s.world().get<Sheltered>(anchor)) return false;
    bool taken = false;   // egy horgonyért egyszerre egy mentő
    s.world().pool<ImpRescue>().each([&](EntityId, const ImpRescue& r) { taken = taken || r.anchor == anchor; });
    if (taken) return false;
    const uint8_t owner = s.world().get<Health>(anchor)->owner;
    const Position& ap = *s.world().get<Position>(anchor);
    EntityId best = kNoEntity;
    int64_t bd = 0;
    for (const EntityId u : units) {
      const Health* h = s.world().get<Health>(u);
      if (!h || h->owner != owner || s.world().get<ImpRescue>(u)) continue;
      const Position& p = *s.world().get<Position>(u);
      const int64_t d = core::length_sq_raw(p.x - ap.x, p.y - ap.y);
      if (best == kNoEntity || d < bd || (d == bd && u < best)) {
        best = u;
        bd = d;
      }
    }
    if (best == kNoEntity) return false;
    s.interrupt(best);
    ImpRescue r;
    r.anchor = anchor;
    r.x = x;
    r.y = y;
    s.world().add(best, r);
    return true;
  }

  static void drop_anchor(Sim& s, EntityId anchor, EntityId carrier) {
    if (!s.world().alive(anchor) || !s.world().get<Sheltered>(anchor)) return;
    s.world().pool<Sheltered>().remove(anchor);
    const Position cp = *s.world().get<Position>(carrier);
    if (s.nav_) {
      s.world().add(anchor, Body{});
      s.world().add(anchor, Path{});
      const EntityDef& d = s.rules()->defs[s.world().get<Unit>(anchor)->def];
      s.set_body(anchor, d.radius, 0xFF);
    }
    s.place_at_free(anchor, cp.x, cp.y);
  }

  static void step_rescue(Sim& s, EntityId e) {
    ImpRescue r = *s.world().get<ImpRescue>(e);
    const Health* h = s.world().get<Health>(e);
    if (!h || h->hp <= 0) return;
    if (!s.world().alive(r.anchor) || !s.world().get<ImpAnchor>(r.anchor)) {
      s.world().pool<ImpRescue>().remove(e);
      return;
    }
    if (Combat* c = s.world().get<Combat>(e)) {   // a mentő nem harcol (mint a sima mozgásparancs)
      c->order = Combat::None;
      c->target = 0;
      c->autonomous = 0;
      c->moving = 0;
      c->scan = std::max<int32_t>(c->scan, 2);
    }
    const Position p = *s.world().get<Position>(e);
    const Fixed rad = s.rules()->defs[s.world().get<Unit>(e)->def].radius;
    if (!r.carrying) {   // a horgonyhoz megy, és felveszi
      const Position ap = *s.world().get<Position>(r.anchor);
      if (core::length(ap.x - p.x, ap.y - p.y) <= rad + Fixed::from_ratio(1, 2) + kReach) {
        s.world().pool<Body>().remove(r.anchor);
        s.world().pool<Path>().remove(r.anchor);
        s.world().add(r.anchor, Sheltered{e});
        r.carrying = 1;
        r.repath = 0;
        s.halt(e);
      } else if (--r.repath <= 0) {
        s.order_move(e, ap.x, ap.y);
        r.repath = 24;
      }
      *s.world().get<ImpRescue>(e) = r;
      return;
    }
    if (Position* ap = s.world().get<Position>(r.anchor)) *ap = p;   // a hordozóval együtt mozog
    if (core::length(r.x - p.x, r.y - p.y) <= rad + kReach + Fixed::from_ratio(1, 2)) {   // leteszi
      s.world().pool<ImpRescue>().remove(e);
      drop_anchor(s, r.anchor, e);
      if (Position* ap = s.world().get<Position>(r.anchor)) {
        const Position tp{r.x, r.y};
        s.place_at_free(r.anchor, tp.x, tp.y);
        (void)ap;
      }
      s.halt(e);
      return;
    }
    if (--r.repath <= 0) {
      s.order_move(e, r.x, r.y);
      r.repath = 24;
    }
    *s.world().get<ImpRescue>(e) = r;
  }
};

}  // namespace

std::unique_ptr<FactionModule> make_imperium_module() { return std::make_unique<Module>(); }

}  // namespace totu::sim
