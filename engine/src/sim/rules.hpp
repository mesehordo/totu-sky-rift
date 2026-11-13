// SPDX-License-Identifier: GPL-3.0-or-later
// Játékszabály-adatok a szimulációnak (M6, ADR-007): a `game.json` és a sablonok (egység, épület, nép) tömör,
// fixpontos C++-alakban. A szimuláció csak ezt olvassa (a JSON-t nem); a betöltés egyszer, a meccs előtt történik.
// A számok a régi játék adatai (D37); az idők másodpercben vannak az adatban, itt tickben (30 Hz).
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "content/json.hpp"
#include "content/loader.hpp"
#include "core/fixed.hpp"

namespace totu::sim {

using core::Fixed;

constexpr int kTickHz = 30;
constexpr int kResources = 4;   // a nyersanyagok rögzített sorrendje
enum Res : uint8_t { kWood = 0, kStone = 1, kMeteor = 2, kFaith = 3 };
const char* resource_id(int r);           // "wood", "stone", "meteor", "faith"
int resource_index(const std::string& id);   // -1, ha ismeretlen
constexpr uint16_t kNoDef = 0xFFFF;

int seconds_to_ticks(Fixed seconds);       // kerekítve

struct ResourceDef {
  Fixed gather_time;      // másodperc egy teljes rakományra (az alap `carry`-hez)
  int32_t carry = 0;      // alap rakomány
  int32_t node_amount = 0;
  bool special = false;   // nem gyűjthető lelőhelyről (áhítat)
};

// lövedékfajta (D40; `game.json` → `projectiles`): a fajta adja a viselkedést
struct ProjectileKind {
  std::string id;
  Fixed speed = Fixed::from_ratio(1, 2);      // csempe / tick
  bool homing = true;                         // követi a célt
  bool ground = false;                        // a kilövéskori célponthelyre érkezik
  bool line = false;                          // egyenes vonalú, az útjába kerülő első ellenség
  Fixed arc;                                  // az ív magassága a táv arányában (látvány)
  bool never_miss = false;                    // a magaslat nem zavarja
  int32_t bounces = 0;                        // továbbpattanások
  Fixed bounce_range = Fixed::from_ratio(7, 2);
  int32_t bounce_falloff = 750;               // ezrelék
  bool returns = false;                       // a végén visszarepül a dobóhoz (látvány)
  Fixed impact = Fixed::from_ratio(2, 5);     // a talajra érkező becsapódási sugara (csempe)
  Fixed range_mult = Fixed::from_int(1);      // egyenes vonalú: a pálya hossza a lőtáv arányában
};

// támadás (`Attack`), a régi játék adatai szerint
struct AttackDef {
  bool has = false;
  Fixed damage;                               // alap sebzés
  Fixed range;                                // csempe (a test szélétől)
  int32_t cooldown = 30;                      // tick
  bool ground = true, air = false;            // `targets`
  uint8_t projectile = 0;                     // 0: közelharc; különben a lövedékfajta sorszáma + 1 (Rules::projectiles)
  Fixed proj_speed;                           // csempe / tick (0: a fajtáé)
  Fixed splash;                               // csempe (0: nincs)
  std::vector<std::pair<std::string, Fixed>> bonus;        // címke → +sebzés
  std::vector<std::pair<std::string, Fixed>> type_bonus;   // célosztály → szorzó (a mátrixnál erősebb)
  bool ranged() const { return projectile != 0; }
};

// Képesség (M6 8. lépés; KEPESSEGEK.md A. rész – a régi játék szerint). A hősképességnél az 1. szint (`levels[0]`)
// már bele van fésülve (az M6-ban a hősnek nincs szintje, D37). Az idők tickben, a távok csempében.
enum class Element : uint8_t { None = 0, Ice, Fear };
struct AbilityMods {   // a `modifiers`: szorzók (1 = nincs) és összeadandók (0 = nincs)
  Fixed speed_mult = Fixed::from_int(1), damage_mult = Fixed::from_int(1), ranged_taken_mult = Fixed::from_int(1);
  Fixed armor_add, range_add, regen_add;
  bool empty() const;
};
struct AbilityDef {
  enum class Type : uint8_t { Unknown = 0, Garrison, SelfBuff, Toggle, Nova, Chant, Aura, Autoheal, Bolt, Summon, Area,
                              Passive, Power };
  std::string id;
  Type type = Type::Unknown;
  int32_t cooldown = 0, duration = 0;   // tick
  Fixed radius, range;
  int32_t energy = 0;                   // költség (az egység energiájából)
  Fixed damage, dps, heal;              // varázssebzés (páncél nélkül), területi sebzés / s, gyógyítás
  int32_t stun = 0;                     // tick
  bool has_slow = false;                // `slow` megadva (bolt: 0 = kábítás `duration` ideig; különben lassítás)
  Fixed slow;
  uint8_t projectile = 0;               // 0: azonnal; különben Rules::projectiles sorszáma + 1
  uint16_t unit = kNoDef;               // idézés: a sablon
  int32_t count = 1, lifetime = 0;      // idézés; a `lifetime` tick
  bool target_placement = false, drop_work = false, ultimate = false;
  Element element = Element::None;
  AbilityMods mods;
  std::string buff;                     // `gpower` (`levy`): a kapott önerősítés
  std::string effect;                   // `gpower`: `levy` (mozgósítás) …
  char hotkey = 0;                      // gyorsbillentyű (nagybetű; 0 = nincs)
};

// Szabálymódosítók (KUT-RULE, a régi `rules`): kulcsonként az utolsó felülír (a régi `merge_rules`).
struct RuleSet {
  int32_t garrison_dmg = -1;                                   // helyőrség sebzése fejenként (-1: a játék alapja)
  bool has_supply_add = false;
  std::vector<std::pair<uint16_t, int32_t>> supply_add;        // épület → + létszám
  bool has_low_hp_rage = false;                                // Medve-ösztön: `below` életerő-arány alatt × `mult`
  Fixed lhr_below, lhr_mult = Fixed::from_int(1);
  bool has_no_weak = false;                                    // a sablon / címke nem gyenge ezek ellen
  std::vector<std::pair<std::string, std::vector<std::string>>> no_weak;
  bool has_rage = false;                                       // vérszomj-irány: × (1 + max × hiányzó életerő-arány)
  std::vector<std::string> rage_tags;
  Fixed rage_max;
  void merge(const RuleSet& o);
};

struct EntityDef {
  std::string id;
  bool building = false;
  std::string faction;
  std::vector<std::string> tags;
  int32_t hp = 1, armor = 0;
  int32_t sight = 0;                          // csempe
  std::array<int32_t, kResources> cost{};    // nyersanyag-költség
  int32_t supply = 0;                         // a felhasznált létszám (egység)
  int32_t time_ticks = 0;                     // termelési / építési idő
  // egység
  Fixed speed;                                // csempe / tick
  Fixed radius = Fixed::from_ratio(4, 10);
  bool flying = false;
  bool worker = false;
  uint8_t gathers = 0;                        // bitmaszk (Res); a munkás alapból mindent
  std::array<int32_t, kResources> carry{};   // 0: a nyersanyag alap `carry`-je
  // épület
  int32_t fw = 0, fh = 0;                     // lábnyom (csempe)
  int32_t supply_provides = 0;
  uint8_t dropoff = 0;                        // bitmaszk (Res)
  // előfeltételek (`Requirements.all`): kész épület, kutatás, fejlődési irány, felemelkedés
  struct Req {
    enum Kind : uint8_t { Building, Research, Path, Ascended } kind = Building;
    std::string id;
  };
  std::vector<Req> reqs;
  // harc (M6 5. lépés)
  AttackDef attack;
  std::string utype;                          // egységosztály (`Identity.class`): a típusszorzó-mátrixhoz
  int32_t shield = 0;                         // pajzs
  int32_t detect = 0;                         // felfedés sugara (csempe; `Vision.detect`)
  bool always_hidden = false;                 // `Vision.always_hidden`
  int32_t shield_delay = 0;                   // tick (0: a játék alapja)
  // termelés (M6 4. lépés)
  bool hero = false;
  std::vector<uint16_t> produces;             // épület: a képezhető egységek (`Producer.units`)
  std::vector<std::string> researches;        // épület: a kutatások (`Researcher.list`)
  // képességek (M6 8. lépés): az `Abilities.list` és a hős `Hero.abilities` (Rules::abilities sorszámai); a
  // kutatáshoz kötöttek (`Abilities.research`) csak a kész kutatással használhatók
  std::vector<uint16_t> abilities;
  std::vector<std::pair<uint16_t, std::string>> ability_research;
  int32_t energy_max = 0;                     // `Energy.max`
  Fixed regen;                                // életerő / s (`Health.regen`)
  int32_t garrison = 0;                       // helyőrség-helyek (`Garrison.slots`)
  std::string shape;                          // `Identity.shape` (a felemelkedés nem számolja: wall, node, dropoff)
  bool rift_source = false;                   // Égkő (`Mechanics.rift_source`)
  std::vector<uint16_t> powers;               // főépület-képességek (`Powers.list`, Rules::abilities)
  bool has_tag(const std::string& t) const;
};

// Statmódosító (D24, egységes módosítórendszer): `path` a 0 A.D.-szerű útvonal (a mai tartalom e nyolcat használja:
// Attack.damage, Attack.range, Health.armor, Health.max, Health.shield, Movement.speed_pct, Vision.sight,
// Worker.carry_pct); `op` add | mul | replace; `filter`: üres = mindenki, különben címke vagy pontos azonosító.
// Halmozás (rögzített sorrend): minden `add` összegződik, utána minden `mul` szorzódik, végül az utolsó `replace`
// felülír. Csak a kutatások (állandó) használják; a napszak/időjárás saját, egyszerű szorzókat ad (lásd lejjebb).
struct Modifier {
  enum Op : uint8_t { Add, Mul, Replace } op = Add;
  std::string path;
  Fixed value;
  std::vector<std::string> filter_tags;   // üres = szűrés nélkül
  std::string filter_id;                  // ha nem üres, csak ez a sablon (a címkék mellett/helyett)
};

Fixed apply_modifiers(const std::vector<const std::vector<Modifier>*>& lists, const std::string& path, Fixed base,
                      const EntityDef& d);

struct ResearchDef {
  std::string id, faction;
  std::array<int32_t, kResources> cost{};
  int32_t time_ticks = 0;
  std::vector<EntityDef::Req> reqs;
  std::vector<Modifier> modifiers;   // állandó statváltozás kész kutatáskor (D24)
  RuleSet rules;                     // szabálymódosítók (KUT-RULE)
};

// fejlődési irány (MECH-ASC): az irány hatásai kutatásként (`effects`), a szabályai a kutatásokéi mellé
struct PathDef {
  std::string id;
  std::vector<Modifier> effects;
  RuleSet rules;
};

// Napszak és időjárás (M6, ADR-007 7. lépés; D35): egyszerű, egymással szorzódó hatásszorzók – nem az állandó
// Modifier-rendszer (az a kutatásoké), mert a hatás ideiglenes és a napszak/időjárás erőssége szerint lépcsőzött.
struct EnvEffect {
  Fixed speed_mult = Fixed::from_int(1);
  Fixed sight_mult = Fixed::from_int(1);
  Fixed range_mult = Fixed::from_int(1);     // csak a `range_tags` címkéjű (üres = mindenki) támadóknak
  Fixed gather_mult = Fixed::from_int(1);    // a gyűjtés sebessége (munkás)
  Fixed build_mult = Fixed::from_int(1);     // az építés haladása
  std::vector<std::string> range_tags, sight_tags;
  int32_t lightning_per_min = 0, lightning_dmg = 0;     // zivatar: ritkán a magaslaton állóba csap
  int32_t open_dmg_per_min = 0, open_dmg = 0;           // homokvihar: a nyílt terepen állók
  int32_t organic_dmg_per_min = 0, organic_dmg = 0;     // hamueső: az élő (`organic`) egységek
};

struct Environment {
  int32_t day_cycle_ticks = 7200;                        // egy teljes napszak-kör hossza
  int32_t hold_min_ticks = 3600, hold_max_ticks = 10800;  // 2–6 perc (D35)
  int32_t transition_ticks = 900;                         // fokozatos átmenet
  Fixed night_build_mult = Fixed::from_ratio(9, 10);
  Fixed night_sight_mult = Fixed::from_ratio(3, 4);
  std::map<std::string, std::vector<std::string>> climates;   // éghajlat → a rajta előforduló időjárások
  std::map<std::string, EnvEffect> weather;                    // időjárás-azonosító → általános hatás
};

struct FactionDef {
  std::string id;                             // pl. `ursan` (a sablon `faction_ursan`)
  uint16_t hq = kNoDef, worker = kNoDef;
  std::vector<std::pair<uint16_t, int>> start_units;
  std::string builder;                        // MECH-BUILD: stay, stay_single, enter, consume, leave
  bool demolish = false;                      // kész épület lebontható-e (D38: Ursanok, Ooni, Wullof)
  uint8_t start_gather = kWood;
  std::array<int32_t, kResources> start_resources{};
  // faji napszak/időjárás-hatás (D35, 4a. pont): azonosító ("ejszaka", "hoseg", …) → az általánoshoz szorzódó extra
  std::map<std::string, EnvEffect> env_daytime, env_weather;
  int32_t ascension_buildings = 3;            // MECH-ASC: ennyi különböző fajta kész épület
  std::vector<PathDef> paths;
  const PathDef* path(const std::string& id) const;
};

struct Rules {
  std::array<ResourceDef, kResources> resources{};
  std::array<int32_t, kResources> start_resources{};
  int32_t max_supply = 200;
  std::vector<std::pair<int32_t, int32_t>> upkeep_tiers;   // (felhasznált létszám felső határa, ezrelék)
  std::vector<EntityDef> defs;                             // azonosító szerint rendezve
  std::vector<FactionDef> factions;                        // azonosító szerint rendezve
  std::vector<ResearchDef> researches;                     // azonosító szerint rendezve
  static constexpr int kQueueMax = 5;                      // a termelési sor hossza (a régi játék szerint)
  // harc (`game.json`): a típusszorzó-mátrix (támadó osztály → célosztály → szorzó; az épület osztálya `epulet`),
  // a pajzs visszatöltése, a magaslat-szabályok (D33)
  std::map<std::string, std::map<std::string, Fixed>> type_matrix;
  std::vector<ProjectileKind> projectiles;                 // a `game.json` sorrendjében
  int projectile_index(const std::string& id) const;       // -1, ha nincs
  int32_t shield_regen_delay = 300;                        // tick
  Fixed shield_regen_per_tick;
  struct Elevation {
    Fixed ranged_range_per_level = Fixed::from_ratio(1, 2), ranged_range_max = Fixed::from_int(1);
    Fixed building_range_per_level = Fixed::from_int(1);
    int32_t uphill_miss_per_level = 250, uphill_miss_max = 400;   // ezrelék
    int32_t damage_per_level = 100;                                // ezrelék
    int32_t vision_per_level = 1, depression_vision = -1;          // látótáv szintenként (csempe)
    int32_t uphill_presence = 2;                                   // magasabb szintre csak ennyi csempéről lát
    int32_t reveal_shooter_ticks = 60;                             // a fentről lövő ennyi ideig látható
    int32_t depression_hidden_beyond = 6;                          // a mélységben állót fentről ennyin túl nem látja
  } elevation;
  Fixed type_mult(const std::string& attacker, const std::string& target) const;
  static constexpr int kMaxHeroes = 3;

