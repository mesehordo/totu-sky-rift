// SPDX-License-Identifier: GPL-3.0-or-later
// A játék közbeni HUD (M8): a régi `tools/test_hud.py` pontjai – panelsorrend és rácsméretek 3 felbontáson, 1:1
// portré, billentyű = kattintás (ugyanaz a parancs), ismétlés-védelem, modális tiltás, kijelölés-rács, hős/nem hős
// középső panel, a felső erőforrássáv sorrendje. A HUD-modell grafikus kártya nélkül fut; a parancsok a közös
// parancs-API-n (sim::Command → a munkamenet lépése) hajtódnak végre, és a szimuláció állapotán ellenőrizzük őket.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "check.hpp"
#include "client/hud_model.hpp"
#include "client/session.hpp"
#include "content/json.hpp"
#include "render/font.hpp"

using namespace totu;
using client::HudModel;

namespace {

const std::string kMap = std::string(TOTU_TEST_DATA) + "/maps/ECO-ALAP.json";

// egy meccs a HUD-dal: a játékos népe a croftianok ellen (gép nélkül), 1920×1080
struct HudMatch {
  client::Session s;
  std::shared_ptr<const client::Snapshot> snap;
  client::Snapshot custom;            // módosított pillanatkép (pl. felemelkedés)
  HudModel hud;
  std::vector<sim::Command> calls;    // a kiadott parancsok
  bool execute = true;                // false: csak rögzít (a következő lépés nem kapja meg)
  std::vector<sim::Command> pending;
  bool ok = false;
  explicit HudMatch(const std::string& fac) {
    std::string err;
    ok = s.load(TOTU_CONTENT_DIR, kMap, 20261112, &err) && s.start_match({fac, "croftian"}, true, &err, {});
    if (!ok) std::printf("  meccs: %s\n", err.c_str());
    hud.set_screen(1920, 1080);
    hud.set_map_size(s.map().width(), s.map().height());
    step(2);
  }
  void bind(const client::Snapshot* sn) {
    hud.bind(sn, s.rules(), [this](const std::string& k) { return s.display_name(k); },
             [this](const sim::Command& c) {
               calls.push_back(c);
               if (execute) pending.push_back(c);
             });
  }
  void step(int n = 1) {
    std::vector<client::FxEvent> fx;
    for (int i = 0; i < n; ++i) {
      snap = s.step(pending, fx);
      pending.clear();
    }
    bind(snap.get());
  }
  void key(int k, bool up = true) {
    hud.key_down(k);
    if (up) hud.key_up(k);
  }
  void click(const client::Rect& r, int button = 1) {
    hud.click(static_cast<float>(r.centerx()), static_cast<float>(r.centery()), button);
  }
  std::vector<sim::EntityId> own(bool workers) const {
    std::vector<sim::EntityId> out;
    for (const auto& u : snap->units) {
      if (u.owner == 1 && u.kind == client::SnapUnit::kUnit && u.def < s.rules()->defs.size() &&
          s.rules()->defs[u.def].worker == workers) {
        out.push_back(u.id);
      }
    }
    return out;
  }
  const client::SnapUnit* hq() const {
    const sim::FactionDef* f = s.rules()->faction(snap->players[1].faction);
    for (const auto& u : snap->units) {
      if (u.owner == 1 && u.kind == client::SnapUnit::kBuilding && f && u.def == f->hq) return &u;
    }
    return nullptr;
  }
};

// a parancs összehasonlítható alakja
std::string sig(const sim::Command& c) {
  std::string s = std::to_string(static_cast<int>(c.kind)) + "|" + c.def + "|" + std::to_string(c.target) + "|" +
                  std::to_string(c.rx) + "," + std::to_string(c.ry) + "|" + std::to_string(c.x.raw()) + "," +
                  std::to_string(c.y.raw()) + "|";
  for (const auto u : c.units) s += std::to_string(u) + ",";
  return s;
}

}  // namespace

