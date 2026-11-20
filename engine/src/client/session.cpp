// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/session.hpp"
#include "client/diagnostics.hpp"
#include "client/savegame.hpp"

#include "sim/mission.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>

#include "content/loader.hpp"
#include "script/ai.hpp"
#include "script/victory.hpp"

namespace totu::client {

using core::Fixed;

namespace {

constexpr float kTilePx = 32.0f;
float to_px(Fixed v) { return static_cast<float>(v.raw()) / 65536.0f * kTilePx; }

const auto kEpoch = std::chrono::steady_clock::now();

}  // namespace

const SnapUnit* Snapshot::find(sim::EntityId id) const {
  const auto it = std::lower_bound(units.begin(), units.end(), id, [](const SnapUnit& u, sim::EntityId v) { return u.id < v; });
  return it != units.end() && it->id == id ? &*it : nullptr;
}

bool Session::load(const std::string& content_dir, const std::string& map_path, uint64_t seed, std::string* error) {
  if (!load_content(content_dir, error)) return false;
  seed_ = seed;
  std::vector<std::string> errors;
  std::ifstream in(map_path, std::ios::binary);
  const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  map_text_ = text;
  desc_.reset();
  const auto doc = content::parse(text);
  if (doc) own_map_ = map::Map::from_json(*doc, terrain_, errors);
  if (!own_map_) {
    if (error) *error = "a pálya nem tölthető be: " + map_path + (errors.empty() ? "" : " – " + errors.front());
    return false;
  }
  map_ = &*own_map_;
  nav_ = std::make_unique<map::NavGrid>(*map_, terrain_, nav_config_);
  pf_ = std::make_unique<map::Pathfinder>(*nav_);
  own_sim_ = std::make_unique<sim::Sim>(seed);
  sim_ = own_sim_.get();
  sim_->attach_navigation(pf_.get());
  return true;
}

void Session::adopt(sim::MatchInstance& match) {
  ais_.clear();
  ai_.reset();
  victory_.reset();
  victory_view_ = nullptr;
  view_.reset();
  desc_.reset();   // a hívó adja át (set_match_desc), ha menthető
  for (const auto& h : match.hosts) {
    if (h->name() == "victory") victory_view_ = h.get();
  }
  own_sim_.reset();
  pf_.reset();
  nav_.reset();
  own_map_.reset();
  last_.reset();
  fog_.reset();
  remembered_.clear();
  map_ = match.map.get();
  sim_ = match.sim.get();
  mission_ = match.mission.get();
  match_rules_ = match.rules.get();
}

bool Session::load_content(const std::string& content_dir, std::string* error) {
  // az előző meccs állapota nem marad meg (H-61: teljes felfedés; H-62: gépek; H-63: szabályok)
  ais_.clear();
  ai_.reset();
  victory_.reset();
  victory_view_ = nullptr;
  own_sim_.reset();
  sim_ = nullptr;
  pf_.reset();
  nav_.reset();
  own_map_.reset();
  map_ = nullptr;
  mission_ = nullptr;
  match_rules_ = nullptr;
  rules_.reset();
  reveal_all_ = false;
  last_.reset();
  fog_.reset();
  remembered_.clear();
  content_dir_ = content_dir;
  content::Vfs vfs;
  content::add_mod_layers(vfs, content_dir, mods_);   // M13: az alap és az engedélyezett modok
  for (const auto& layer : vfs.layers()) {   // a nevek (hu.po): msgctxt "kulcs" után az msgid "szöveg"; a mod felülír
    std::ifstream po(layer.second / "locale" / "hu.po", std::ios::binary);
    if (!po) continue;
    std::string line, ctx;
    auto quoted = [](const std::string& l) {
      const size_t a = l.find('"'), b = l.rfind('"');
      return a != std::string::npos && b > a ? l.substr(a + 1, b - a - 1) : std::string();
    };
    while (std::getline(po, line)) {
      if (line.rfind("msgctxt ", 0) == 0) ctx = quoted(line);
      else if (line.rfind("msgid ", 0) == 0 && !ctx.empty()) {
        names_[ctx] = quoted(line);
        ctx.clear();
      }
    }
  }
  content::ContentSet set;
  const auto report = content::ContentLoader(vfs, std::filesystem::path(content_dir) / "schema").load(set);
  std::vector<std::string> errors = report.errors;   // H-07/H-68: a tartalomhiba nem marad csendben
  if (!errors.empty()) {
    if (error) *error = "tartalomhiba: " + errors.front();
    return false;
  }
  if (!terrain_.load(set, errors)) {
    if (error) *error = errors.empty() ? "terepadat" : errors.front();
    return false;
  }
  {   // játékszabályok (M6): a game.json és a sablonok
    const std::string gtext = vfs.read_text("game.json").value_or(std::string());   // a mod felülírhatja
    const auto game = content::parse(gtext);
    std::vector<std::string> rerr;
    if (game) rules_ = sim::Rules::load(*game, set, rerr);
    if (!game || !rules_ || !rerr.empty()) {   // H-07: a hibás szabályadat hiba, nem csendes folytatás
      rules_.reset();
      if (error) *error = "a játékszabályok hibásak: " + (rerr.empty() ? std::string("game.json") : rerr.front());
      return false;
    }
    nav_config_ = map::NavConfig::from_json(game->find("navigation"), rerr);   // H-90: a game.json navigációs beállítása
  }
  return true;
}

void Session::spawn_demo(int players, int per_player) {
  const uint8_t land = terrain_.movement_class("land").value_or(0);
  const int w = map_->width(), h = map_->height();
  for (int p = 0; p < players; ++p) {
    const int side = 12;
    for (int i = 0; i < per_player; ++i) {
      sim::Command c;
      c.owner = static_cast<uint8_t>(p + 1);
      const int bx = p % 2 == 0 ? 4 : w - 4 - side, by = h / 2 - side / 2 + (p / 2) * (side + 2) - (players > 2 ? side : 0);
      c.x = Fixed::from_ratio(bx * 10 + (i % side) * 11, 10);
      c.y = Fixed::from_ratio(by * 10 + (i / side) * 11, 10);
      c.speed = Fixed::from_ratio(8 + i % 3, 100);
      c.hp = 120;
      c.damage = 6;
      c.spread = 4;
      c.cooldown = 30 + i % 10;
      c.range = Fixed::from_ratio(i % 4 == 0 ? 450 : 130, 100);
      const sim::EntityId e = sim_->spawn(c);
      sim_->set_body(e, Fixed::from_ratio(38, 100), land);
    }
  }
  last_.reset();   // a következő pillanatkép már az új egységekkel
}

std::string Session::display_name(const std::string& key) const {
  const auto it = names_.find(key);
  if (it != names_.end() && !it->second.empty()) return it->second;
  Diagnostics::get().count_once("LOCALE_MISSING", key);   // a kulcs nem kerül az exportba
  return key;
}

bool Session::start_match(const std::vector<std::string>& factions, bool upkeep, std::string* error, std::vector<int> ai) {
  sim::MatchSetup setup;
  setup.factions = factions;
  setup.upkeep = upkeep;
  setup.camps = true;   // egyéni csata: a Vadon táboraival (D25)
  return start_match(setup, ai, "ai_ursan.lua", error);
}

bool Session::start_match(const sim::MatchSetup& setup, const std::vector<int>& ai, const std::string& ai_script,
                          std::string* error) {
  if (!rules_) {
    if (error) *error = "a játékszabályok nem tölthetők be";
    return false;
  }
  sim_->attach_rules(&*rules_);
  if (!sim_->setup_match(*map_, setup, error)) return false;
  if (setup.fog == sim::MatchSetup::Fog::None) reveal_all_ = true;
  victory_.reset();
  victory_view_ = nullptr;
  // a mentéshez: a meccs leírása (a betöltés ebből építi fel ugyanígy – start_match, ugyanaz a csatlakozó-sorrend)
  sim::Replay desc;
  desc.seed = seed_;
  desc.map = map_text_;
  desc.factions = setup.factions;
  desc.weather = static_cast<uint8_t>(setup.weather);
  desc.ai = setup.ai;
  desc.upkeep = setup.upkeep;
  desc.camps = setup.camps;
  desc.fog = static_cast<uint8_t>(setup.fog);
  desc.daytime = static_cast<uint8_t>(setup.daytime);
  desc.victory = static_cast<uint8_t>(setup.victory);
  desc.tempo = static_cast<uint8_t>(setup.tempo);
  desc.minutes = setup.minutes;
  desc.teams = setup.teams;
  desc.starts = setup.starts;
  desc.resources = setup.resources;
  desc.climate = setup.climate;
  if (setup.victory == sim::MatchSetup::Victory::Script) {   // M13: a modbeli győzelmi szkript (a gépek előtt csatolva)
    content::Vfs vfs;
    content::add_mod_layers(vfs, content_dir_, mods_);
    auto pkg = script::load_victory(vfs, setup.victory_script, error);
    if (!pkg) return false;
    victory_ = script::attach_victory(*sim_, *pkg, seed_, true, error);
    if (!victory_ || victory_->errors() > 0) return false;
    script::add_to_replay(desc, *pkg);
    victory_view_ = victory_.get();
  }
  if (!setup.ai.empty()) {   // M9: az új gépi ellenfél – gépi játékosonként saját Lua-állapot (AI-csomag a VFS-ből)
    content::Vfs vfs;
    content::add_mod_layers(vfs, content_dir_, mods_);
    std::vector<script::AiPackage> pkgs;
    for (const auto& a : setup.ai) {
      if (std::any_of(pkgs.begin(), pkgs.end(), [&](const script::AiPackage& p) { return p.id == a.ai; })) continue;
      auto pkg = script::load_ai_package(vfs, a.ai, error);
      if (!pkg) return false;
      pkgs.push_back(std::move(*pkg));
    }
    ais_ = script::attach_ais(*sim_, setup.ai, pkgs, seed_, true, error);
    if (ais_.empty()) return false;
    for (const auto& p : pkgs) script::add_to_replay(desc, p);
  } else if (!ai.empty()) {   // a szkriptelt ellenfél (Lua; determinisztikus, a hash és a mentés része)
    std::ifstream in(std::filesystem::path(content_dir_) / "base" / "scripts" / ai_script, std::ios::binary);
    const std::string src{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    std::string players = "AI_PLAYERS = {";
    for (size_t i = 0; i < ai.size(); ++i) players += (i ? ", " : "") + std::to_string(ai[i]);
    players += "}";
    ai_ = std::make_unique<script::ScriptHost>("ai", seed_);
    if (src.empty() || !ai_->run("ai_players", players) || !ai_->run(ai_script, src)) {
      if (error) *error = "a gép szkriptje nem tölthető be";
      ai_.reset();
      return false;   // H-05: hibás gépi szkript – sikertelen indítás
    }
    sim_->add_hooks(ai_.get());
    ai_->start(*sim_);
    if (ai_->errors() > 0) {   // H-76: az on_start hibája is sikertelen indítás
      if (error) *error = "a gép szkriptje hibát jelzett az indításkor";
      return false;
    }
  }
  last_.reset();
  fog_.reset();
  remembered_.clear();
  if (ai_) desc_.reset();   // a régi, egyfájlos gépi szkript: nem menthető
  else desc_ = std::move(desc);
  return true;
}

bool Session::save_game(const std::string& path, const std::string& meta, std::string* error) {
  if (!desc_ || !sim_) {
    if (error) *error = "ez a meccs nem menthető";
    return false;
  }
  SaveFile f;
  f.meta = meta;
  f.desc = *desc_;
  f.desc.commands.clear();
  f.desc.checkpoints.clear();
  f.desc.ticks = 0;
  f.desc.checkpoint_every = 1;   // a dekóder: legfeljebb a hossz + 1
  f.state = sim_->save();
  return write_save(path, f, error);
}

void Session::battle_demo(int per_side) {
  if (!rules_ || !sim_->rules()) return;
  const char* kinds[] = {"urs_pajzshordozo", "urs_berserker", "urs_szigonyvezo", "urs_dardaor", "urs_kardfogu",
                         "urs_mammut", "urs_szigonyvezo", "urs_berserker"};
  const int cx = map_->width() / 2, cy = map_->height() / 2;
  for (int side = 0; side < 2; ++side) {
    std::vector<sim::EntityId> army;
    for (int i = 0; i < per_side; ++i) {
      const int col = i / 6, row = i % 6;
      const int x = side == 0 ? cx - 7 - col : cx + 7 + col;
      const sim::EntityId e = sim_->spawn_unit(rules_->def(kinds[i % 8]), static_cast<uint8_t>(side + 1),
                                               Fixed::from_int(x) + Fixed::from_ratio(1, 2),
                                               Fixed::from_int(cy - 3 + row) + Fixed::from_ratio(1, 2));
      if (e != sim::kNoEntity) army.push_back(e);
    }
    for (const sim::EntityId e : army) {
      sim_->order_attack_move(e, Fixed::from_int(side == 0 ? cx + 10 : cx - 10), Fixed::from_int(cy));
    }
  }
  last_.reset();
}

void Session::order_demo_attack() {
  std::map<uint8_t, std::vector<sim::EntityId>> armies;
  sim_->world().pool<sim::Health>().each([&](sim::EntityId e, const sim::Health& hh) { armies[hh.owner].push_back(e); });
  const Fixed cx = Fixed::from_ratio(map_->width(), 2), cy = Fixed::from_ratio(map_->height(), 2);
  for (auto& [owner, ids] : armies) {
    sim_->order_group_move(ids, owner % 2 ? cx + Fixed::from_int(6) : cx - Fixed::from_int(6), cy,
                           owner % 2 ? sim::Formation::Auto : sim::Formation::Line);
  }
}

void Session::update_fog() {
  const int w = map_->width(), h = map_->height();
  auto fog = fog_ ? std::make_shared<std::vector<uint8_t>>(*fog_)
                  : std::make_shared<std::vector<uint8_t>>(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
  for (auto& f : *fog) {
    if (f == 255) f = 128;   // a korábban látott rész felderített marad
  }
  constexpr int kSight = 8;
  sim_->world().pool<sim::Health>().each([&](sim::EntityId e, const sim::Health& hh) {
    if (hh.owner != local_ || hh.hp <= 0) return;
    const auto* p = sim_->world().get<sim::Position>(e);
    if (!p) return;
    const int cx = static_cast<int>(p->x.raw() >> 16), cy = static_cast<int>(p->y.raw() >> 16);
    for (int y = std::max(0, cy - kSight); y <= std::min(h - 1, cy + kSight); ++y) {
      for (int x = std::max(0, cx - kSight); x <= std::min(w - 1, cx + kSight); ++x) {
        if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= kSight * kSight) (*fog)[static_cast<size_t>(y * w + x)] = 255;
      }
    }
  });
  fog_ = std::move(fog);
}

bool Session::tile_visible(int x, int y) const {
  if (reveal_all_ || !fog_ || fog_->empty()) return true;
  const int w = map_->width(), h = map_->height();
  x = std::clamp(x, 0, w - 1);
  y = std::clamp(y, 0, h - 1);
  return (*fog_)[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] == 255;
}

bool Session::rect_visible(int x, int y, int w, int h) const {
  for (int yy = y; yy < y + h; ++yy) {
    for (int xx = x; xx < x + w; ++xx) {
      if (tile_visible(xx, yy)) return true;
    }
  }
  return false;
}

void Session::hud_fields(sim::EntityId e, SnapUnit& u) {
  auto& w = sim_->world();
  const sim::Rules* r = sim_->rules();
  if (!r || u.def >= r->defs.size()) return;
  const sim::EntityDef& d = r->defs[u.def];
  const auto fl = [](Fixed f) { return static_cast<float>(f.raw()) / 65536.0f; };
  u.armor = sim_->stat(e, "Health.armor", Fixed::from_int(d.armor)).floor_int();
  if (const auto* a = w.get<sim::Attack>(e)) {   // a bemutató egységei (M1-es komponens)
    u.damage = sim_->stat(e, "Attack.damage", Fixed::from_int(a->damage)).floor_int();
    u.range = fl(a->range);
  } else if (d.attack.has) {   // a sablonból indított egységek és épületek: a sablon támadása a kutatásokkal
    u.damage = sim_->stat(e, "Attack.damage", d.attack.damage).floor_int();
    u.range = fl(d.attack.range);
  }
  u.shield_max = d.shield;
  if (const auto* v = w.get<sim::Vital>(e)) u.shield = v->shield / 1000;
  if (d.building) {
    if (const auto* c = w.get<sim::Caster>(e)) u.ready = c->ready;   // a főépület-képességek újratöltése
    if (u.owner >= 1 && u.complete) u.prayers = static_cast<int16_t>(std::clamp(sim_->prayers_at(e), -1, 999));
    return;
  }
  if (const auto* vt = w.get<sim::Veteran>(e)) u.rank = vt->rank;
  int32_t mv = 0, mm = 0;
  bool br = false;
  if (sim_->morale_of(e, &mv, &mm, &br)) {   // Ooni harci kedv (a népmodul csak olvasó lekérdezése)
    u.morale = mv;
    u.morale_max = mm;
    u.broken = br;
  }
  u.energy_max = sim_->stat(e, "Energy.max", Fixed::from_int(d.energy_max)).floor_int();
  if (const auto* c = w.get<sim::Caster>(e)) {
    u.energy = c->energy / 1000;
    u.ready = c->ready;
    u.buffs = c->buffs;
    u.toggles = c->toggles;
    if (c->cast != sim::kNoAbility) u.order = SnapUnit::kCast;
  }
  if (const auto* m = w.get<sim::Movement>(e)) u.speed = fl(m->speed) * 30.0f;
  if (const auto* hs = w.get<sim::HeroState>(e)) u.xp = hs->xp / 1000;
  if (const auto* l = w.get<sim::Lifetime>(e)) u.life_until = l->until;
  if (const auto* wg = w.get<sim::Wagon>(e)) u.packed = wg->building;
  if (const auto* af = w.get<sim::Afflict>(e)) {
    u.afflict = {af->burn_until, af->poison_until, af->frozen_until, af->slow_until, af->wet_until, af->root_until,
                 af->sleep_until, af->stun_until, af->fear_until, af->blind_until, af->silence_until, af->mark_until,
                 af->sunder_until};
  }
  {   // a figura animációja: támadott-e most (a töltési idő újraindult), varázsolt-e (egy képesség újratöltése nőtt)
    AnimTrack& tr = anim_[e];
    const int64_t now = sim_->tick();
    if (const auto* cb = w.get<sim::Combat>(e)) {
      if (tr.seen > 0 && cb->cooldown > tr.cooldown) tr.attack_tick = now;
      tr.cooldown = cb->cooldown;
      if (cb->target != 0 && w.alive(cb->target)) {
        if (const auto* tp = w.get<sim::Position>(cb->target)) {
          u.has_face = true;
          u.face_x = to_px(tp->x);
          u.face_y = to_px(tp->y);
        }
      }
    }
    if (const auto* c = w.get<sim::Caster>(e)) {
      int64_t mx = 0;
      for (const auto& [ab, t] : c->ready) {
        mx = std::max(mx, t);
        // melyik képesség újratöltése nőtt: az volt a varázslat
        const auto old = std::find_if(tr.ready.begin(), tr.ready.end(), [&](const auto& x) { return x.first == ab; });
        if (tr.seen > 0 && (old == tr.ready.end() ? t > now : t > old->second)) tr.cast_ability = ab;
      }
      if (tr.seen > 0 && mx > tr.ready_max) tr.cast_tick = now;
      tr.ready_max = mx;
      tr.ready = c->ready;
    }
    tr.seen = now + 1;
    u.attack_tick = tr.attack_tick;
    u.cast_tick = tr.cast_tick;
    u.cast_ability = tr.cast_ability;
  }
  if (u.order != SnapUnit::kRest) return;
  if (w.get<sim::RepairOrder>(e)) {
    u.order = SnapUnit::kRepair;
  } else if (const auto* k = w.get<sim::Worker>(e); k && k->state != sim::Worker::Idle) {
    u.order = k->state == sim::Worker::Build ? SnapUnit::kGoBuild
              : k->state == sim::Worker::Construct ? SnapUnit::kConstruct : SnapUnit::kGather;
  } else if (const auto* cb = w.get<sim::Combat>(e); cb && cb->order != sim::Combat::None && cb->order != sim::Combat::Leash) {
    u.order = cb->order == sim::Combat::Attack ? SnapUnit::kAttack
              : cb->order == sim::Combat::AttackMove ? SnapUnit::kAttackMove : SnapUnit::kHold;
  } else if (const auto* m = w.get<sim::Movement>(e); m && m->moving) {
    u.order = SnapUnit::kMove;
  }
}

std::shared_ptr<const Snapshot> Session::snapshot() {
  if (last_ && last_->tick == sim_->tick() &&
      (!sim_->rules() || static_cast<uint16_t>(probe_.load() >> 32) == sim::kNoDef ||
       (last_->place.def == static_cast<uint16_t>(probe_.load() >> 32) &&
        last_->place.tx == static_cast<int16_t>((probe_.load() >> 16) & 0xffff) &&
        last_->place.ty == static_cast<int16_t>(probe_.load() & 0xffff)))) {
    return last_;
  }
  if (sim_->rules()) {   // meccs: a köd az 1. játékos látóteréből (a szimulációé)
    const sim::PlayerState& p1 = sim_->player(local_);
    if (!fog_ || sim_->tick() % 6 == 0) {
      auto f = std::make_shared<std::vector<uint8_t>>(p1.visible.size(), 0);
      for (size_t i = 0; i < f->size(); ++i) (*f)[i] = p1.visible[i] ? 255 : (p1.explored[i] ? 128 : 0);
      if (!fog_ || *fog_ != *f) fog_ = std::move(f);
    }
  } else if (!fog_) {
    update_fog();
  }
  auto s = std::make_shared<Snapshot>();
  s->tick = sim_->tick();
  if (s->tick % 300 == 0) {   // a figuraanimáció nyilvántartásából kihullanak a megszűnt entitások
    for (auto it = anim_.begin(); it != anim_.end();) it = sim_->world().alive(it->first) ? std::next(it) : anim_.erase(it);
  }
  const bool match = sim_->rules() != nullptr;
  std::vector<sim::EntityId> seen_now;
  auto& w = sim_->world();
  w.pool<sim::Position>().each([&](sim::EntityId e, const sim::Position& p) {
    const auto* hh = w.get<sim::Health>(e);
    SnapUnit u;
    u.id = e;
    u.x = to_px(p.x);
    if (match && hh && hh->owner == local_) {   // a régi talajjel (a rejtőzés csak meccsben van)
      if (const auto* st = w.get<sim::Stealth>(e); st && st->hidden) u.hidden = true;
    }
    u.y = to_px(p.y);
    if (hh) {
      u.hp = hh->hp;
      u.max_hp = hh->max;
      u.owner = hh->owner;
    }
    if (match) {
      if (const auto* sh = w.get<sim::Shot>(e)) {   // lövedék
        u.kind = SnapUnit::kShot;
        u.res = sh->kind;
        if (const auto* sp = w.get<sim::Spell>(e)) u.spell = sp->ability;   // varázslövedék (a régi `vfx_<elem>_bolt`)
        u.owner = sh->owner;
        u.hp = 1;
        u.sx = to_px(sh->sx);
        u.sy = to_px(sh->sy);
        const float tot = std::hypot(to_px(sh->ax) - u.sx, to_px(sh->ay) - u.sy);
        const float done = std::hypot(u.x - u.sx, u.y - u.sy);
        u.progress = static_cast<uint16_t>(tot > 0.5f ? std::clamp(done / tot, 0.0f, 1.0f) * 1000.0f : 1000.0f);
        if (sh->kind < sim_->rules()->projectiles.size()) {   // parabola: a félúton a legmagasabb
          const float arc = static_cast<float>(sim_->rules()->projectiles[sh->kind].arc.raw()) / 65536.0f;
          const float t = static_cast<float>(u.progress) / 1000.0f;
          u.lift = arc * tot * 4.0f * t * (1.0f - t);
        }
      } else if (const auto* b = w.get<sim::Building>(e)) {
        u.kind = SnapUnit::kBuilding;
        u.def = b->def;
        u.tx = b->tx; u.ty = b->ty; u.w = b->w; u.h = b->h;
        u.complete = b->complete != 0;
        const int64_t total = int64_t{std::max(1, sim_->rules()->defs[b->def].time_ticks)} * 1000;
        u.progress = static_cast<uint16_t>(std::min<int64_t>(1000, int64_t{b->progress} * 1000 / total));
        u.slots = static_cast<uint8_t>(std::clamp(sim_->rules()->defs[b->def].garrison, 0, 255));
        if (const auto* g = w.get<sim::Garrison>(e)) u.garrison = static_cast<uint8_t>(std::min<size_t>(g->units.size(), 255));
        if (const auto* pr = w.get<sim::Producer>(e)) {
          for (const auto& q : pr->queue) {
            if (u.queue_n >= 5) break;
            u.queue_def[u.queue_n] = q.def;
            if (q.research) u.queue_research = static_cast<uint8_t>(u.queue_research | (1u << u.queue_n));
            ++u.queue_n;
          }
          if (pr->has_rally) {
            u.has_rally = true;
            u.rally_x = to_px(pr->rx);
            u.rally_y = to_px(pr->ry);
          }
          if (!pr->queue.empty()) u.queue_progress = static_cast<uint16_t>(pr->queue[0].progress * 1000 / std::max(1, pr->queue[0].time));
        }
        hud_fields(e, u);
      } else if (const auto* n = w.get<sim::ResourceNode>(e)) {
        u.kind = SnapUnit::kResource;
        u.tx = n->tx; u.ty = n->ty; u.w = u.h = n->size;
        u.res = n->kind;
        u.amount = n->amount;
      } else if (const auto* un = w.get<sim::Unit>(e)) {
        if (w.get<sim::Sheltered>(e)) return;   // helyőrségben: nem látszik
        u.def = un->def;
        if (const auto* hs = w.get<sim::HeroState>(e)) {
          u.hero_level = hs->level;
          u.hero_points = hs->points;
          u.items = hs->items;
          u.learned = hs->learned;
        }
        if (const auto* k = w.get<sim::Worker>(e)) {
          if (k->inside) return;   // bent épít: nem látszik
          u.amount = k->carry;
          u.res = k->carry_kind;
        }
        hud_fields(e, u);
      }
    }
    // D41: ami nem az 1. játékosé, az a ködben nem látszik (egység, lövedék); az épület és a lelőhely a legutóbb
    // látott állapotában
    const auto tile_of = [](float px) { return static_cast<int>(std::floor(px / 32.0f)); };
    if (u.kind == SnapUnit::kUnit && u.owner != local_) {
      if (!reveal_all_ && (match ? !sim_->is_visible(local_, e) : !tile_visible(tile_of(u.x), tile_of(u.y)))) return;
    } else if (u.kind == SnapUnit::kShot) {
      if (!tile_visible(tile_of(u.x), tile_of(u.y))) return;
    } else if (u.kind == SnapUnit::kBuilding || u.kind == SnapUnit::kResource) {
      if (u.kind == SnapUnit::kResource || u.owner != local_) {
        if (rect_visible(u.tx, u.ty, u.w, u.h)) {
          remembered_[e] = u;
        } else {
          const auto it = remembered_.find(e);
          if (it == remembered_.end()) return;   // még sosem látta
          seen_now.push_back(e);
          s->units.push_back(it->second);
          return;
        }
      }
    }
    seen_now.push_back(e);
    s->units.push_back(u);
  });
  // a ködben elpusztult (vagy kimerült) épület és lelőhely a legutóbb látott állapotában marad, amíg újra oda nem lát
  std::sort(seen_now.begin(), seen_now.end());
  for (auto it = remembered_.begin(); it != remembered_.end();) {
    if (std::binary_search(seen_now.begin(), seen_now.end(), it->first)) {
      ++it;
      continue;
    }
    const SnapUnit& r = it->second;
    if (rect_visible(r.tx, r.ty, r.w, r.h)) {
      it = remembered_.erase(it);
      continue;
    }
    s->units.push_back(r);
    ++it;
  }
  if (match) {
    for (int i = 0; i <= sim::Sim::kMaxPlayers; ++i) {
      const sim::PlayerState& p = sim_->player(i);
      SnapPlayer& o = s->players[static_cast<size_t>(i)];
      o.active = p.active;
      o.faction = p.faction;
      for (int r = 0; r < sim::kResources; ++r) o.res[static_cast<size_t>(r)] = p.resource(r);
      o.supply_cap = p.supply_cap;
      o.supply_used = p.supply_used;
      o.defeated = p.defeated;
      o.team = p.team;
      o.ascended = p.ascended;
      o.moon = i >= 1 && sim_->moon_mult(static_cast<uint8_t>(i)) > sim::Fixed::from_int(1);
      o.ally = p.ally;
      for (const auto& fh : p.fallen) o.fallen.push_back(fh.first);
      o.path = p.path;
      if (i == 1) o.researched = p.researched;
      for (int r = 0; r < sim::kResources; ++r) o.gathered[static_cast<size_t>(r)] = static_cast<int32_t>(p.gathered[static_cast<size_t>(r)] / 1000);
      o.stats = p.stats;
      if (!p.defeated && sim_->winner() == 0 && !(mission_ && mission_->result() != sim::MissionHost::Result::None)) o.stats.timeline.clear();
    }
    {   // az építési hely próbája (a helyi játékos szemszögéből)
      const uint64_t pr = probe_.load();
      auto& pp = s->place;
      pp.def = static_cast<uint16_t>(pr >> 32);
      pp.tx = static_cast<int16_t>((pr >> 16) & 0xffff);
      pp.ty = static_cast<int16_t>(pr & 0xffff);
      if (pp.def != sim::kNoDef) pp.ok = sim_->can_place(local_, pp.def, pp.tx, pp.ty, &pp.why);
      if (pp.def != sim::kNoDef && map_ && pp.def < sim_->rules()->defs.size()) {
        // a régi `draw_placement_area`: a kurzor körüli 48 × 32 csempe közül azok, amelyeket egy lerakható helyzet
        // lefed; újraszámolás: más épület, a kurzor messzebb került, vagy fél másodperc telt el
        PlaceGrid& g = place_grid_;
        const int64_t now = sim_->tick();
        if (g.def != pp.def || std::abs(g.cx - pp.tx) > 6 || std::abs(g.cy - pp.ty) > 4 || now - g.tick >= 15 || !g.cells) {
          const auto& d = sim_->rules()->defs[pp.def];
          const int bw = std::max(1, static_cast<int>(d.fw)), bh = std::max(1, static_cast<int>(d.fh));
          g.def = pp.def;
          g.cx = pp.tx;
          g.cy = pp.ty;
          g.tick = now;
          g.gx = std::max(0, pp.tx - 24);
          g.gy = std::max(0, pp.ty - 16);
          g.gw = std::min(map_->width(), pp.tx + 24) - g.gx;
          g.gh = std::min(map_->height(), pp.ty + 16) - g.gy;
          auto cells = std::make_shared<std::vector<uint8_t>>(static_cast<size_t>(std::max(0, g.gw) * std::max(0, g.gh)), 0);
          for (int y = g.gy - bh + 1; y < g.gy + g.gh; ++y) {
            for (int x = g.gx - bw + 1; x < g.gx + g.gw; ++x) {
              if (!sim_->can_place(local_, pp.def, x, y)) continue;
              for (int yy = std::max(y, g.gy); yy < std::min(y + bh, g.gy + g.gh); ++yy) {
                for (int xx = std::max(x, g.gx); xx < std::min(x + bw, g.gx + g.gw); ++xx) {
                  (*cells)[static_cast<size_t>((yy - g.gy) * g.gw + (xx - g.gx))] = 1;
                }
              }
            }
          }
          g.cells = std::move(cells);
        }
        pp.gx = g.gx;
        pp.gy = g.gy;
        pp.gw = g.gw;
        pp.gh = g.gh;
        pp.grid = g.cells;
      }
    }
    {   // a tervezett épületek: a munkás oda tart (Build állapot), vagy a parancssorában vár (Build parancs)
      auto add_plan = [&](uint16_t def, int tx, int ty) {
        if (def == sim::kNoDef) return;
        for (const auto& p : s->planned) {
          if (p.def == def && p.tx == tx && p.ty == ty) return;
        }
        s->planned.push_back({def, tx, ty});
      };
      w.pool<sim::Worker>().each([&](sim::EntityId e, const sim::Worker& k) {
        const auto* hh = w.get<sim::Health>(e);
        if (!hh || hh->owner != local_) return;
        if (k.state == sim::Worker::Build) add_plan(k.build_def, k.btx, k.bty);
        const auto q = sim_->order_queue_.find(e);
        if (q == sim_->order_queue_.end()) return;
        for (const auto& c : q->second) {
          if (c.kind == sim::Command::Kind::Build) add_plan(sim_->rules()->def(c.def), c.rx, c.ry);
        }
      });
    }
    {   // holttestek (csak a látott helyen) és a földön heverő tárgyak (a felderített helyen)
      const sim::PlayerState& me = sim_->player(local_);
      auto seen = [&](float px, float py, bool need_visible) {
        if (reveal_all_ || !map_) return true;
        const int tx = static_cast<int>(px / 32.0f), ty = static_cast<int>(py / 32.0f);
        if (tx < 0 || ty < 0 || tx >= map_->width() || ty >= map_->height()) return false;
        const size_t k = static_cast<size_t>(ty) * static_cast<size_t>(map_->width()) + static_cast<size_t>(tx);
        const auto& v = need_visible ? me.visible : me.explored;
        return k < v.size() && v[k] != 0;
      };
      for (const auto& c : sim_->corpses_) {
        Snapshot::Corpse o;
        o.def = c.def;
        o.owner = c.owner;
        o.x = to_px(c.x);
        o.y = to_px(c.y);
        o.age = static_cast<float>(sim_->tick() - c.tick) / 30.0f;
        if (seen(o.x, o.y, true)) s->corpses.push_back(o);
      }
      w.pool<sim::GroundItem>().each([&](sim::EntityId e, const sim::GroundItem& g) {
        const auto* p = w.get<sim::Position>(e);
        if (!p || g.item >= sim_->rules()->items.size()) return;
        Snapshot::Item it;
        it.x = to_px(p->x);
        it.y = to_px(p->y);
        it.id = e;
        it.rarity = sim_->rules()->items[g.item].rarity;
        it.item = sim_->rules()->items[g.item].id;
        if (seen(it.x, it.y, false)) s->items.push_back(it);
      });
      if (!view_ || s->tick % 10 == 0) {   // a népmechanikák nézete (csak ha változott, új példány – a rajzoló így tudja)
        auto v = std::make_shared<sim::ModuleView>();
        sim_->module_view(*v);
        const bool same = view_ && view_->creep == v->creep && view_->pond == v->pond && view_->found == v->found && view_->larva == v->larva && view_->rot == v->rot && view_->meals == v->meals &&
                          view_->links.size() == v->links.size() &&
                          std::equal(v->links.begin(), v->links.end(), view_->links.begin(),
                                     [](const auto& x, const auto& y) { return x.a == y.a && x.b == y.b && x.owner == y.owner; });
        if (!same) view_ = std::move(v);
      }
      s->view = view_;
      w.pool<sim::Field>().each([&](sim::EntityId e, const sim::Field& f) {   // területi mezők (a régi `draw_area`)
        const auto* p = w.get<sim::Position>(e);
        if (!p || f.ability >= sim_->rules()->abilities.size() || f.until <= sim_->tick()) return;
        const sim::AbilityDef& a = sim_->rules()->abilities[f.ability];
        Snapshot::Area ar;
        ar.x = to_px(p->x);
        ar.y = to_px(p->y);
        ar.r = to_px(a.radius);
        ar.ability = f.ability;
        ar.until = f.until;
        ar.start = f.until - a.duration;
        if (seen(ar.x, ar.y, true)) s->areas.push_back(ar);
      });
    }
    s->winner = sim_->winner();
    s->night = sim_->night_permille();
    s->weather = sim_->weather_info().kind;
    s->mode = sim_->match_mode();
    if (victory_view_) s->victory_status = victory_view_->victory_status();
  }
  if (mission_) {   // M11: a küldetés a felületnek
    SnapMission& m = s->mission;
    const sim::MissionDef& d = mission_->def();
    m.active = true;
    for (const auto& o : d.objectives) {
      const uint8_t st = mission_->objective_state(o.id);
      if (st == 0) continue;
      SnapMission::Objective so;
      so.text = o.text;
      so.kind = static_cast<uint8_t>(o.kind == 1 ? 1 : 0);
      so.state = st;
      so.marker = o.marker;
      if (const sim::MissionRegion* rg = o.at.empty() ? nullptr : d.region(o.at)) {   // régió: a közepe, a nagyobbik fél oldal
        so.placed = true;
        so.x = (static_cast<float>(rg->x) + static_cast<float>(rg->w) / 2) * 32.0f;
        so.y = (static_cast<float>(rg->y) + static_cast<float>(rg->h) / 2) * 32.0f;
        so.r = static_cast<float>(std::max(rg->w, rg->h)) / 2 * 32.0f;
      } else if (const sim::MissionPoint* pt = o.at.empty() ? nullptr : d.point(o.at)) {   // pont: 3 csempe sugár
        so.placed = true;
        so.x = to_px(pt->x);
        so.y = to_px(pt->y);
        so.r = 3 * 32.0f;
      }
      m.objectives.push_back(std::move(so));
    }
    const auto& tv = mission_->dialogue();
    if (const sim::MissionDialogue* md = d.dialogue(tv.dialogue)) {
      if (const sim::DialogueNode* n = md->node(tv.node)) {
        m.dialogue = md->id;
        m.speaker = n->speaker;
        m.portrait = n->portrait;
        m.text = n->text;
        for (const auto& c : n->choices) m.choices.push_back(c.text);
      }
    }
    const auto& cv = mission_->cutscene();
    m.cutscene = !cv.id.empty();
    m.letterbox = cv.letterbox;
    m.camera = cv.camera;
    if (const sim::MissionPoint* p = d.point(cv.camera)) {
      m.cam_x = static_cast<float>(p->x.to_double());
      m.cam_y = static_cast<float>(p->y.to_double());
      m.cam_zoom = static_cast<float>(p->zoom.to_double());
    }
    m.cam_ticks = cv.camera_ticks;
    if (cv.subtitle_until >= sim_->tick()) {
      m.subtitle = cv.subtitle;
      m.sub_speaker = cv.speaker;
    }
    for (const auto& msg : mission_->messages()) m.messages.push_back(msg.text);
    m.result = static_cast<uint8_t>(mission_->result());
  }
  std::sort(s->units.begin(), s->units.end(), [](const SnapUnit& a, const SnapUnit& b) { return a.id < b.id; });
  s->fog = fog_;
  last_ = s;
  return last_;
}

std::shared_ptr<const Snapshot> Session::step(const std::vector<sim::Command>& commands, std::vector<FxEvent>& fx) {
  const auto before = snapshot();
  for (sim::Command c : commands) {
    c.tick = sim_->tick();
    sim_->queue(c);   // a parancssoron át (mint a hálózati játékban és a visszajátszásban)
  }
  sim_->step();
  for (const sim::SimEvent& ev : sim_->events()) {
    if (ev.owner == local_ && notes_.size() < 256) notes_.push_back(ev);
  }
  if (!sim_->rules() && sim_->tick() % 5 == 0) update_fog();
  const auto after = snapshot();
  // látványesemények azonosító szerinti sorrendben: eltűnt egység → halál (az előző helyén), csökkent életerő → találat
  size_t j = 0;
  for (const SnapUnit& a : before->units) {
    if (a.kind != SnapUnit::kUnit) continue;   // a kivágott fa és a kimerült lelőhely nem „hal meg”
    while (j < after->units.size() && after->units[j].id < a.id) ++j;
    if (j >= after->units.size() || after->units[j].id != a.id) {
      if (!sim_->world().alive(a.id)) {   // a ködbe lépő nem hal meg (D41)
        const sim::Rules* rr = sim_->rules();
        const bool big = rr && a.def < rr->defs.size() && rr->defs[a.def].has_tag("large");
        fx.push_back({a.x, a.y, true, big, a.def, a.owner});
      }
      continue;
    }
    const SnapUnit& b = after->units[j];
    if (b.hp < a.hp) fx.push_back({b.x, b.y - 6.0f, false, false, b.def, b.owner});
  }
  return after;
}

// ---------------------------------------------------------------------------------------------------------------

double SimRunner::now() const {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - kEpoch).count();
}

bool SimRunner::run_one() {
  std::lock_guard<std::mutex> step_lock(step_mu_);   // a with_sim (mentés) addig vár
  std::vector<sim::Command> cmds;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (!cur_) cur_ = s_.snapshot();
    if (!source_) cmds.swap(pending_);
  }
  if (source_ && !source_->begin(cmds)) return false;   // hálózat: a következő tick még nincs megerősítve
  fx_scratch_.clear();
  const auto t0 = std::chrono::steady_clock::now();
  auto snap = s_.step(cmds, fx_scratch_);
  s_.take_notes(notes_scratch_);
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  if (source_) source_->end(s_.sim().hash());
  auto pub = std::make_shared<Snapshot>(*snap);   // a közzététel ideje a futtató órája szerint
  pub->published = now();
  std::lock_guard<std::mutex> lock(mu_);
  prev_ = cur_;
  cur_ = std::move(pub);
  if (fx_.size() < 16384) fx_.insert(fx_.end(), fx_scratch_.begin(), fx_scratch_.end());
  if (notes_.size() < 256) notes_.insert(notes_.end(), notes_scratch_.begin(), notes_scratch_.end());
  notes_scratch_.clear();  if (tick_ms_.size() < 1000000) tick_ms_.push_back(ms);
  Diagnostics::get().tick_ms(ms);
  return true;
}

