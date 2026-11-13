// SPDX-License-Identifier: GPL-3.0-or-later
// A népmodulok rögzített sorrendben (a determinizmus miatt – a sorrend a hash része).
#include "sim/faction_module.hpp"

namespace totu::sim {

std::unique_ptr<FactionModule> make_imperium_module();
std::unique_ptr<FactionModule> make_ooni_module();
std::unique_ptr<FactionModule> make_croftian_module();
std::unique_ptr<FactionModule> make_hukucha_module();
std::unique_ptr<FactionModule> make_elves_module();
std::unique_ptr<FactionModule> make_micelia_module();

std::vector<std::unique_ptr<FactionModule>> make_faction_modules() {
  std::vector<std::unique_ptr<FactionModule>> out;
  out.push_back(make_imperium_module());
  out.push_back(make_ooni_module());
  out.push_back(make_croftian_module());
  out.push_back(make_hukucha_module());
  out.push_back(make_elves_module());
  out.push_back(make_micelia_module());
  return out;
}

}  // namespace totu::sim
