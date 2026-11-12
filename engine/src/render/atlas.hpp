// SPDX-License-Identifier: GPL-3.0-or-later
// Futásidejű sprite-atlasz (M5, 2. lépés): a leírás betöltése; a meccshez szükséges lapok egyetlen tömbtextúrába
// kerülnek (a lap rétege a sprite-példányban), így a népek keverve, Y szerint rendezve is egy rajzoló hívásban
// rajzolhatók. A csapatmaszk egycsatornás tömb. Ha menet közben új típus kell, a tömb kiegészül a lapjaival.
#pragma once

#include <string>
#include <vector>

#include "render/atlas_build.hpp"
#include "render/rhi.hpp"
#include "render/sprite_batch.hpp"

namespace totu::render {

class Atlas {
 public:
  bool load(Device& dev, const std::string& dir, std::string* error);
  void release();
  const AtlasData& data() const { return data_; }
  const AtlasSheet* sheet(const std::string& id) const { return data_.sheet(id); }
  int loaded_pages() const { return static_cast<int>(loaded_.size()); }

  // A megadott sprite-lapok lapjainak betöltése (a már betöltöttekkel együtt egy tömbbe). Előre hívható (pl. a meccs
  // indulásakor a résztvevő népek típusaival); a draw magától is pótolja, ha kell.
  bool prepare(const std::vector<const AtlasSheet*>& sheets, std::string* error = nullptr);

  // A képkocka rajzolása: (ax, ay) a horgonypont helye, `scale` a rajzolási méretarány a lap képpontjaihoz képest; a
  // csapatszín negyedik bájtja az erősség (0 = nincs).
  void draw(SpriteBatch& batch, const AtlasSheet& s, int direction, int column, float ax, float ay, float scale,
            const uint8_t team[4], const uint8_t tint[4] = nullptr);

 private:
  Device* dev_ = nullptr;
  std::string dir_;
  AtlasData data_;
  Texture base_, mask_;
  std::vector<int> loaded_;      // a tömb rétegeinek lapjai, növekvő sorrendben
  std::vector<int> layer_of_;    // lap → réteg (-1: nincs betöltve)
};

}  // namespace totu::render