TEST_CASE("HUD elrendezés: panelsorrend, rácsok, 1:1 portré, felső sáv 3 felbontáson (test_hud.py)") {
  for (const auto& [sw, sh] : {std::pair{1280, 720}, std::pair{1920, 1080}, std::pair{2560, 1440}}) {
    const client::HudLayout L = client::hud_layout(sw, sh);
    // minimap → banner → karakterlap → tárgyak → kijelölés → banner → parancsrács
    const client::Rect order[7] = {L.minimap, L.banner1, L.char_sheet, L.inv, L.sel, L.banner2, L.cmd};
    bool sorted = true;
    for (int i = 0; i + 1 < 7; ++i) sorted = sorted && order[i].right() <= order[i + 1].x;
    CHECK(sorted);
    CHECK(L.hud.x >= 0 && L.hud.right() <= sw && L.hud.bottom() == sh);   // a HUD a képernyőn belül, alul
    CHECK(L.portrait.w == L.portrait.h);                                  // portré 1:1
    auto shape = [](const auto& cells) {
      std::set<int> xs, ys;
      for (const auto& c : cells) {
        xs.insert(c.x);
        ys.insert(c.y);
      }
      return std::pair{xs.size(), ys.size()};
    };
    CHECK(shape(L.inv_cells) == (std::pair<size_t, size_t>{2, 3}));    // tárgyak 2×3
    CHECK(shape(L.sel_cells) == (std::pair<size_t, size_t>{4, 3}));    // kijelölés 4×3
    CHECK(shape(L.cmd_cells) == (std::pair<size_t, size_t>{4, 3}));    // parancsrács 4×3
    CHECK(L.cmd_cells[0].w >= 40);                                      // parancsikon legalább 40 px
    CHECK(std::abs(L.topbar.centerx() - sw / 2) <= 1);                  // felső sáv középen
    CHECK(L.allies.right() <= L.resbar.x && L.resbar.right() <= L.gear.x);   // [szövetségesek][erőforrások][fogaskerék]
  }
  CHECK(std::string(client::kSlotKeys) == "WERTSDFGXCVB");   // billentyűk: W E R T / S D F G / X C V B
  bool keys = true;
  for (int i = 0; i < 12; ++i) keys = keys && client::key_slot("WERTSDFGXCVB"[i]) == i && client::key_slot("wertsdfgxcvb"[i]) == i;
  CHECK(keys);                                               // billentyű → hely sorrend
  CHECK(client::key_slot('A') == -1 && client::key_slot('Q') == -1);
  // erőforrás-sorrend: meteorit, fa, arany (+ létszám)
  CHECK(client::kResOrder[0] == sim::kMeteor && client::kResOrder[1] == sim::kWood && client::kResOrder[2] == sim::kGold);
  CHECK(client::hud_layout(1920, 1080).res.size() == 4);
}

TEST_CASE("HUD parancsrács: billentyű = kattintás, munkás Építés oldal, főépület W kiképez (test_hud.py)") {
  for (const char* fac : {"wullof", "ursan", "elves", "croftian"}) {
    HudMatch m(fac);
    CHECK(m.ok);
    if (!m.ok) continue;
    const auto workers = m.own(true);
    const client::SnapUnit* hq = m.hq();
    CHECK(!workers.empty() && hq);
    if (workers.empty() || !hq) continue;
    const sim::EntityId hq_id = hq->id;
    for (const auto& sel : {std::vector<sim::EntityId>{workers[0]}, std::vector<sim::EntityId>{hq_id}}) {
      bool same = true;
      for (int i = 0; i < 12; ++i) {
        std::string res[2];
        for (int via = 0; via < 2; ++via) {   // 0: billentyű, 1: kattintás
          m.hud.sel = sel;
          m.hud.mode = {};
          m.hud.cmd_page.clear();
          m.hud.page_no = 0;
          m.calls.clear();
          m.execute = false;
          if (via == 0) m.key(client::kSlotKeys[i]);
          else m.click(m.hud.layout().cmd_cells[static_cast<size_t>(i)]);
          std::string r;
          for (const auto& c : m.calls) r += sig(c) + ";";
          res[via] = r + "#" + std::to_string(m.hud.mode.kind) + m.hud.mode.ability + "#" + m.hud.cmd_page;
        }
        if (res[0] != res[1]) {
          std::printf("  %s %c: billentyű %s ≠ kattintás %s\n", fac, client::kSlotKeys[i], res[0].c_str(), res[1].c_str());
          same = false;
        }
      }
      CHECK(same);   // mind a 12 hely billentyűre és kattintásra ugyanazt teszi
    }
    m.execute = true;
    m.calls.clear();
    // munkás: W = mozgás-célzás, Esc megszakítja, S = Építés oldal, ott B = Vissza
    m.hud.sel = {workers[0]};
    m.hud.mode = {};
    m.hud.cmd_page.clear();
    m.key('W');
    CHECK(m.hud.mode.kind == client::HudMode::Move);
    m.key(HudModel::kKeyEscape);
    CHECK(m.hud.mode.kind == client::HudMode::None);
    m.key('S');
    CHECK(m.hud.cmd_page == "build");
    const client::CommandCard card = m.hud.command_card();
    CHECK(card[11] && card[11]->id == "back");
    m.key('B');
    CHECK(m.hud.cmd_page.empty());
    // főépület: W = az első kiképezhető egység → a sorba kerül (a szimulációban)
    m.hud.sel = {hq_id};
    const client::CommandCard hcard = m.hud.command_card();
    CHECK(hcard[0] && hcard[0]->kind == client::HudSlot::Unit);
    if (hcard[0] && hcard[0]->kind == client::HudSlot::Unit) {
      for (int k = 0; k < sim::kResources; ++k) m.s.sim().add_resources(1, k, 9999);
      m.step();
      const int n0 = m.snap->find(hq_id)->queue_n;
      m.key('W');
      m.step();
      const client::SnapUnit* h2 = m.snap->find(hq_id);
      CHECK(h2 && h2->queue_n == n0 + 1);
      CHECK(h2 && h2->queue_n > 0 && h2->queue_def[0] == hcard[0]->def);
    }
  }
}

