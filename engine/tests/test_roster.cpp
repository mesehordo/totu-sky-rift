// SPDX-License-Identifier: GPL-3.0-or-later
// Népi lefedettség (M7; ADR-008): minden főnép listája – a sablon `Roster` mezőjéből, adatvezérelten – betöltve;
// minden egysége képezhető a saját épületéből a feltételeivel (vagy képességgel idézhető), minden épülete felépíthető
// a saját munkásával, és minden kutatása elkészül. Új nép: csak a sablonja kell, a próba magától lefedi.
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "check.hpp"
#include "match_util.hpp"

using namespace totu;
using namespace totu::testing;

namespace {

const sim::Rules& R() { return *content_once().rules; }
uint16_t def_of(const std::string& id) { return R().def(id); }

// a BUILD-ALAP szabad, sík helyei a 3 × 3-as épületeknek
const std::vector<std::pair<int, int>> kSpots = {{12, 9}, {16, 9}, {12, 13}, {16, 13}, {0, 16}, {3, 16}, {12, 17}, {16, 17}};

std::vector<std::string> main_factions() {
  std::vector<std::string> out;
  for (const auto& f : R().factions) {
    if (!f.ally && !f.neutral) out.push_back(f.id);
  }
  return out;
}

std::vector<std::string> roster(const std::string& faction, const char* key) {
  std::vector<std::string> out;
  const auto* v = content_once().set["faction"].get("faction_" + faction);
  const auto* r = v ? v->find("Roster") : nullptr;
  const auto* list = r ? r->find(key) : nullptr;
  if (list) {
    for (const auto& i : list->items()) out.push_back(i.text());
  }
  return out;
}

struct Setup {
  std::string f;
  Match m;
  size_t next = 0;
  explicit Setup(const std::string& faction)
      : f(faction), m("BUILD-ALAP", true, true, {faction, faction == "wullof" ? "ursan" : "wullof"}) {}
  sim::Sim& s() { return *m.sim; }
  std::pair<int, int> spot() { return kSpots[next++ % kSpots.size()]; }
  bool has_building(uint16_t def) {
    bool found = false;
    s().world().pool<sim::Building>().each([&](sim::EntityId e, const sim::Building& b) {
      found = found || (b.def == def && b.complete && s().world().get<sim::Health>(e)->owner == 1);
    });
    return found;
  }
  // csempét teremtő épület (tavacska, lapkő): elkészülve a csempébe olvad – a célhely lábnyomát fedi le
  static bool tile_maker(const sim::EntityDef& d) {
    const auto* l = d.raw ? d.raw->find("Legacy") : nullptr;
    return l && (l->find("pond_tile") || l->find("foundation_tile"));
  }
  std::pair<int, int> target;   // a csempét váró célhely (a lista első szabad helye)
  std::pair<int, int> cover_site(uint16_t maker, uint16_t def) {
    const auto& md = R().defs[maker];
    const auto& dd = R().defs[def];
    if (target.first < 0) {
      for (size_t k = 0; k < kSpots.size() && target.first < 0; ++k) {
        if (margin_free(def, kSpots[k].first, kSpots[k].second)) target = kSpots[k];
      }
    }
    if (target.first < 0) return {-1, -1};
    std::pair<int, int> best{-1, -1};
    int bo = 0;
    for (int y = target.second - md.fh + 1; y < target.second + dd.fh; ++y) {
      for (int x = target.first - md.fw + 1; x < target.first + dd.fw; ++x) {
        const int ox = std::min(x + md.fw, target.first + dd.fw) - std::max(x, target.first);
        const int oy = std::min(y + md.fh, target.second + dd.fh) - std::max(y, target.second);
        if (ox * oy > bo && s().can_place(1, maker, x, y)) bo = ox * oy, best = {x, y};
      }
    }
    return best;
  }
  // szabályos hely a főépület körül (a nép elhelyezési szabálya – mező, fény, mélység, tó – szerint); ha nincs, a lista
  // a lábnyom körül egy csempe szabad marad (a munkás nem zárul be)
  bool margin_free(uint16_t def, int x, int y) {
    const auto& d = R().defs[def];
    bool ok = true;
    s().world().pool<sim::Building>().each([&](sim::EntityId, const sim::Building& b) {
      ok = ok && !(x - 1 < b.tx + b.w && b.tx < x + d.fw + 1 && y - 1 < b.ty + b.h && b.ty < y + d.fh + 1);
    });
    return ok;
  }
  std::pair<int, int> hq_at() {
    std::pair<int, int> c{0, 0};
    s().world().pool<sim::Building>().each([&](sim::EntityId e, const sim::Building& b) {
      if (b.def == R().faction(f)->hq && s().world().get<sim::Health>(e)->owner == 1) c = {b.tx, b.ty};
    });
    return c;
  }
  std::pair<int, int> legal_site(uint16_t def) {
    const auto [cx, cy] = hq_at();
    std::pair<int, int> best{-1, -1};
    int bd = 1 << 30;
    for (int y = 0; y < m.map->height(); ++y) {
      for (int x = 0; x < m.map->width(); ++x) {
        const int d2 = (x - cx) * (x - cx) + (y - cy) * (y - cy);
        if (d2 < bd && s().can_place(1, def, x, y) && margin_free(def, x, y)) bd = d2, best = {x, y};
      }
    }
    return best;
  }
  // szabályos hely a főépület körül (a nép elhelyezési szabálya – mező, fény, mélység, tó – szerint); ha nincs, a nép egy
  // feltétel nélküli épülete teremti meg (mezőadó, lámpás, tavacska …): a munkás rendesen felépíti (a befejezési horog
  // is lefut), amíg kell; végső esetben a lista
  std::pair<int, int> site(uint16_t def) {
    target = {-1, -1};
    auto best = legal_site(def);
    for (int round = 0; best.first < 0 && round < 16; ++round) {
      bool progressed = false;
      // előbb a csempeteremtők (a célhelyre), csak ha azok nem segítenek, a többi (a célhelyen kívül)
      for (size_t n = 0; n < 2 * R().defs.size() && best.first < 0; ++n) {
        if (n == R().defs.size() && progressed) break;
        const size_t i = n % R().defs.size();
        const auto& d = R().defs[i];
        const uint16_t id = static_cast<uint16_t>(i);
        if (d.faction != f || !d.building || !d.reqs.empty() || d.fw <= 0 || id == def || has_building(id)) continue;
        if (tile_maker(d) != (n < R().defs.size())) continue;
        const auto at = tile_maker(d) ? cover_site(id, def) : legal_site(id);
        if (at.first < 0) continue;
        const auto& dd = R().defs[def];
        if (!tile_maker(d) && target.first >= 0 && at.first < target.first + dd.fw && target.first < at.first + d.fw &&
            at.second < target.second + dd.fh && target.second < at.second + d.fh) continue;
        rich();
        auto ws = m.workers(1);
        const auto [cx, cy] = hq_at();
        if (ws.empty()) ws.push_back(s().spawn_unit(R().faction(f)->worker, 1, core::Fixed::from_int(cx), core::Fixed::from_int(cy + 5)));
        if (!s().order_build(ws.front(), id, at.first, at.second)) continue;
        m.run(d.time_ticks + 400);
        progressed = true;
        best = legal_site(def);
      }
      if (!progressed) break;
    }
    return best.first >= 0 ? best : spot();
  }
  void place(uint16_t def) {
    const auto [x, y] = site(def);
    s().place_building(def, 1, x, y, true);
    m.run(1);
  }
  void rich() {
    for (int r = 0; r < sim::kResources; ++r) s().add_resources(1, r, 50000);
  }
  // felemelkedés: a nép feltétel nélküli épületeiből annyi különböző fajta, amennyi kell
  void ascend() {
    for (size_t i = 0; i < R().defs.size() && !s().player(1).ascended; ++i) {
      const auto& d = R().defs[i];
      if (d.faction != f || !d.building || !d.reqs.empty() || d.fw <= 0 || has_building(static_cast<uint16_t>(i))) continue;
      place(static_cast<uint16_t>(i));
      m.run(16);
    }
  }
  bool research_done(const std::string& id) {
    const auto& r = s().player(1).researched;
    return std::binary_search(r.begin(), r.end(), id);
  }
  bool do_research(const std::string& id) {
    if (research_done(id)) return true;
    const sim::ResearchDef* rd = R().research(id);
    if (!rd) return false;
    satisfy(rd->reqs);
    for (size_t i = 0; i < R().defs.size(); ++i) {   // a kutató épület
      const auto& d = R().defs[i];
      if (d.faction != f || std::find(d.researches.begin(), d.researches.end(), id) == d.researches.end()) continue;
      if (!has_building(static_cast<uint16_t>(i))) {
        satisfy(d.reqs);
        place(static_cast<uint16_t>(i));
      }
      break;
    }
    m.run(1);
    rich();
    std::string err;
    if (!s().research(1, id, &err)) {
      std::printf("  %s kutatás %s: %s\n", f.c_str(), id.c_str(), err.c_str());
      return false;
    }
    m.run(rd->time_ticks + 2);
    return research_done(id);
  }
  void satisfy(const std::vector<sim::EntityDef::Req>& reqs) {
    for (const auto& q : reqs) {
      switch (q.kind) {
        case sim::EntityDef::Req::Building: {
          satisfy(R().defs[def_of(q.id)].reqs);
          if (!has_building(def_of(q.id))) place(def_of(q.id));
          break;
        }
        case sim::EntityDef::Req::Research:
          do_research(q.id);
          break;
        case sim::EntityDef::Req::Path:
          ascend();
          if (s().player(1).path.empty()) s().choose_path(1, q.id);
          break;
        case sim::EntityDef::Req::Ascended:
          ascend();
          break;
        case sim::EntityDef::Req::Ally:
          for (size_t i = 0; i < R().defs.size(); ++i) {
            if (R().defs[i].faction == f && R().defs[i].diplomacy && !has_building(static_cast<uint16_t>(i))) place(static_cast<uint16_t>(i));
          }
          m.run(1);
          rich();
          s().choose_ally(1, q.id);
          break;
      }
    }
  }
  void supply() {   // létszám: a nép létszámépülete(i)
    int n = 0;
    for (size_t i = 0; i < R().defs.size() && n < 2; ++i) {
      const auto& d = R().defs[i];
      if (d.faction == f && d.building && d.supply_provides > 0 && d.reqs.empty() && d.fw > 0) {
        place(static_cast<uint16_t>(i));
        ++n;
      }
    }
  }
};

int count_owned(sim::Sim& s, uint16_t def) {
  int n = 0;
  s.world().pool<sim::Unit>().each([&](sim::EntityId e, const sim::Unit& u) {
    if ((def == sim::kNoDef || u.def == def) && s.world().get<sim::Health>(e)->owner == 1 && s.world().get<sim::Health>(e)->hp > 0) ++n;
  });
  return n;
}

// más módon keletkező egység: egy sablon (képesség: idézés, átalakulás, gubó; egység: lerakott lény; nép: szekér,
// lélekhorgony …) szöveges értékként hivatkozik rá – a listák, az AI-hadrendek és a címkeszűrők nem számítanak
bool refers(const content::Value& v, const std::string& id) {
  if (v.is_string()) return v.text() == id;
  for (const auto& i : v.items()) {
    if (refers(i, id)) return true;
  }
  for (const auto& [k, m] : v.members()) {
    if (k == "Roster" || k == "army" || k == "units" || k == "tags" || k == "filter" || k == "Identity") continue;
    if (refers(m, id)) return true;
  }
  return false;
}

std::string origin_of(const std::string& id) {
  for (auto& [ns, store] : content_once().set.ns) {
    for (const auto& t : store.ids()) {
      if (t == id) continue;
      const content::Value* v = store.get(t);
      if (v && refers(*v, id)) return ns + "/" + t;
    }
  }
  return {};
}

uint16_t producer_of(const std::string& faction, uint16_t unit) {
  for (size_t i = 0; i < R().defs.size(); ++i) {
    const auto& d = R().defs[i];
    if (d.faction == faction && d.building && std::find(d.produces.begin(), d.produces.end(), unit) != d.produces.end()) {
      return static_cast<uint16_t>(i);
    }
  }
  return sim::kNoDef;
}

}  // namespace

