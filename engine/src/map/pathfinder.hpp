// SPDX-License-Identifier: GPL-3.0-or-later
// Hierarchikus útkereső (ADR-006 5. pont, M4 2. lépés), a 0 A.D. mintájára.
//
// Minden (mozgásosztály, méretosztály) párhoz külön hierarchia: a navrácsot 16 × 16-os klaszterek osztják fel; egy
// klaszteren belül az egymással (4-szomszédsággal) összefüggő, az egységnek elférő navcellák egy *régiót* alkotnak.
// A szomszédos klaszterek határán érintkező régiók gráfot adnak; ennek összefüggő komponensei globális azonosítót
// kapnak. Ebből:
//   - elérhetőség O(1): két navcella között akkor van út, ha a régiójuk egy komponensben van;
//   - elérhetetlen cél: azonnal a cél körüli legközelebbi, a kezdőpontéval azonos komponensbe eső navcella lesz a cél
//     (nincs hosszú, sikertelen keresés – P-002);
//   - hosszú út: A* a régiógráfon, majd finom A* csak a kapott folyosó (és a szomszédos régiók) navcelláin, a terep
//     sebességszorzójával súlyozva; végül útsimítás látóvonallal.
// Átlós lépés csak akkor, ha mindkét oldalsó navcella is szabad (nincs sarokvágás); így a 4-szomszédos régiók és a
// 8-irányú finom keresés ugyanazt az elérhetőséget adja.
// Akadályváltozáskor csak az érintett klaszterek számolódnak újra (a régiógráf és a komponensek globálisan, ami
// olcsó). Minden számítás egész, a döntetlenek rögzített sorrendben dőlnek el – determinisztikus.
#pragma once

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "map/navgrid.hpp"

namespace totu::map {

struct NavCell {
  int x = 0, y = 0;
  friend bool operator==(const NavCell&, const NavCell&) = default;
};

struct PathResult {
  bool found = false;          // van út (legalább a legközelebbi elérhető pontig)
  bool goal_reachable = false; // a kért cél maga elérhető
  NavCell goal_cell;           // a ténylegesen használt célnavcella
  std::vector<std::pair<Fixed, Fixed>> waypoints;   // csempe-koordinátában, a kezdőpont nélkül
  int64_t cost = 0;            // a finom keresés költsége (simítás előtt; a heurisztika mértékében)
  size_t expanded = 0;         // a finom keresés kibontott navcellái (méréshez)
  size_t region_path = 0;      // a régióút hossza
};

class Pathfinder {
 public:
  explicit Pathfinder(NavGrid& nav, int cluster = 16);

  NavGrid& nav() { return nav_; }
  int cluster_size() const { return cs_; }

  // Akadály- és terepváltozás: a navrácsot is frissíti, majd a hierarchiát.
  void add_obstacle(TileRect r);
  void remove_obstacle(TileRect r);
  void terrain_changed(TileRect r);

  static NavCell cell_of(Fixed x, Fixed y);              // csempe-koordinátából navcella
  static std::pair<Fixed, Fixed> center_of(NavCell c);   // a navcella közepe csempe-koordinátában

  // 0 = nem fér el / járhatatlan; különben a komponens azonosítója + 1
  uint32_t component(int grid, int size, NavCell c) const;
  bool reachable(int grid, int size, NavCell a, NavCell b) const;
  // A `target`-hez legközelebbi navcella, amely a `component` (component() értéke) komponensbe esik; 0 = bármely
  // elférő navcella. Euklideszi távolság, döntetlennél a kisebb (y, x).
  std::optional<NavCell> nearest(int grid, int size, uint32_t component, NavCell target) const;

  // Útkeresés csempe-koordinátákkal. grid < 0 (repülő): egyenes út.
  PathResult find_path(int grid, int size, Fixed sx, Fixed sy, Fixed gx, Fixed gy);
  // Ugyanez hierarchia nélkül, a teljes rácson (összehasonlító referencia a tesztekhez és a méréshez).
  PathResult find_path_flat(int grid, int size, Fixed sx, Fixed sy, Fixed gx, Fixed gy);

  // Látóvonal két navcella között: minden érintett navcellában elfér-e az egység (sarkon mindkét szomszéd kell).
  bool line_of_sight(int grid, int size, NavCell a, NavCell b) const;

  size_t region_count(int grid, int size) const;

 private:
  struct Region {
    NavCell rep;               // a klaszter közepéhez legközelebbi navcella
    uint32_t component = 0;
  };
  struct Hier {
    std::vector<uint16_t> label;          // navcellánként a klaszteren belüli régió (1…), 0 = nem fér el
    std::vector<std::vector<NavCell>> reps;   // klaszterenként a régiók képviselője
    std::vector<uint32_t> offset;         // klaszterenként az első régió globális indexe
    std::vector<Region> regions;
    std::vector<uint32_t> adj_start;      // CSR szomszédság
    std::vector<uint32_t> adj;
  };

  Hier& hier(int grid, int size) { return hiers_[static_cast<size_t>(grid * sizes_ + size)]; }
  const Hier& hier(int grid, int size) const { return hiers_[static_cast<size_t>(grid * sizes_ + size)]; }
  size_t idx(int x, int y) const { return static_cast<size_t>(y) * static_cast<size_t>(nav_.width()) + static_cast<size_t>(x); }
  int cluster_of(int x, int y) const { return (y / cs_) * ccx_ + x / cs_; }
  uint32_t region_index(const Hier& h, int x, int y) const;   // globális régióindex + 1 (0 = nincs)

  void label_cluster(int grid, int size, int cluster);
  void rebuild_graph(int grid, int size);
  void update_area(TileRect r);

  std::vector<uint32_t> region_route(const Hier& h, uint32_t from, uint32_t to) const;
  PathResult search(int grid, int size, Fixed sx, Fixed sy, Fixed gx, Fixed gy, bool hierarchical);
  bool fine_astar(int grid, int size, NavCell s, NavCell g, const std::vector<uint8_t>* allowed, const Hier* h,
                  std::vector<NavCell>& out, int64_t& cost, size_t& expanded);
  std::vector<NavCell> smooth(int grid, int size, NavCell start, const std::vector<NavCell>& cells) const;
  int64_t step_cost(int grid, int x, int y, bool diagonal) const;
  int64_t heuristic(int grid, NavCell a, NavCell b) const;
  void compute_max_speed();

  NavGrid& nav_;
  int cs_ = 16;
  int ccx_ = 0, ccy_ = 0;   // klaszterek száma
  int sizes_ = 0;
  std::vector<Hier> hiers_;
  std::vector<int32_t> max_speed_raw_;   // gridenként a leggyorsabb terep (a heurisztikához)

  // a finom keresés újrahasznált pufferei
  std::vector<uint32_t> stamp_;
  uint32_t gen_ = 0;
  std::vector<int64_t> g_;
  std::vector<int32_t> parent_;
  std::vector<uint8_t> closed_;
};

}  // namespace totu::map