TEST_CASE("HUD bemenet-védelem: ismétlés, szövegbevitel, szünet, irányválasztó ablak, Esc (test_hud.py)") {
  HudMatch m("ursan");
  CHECK(m.ok);
  if (!m.ok) return;
  const auto w = m.own(true);
  CHECK(!w.empty());
  if (w.empty()) return;
  m.hud.sel = {w[0]};
  m.calls.clear();
  m.hud.key_down('E');
  m.hud.key_down('E');   // ismétlés, nincs felengedés
  m.hud.key_up('E');
  int stops = 0;
  for (const auto& c : m.calls) stops += c.kind == sim::Command::Kind::Stop;
  CHECK(stops == 1);     // egy lenyomás = egy parancs
  m.hud.key_down('E', true);   // az SDL ismétlés-jelzője sem ismétel
  m.hud.key_up('E');
  stops = 0;
  for (const auto& c : m.calls) stops += c.kind == sim::Command::Kind::Stop;
  CHECK(stops == 1);
  m.calls.clear();
  m.hud.text_focus = true;
  m.key('E');
  m.hud.text_focus = false;
  CHECK(m.calls.empty());   // szövegbevitel alatt nincs parancs
  m.hud.paused = true;
  m.key('E');
  m.click(m.hud.layout().cmd_cells[1]);
  m.hud.paused = false;
  CHECK(m.calls.empty());   // szünetben nincs parancs
  m.custom = *m.snap;       // felemelkedett, még nincs iránya: az irányválasztó ablak nyitva
  m.custom.players[1].ascended = true;
  m.custom.players[1].path.clear();
  m.bind(&m.custom);
  m.hud.path_hidden = false;
  CHECK(m.hud.path_pending());
  m.key('E');
  m.click(m.hud.layout().cmd_cells[1]);
  CHECK(m.calls.empty());   // irányválasztó ablak alatt nincs parancs
  m.key(HudModel::kKeyEscape);   // Esc: később
  CHECK(!m.hud.path_pending());
  m.bind(m.snap.get());
  m.hud.mode = {client::HudMode::Move, {}, sim::kNoDef, 0};
  m.key(HudModel::kKeyEscape);
  CHECK(m.hud.mode.kind == client::HudMode::None);   // Esc: célzás megszakítása
  // célzás után a világba kattintva: a mozgásparancs a szimulációba megy
  m.hud.sel = {w[0]};
  m.key('W');
  m.calls.clear();
  CHECK(m.hud.world_click(10 * 32.0f, 10 * 32.0f, 1, 0));
  CHECK(m.calls.size() == 1 && m.calls[0].kind == sim::Command::Kind::GroupMove && m.calls[0].units == std::vector<sim::EntityId>{w[0]});
  CHECK(m.hud.mode.kind == client::HudMode::None);
}

