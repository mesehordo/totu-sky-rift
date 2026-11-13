// SPDX-License-Identifier: GPL-3.0-or-later
// Szürke elfek (MECH-POWER-1–3, MECH-FAITH, MECH-MINE; Égkő-fény) – M7, ADR-008; a régi játék (G 1511–1617,
// 2268–2272, 2990–3001, 3155–3197; E 1093, 1736, 1748) és a spec/FAJMECHANIKAK.md szerint:
//   - energiamező: a `no_power` nélküli épület középpontja egy kész, saját mezőadó (`Mechanics.power_radius`) körében
//     építhető; az ellátatlan épület nem termel, nem kutat és nem lő (30 tickenként és az épületváltozáskor újraszámolva);
//   - behívás (`warp_in`): a gyülekezőponthoz legközelebbi mezőadó körébe eső gyülekezőpontnál a nem munkás egység a
//     mezőadónál jelenik meg;
//   - Áhítat: a szentély (`Mechanics.pray_slots`) mellett álló, tétlen munkás fohászkodik; 15 tickenként azonosító
//     sorrendben aktív, amíg szentélyenként ≤ hely és összesen ≤ `faith.max`; bevétel aktívonként `rate` × 0,5 s;
//   - Napfúró (MECH-MINE): a lelőhelyre épült bánya `passive` + a mellette álló munkások (≤ `slots`) szerint termel
//     (`carry / (gather_time + 7,4)` / s fejenként), Napáldással (`bpower` mine_boost) gyorsabban;
//   - Áhítat-parancs (`elves.pray`, népi parancs): a kijelölt munkások a szentélyhez (`target`, 0: a legközelebbi) mennek;
//   - Rezonancia (`Rules.power_add`) a mezőadók sugarát, Arany fohász (`Rules.faith_rate`) a fohász ütemét állítja;
//   - Égkő-fény: a faji `rift.energy_mult` (2,0), `shield_regen_mult` (2,0) és `spell_mult` (1,25) a közös szabály fölött.
#include <algorithm>
#include <string>

#include "content/json.hpp"
#include "sim/rules.hpp"
#include "sim/faction_module.hpp"
#include "sim/sim.hpp"

namespace totu::sim {

namespace {

const Fixed kPrayReach = Fixed::from_ratio(3, 2);   // a szentély lábnyomától (csempe)
const Fixed kMineReach = Fixed::from_ratio(7, 10);  // a bánya lábnyomától (csempe)

struct ElfPower {   // az épület ellátottsága (1: a mezőben, vagy `no_power`)
  uint8_t powered = 1;
  void hash(core::StateHash& h) const { h.add_u64(powered); }
  void save(core::ByteWriter& w) const { w.u8(powered); }
  static ElfPower load(core::ByteReader& r) { ElfPower p; p.powered = r.u8(); return p; }
};

struct ElfMine {   // Napfúró: a tört bevétel (ezred) és a gyorsítás
  int32_t acc = 0;
  int64_t boost_until = 0;
  int32_t boost = 1000;   // ezrelék
  void hash(core::StateHash& h) const { h.add_i64(acc); h.add_i64(boost_until); h.add_i64(boost); }
  void save(core::ByteWriter& w) const { w.i32(acc); w.i64(boost_until); w.i32(boost); }
  static ElfMine load(core::ByteReader& r) {
    ElfMine m;
    m.acc = r.i32(); m.boost_until = r.i64(); m.boost = r.i32();
    return m;
  }
};

const content::Value* sub(const std::shared_ptr<const content::Value>& v, const char* a, const char* b = nullptr) {
  const content::Value* x = v ? v->find(a) : nullptr;
  if (x && b) x = x->is_object() ? x->find(b) : nullptr;
  return x;
}

Fixed num(const content::Value* v, Fixed def) { return v ? v->as_fixed().value_or(def) : def; }

bool flag(const content::Value* v) { return v && v->is_bool() && v->as_bool(); }

Fixed rect_gap(Fixed px, Fixed py, const Building& b) {
  const Fixed dx = std::max({Fixed::from_int(b.tx) - px, Fixed{}, px - Fixed::from_int(b.tx + b.w)});
  const Fixed dy = std::max({Fixed::from_int(b.ty) - py, Fixed{}, py - Fixed::from_int(b.ty + b.h)});
  return core::length(dx, dy);
}

class Module final : public FactionModule {
 public:
  const char* name() const override { return "elves"; }

  void register_components(World& w) override {
    w.register_component<ElfPower>("ElfPower");
    w.register_component<ElfMine>("ElfMine");
  }