TEST_CASE("népi lefedettség: minden főnép listája betöltve (egység, épület, kutatás), munkással és főépülettel") {
  const auto fs = main_factions();
  CHECK(fs.size() >= 7);
  for (const auto& f : fs) {
    const auto units = roster(f, "units"), blds = roster(f, "buildings"), res = roster(f, "researches");
    CHECK(!units.empty() && !blds.empty());
    for (const auto& u : units) {
      const bool ok = def_of(u) != sim::kNoDef && !R().defs[def_of(u)].building;
      if (!ok) std::printf("  %s: hiányzó egység %s\n", f.c_str(), u.c_str());
      CHECK(ok);
    }
    for (const auto& b : blds) {
      const bool ok = def_of(b) != sim::kNoDef && R().defs[def_of(b)].building;
      if (!ok) std::printf("  %s: hiányzó épület %s\n", f.c_str(), b.c_str());
      CHECK(ok);
    }
    for (const auto& r : res) {
      if (!R().research(r)) std::printf("  %s: hiányzó kutatás %s\n", f.c_str(), r.c_str());
      CHECK(R().research(r) != nullptr);
    }
    CHECK(R().faction(f)->hq != sim::kNoDef && R().faction(f)->worker != sim::kNoDef);
    std::printf("  %s: %zu egység, %zu épület, %zu kutatás\n", f.c_str(), units.size(), blds.size(), res.size());
  }
}