TEST_CASE("HUD kijelölés és karakterlap: összevonás, stabil sorrend, hős, fajcímer, épület (test_hud.py)") {
  HudMatch m("elves");
  CHECK(m.ok);
  if (!m.ok) return;
  const sim::Rules& r = *m.s.rules();
  const client::SnapUnit* hq = m.hq();
  CHECK(hq);
  if (!hq) return;
  const sim::EntityId hq_id = hq->id;
  const float hx = hq->x / 32.0f, hy = hq->y / 32.0f;
  // a nép első három nem hős, nem munkás egysége (a névsor sorrendjében), 4, 5 és 6 darab
  std::vector<uint16_t> ids;
  const auto* f = r.faction("elves");
  const content::Value* roster = f && f->raw ? f->raw->find("Roster") : nullptr;
  const content::Value* units = roster ? roster->find("units") : nullptr;
  for (const auto& v : units ? units->items() : std::vector<content::Value>{}) {
    const uint16_t d = r.def(v.text());
    if (d != sim::kNoDef && !r.defs[d].hero && !r.defs[d].worker && ids.size() < 3) ids.push_back(d);
  }
  CHECK(ids.size() == 3);
  std::vector<sim::EntityId> army;
  for (size_t k = 0; k < ids.size(); ++k) {
    for (size_t j = 0; j < 4 + k; ++j) {
      army.push_back(m.s.sim().spawn_unit(ids[k], 1, core::Fixed::from_ratio(static_cast<int>((hx + 4 + 0.5f * static_cast<float>(j)) * 100), 100),
                                          core::Fixed::from_ratio(static_cast<int>((hy + 3 + 0.7f * static_cast<float>(k)) * 100), 100)));
    }
  }
  m.step();
  m.hud.sel = army;
  auto cells = m.hud.sel_cells();
  CHECK(cells.size() == 3 && cells[0].ents.size() == 4 && cells[1].ents.size() == 5 && cells[2].ents.size() == 6);
  CHECK(!cells.empty() && cells[0].kind == client::SelCell::Group);   // vegyes sereg: típusonként összevonva
  std::vector<uint16_t> order1;
  for (const auto& c : cells) order1.push_back(m.snap->find(c.ents[0])->def);
  auto* h0 = m.s.sim().world().get<sim::Health>(army[0]);
  h0->hp = std::max(1, h0->max / 5);   // sérülés
  m.step();
  std::vector<uint16_t> order2;
  for (const auto& c : m.hud.sel_cells()) order2.push_back(m.snap->find(c.ents[0])->def);
  CHECK(order1 == order2);   // stabil sorrend sérüléskor
  m.s.sim().deal_damage(0, 0, sim::kNoDef, army[1], 100000000, false, true);   // halál
  m.step(2);
  CHECK(m.snap->find(army[1]) == nullptr);
  cells = m.hud.sel_cells();
  CHECK(!cells.empty() && cells[0].ents.size() == 3);   // halál után a darabszám frissül
  std::vector<sim::EntityId> one_type;
  for (const auto e : army) {
    if (const auto* u = m.snap->find(e); u && u->def == ids[2]) one_type.push_back(e);
  }
  m.hud.sel = one_type;
  bool all_one = !one_type.empty();
  for (const auto& c : m.hud.sel_cells()) all_one = all_one && c.kind == client::SelCell::One;
  CHECK(all_one);   // egyféle típus: egyenként (felbontva)
  CHECK(m.hud.selection_kind() == "multi");   // többes kijelölés
  // hős a kijelölésben: ő az elsődleges (karakterlap, tárgyak 2×3 kattinthatók)
  uint16_t hid = sim::kNoDef;
  for (const auto& v : units ? units->items() : std::vector<content::Value>{}) {
    const uint16_t d = r.def(v.text());
    if (d != sim::kNoDef && r.defs[d].hero && hid == sim::kNoDef) hid = d;
  }
  CHECK(hid != sim::kNoDef);
  if (hid != sim::kNoDef) {
    const sim::EntityId h = m.s.sim().spawn_unit(hid, 1, core::Fixed::from_ratio(static_cast<int>((hx + 3) * 100), 100),
                                                 core::Fixed::from_ratio(static_cast<int>((hy + 3) * 100), 100));
    m.step();
    m.hud.sel = {h, army[0], army[2], army[3]};
    CHECK(m.hud.primary() && m.hud.primary()->id == h);
    CHECK(m.hud.selection_kind() == "multi");
    int items = 0;
    for (const auto& rg : m.hud.middle_regions()) items += rg.kind == "item";
    CHECK(items == 0);
    CHECK(m.s.sim().give_item(h, 0));
    m.step();
    std::vector<client::HudRegion> regs = m.hud.middle_regions();
    items = 0;
    for (const auto& rg : regs) items += rg.kind == "item";
    CHECK(items == 1);   // hősnél a 2×3-as rácsban a tárgyak kattinthatók
    m.calls.clear();
    m.execute = false;
    if (!regs.empty()) m.click(regs[0].r);   // kattintás: a tárgy használata (a hős, a hely)
    CHECK(m.calls.size() == 1 && m.calls[0].kind == sim::Command::Kind::UseItem && m.calls[0].units.size() == 1 &&
          m.calls[0].units[0] == h && m.calls[0].rx == 0);
    m.execute = true;
  }
  m.hud.sel = {army[2]};
  auto emb = m.hud.middle_regions();
  CHECK(emb.size() == 1 && emb[0].kind == "emblem" && emb[0].r == m.hud.layout().inv &&
        emb[0].data == m.s.display_name("faction.elves.name"));   // nem hősnél a 2×3 helyén a fajcímer
  m.hud.sel = {hq_id};
  cells = m.hud.sel_cells();
  CHECK(m.hud.selection_kind() == "building" && !cells.empty() && cells[0].ents[0] == hq_id);   // épület: karakterlap és rács
  // a kijelölés-rács kattintása: csak ezek (a kijelölés változik)
  m.hud.sel = army;
  m.hud.sel_changed = false;
  m.click(m.hud.layout().sel_cells[1]);
  CHECK(m.hud.sel_changed && m.hud.sel.size() == 5);
}