  // --- adatok a nyers JSON-ból ------------------------------------------------------------------------------------
  static const FactionDef* faction(Sim& s, uint8_t owner) {
    if (owner < 1 || owner > Sim::kMaxPlayers) return nullptr;
    return s.rules_->faction(s.players_[owner].faction);
  }
  static bool power_faction(Sim& s, uint8_t owner) {
    const FactionDef* f = faction(s, owner);
    const content::Value* m = f ? sub(f->raw, "Faction", "mechanic") : nullptr;
    return m && m->is_string() && m->text() == "power";
  }
  static Fixed power_radius(const EntityDef& d) { return num(sub(d.raw, "Mechanics", "power_radius"), Fixed{}); }
  // a kész kutatások `Rules.<key>` értékei: összeg (`sum`) vagy a legnagyobb
  static Fixed research_rule(Sim& s, uint8_t owner, const char* key, bool sum, Fixed def) {
    Fixed out = def;
    bool any = false;
    for (const std::string& id : s.players_[owner].researched) {
      const ResearchDef* r = s.rules_->research(id);
      const content::Value* v = r ? sub(r->raw, "Rules", key) : nullptr;
      if (!v) continue;
      const Fixed x = num(v, Fixed{});
      if (sum) out = out + x;
      else if (!any || x > out) out = x;
      any = true;
    }
    return out;
  }
  static bool no_power(const EntityDef& d) { return flag(sub(d.raw, "Placement", "no_power")); }
  static int pray_slots(const EntityDef& d) {
    const content::Value* v = sub(d.raw, "Mechanics", "pray_slots");
    return v ? num(v, Fixed{}).floor_int() : 0;
  }

  // a (x, y) pont a játékos valamely kész mezőadójának körében van-e
  static bool in_field(Sim& s, uint8_t owner, Fixed x, Fixed y, EntityId* source = nullptr, EntityId skip = kNoEntity) {
    bool in = false;
    int64_t best = 0;
    const Fixed extra = research_rule(s, owner, "power_add", true, Fixed{});   // Rezonancia
    s.world_.pool<Building>().each([&](EntityId e, const Building& b) {
      const Health* h = s.world_.get<Health>(e);
      if (e == skip || !b.complete || !h || h->owner != owner || h->hp <= 0) return;
      Fixed r = power_radius(s.rules_->defs[b.def]);
      if (r.raw() <= 0) return;
      r = r + extra;
      const Position& p = *s.world_.get<Position>(e);
      const Fixed d = core::length(p.x - x, p.y - y);
      if (d > r) return;
      if (!in || d.raw() < best) {
        in = true;
        best = d.raw();
        if (source) *source = e;
      }
    });
    return in;
  }

  // --- horgok -----------------------------------------------------------------------------------------------------
  bool can_place(Sim& s, uint8_t owner, uint16_t def, int tx, int ty, std::string* error) override {
    if (!power_faction(s, owner)) return true;
    const EntityDef& d = s.rules_->defs[def];
    if (!d.building || no_power(d)) return true;
    const Fixed cx = Fixed::from_int(tx) + Fixed::from_ratio(std::max(1, d.fw), 2);
    const Fixed cy = Fixed::from_int(ty) + Fixed::from_ratio(std::max(1, d.fh), 2);
    if (in_field(s, owner, cx, cy)) return true;
    if (error) *error = "nincs energiamező (Napobeliszk) a helyen";
    return false;
  }

  void on_building_placed(Sim& s, EntityId e) override {
    const Building* b = s.world_.get<Building>(e);
    const Health* h = s.world_.get<Health>(e);
    if (!b || !h || !power_faction(s, h->owner)) return;
    const EntityDef& d = s.rules_->defs[b->def];
    if (!no_power(d)) s.world_.add(e, ElfPower{static_cast<uint8_t>(in_field(s, h->owner, s.world_.get<Position>(e)->x, s.world_.get<Position>(e)->y) ? 1 : 0)});
    if (sub(d.raw, "Mechanics", "mine")) s.world_.add(e, ElfMine{});
    refresh_power(s);   // kész mezőadó lerakva (place_building)
  }

  void on_building_complete(Sim& s, EntityId) override { refresh_power(s); }
  void on_death(Sim& s, EntityId victim, EntityId, uint8_t) override {
    if (s.world_.get<Building>(victim)) refresh_power(s, victim);   // a halott mezőadó már nem lát el
  }

