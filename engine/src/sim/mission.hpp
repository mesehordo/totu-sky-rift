// SPDX-License-Identifier: GPL-3.0-or-later
// Küldetés (M11, ADR-010): a küldetés leírása (mission.json) és a trigger-motor. A motor a szimuláción belül fut
// (SimHooks): az események, a feltételek és az akciók determinisztikusak, az állapota (változók, célok, triggerek,
// időzítők, névvel ellátott egységek, párbeszéd, átvezető, eredmény) a mentés és a hash része. A játékos választása
// parancs (Command::Kind::Mission), így hálózaton és visszajátszásban is ugyanaz.
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "content/json.hpp"
#include "core/fixed.hpp"
#include "sim/sim.hpp"

namespace totu::sim {

struct MissionRegion {
  std::string id;
  int x = 0, y = 0, w = 1, h = 1;   // csempe
};
struct MissionPoint {
  std::string id;
  core::Fixed x, y;
  core::Fixed zoom;   // kamerapont: a nagyítás (0 = változatlan)
};
struct MissionPath {
  std::string id;
  std::vector<std::pair<core::Fixed, core::Fixed>> points;
};
struct MissionUnit {   // a pályára helyezett, névvel ellátható egység vagy épület
  std::string name;    // üres: névtelen
  std::string def;
  uint8_t owner = 0;
  core::Fixed x, y;
  const content::Value* hero = nullptr;   // hadjárat: az átvitt hős állapota (szint, tapasztalat, képességek, tárgyak)
};

// Objektumszerkesztő (M11, ADR-010 2. pont): a küldetés `objects` része – sablononként felülírt értékek, illetve egy
// alapból (`base`) származtatott új sablon. A szabályok másolatán (a küldetés saját szabályai); hibánál nullptr.
// Mezők: hp, armor, sight, supply, time (tick), speed (csempe/s), damage, range (csempe), cooldown (tick), cost {…}.
std::unique_ptr<Rules> apply_objects(const Rules& base, const content::Value& objects, std::vector<std::string>& errors);

// a hős állapota JSON-ban (a hadjárat viszi át küldetésről küldetésre) – és vissza
content::Value export_hero(const Sim& s, EntityId e);
bool apply_hero(Sim& s, EntityId e, const content::Value& hero);
struct MissionPlayer {
  uint8_t slot = 0;
  std::string faction;
  bool human = false;
  std::array<int32_t, kResources> res{};
  std::string ai;   // gépi játékos személyisége (üres: nem gép)
};
struct MissionObjective {
  std::string id, text;
  uint8_t kind = 0;      // 0 elsődleges, 1 másodlagos, 2 rejtett
  bool visible = true;   // a rejtett a megjelenítésig nem látszik
  std::string at;        // a cél helye a pályán (régió vagy pont azonosítója; üres: nincs) – a régi `at`
  std::string marker;    // a jelzőfény felirata (fordítási kulcs vagy szöveg; üres: nincs)
};
struct DialogueChoice {
  std::string text, next;
  std::string var;   // a választás ezt a változót állítja (üres: nem)
  int64_t value = 0;
};
struct DialogueNode {
  std::string id, speaker, portrait, text, next;   // `next` üres: vége
  int32_t ticks = 90;                               // ennyi ideig látszik (válasz nélkül utána tovább)
  std::vector<DialogueChoice> choices;
};
struct MissionDialogue {
  std::string id;
  std::vector<DialogueNode> nodes;   // az első a kezdő
  const DialogueNode* node(const std::string& id) const;
};

struct MissionDef {
  std::string id, name, description;
  std::string map;   // a pálya fájlneve a küldetés mappájában
  std::vector<MissionPlayer> players;
  std::vector<MissionRegion> regions;
  std::vector<MissionPoint> points, cameras;
  std::vector<MissionPath> paths;
  std::vector<MissionUnit> units;
  std::vector<std::pair<std::string, int64_t>> variables;
  std::vector<MissionObjective> objectives;
  std::vector<MissionDialogue> dialogues;
  std::shared_ptr<const content::Value> doc;   // a forrás (a triggerek, átvezetők közvetlenül ebből)
  std::vector<const content::Value*> triggers;
  std::map<std::string, const content::Value*> cutscenes;

  const MissionRegion* region(const std::string& id) const;
  const MissionPoint* point(const std::string& id) const;   // pont vagy kamerapont
  const MissionPath* path(const std::string& id) const;
  const MissionDialogue* dialogue(const std::string& id) const;
  const MissionObjective* objective(const std::string& id) const;