TEST_CASE("népi lefedettség: minden főnép minden egysége képezhető a feltételeivel (vagy képességgel idézhető)") {
  for (const auto& f : main_factions()) {
    int trained = 0, summoned = 0, total = 0;
    for (const auto& id : roster(f, "units")) {
      ++total;
      const uint16_t ud = def_of(id);
      const uint16_t prod = producer_of(f, ud);
      if (prod == sim::kNoDef) {
        const bool by_ability = !origin_of(id).empty();
        if (!by_ability) std::printf("  %s: %s se nem képezhető, se más módon nem keletkezik\n", f.c_str(), id.c_str());
        CHECK(by_ability);
        summoned += by_ability ? 1 : 0;
        continue;
      }
      Setup st(f);
      const sim::EntityDef& d = R().defs[ud];
      st.satisfy(R().defs[prod].reqs);
      if (!st.has_building(prod)) st.place(prod);
      st.satisfy(d.reqs);
      st.supply();
      st.m.run(1);
      st.rich();
      const int before = count_owned(st.s(), sim::kNoDef);   // a falka több (másik) egység
      std::string err;
      const bool ok = st.s().train(1, ud, 0, &err);
      if (!ok) std::printf("  %s: %s: %s\n", f.c_str(), id.c_str(), err.c_str());
      CHECK(ok);
      st.m.run(d.time_ticks + 30);
      const bool out = count_owned(st.s(), sim::kNoDef) > before;
      if (ok && !out) std::printf("  %s: %s nem jelent meg\n", f.c_str(), id.c_str());
      CHECK(out);
      trained += ok && out ? 1 : 0;
    }
    std::printf("  %s: %d/%d képezve, %d idézve\n", f.c_str(), trained, total, summoned);
    CHECK(trained + summoned == total);
  }
}