  // behívás: a gyülekezőponthoz legközelebbi mezőadó körében a nem munkás egység a mezőadónál jelenik meg
  void on_unit_spawned(Sim& s, EntityId u) override {
    const Health* h = s.world_.get<Health>(u);
    const Unit* un = s.world_.get<Unit>(u);
    if (!h || !un || s.rules_->defs[un->def].worker) return;
    const FactionDef* f = faction(s, h->owner);
    if (!f || !flag(sub(f->raw, "Faction", "warp_in"))) return;
    const Position up = *s.world_.get<Position>(u);
    EntityId prod = kNoEntity;   // a termelő épület: saját, gyülekezőponttal, és az egység a lábnyoma mellett van
    s.world_.pool<Producer>().each([&](EntityId e, const Producer& pr) {
      const Building* b = s.world_.get<Building>(e);
      const Health* bh = s.world_.get<Health>(e);
      if (prod || !b || !bh || bh->owner != h->owner || !pr.has_rally) return;
      if (rect_gap(up.x, up.y, *b) <= Fixed::from_int(2)) prod = e;
    });
    if (!prod) return;
    const Producer& pr = *s.world_.get<Producer>(prod);
    EntityId src = kNoEntity;
    if (!in_field(s, h->owner, pr.rx, pr.ry, &src) || !src) return;
    const Building& sb = *s.world_.get<Building>(src);
    const Position& sp = *s.world_.get<Position>(src);
    s.place_at_free(u, sp.x, sp.y + Fixed::from_ratio(sb.h, 2) + Fixed::from_int(1));
  }

  // Napáldás (`bpower` mine_boost): a költséget, az újratöltést és a jogosultságot a közös `Sim::use_power` intézi
  bool use_power(Sim& s, uint8_t, EntityId b, uint16_t ability, Fixed, Fixed) override {
    const AbilityDef& a = s.rules_->abilities[ability];
    if (a.effect != "mine_boost") return false;
    ElfMine* m = s.world_.get<ElfMine>(b);
    if (!m) return false;
    m->boost_until = s.tick_ + a.duration;
    m->boost = static_cast<int32_t>(int64_t{num(a.spec ? a.spec->find("mult") : nullptr, Fixed::from_ratio(3, 2)).raw()} * 1000 >> 16);
    return true;
  }

  // `elves.pray`: a `units` saját munkásai a `target` szentélyhez (0: a legközelebbi kész, saját szentélyhez) mennek, és
  // ott – megérkezve, tétlenül – fohászkodnak; a szentély köré, egymás mellé állnak
  bool on_command(Sim& s, const Command& c) override {
    if (c.def != "elves.pray") return false;
    int k = 0;
    for (const EntityId u : c.units) {
      const Health* h = s.world_.get<Health>(u);
      if (!h || h->owner != c.owner || h->hp <= 0 || !s.world_.get<Worker>(u)) continue;
      const Position up = *s.world_.get<Position>(u);
      EntityId sh = c.target;
      if (!shrine_of(s, c.owner, sh)) {
        sh = kNoEntity;
        Fixed best;
        s.world_.pool<Building>().each([&](EntityId e, const Building& b) {
          if (!shrine_of(s, c.owner, e)) return;
          const Fixed d = rect_gap(up.x, up.y, b);
          if (!sh || d < best) { sh = e; best = d; }
        });
      }
      if (!sh) return true;
      if (!s.interrupt(u)) continue;
      s.stop(u);
      // a szentély alsó és felső sora mellé, a munkások sorrendjében (a hely a lábnyomtól 0,6 csempére)
      const Building& b = *s.world_.get<Building>(sh);
      const int side = std::max(1, static_cast<int>(b.w));
      const int col = k % side, row = (k / side) % 2;
      const Fixed x = Fixed::from_int(b.tx + col) + Fixed::from_ratio(1, 2);
      const Fixed y = row == 0 ? Fixed::from_int(b.ty + b.h) + Fixed::from_ratio(3, 5) : Fixed::from_int(b.ty) - Fixed::from_ratio(3, 5);
      s.order_move(u, x, y);
      ++k;
    }
    return true;
  }

  // Égkő-fény: a faj varázssebzése × `rift.spell_mult` (a közös szorzó helyett; a támadó a `src`)
  Fixed damage_taken_mult(Sim& s, EntityId src, EntityId, bool, bool magic) override {
    if (!magic || !src || !s.world_.alive(src) || !s.in_rift(src)) return Fixed::from_int(1);
    const Health* h = s.world_.get<Health>(src);
    const FactionDef* f = h ? faction(s, h->owner) : nullptr;
    const content::Value* rf = f ? sub(f->raw, "Faction", "rift") : nullptr;
    const Fixed mine = num(rf && rf->is_object() ? rf->find("spell_mult") : nullptr, Fixed{});
    const Fixed common = s.rules_->rift.spell_mult;
    if (mine.raw() <= 0 || common.raw() <= 0 || mine == common) return Fixed::from_int(1);
    return mine / common;
  }