void SimRunner::step_sync() { run_one(); }

void SimRunner::loop() {
  using clock = std::chrono::steady_clock;
  auto period = std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(tick_seconds()));
  auto next = clock::now() + period;
  while (!stop_.load()) {
    period = std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(tick_seconds()));   // a sebesség
    if (paused_.load()) {   // szünet: nem lép, az óra nem halmoz lemaradást
      std::this_thread::sleep_for(period);
      next = clock::now() + period;
      continue;
    }
    run_one();
    // hálózati lemaradás (pl. visszatérés után): több tick egy ütemben, amíg utol nem éri (legfeljebb 4×)
    for (int k = 0; source_ && k < 3 && source_->backlog() > 2; ++k) run_one();
    const auto t = clock::now();
    if (t < next) {
      std::this_thread::sleep_until(next);
      next += period;
    } else {
      next = t + period;   // lemaradás: nem pótol (a szimuláció lassul, a kép nem akad)
    }
  }
}

void SimRunner::start() {
  if (thread_.joinable()) return;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (!cur_) {
      auto s = std::make_shared<Snapshot>(*s_.snapshot());
      s->published = now();
      cur_ = s;
    }
  }
  stop_ = false;
  thread_ = std::thread([this] { loop(); });
}

void SimRunner::stop() {
  stop_ = true;
  if (thread_.joinable()) thread_.join();
}