TEST_CASE("népi lefedettség: minden főnép minden épülete felépíthető a saját munkásával, a feltételeivel") {
  for (const auto& f : main_factions()) {
    int built = 0, total = 0;
    for (const auto& id : roster(f, "buildings")) {
      ++total;
      const uint16_t bd = def_of(id);
      const sim::EntityDef& d = R().defs[bd];
      Setup st(f);
      st.satisfy(d.reqs);
      st.m.run(1);
      st.rich();
      const auto [x, y] = st.site(bd);
      const auto workers = st.m.workers(1);
      std::string err;
      const bool ok = !workers.empty() && st.s().order_build(workers.front(), bd, x, y, &err);
      if (!ok) std::printf("  %s: %s: %s\n", f.c_str(), id.c_str(), err.c_str());
      CHECK(ok);
      // kész; vagy (lapkő, tavacska) elkészülve a csempébe olvadt: a helyén állt, majd eltűnt
      bool done = false, seen = false, gone = false;
      for (int t = 0; t < d.time_ticks + 400 && !done; t += 10) {
        st.m.run(10);
        bool here = false;
        st.s().world().pool<sim::Building>().each([&](sim::EntityId, const sim::Building& b) {
          if (b.def != bd || b.tx != x || b.ty != y) return;
          here = true;
          done = done || b.complete;
        });
        seen = seen || here;
        gone = gone || (seen && !here);
      }
      done = done || gone;
      if (ok && !done) std::printf("  %s: %s nem készült el\n", f.c_str(), id.c_str());
      CHECK(done);
      built += ok && done ? 1 : 0;
    }
    std::printf("  %s: %d/%d felépítve\n", f.c_str(), built, total);
    CHECK(built == total);
  }
}

TEST_CASE("népi lefedettség: minden főnép minden kutatása elkészül a feltételeivel") {
  for (const auto& f : main_factions()) {
    int done = 0, total = 0;
    for (const auto& id : roster(f, "researches")) {
      ++total;
      Setup st(f);
      const bool ok = st.do_research(id);
      if (!ok) std::printf("  %s: %s nem készült el\n", f.c_str(), id.c_str());
      CHECK(ok);
      done += ok ? 1 : 0;
    }
    std::printf("  %s: %d/%d kutatás\n", f.c_str(), done, total);
    CHECK(done == total);
  }
}