TEST_CASE("HUD karakterlap M9: Ooni harci kedv, veteran fokozat, type_bonus, epuletkepesseg ujratoltese, fohaszhelyek") {
  // Ooni: a harci egység harci kedve a pillanatképben, a karakterlapon a második sáv
  {
    HudMatch m("ooni");
    CHECK(m.ok);
    if (!m.ok) return;
    const sim::Rules& r = *m.s.rules();
    const client::SnapUnit* hq = m.hq();
    CHECK(hq);
    if (!hq) return;
    uint16_t fighter = sim::kNoDef;
    for (uint16_t d = 0; d < r.defs.size() && fighter == sim::kNoDef; ++d) {
      const sim::EntityDef& ed = r.defs[d];
      if (ed.faction == "ooni" && !ed.building && !ed.worker && !ed.hero && ed.attack.has && !ed.has_tag("summoned")) fighter = d;
    }
    CHECK(fighter != sim::kNoDef);
    if (fighter == sim::kNoDef) return;
    const sim::EntityId e = m.s.sim().spawn_unit(fighter, 1, core::Fixed::from_int(static_cast<int>(hq->x / 32) + 4),
                                                 core::Fixed::from_int(static_cast<int>(hq->y / 32) + 3));
    m.step();
    const client::SnapUnit* u = m.snap->find(e);
    CHECK(u && u->morale > 0 && u->morale_max > 0 && u->morale == u->morale_max && !u->broken);
    if (u) {
      bool has = false;
      for (const auto& b : m.hud.char_bars(*u)) {
        has = has || (b.tip_title == m.s.display_name("hud.morale") &&
                      b.label == std::to_string(u->morale) + "/" + std::to_string(u->morale_max) && b.frac > 0.99f);
      }
      CHECK(has);                            // harci kedv sáv: érték/max
      CHECK(m.hud.rank_name(*u).empty());    // újonc
    }
    // előléptetés: a sim::Veteran fokozata a pillanatképben és a típus sorában
    m.s.sim().world().add(e, sim::Veteran{0, 1});
    m.step();
    u = m.snap->find(e);
    CHECK(u && u->rank == 1 && m.hud.rank_name(*u) == m.s.display_name("hud.rank.veteran"));
    bool none = !m.own(true).empty();   // a munkásnak nincs harci kedve
    for (const auto id : m.own(true)) none = none && m.snap->find(id)->morale < 0;
    CHECK(none);
  }
  // type_bonus: a sablon saját szorzója az „erős ez ellen” sorban (a régi counters)
  {
    HudMatch m("croftian");
    CHECK(m.ok);
    if (!m.ok) return;
    const sim::Rules& r = *m.s.rules();
    uint16_t tb = sim::kNoDef;
    for (uint16_t d = 0; d < r.defs.size() && tb == sim::kNoDef; ++d) {
      if (r.defs[d].attack.has && !r.defs[d].attack.type_bonus.empty() && !r.defs[d].building) tb = d;
    }
    CHECK(tb != sim::kNoDef);
    if (tb != sim::kNoDef) {
      client::SnapUnit fake;
      fake.def = tb;
      fake.kind = client::SnapUnit::kUnit;
      const std::string key = m.hud.unit_type(fake);
      const auto cs = m.hud.counters(fake, key.empty() ? "kozelharcos" : key);
      const auto& tbf = r.defs[tb].attack.type_bonus.front();
      bool found = false;
      for (const auto& kv : cs.first) found = found || (kv.first == tbf.first && kv.second + 1e-4f >= static_cast<float>(tbf.second.raw()) / 65536.0f);
      CHECK(found);
    }
  }
  // épületképesség újratöltése: a főépület Caster.ready → a parancsrács helyének `cd`-je
  bool tested_power = false;
  for (const char* fac : {"ursan", "ooni", "croftian", "hukucha", "elves", "micelia"}) {
    if (tested_power) break;
    HudMatch m(fac);
    if (!m.ok) continue;
    const client::SnapUnit* hq = m.hq();
    if (!hq || m.s.rules()->defs[hq->def].powers.empty()) continue;
    const sim::EntityId hid = hq->id;
    const uint16_t pw = m.s.rules()->defs[hq->def].powers.front();
    const sim::AbilityDef& a = m.s.rules()->abilities[pw];
    auto* c = m.s.sim().world().get<sim::Caster>(hid);
    CHECK(c);
    if (!c) break;
    const int32_t cd = a.cooldown > 0 ? a.cooldown : 1800;
    c->ready = {{pw, m.s.sim().tick() + 1 + cd / 2}};
    m.step();
    m.hud.sel = {hid};
    m.hud.cmd_page.clear();
    float got = -1;
    for (const auto& sl : m.hud.command_card()) {
      if (sl && sl->id == "bp:" + a.id) got = sl->cd;
    }
    CHECK(got > 0.4f && got < 0.6f);   // félig töltött
    CHECK(m.snap->find(hid) && m.hud.power_cd(*m.snap->find(hid), pw) == got);
    tested_power = true;
  }
  CHECK(tested_power);
  // fohászhelyek (elves): a szentély foglalt / összes helye a parancsrácsban (a régi pray_info)
  {
    HudMatch m("elves");
    CHECK(m.ok);
    if (!m.ok) return;
    const sim::Rules& r = *m.s.rules();
    uint16_t shrine = sim::kNoDef;
    for (uint16_t d = 0; d < r.defs.size() && shrine == sim::kNoDef; ++d) {
      const content::Value* me = r.defs[d].raw ? r.defs[d].raw->find("Mechanics") : nullptr;
      if (r.defs[d].faction == "elves" && me && me->is_object() && me->find("pray_slots")) shrine = d;
    }
    CHECK(shrine != sim::kNoDef);
    const client::SnapUnit* hq = m.hq();
    if (shrine == sim::kNoDef || !hq) return;
    const sim::EntityId b = m.s.sim().place_building(shrine, 1, hq->tx + hq->w + 3, hq->ty, true);
    CHECK(b != sim::kNoEntity);
    m.step(2);
    const client::SnapUnit* bu = m.snap->find(b);
    CHECK(bu && bu->prayers >= 0);
    if (!bu) return;
    const auto ps = m.hud.pray_slots(*bu);
    CHECK(ps.second == 4 && ps.first == bu->prayers);
    // egy tétlen munkás a szentély mellett: fohászkodik (a népmodul csak olvasó lekérdezése)
    const int btx = bu->tx, bty = bu->ty, bw = bu->w;
    const uint16_t wdef = m.snap->find(m.own(true).front())->def;
    m.s.sim().spawn_unit(wdef, 1, core::Fixed::from_int(btx + bw) + core::Fixed::from_ratio(1, 2),
                         core::Fixed::from_int(bty) + core::Fixed::from_ratio(1, 2));
    m.step(2);
    bu = m.snap->find(b);
    CHECK(bu && bu->prayers >= 1);
    m.hud.sel = {b};
    m.hud.cmd_page.clear();
    bool slot = false;
    for (const auto& sl : m.hud.command_card()) {
      if (sl && sl->id == "pray_info") slot = sl->passive && bu && sl->label.find(std::to_string(bu->prayers) + "/4") != std::string::npos;
    }
    CHECK(slot);
  }
}