void SimRunner::post(sim::Command c) {
  if (source_) {   // hálózat: a lockstep-kliens küldi el (a végrehajtás tickjét a relé adja)
    source_->submit(std::move(c));
    return;
  }
  std::lock_guard<std::mutex> lock(mu_);
  pending_.push_back(std::move(c));
}

void SimRunner::latest(std::shared_ptr<const Snapshot>* prev, std::shared_ptr<const Snapshot>* cur) {
  std::lock_guard<std::mutex> lock(mu_);
  if (!cur_) cur_ = s_.snapshot();   // szinkron mód, még egy tick sem futott
  *cur = cur_;
  *prev = prev_ ? prev_ : cur_;
}

void SimRunner::take_fx(std::vector<FxEvent>& out) {
  std::lock_guard<std::mutex> lock(mu_);
  out.insert(out.end(), fx_.begin(), fx_.end());
  fx_.clear();
}

void SimRunner::take_notes(std::vector<sim::SimEvent>& out) {
  std::lock_guard<std::mutex> lock(mu_);
  out.insert(out.end(), notes_.begin(), notes_.end());
  notes_.clear();
}

void SimRunner::take_tick_ms(std::vector<double>& out) {
  std::lock_guard<std::mutex> lock(mu_);
  out.insert(out.end(), tick_ms_.begin(), tick_ms_.end());
  tick_ms_.clear();
}

}  // namespace totu::client