  // a leírás betöltése és ellenőrzése (ismeretlen esemény-, feltétel- és akciótípus, hiányzó hivatkozás, ismeretlen
  // sablon); hibánál nullopt és az okok
  static std::optional<MissionDef> parse(const std::string& text, const Rules* rules, std::vector<std::string>& errors);
};

// Futó küldetés: a szimuláció csatlakozója.
class MissionHost : public SimHooks {
 public:
  explicit MissionHost(std::shared_ptr<const MissionDef> def);
  // új küldetés: a játékosok nyersanyaga, a pályára helyezett egységek és a változók (a meccsindítás után)
  void start(Sim& s);

  void on_tick(Sim& s) override;
  void on_death(Sim& s, EntityId id, uint8_t owner) override;
  void on_command(Sim& s, const Command& c) override;
  uint64_t hash() const override;
  void save(core::ByteWriter& w) const override;
  bool load(core::ByteReader& r) override;

  // a felületnek (a pillanatképhez) és a próbáknak
  enum class Result : uint8_t { None = 0, Won = 1, Lost = 2 };
  Result result() const { return result_; }
  int64_t var(const std::string& name) const;
  uint8_t objective_state(const std::string& id) const;   // 0 rejtett, 1 aktív, 2 teljesítve, 3 elbukva
  const std::vector<EntityId>* named(const std::string& name) const;
  struct DialogueView {
    std::string dialogue, node;   // üres: nincs párbeszéd
    int64_t started = 0;
  };
  const DialogueView& dialogue() const { return talk_; }
  struct CutsceneView {
    std::string id;   // üres: nincs átvezető
    int64_t started = 0;
    bool letterbox = false;
    std::string camera;      // az utolsó kameraparancs célpontja
    int32_t camera_ticks = 0;
    std::string subtitle, speaker;
    int64_t subtitle_until = 0;
  };
  const CutsceneView& cutscene() const { return cut_; }
  struct Message {
    int64_t tick = 0, until = 0;
    std::string text;
  };
  const std::vector<Message>& messages() const { return messages_; }
  const MissionDef& def() const { return *def_; }
  int errors() const { return errors_; }
  const std::vector<std::string>& log() const { return log_; }

  // Lua-hídhoz és a szerkesztőhöz: akció futtatása közvetlenül (a tick rögzített pontján)
  bool run_action(Sim& s, const content::Value& action, EntityId trigger_unit = kNoEntity);

 private:
  struct TriggerState {
    bool enabled = true, done = false;
    int64_t last = -1;   // az utolsó tüzelés tickje
  };
  struct Event {
    std::string type;
    std::string a;                 // régió / párbeszéd / cél / időzítő / választás
    uint8_t owner = 0;
    EntityId unit = kNoEntity;
    int64_t value = 0;
  };
  void fire(Sim& s, const Event& e);
  bool matches(Sim& s, const content::Value& ev, const Event& e) const;
  bool check(Sim& s, const content::Value& cond, EntityId unit) const;
  bool in_region(Sim& s, EntityId e, const MissionRegion& r) const;
  std::vector<EntityId> resolve(Sim& s, const content::Value& a, EntityId trigger_unit) const;
  void advance_dialogue(Sim& s, const std::string& next);
  void step_cutscene(Sim& s);
  void note(const std::string& m);

  std::shared_ptr<const MissionDef> def_;
  std::vector<TriggerState> triggers_;
  std::map<std::string, int64_t> vars_;
  std::map<std::string, uint8_t> objectives_;
  std::map<std::string, std::vector<EntityId>> names_;
  std::map<std::string, int64_t> timers_;
  std::map<std::string, std::vector<EntityId>> inside_;   // régió → bent lévő egységek (rendezve)
  std::vector<std::string> dialogue_queue_;
  DialogueView talk_;
  CutsceneView cut_;
  size_t cut_step_ = 0;
  std::vector<Message> messages_;
  Result result_ = Result::None;
  bool started_ = false;
  int depth_ = 0;          // egymásba ágyazott tüzelés (végtelen lánc ellen)
  int budget_ = 0;         // tickenkénti akciókeret
  int errors_ = 0;
  std::vector<std::string> log_;
};

struct Replay;
// Küldetéscsomag (mappa) → meccsleírás: a mission.json, a pálya (a küldetés mappájából, különben a `maps_dir`-ből) és a
// küldetés szkriptje (`script`, a „mission” modként). A tartalmat a start_match ellenőrzi.
// `extra_maps_dirs`: a modok pályamappái (M13) – a küldetés mappája és az alap pályái után ezekben is keres
std::optional<Replay> load_mission_package(const std::string& mission_dir, const std::string& maps_dir, uint64_t seed,
                                           std::string* error, const std::vector<std::string>& extra_maps_dirs = {});

}  // namespace totu::sim
