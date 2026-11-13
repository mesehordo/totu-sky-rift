// SPDX-License-Identifier: GPL-3.0-or-later
#include "client/hud.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace totu::client {

namespace {

void rect(render::SpriteBatch& batch, const render::Texture& white, float x, float y, float w, float h, uint8_t r,
          uint8_t g, uint8_t b, uint8_t a = 255) {
  render::SpriteInstance in;
  in.x = x; in.y = y; in.w = w; in.h = h;
  in.tint[0] = r; in.tint[1] = g; in.tint[2] = b; in.tint[3] = a;
  batch.draw(white, nullptr, in);
}

const char* weather_name(const std::string& k) {
  if (k.empty() || k == "derult") return "derült";
  if (k == "felhos") return "felhős";
  if (k == "eso") return "eső";
  if (k == "zivatar") return "zivatar";
  if (k == "kod") return "köd";
  if (k == "ho") return "hó";
  if (k == "hovihar") return "hóvihar";
  if (k == "homokvihar") return "homokvihar";
  if (k == "hoseg") return "hőség";
  if (k == "hamueso") return "hamueső";
  return k.c_str();
}

}  // namespace

bool Hud::init(render::Device& dev, const std::string& assets_dir, std::string* error) {
  std::ifstream in(std::filesystem::path(assets_dir) / "fonts" / "egszakadas_rovaskonyv_text.ttf", std::ios::binary);
  const std::vector<uint8_t> ttf{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  if (!render::bake_font(ttf, 15.0f, small_) || !render::bake_font(ttf, 30.0f, big_)) {
    if (error) *error = "a HUD betűje nem tölthető be (assets/fonts)";
    return false;
  }
  small_tex_ = dev.create_array_from(small_.atlas, false);
  big_tex_ = dev.create_array_from(big_.atlas, false);
  ready_ = true;
  return true;
}

void Hud::text(render::SpriteBatch& batch, bool big, float x, float y, const std::string& s, uint8_t r, uint8_t g,
               uint8_t b, uint8_t a) const {
  if (!ready_) return;
  const render::FontBake& f = big ? big_ : small_;
  const render::Texture& t = big ? big_tex_ : small_tex_;
  float pen = x;
  const float base = y + f.ascent;
  for (const uint32_t cp : render::utf8_codepoints(s)) {
    const render::Glyph* gl = f.find(cp);
    if (!gl) gl = f.find('?');
    if (!gl) continue;
    if (gl->x1 > gl->x0) {
      render::SpriteInstance in;
      in.x = pen + gl->x0; in.y = base + gl->y0; in.w = gl->x1 - gl->x0; in.h = gl->y1 - gl->y0;
      in.u0 = gl->u0; in.v0 = gl->v0; in.u1 = gl->u1; in.v1 = gl->v1;
      in.tint[0] = r; in.tint[1] = g; in.tint[2] = b; in.tint[3] = a;
      batch.draw(t, nullptr, in);
    }
    pen += gl->advance;
  }
}

bool Hud::contains(float, float y, int h) const { return y < kTop || y > static_cast<float>(h) - kBottom; }

HudAction Hud::click(float x, float y) const {
  for (const Button& b : buttons_) {
    if (x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h) return b.action;
  }
  return {};
}

void Hud::draw(render::SpriteBatch& batch, const render::Texture& white, World& world, Session& session, int w, int h) {
  buttons_.clear();
  const Snapshot* snap = world.snapshot();
  const sim::Rules* rules = session.rules();
  if (!snap || !rules) return;
  const float W = static_cast<float>(w), H = static_cast<float>(h);
  const SnapPlayer& me = snap->players[1];
  char buf[256];
  // felső sáv
  rect(batch, white, 0, 0, W, kTop, 24, 20, 16, 225);
  std::snprintf(buf, sizeof buf, "Fa %d    Arany %d    Meteorit %d    Létszám %d/%d", me.res[0], me.res[1], me.res[2],
                me.supply_used, me.supply_cap);
  text(batch, false, 10, 5, buf, 235, 220, 180);
  std::snprintf(buf, sizeof buf, "%s · %s", snap->night > 600 ? "éjjel" : snap->night > 0 ? "szürkület" : "nappal",
                weather_name(snap->weather));
  text(batch, false, W - small_.width(buf) - 10, 5, buf, 200, 215, 235);
  // alsó panel
  const float py = H - kBottom;
  rect(batch, white, 0, py, W, kBottom, 24, 20, 16, 235);
  rect(batch, white, 0, py, W, 2, 120, 100, 70);
  auto button = [&](float x, float y, float bw, const std::string& label, HudAction act, bool enabled = true) {
    rect(batch, white, x, y, bw, 26, enabled ? 60 : 40, enabled ? 50 : 38, enabled ? 38 : 32);
    rect(batch, white, x, y, bw, 1, 140, 120, 85);
    text(batch, false, x + 6, y + 5, label, enabled ? 240 : 140, enabled ? 225 : 135, enabled ? 190 : 120);
    if (enabled) buttons_.push_back({x, y, bw, 26, label, act});
  };
  const float bx0 = W * 0.42f, bw = 168, bh = 30;
  int slot = 0;
  auto next_button = [&](const std::string& label, HudAction act, bool enabled = true) {
    const int cols = std::max(1, static_cast<int>((W - bx0 - 10) / (bw + 6)));
    const float x = bx0 + static_cast<float>(slot % cols) * (bw + 6), y = py + 10 + static_cast<float>(slot / cols) * bh;
    if (y + 26 > H) return;
    button(x, y, bw, label, act, enabled);
    ++slot;
  };
  const sim::EntityId bsel = world.selected_building();
  const auto& sel = world.selection();
  if (bsel) {   // kijelölt épület
    const SnapUnit* b = snap->find(bsel);
    if (b && b->def < rules->defs.size()) {
      const sim::EntityDef& d = rules->defs[b->def];
      text(batch, true, 12, py + 8, session.display_name("building." + d.id + ".name"), 245, 230, 190);
      std::snprintf(buf, sizeof buf, "Életerő %d/%d%s", b->hp, b->max_hp, b->complete ? "" : " · épül");
      text(batch, false, 12, py + 46, buf, 210, 200, 170);
      if (b->slots > 0) {
        std::snprintf(buf, sizeof buf, "Helyőrség %d/%d", b->garrison, b->slots);
        text(batch, false, 12, py + 66, buf, 210, 200, 170);
      }
      std::string q;   // a termelési sor
      for (int i = 0; i < b->queue_n; ++i) {
        const uint16_t qd = b->queue_def[static_cast<size_t>(i)];
        const bool res = (b->queue_research >> i) & 1;
        const std::string nm = res ? session.display_name("research." + rules->researches[qd].id + ".name")
                                   : session.display_name("unit." + rules->defs[qd].id + ".name");
        if (i == 0) std::snprintf(buf, sizeof buf, "%s %d%%", nm.c_str(), b->queue_progress / 10);
        q += (i ? ", " : "") + (i == 0 ? std::string(buf) : nm);
      }
      if (!q.empty()) text(batch, false, 12, py + 88, "Sor: " + q, 200, 220, 200);
      if (b->complete) {
        for (const uint16_t u : d.produces) {
          next_button(session.display_name("unit." + rules->defs[u].id + ".name"), {HudAction::Train, rules->defs[u].id, u, bsel});
        }
        for (const std::string& r : d.researches) {
          if (std::find(me.researched.begin(), me.researched.end(), r) != me.researched.end()) continue;
          next_button(session.display_name("research." + r + ".name"), {HudAction::Research, r, sim::kNoDef, bsel});
        }
        for (const uint16_t pw : d.powers) {
          next_button(session.display_name("ability." + rules->abilities[pw].id + ".name"),
                      {HudAction::Power, rules->abilities[pw].id, sim::kNoDef, bsel});
        }
        if (b->garrison > 0) next_button("Kiszállás (U)", {HudAction::Unload, "", sim::kNoDef, bsel});
        if (d.diplomacy && me.ally.empty()) {   // szövetség egy kisnéppel (D26)
          for (const sim::FactionDef& f : rules->factions) {
            if (f.ally) next_button("Szövetség: " + session.display_name("faction." + f.id + ".name"), {HudAction::ChooseAlly, f.id, sim::kNoDef, bsel});
          }
        }
      }
    }
  } else if (!sel.empty()) {   // kijelölt egységek
    const SnapUnit* u = snap->find(sel.front());
    if (u && u->def < rules->defs.size()) {
      const sim::EntityDef& d = rules->defs[u->def];
      std::string title = session.display_name("unit." + d.id + ".name");
      if (sel.size() > 1) title += " +" + std::to_string(sel.size() - 1);
      text(batch, true, 12, py + 8, title, 245, 230, 190);
      std::snprintf(buf, sizeof buf, "Életerő %d/%d", u->hp, u->max_hp);
      text(batch, false, 12, py + 46, buf, 210, 200, 170);
      if (u->hero_level > 0) {   // hős: szint, pont, tanulás, leltár (D25)
        std::snprintf(buf, sizeof buf, "%d. szint · %d pont", u->hero_level, u->hero_points);
        text(batch, false, 12, py + 66, buf, 230, 210, 140);
        if (u->hero_points > 0) {
          for (const uint16_t ab : d.abilities) {
            next_button("Tanul: " + session.display_name("ability." + rules->abilities[ab].id + ".name"),
                        {HudAction::Learn, rules->abilities[ab].id, sim::kNoDef, 0});
          }
        }
        for (int i = 0; i < 6; ++i) {
          const uint16_t it = u->items[static_cast<size_t>(i)];
          if (it == sim::kNoItem || it >= rules->items.size()) continue;
          HudAction act{HudAction::UseItem, rules->items[it].id, sim::kNoDef, 0};
          act.slot = i;
          next_button(session.display_name("item." + rules->items[it].id + ".name"), act, !rules->items[it].equip);
        }
      }
      // képességek (a kijelöltek közös listája, egyszer)
      std::vector<uint16_t> abs;
      bool worker = false;
      for (const sim::EntityId id : sel) {
        const SnapUnit* x = snap->find(id);
        if (!x || x->def >= rules->defs.size()) continue;
        worker = worker || rules->defs[x->def].worker;
        for (const uint16_t ab : rules->defs[x->def].abilities) {
          if (std::find(abs.begin(), abs.end(), ab) == abs.end()) abs.push_back(ab);
        }
      }
      using T = sim::AbilityDef::Type;
      for (const uint16_t ab : abs) {
        const sim::AbilityDef& a = rules->abilities[ab];
        if (a.type == T::Passive || a.type == T::Aura || a.type == T::Autoheal || a.type == T::Unknown) continue;
        std::string label = session.display_name("ability." + a.id + ".name");
        if (a.hotkey) label += std::string(" (") + a.hotkey + ")";
        const bool targeted = a.type == T::Bolt || a.type == T::Area || (a.type == T::Summon && a.target_placement) ||
                              a.type == T::Garrison;
        next_button(label, {targeted ? HudAction::TargetAbility : HudAction::Ability, a.id, sim::kNoDef, 0});
      }
      if (worker) {   // építési menü
        for (const uint16_t bdef : world.buildable_defs("ursan")) {
          next_button(session.display_name("building." + rules->defs[bdef].id + ".name"), {HudAction::Build, rules->defs[bdef].id, bdef, 0});
        }
      }
    }
  } else {
    text(batch, false, 12, py + 12, "Bal gomb: kijelölés · jobb gomb: mozgás, támadás, beszállás · A: támadó mozgás", 190, 180, 150);
    text(batch, false, 12, py + 34, "B: építés · U: kiszállás · képesség: a gombja vagy a betűje · görgő: nagyítás", 190, 180, 150);
  }
  // fejlődési irány (felemelkedés után)
  if (me.active && me.ascended && me.path.empty()) {
    if (const sim::FactionDef* f = rules->faction("ursan")) {
      float x = 10;
      for (const auto& p : f->paths) {
        const std::string label = "Irány: " + session.display_name("path.ursan." + p.id + ".name");
        const float bwid = small_.width(label) + 14;
        rect(batch, white, x, kTop + 4, bwid, 24, 50, 44, 30, 210);
        text(batch, false, x + 7, kTop + 8, label, 240, 220, 160);
        buttons_.push_back({x, kTop + 4, bwid, 24, label, {HudAction::ChoosePath, p.id, sim::kNoDef, 0}});
        x += bwid + 6;
      }
    }
  }
  // a meccs vége
  if (snap->winner != 0 || me.defeated) {
    const bool won = snap->winner == 1;
    const std::string msg = won ? "Győzelem!" : "Vereség";
    rect(batch, white, 0, H / 2 - 40, W, 80, 10, 8, 6, 200);
    text(batch, true, W / 2 - big_.width(msg) / 2, H / 2 - 20, msg, won ? 250 : 240, won ? 220 : 90, won ? 120 : 70);
  }
}

}  // namespace totu::client