  // A `game` a `game.json` tartalma; a sablonok a `set`-ből. Hiányzó kötelező adatnál nullopt (az ok az errors-ban).
  static std::optional<Rules> load(const content::Value& game, content::ContentSet& set, std::vector<std::string>& errors);
  uint16_t def(const std::string& id) const;               // kNoDef, ha nincs
  const FactionDef* faction(const std::string& id) const;
  const ResearchDef* research(const std::string& id) const;
  int research_index(const std::string& id) const;         // -1, ha nincs
  int32_t upkeep_permille(int32_t supply_used) const;      // MECH-UPKEEP
  // A félkész épület megszakításakor visszajáró költség ezreléke (a régi játékban nem volt megszakítás – ADR-007;
  // alapérték a WC3-é: 75%; `game.json` → `build_cancel_refund`).
  int32_t cancel_refund_permille = 750;
  // Kész épület lebontásakor visszajáró költség ezreléke (D38: 70%; `game.json` → `demolish_refund`); csak annál a
  // népnél, amelynek adatában `demolish: true` áll.
  int32_t demolish_refund_permille = 700;
  Environment env;   // `game.json` → `environment` (M6 7. lépés)
  // képességek és elemek (M6 8. lépés)
  std::vector<AbilityDef> abilities;                       // azonosító szerint rendezve
  int ability_index(const std::string& id) const;          // -1, ha nincs
  Fixed energy_regen_per_tick;                             // `game.json` → `energy_regen` (/ s)
  struct ElementFx {
    int32_t ice_ticks = 60;                                // fagyás: lassítás ideje
    Fixed ice_slow = Fixed::from_ratio(3, 5);
    int32_t fear_ticks = 48;                               // félelem: menekülés ideje
    Fixed fear_flee = Fixed::from_int(3);                  // csempe, az okozótól elfelé
  } element_fx;                                            // `statuses.json` (status_ice, status_fear)
  struct Rift {                                            // Égkő-fény (MECH-RIFT; `game.json` → `rift`)
    Fixed radius = Fixed::from_int(5), energy_mult = Fixed::from_ratio(3, 2), mage_range = Fixed::from_int(1);
    Fixed spell_mult = Fixed::from_int(1);
  } rift;
  int32_t garrison_dmg = 4;                                // helyőrség sebzése fejenként (a régi alapja)
};

}  // namespace totu::sim
