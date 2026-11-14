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
// szövetséges kisnépek (minor/)
std::unique_ptr<FactionModule> make_daresh_module();
std::unique_ptr<FactionModule> make_galoca_module();
std::unique_ptr<FactionModule> make_ghoblin_module();
std::unique_ptr<FactionModule> make_mhonyo_module();
std::unique_ptr<FactionModule> make_niil_elf_module();
std::unique_ptr<FactionModule> make_nila_uchana_module();
std::unique_ptr<FactionModule> make_pinguu_module();
std::unique_ptr<FactionModule> make_shrissh_matah_module();
std::unique_ptr<FactionModule> make_tenduur_module();
std::unique_ptr<FactionModule> make_tokla_module();
std::unique_ptr<FactionModule> make_tuz_elf_module();
std::unique_ptr<FactionModule> make_zhan_zhama_module();

std::vector<std::unique_ptr<FactionModule>> make_faction_modules() {
  std::vector<std::unique_ptr<FactionModule>> out;
  out.push_back(make_imperium_module());
  out.push_back(make_ooni_module());
  out.push_back(make_croftian_module());
  out.push_back(make_hukucha_module());
  out.push_back(make_elves_module());
  out.push_back(make_micelia_module());
  out.push_back(make_daresh_module());
  out.push_back(make_galoca_module());
  out.push_back(make_ghoblin_module());
  out.push_back(make_mhonyo_module());
  out.push_back(make_niil_elf_module());
  out.push_back(make_nila_uchana_module());
  out.push_back(make_pinguu_module());
  out.push_back(make_shrissh_matah_module());
  out.push_back(make_tenduur_module());
  out.push_back(make_tokla_module());
  out.push_back(make_tuz_elf_module());
  out.push_back(make_zhan_zhama_module());
  return out;
}

}  // namespace totu::sim