TEST_CASE("HUD minimap M9: terepszinek, csapatszinu jelek, a latott resz kerete") {
  HudMatch m("ursan");
  CHECK(m.ok);
  if (!m.ok) return;
  const int mw = m.s.map().width(), mh = m.s.map().height();
  const auto cols = HudModel::minimap_terrain(m.s.map(), m.s.terrain());
  CHECK(cols.size() == static_cast<size_t>(mw) * static_cast<size_t>(mh));
  // a régi MINI_COLS: fű (68, 110, 52), víz (38, 74, 120) – a terep árnyalatával szorozva
  bool ok_cols = true;
  const auto& tt = m.s.terrain().terrains();
  for (int y = 0; y < mh; ++y) {
    for (int x = 0; x < mw; ++x) {
      const auto& t = tt[m.s.map().terrain(x, y)];
      std::array<uint8_t, 3> want{};
      if (t.art.row == "grass") want = {68, 110, 52};
      else if (t.art.row == "water") want = {38, 74, 120};
      else continue;
      for (size_t k = 0; k < 3; ++k) want[k] = static_cast<uint8_t>(want[k] * t.art.tint[k] / 255);
      ok_cols = ok_cols && cols[static_cast<size_t>(y) * static_cast<size_t>(mw) + static_cast<size_t>(x)] == want;
    }
  }
  CHECK(ok_cols);
  const auto marks = m.hud.minimap_marks();
  const client::Rect& mm = m.hud.layout().minimap;
  int b1 = 0, units = 0, res = 0, last = 0;
  bool inside = true, order = true, big = true;
  for (const auto& k : marks) {
    inside = inside && k.x >= -1 && k.y >= -1 && k.x + k.w <= static_cast<float>(mm.w) + 3 && k.y + k.h <= static_cast<float>(mm.h) + 3;
    order = order && static_cast<int>(k.kind) >= last;   // lelőhely → épület → egység
    last = static_cast<int>(k.kind);
    if (k.kind == client::MiniMark::Building) {
      b1 += k.col == m.hud.player_color(1) ? 1 : 0;
      big = big && k.w >= 3 && k.h >= 3;
    }
    units += k.kind == client::MiniMark::Unit ? 1 : 0;
    res += k.kind == client::MiniMark::Resource ? 1 : 0;
  }
  CHECK(inside && order && big);
  CHECK(b1 >= 1 && units >= 1 && res >= 1);   // a saját épület csapatszínnel
  CHECK(m.hud.player_color(1) != m.hud.player_color(2));
  // a látott rész kerete: a minimapre vágva
  const float W = static_cast<float>(mw * 32), H = static_cast<float>(mh * 32);
  const client::Rect v = m.hud.minimap_view(W / 4, H / 4, W / 2, H / 2);
  CHECK(std::abs(v.x - (mm.x + mm.w / 4)) <= 1 && std::abs(v.w - mm.w / 2) <= 1);
  const client::Rect c = m.hud.minimap_view(-W, -H, W * 1.5f, H * 1.5f);
  CHECK(c.x == mm.x && c.y == mm.y && c.right() <= mm.right() && c.bottom() <= mm.bottom());
  CHECK(m.hud.minimap_view(W * 2, H * 2, 100, 100).w == 0);   // kívül: nincs keret
}