  void tick(Sim& s) override {
    if (s.tick_ % 30 == 0) refresh_power(s);
    // ellátatlan épület: a sora áll, nem lő
    s.world_.pool<ElfPower>().each([&](EntityId e, const ElfPower& p) {
      if (p.powered) return;
      if (Producer* pr = s.world_.get<Producer>(e); pr && !pr->queue.empty() && pr->queue.front().progress > 0) {
        --pr->queue.front().progress;   // a termelési rendszer e tickben hozzáadott lépése visszavonva
      }
      if (Combat* c = s.world_.get<Combat>(e)) c->cooldown = std::max(c->cooldown, 2);
    });
    if (s.tick_ % 15 == 0) faith(s);
    mines(s);
    rift(s);
  }

 private:
  static bool shrine_of(Sim& s, uint8_t owner, EntityId e) {
    const Building* b = e ? s.world_.get<Building>(e) : nullptr;
    const Health* h = e ? s.world_.get<Health>(e) : nullptr;
    return b && h && b->complete && h->owner == owner && h->hp > 0 && pray_slots(s.rules_->defs[b->def]) > 0;
  }

  static void refresh_power(Sim& s, EntityId skip = kNoEntity) {
    s.world_.pool<ElfPower>().each([&](EntityId e, ElfPower& p) {
      const Health* h = s.world_.get<Health>(e);
      const Position* pos = s.world_.get<Position>(e);
      p.powered = e != skip && h && pos && in_field(s, h->owner, pos->x, pos->y, nullptr, skip) ? 1 : 0;
    });
  }

  // Áhítat (MECH-FAITH): 15 tickenként
  void faith(Sim& s) {
    for (int o = 1; o <= Sim::kMaxPlayers; ++o) {
      const auto owner = static_cast<uint8_t>(o);
      const FactionDef* f = faction(s, owner);
      const content::Value* fa = f ? sub(f->raw, "Faction", "faith") : nullptr;
      if (!fa || !fa->is_object()) continue;
      const Fixed base = num(fa->find("rate"), Fixed::from_ratio(3, 10));
      const Fixed rate = std::max(base, research_rule(s, owner, "faith_rate", false, base));   // Arany fohász
      const int maxn = num(fa->find("max"), Fixed::from_int(8)).floor_int();
      std::vector<std::pair<EntityId, int>> shrines;   // szentély, szabad hely
      s.world_.pool<Building>().each([&](EntityId e, const Building& b) {
        const Health* h = s.world_.get<Health>(e);
        const int slots = pray_slots(s.rules_->defs[b.def]);
        if (b.complete && h && h->owner == owner && h->hp > 0 && slots > 0) shrines.emplace_back(e, slots);
      });
      if (shrines.empty()) continue;
      int active = 0;
      s.world_.pool<Worker>().each([&](EntityId e, const Worker& w) {   // azonosító szerinti sorrendben
        if (active >= maxn) return;
        const Health* h = s.world_.get<Health>(e);
        const Movement* mv = s.world_.get<Movement>(e);
        if (!h || h->owner != owner || h->hp <= 0 || w.state != Worker::Idle || w.inside || s.sheltered(e) ||
            (mv && mv->moving) || s.incapacitated(e)) {
          return;
        }
        const Position& p = *s.world_.get<Position>(e);
        for (auto& [sh, free] : shrines) {
          if (free <= 0 || rect_gap(p.x, p.y, *s.world_.get<Building>(sh)) > kPrayReach) continue;
          --free;
          ++active;
          break;
        }
      });
      if (!active) continue;
      // aktívonként rate × 0,5 s (ezredben)
      const int64_t milli = (int64_t{active} * rate.raw() * 1000 >> 16) / 2;
      s.players_[owner].res[kFaith] += milli;
      s.players_[owner].gathered[kFaith] += milli;
    }
  }

  // Napfúró (MECH-MINE): tickenként
  void mines(Sim& s) {
    std::vector<EntityId> ids;
    s.world_.pool<ElfMine>().each([&](EntityId e, const ElfMine&) { ids.push_back(e); });
    for (const EntityId e : ids) {
      const Building* b = s.world_.get<Building>(e);
      const Health* h = s.world_.get<Health>(e);
      if (!b || !h || !b->complete || h->hp <= 0) continue;
      const EntityDef& d = s.rules_->defs[b->def];
      const content::Value* mine = sub(d.raw, "Mechanics", "mine");
      const Fixed passive = num(mine ? mine->find("passive") : nullptr, Fixed{});
      const int slots = num(mine ? mine->find("slots") : nullptr, Fixed::from_int(3)).floor_int();
      EntityId node = kNoEntity;   // a lábnyom alatti lelőhely
      s.world_.pool<ResourceNode>().each([&](EntityId ne, const ResourceNode& n) {
        if (node || n.amount <= 0 || (n.kind != kStone && n.kind != kMeteor)) return;
        if (n.tx < b->tx + b->w && n.tx + n.size > b->tx && n.ty < b->ty + b->h && n.ty + n.size > b->ty) node = ne;
      });
      if (!node) continue;
      int miners = 0;
      s.world_.pool<Worker>().each([&](EntityId we, const Worker& w) {
        if (miners >= slots) return;
        const Health* wh = s.world_.get<Health>(we);
        const Movement* mv = s.world_.get<Movement>(we);
        if (!wh || wh->owner != h->owner || wh->hp <= 0 || w.state != Worker::Idle || w.inside || (mv && mv->moving)) return;
        const Position& p = *s.world_.get<Position>(we);
        if (rect_gap(p.x, p.y, *b) <= kMineReach) ++miners;
      });
      ResourceNode& n = *s.world_.get<ResourceNode>(node);
      const ResourceDef& rd = s.rules_->resources[n.kind];
      // fejenként carry / (gather_time + 7,4) egység / s
      const Fixed per_s = Fixed::from_int(rd.carry) / (rd.gather_time + Fixed::from_ratio(74, 10));
      ElfMine& m = *s.world_.get<ElfMine>(e);
      int64_t add = int64_t{((passive + Fixed::from_int(miners)) * per_s).raw()} * 1000 / kTickHz >> 16;
      if (m.boost_until > s.tick_) add = add * m.boost / 1000;
      m.acc += static_cast<int32_t>(add);
      const int32_t whole = std::min(m.acc / 1000, n.amount);
      if (whole <= 0) continue;
      m.acc -= whole * 1000;
      n.amount -= whole;
      const int kind = n.kind;
      s.income(h->owner, kind, whole);
      if (s.world_.get<ResourceNode>(node)->amount <= 0) s.remove_resource(node);
    }
  }

  // Égkő-fény: a faji szorzók a közös (`game.rift`) fölött
  void rift(Sim& s) {
    std::vector<EntityId> ids;
    s.world_.pool<Unit>().each([&](EntityId e, const Unit&) {
      if (s.in_rift(e)) ids.push_back(e);
    });
    for (const EntityId e : ids) {
      const Health* h = s.world_.get<Health>(e);
      const FactionDef* f = h ? faction(s, h->owner) : nullptr;
      if (!f) continue;
      const Fixed em = num(sub(f->raw, "Faction", "rift") ? sub(f->raw, "Faction", "rift")->find("energy_mult") : nullptr, Fixed{});
      const Fixed sm = num(sub(f->raw, "Faction", "rift") ? sub(f->raw, "Faction", "rift")->find("shield_regen_mult") : nullptr, Fixed{});
      const EntityDef& d = s.rules_->defs[s.world_.get<Unit>(e)->def];
      if (em > s.rules_->rift.energy_mult) {
        if (Caster* c = s.world_.get<Caster>(e); c && d.energy_max > 0) {
          const int32_t cap = s.stat(e, "Energy.max", Fixed::from_int(d.energy_max)).floor_int() * 1000;
          const Fixed extra = s.rules_->energy_regen_per_tick * (em - s.rules_->rift.energy_mult);
          if (c->energy < cap) c->energy = std::min<int32_t>(cap, c->energy + static_cast<int32_t>(int64_t{extra.raw()} * 1000 >> 16));
        }
      }
      if (sm > Fixed::from_int(1)) {
        Vital* v = s.world_.get<Vital>(e);
        if (!v) continue;
        const int64_t cap = int64_t{std::max(0, s.stat(e, "Health.shield", Fixed::from_int(d.shield)).floor_int())} * 1000;
        const int64_t delay = d.shield_delay > 0 ? d.shield_delay : s.rules_->shield_regen_delay;
        if (cap <= 0 || v->shield >= cap || s.tick_ - v->last_hit <= delay) continue;
        const Fixed extra = s.rules_->shield_regen_per_tick * (sm - Fixed::from_int(1));
        v->shield = static_cast<int32_t>(std::min<int64_t>(cap, v->shield + (int64_t{extra.raw()} * 1000 >> 16)));
      }
    }
  }
};

}  // namespace

std::unique_ptr<FactionModule> make_elves_module() { return std::make_unique<Module>(); }

}  // namespace totu::sim