TEST_CASE("HUD betu M9: a kis jelek (pont, kettospont) nem halvanyak a sutesben") {
  std::ifstream in(std::filesystem::path(TOTU_ASSETS_DIR) / "fonts" / "egszakadas_rovaskonyv_text.ttf", std::ios::binary);
  const std::vector<uint8_t> ttf((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  for (const float px : {15.0f, 18.0f, 21.0f, 25.0f}) {   // a HUD méretei (em → képpont)
    render::FontBake f;
    CHECK(render::bake_font(ttf, px, f));
    auto peak = [&](uint32_t cp) {
      const render::Glyph* g = f.find(cp);
      if (!g) return 0;
      int mx = 0;
      const int w = f.atlas.width, h = f.atlas.height;
      const int x0 = static_cast<int>(g->u0 * static_cast<float>(w) + 0.5f), x1 = static_cast<int>(g->u1 * static_cast<float>(w) + 0.5f);
      const int y0 = static_cast<int>(g->v0 * static_cast<float>(h) + 0.5f), y1 = static_cast<int>(g->v1 * static_cast<float>(h) + 0.5f);
      for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
          mx = std::max<int>(mx, f.atlas.rgba[(static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 4 + 3]);
        }
      }
      return mx;
    };
    CHECK(peak('.') >= 200);   // korábban ~55/255: elmosódott
    CHECK(peak(':') >= 200);
    CHECK(peak('H') == 255);   // a teljes fedettségű betű nem változik
  }
}

TEST_CASE("AS-03 HUD: a sikertelen kiadas oka szovegesen (hianyzo nyersanyag, betelt letszam)") {
  HudMatch m("ursan");
  CHECK(m.ok);
  if (!m.ok) return;
  const client::SnapUnit* hq = m.hq();
  CHECK(hq != nullptr);
  if (!hq) return;
  m.custom = *m.snap;
  m.custom.players[1].res = {0, 0, 0, 0};
  m.bind(&m.custom);
  m.hud.sel = {hq->id};
  m.hud.t = 5.0;
  m.calls.clear();
  m.execute = false;
  m.key('W');
  CHECK(!m.hud.notice.empty() && m.hud.notice_t == 5.0);
  CHECK(m.hud.notice.find(m.hud.res_name(1)) != std::string::npos ||
        m.hud.notice.find(m.hud.res_name(0)) != std::string::npos);   // a hiányzó nyersanyag neve benne van
  std::printf("  üzenet: %s\n", m.hud.notice.c_str());
  // van nyersanyag, de a létszám betelt
  m.custom.players[1].res = {9999, 9999, 9999, 9999};
  m.custom.players[1].supply_used = m.custom.players[1].supply_cap;
  m.bind(&m.custom);
  m.hud.sel = {hq->id};
  m.hud.notice.clear();
  m.hud.t = 7.0;
  m.key('W');
  CHECK(m.hud.notice.find("Tany") != std::string::npos);   // „Elérted a létszámkorlátot – növeld: Tanyák”
  // minden rendben: nincs üzenet
  m.custom.players[1].supply_used = 0;
  m.bind(&m.custom);
  m.hud.sel = {hq->id};
  m.hud.notice.clear();
  m.key('W');
  CHECK(m.hud.notice.empty());
}

TEST_CASE("HUD karakterlap: a munkas tamadasa is latszik (a regi jatek szerint)") {
  HudMatch m("ursan");
  CHECK(m.ok);
  if (!m.ok) return;
  const auto w = m.own(true);
  CHECK(!w.empty());
  if (w.empty()) return;
  const client::SnapUnit* u = m.snap->find(w[0]);
  CHECK(u && u->damage > 0);
  std::printf("  munkás: sebzés %d, páncél %d\n", u ? u->damage : -1, u ? u->armor : -1);
}

TEST_CASE("HUD jatekebesseg: a regi lepcsok (50 ... 300 %), a szeleken megall") {
  CHECK(HudModel::next_speed(100, 1) == 125 && HudModel::next_speed(100, -1) == 75);
  CHECK(HudModel::next_speed(50, -1) == 50 && HudModel::next_speed(300, 1) == 300);
  CHECK(HudModel::next_speed(200, 1) == 300 && HudModel::next_speed(150, 1) == 200);
  CHECK(HudModel::next_speed(110, 1) == 125);   // a legközelebbi lépcsőről lép
}

TEST_CASE("HUD a regi jatek szerint: F2 hos (ujra: odaugras), Szokoz bazis, Tamadas er riasztas") {
  HudMatch m("ursan");
  CHECK(m.ok);
  if (!m.ok) return;
  const uint16_t hd = m.s.rules()->def("urs_hos_ragna");
  CHECK(hd != sim::kNoDef);
  if (hd == sim::kNoDef) return;
  const client::SnapUnit* hq = m.hq();
  CHECK(hq != nullptr);
  if (!hq) return;
  const float hqx = hq->x, hqy = hq->y;
  m.s.sim().spawn_unit(hd, 1, core::Fixed::from_int(20), core::Fixed::from_int(20));
  m.step(2);
  CHECK(m.hud.my_heroes().size() == 1);
  m.hud.sel.clear();
  m.hud.jump = false;
  m.key(HudModel::kKeyF2);
  CHECK(m.hud.sel.size() == 1 && m.hud.sel[0] == m.hud.my_heroes()[0]->id && !m.hud.jump);
  m.key(HudModel::kKeyF2);
  CHECK(m.hud.jump);   // már kijelölve: odaugrás
  // Szóköz: nincs riasztás → a bázis (főépület)
  m.hud.jump = false;
  m.key(HudModel::kKeySpace);
  CHECK(m.hud.jump && m.hud.jump_x == hqx && m.hud.jump_y == hqy);
  // riasztás: egy saját egység életereje csökken
  m.hud.watch_alerts();
  m.custom = *m.snap;
  m.custom.tick += 1;
  for (auto& u : m.custom.units) {
    if (u.owner == 1 && u.kind == client::SnapUnit::kUnit) {
      u.hp -= 5;
      break;
    }
  }
  m.bind(&m.custom);
  m.hud.watch_alerts();
  CHECK(!m.hud.notice_log.empty() && m.hud.notice_log.back().text == m.hud.tr("alert.attack"));
  m.hud.jump = false;
  m.key(HudModel::kKeySpace);
  CHECK(m.hud.jump && (m.hud.jump_x != hqx || m.hud.jump_y != hqy));   // az utolsó riasztás helye
}

TEST_CASE("HUD a regi jatek szerint: Nem lehet epiteni (piros kiserteskep, ok), Elpusztult epulet") {
  HudMatch m("ursan");
  CHECK(m.ok);
  if (!m.ok) return;
  const client::SnapUnit* hq = m.hq();
  const auto w = m.own(true);
  const uint16_t hd = m.s.rules()->def("urs_hosszuhaz");
  CHECK(hq != nullptr && !w.empty() && hd != sim::kNoDef);
  if (!hq || w.empty() || hd == sim::kNoDef) return;
  const sim::EntityId hq_id = hq->id;
  // a szimuláció próbája: a főépületre nem építhető (foglalt), a pillanatkép válaszol
  int tx = 0, ty = 0;
  m.hud.place_tile(hq->x, hq->y, hd, &tx, &ty);
  m.s.set_place_probe(hd, tx, ty);
  m.step(1);
  std::string why;
  CHECK(m.hud.place_blocked(hd, tx, ty, &why) && why == m.hud.tr("place.busy"));
  // kattintás a tiltott helyre: üzenet, parancs nem megy ki, a mód marad
  m.hud.sel = {w[0]};
  m.hud.mode.kind = client::HudMode::Place;
  m.hud.mode.def = hd;
  m.calls.clear();
  m.execute = false;
  m.hud.world_click(m.snap->find(hq_id)->x, m.snap->find(hq_id)->y, 1, 0);
  CHECK(m.calls.empty() && m.hud.mode.kind == client::HudMode::Place);
  CHECK(!m.hud.notice_log.empty() && m.hud.notice_log.back().text == m.hud.tr("err.place", {m.hud.tr("place.busy")}));
  std::printf("  üzenet: %s\n", m.hud.notice_log.back().text.c_str());
  // a próba nélküli hely nem tiltott
  CHECK(!m.hud.place_blocked(hd, tx + 40, ty, nullptr));
  // a sérült saját épület eltűnik: „Elpusztult: …”; a lebontott (ép) csendben
  m.hud.mode = {};
  m.custom = *m.snap;
  for (auto& u : m.custom.units) {
    if (u.id == hq_id) u.hp = u.max_hp / 2;
  }
  m.bind(&m.custom);
  m.hud.watch_alerts();
  m.custom.tick += 1;
  std::erase_if(m.custom.units, [&](const client::SnapUnit& u) { return u.id == hq_id; });
  m.bind(&m.custom);
  m.hud.notice_log.clear();
  m.hud.watch_alerts();
  CHECK(m.hud.notice_log.size() == 1 && m.hud.notice_log.back().text.rfind("Elpusztult: ", 0) == 0);
}
