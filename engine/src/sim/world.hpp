// SPDX-License-Identifier: GPL-3.0-or-later
// Entitás–komponens tár (UJ_MOTOR_TERV 3., ADR-005). Determinisztikus szabályok:
//   - az entitás-azonosító egy növekvő 32 bites szám, soha nem használódik újra (0 = nincs);
//   - minden komponenstípus egy tömör, azonosító szerint RENDEZETT tárolóban él – a bejárás mindig azonosító szerint
//     növekvő, a beszúrás sorrendjétől és a memóriacímektől függetlenül;
//   - a törlés a tick végén (`flush`) történik, így bejárás közben biztonságos.
// A komponenstípusokat név szerint kell regisztrálni (a regisztráció sorrendje az állapothash sorrendje).
#pragma once

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "core/bytes.hpp"
#include "core/hash.hpp"

namespace totu::sim {

using EntityId = uint32_t;
constexpr EntityId kNoEntity = 0;

// A komponens opcionális műveletei (M2): állapothash, mentés, betöltés. Ha a típus nem adja meg, a tároló
// ezeket kihagyja (pl. tesztkomponenseknél); a szimuláció komponensei mindhármat megadják.
template <class T>
concept Hashable = requires(const T& t, core::StateHash& h) { t.hash(h); };
template <class T>
concept Serializable = requires(const T& t, core::ByteWriter& w, core::ByteReader& r) {
  t.save(w);
  { T::load(r) } -> std::same_as<T>;
};

class PoolBase {
 public:
  virtual ~PoolBase() = default;
  virtual bool remove(EntityId id) = 0;
  virtual size_t size() const = 0;
  virtual void hash(core::StateHash& h) const = 0;
  virtual void save(core::ByteWriter& w) const = 0;
  virtual bool load(core::ByteReader& r) = 0;
  virtual void clear() = 0;
};

template <class T>
class Pool final : public PoolBase {
 public:
  T* get(EntityId id) {
    auto it = std::lower_bound(ids_.begin(), ids_.end(), id);
    return (it != ids_.end() && *it == id) ? &data_[static_cast<size_t>(it - ids_.begin())] : nullptr;
  }
  const T* get(EntityId id) const { return const_cast<Pool*>(this)->get(id); }

  T& add(EntityId id, T value) {
    auto it = std::lower_bound(ids_.begin(), ids_.end(), id);
    const auto pos = static_cast<size_t>(it - ids_.begin());
    if (it != ids_.end() && *it == id) { data_[pos] = std::move(value); return data_[pos]; }
    ids_.insert(it, id);
    data_.insert(data_.begin() + static_cast<std::ptrdiff_t>(pos), std::move(value));
    return data_[pos];
  }

  bool remove(EntityId id) override {
    auto it = std::lower_bound(ids_.begin(), ids_.end(), id);
    if (it == ids_.end() || *it != id) return false;
    const auto pos = it - ids_.begin();
    ids_.erase(it);
    data_.erase(data_.begin() + pos);
    return true;
  }

  size_t size() const override { return ids_.size(); }

  void hash(core::StateHash& h) const override {
    h.add_u64(ids_.size());
    for (size_t i = 0; i < ids_.size(); ++i) {
      h.add_u64(ids_[i]);
      if constexpr (Hashable<T>) data_[i].hash(h);
    }
  }
  void save(core::ByteWriter& w) const override {
    if constexpr (Serializable<T>) {
      w.u32(static_cast<uint32_t>(ids_.size()));
      for (size_t i = 0; i < ids_.size(); ++i) { w.u32(ids_[i]); data_[i].save(w); }
    } else {
      w.u32(0);
    }
  }
  bool load(core::ByteReader& r) override {
    clear();
    const uint32_t n = r.u32();
    if constexpr (Serializable<T>) {
      for (uint32_t i = 0; i < n && r.ok(); ++i) {
        const EntityId id = r.u32();
        if (!ids_.empty() && id <= ids_.back()) return false;   // a mentés rendezett
        ids_.push_back(id);
        data_.push_back(T::load(r));
      }
    } else if (n != 0) {
      return false;
    }
    return r.ok();
  }
  void clear() override { ids_.clear(); data_.clear(); }

  // Bejárás azonosító szerint növekvő sorrendben: f(EntityId, T&).
  template <class F>
  void each(F&& f) {
    for (size_t i = 0; i < ids_.size(); ++i) f(ids_[i], data_[i]);
  }
  template <class F>
  void each(F&& f) const {
    for (size_t i = 0; i < ids_.size(); ++i) f(ids_[i], data_[i]);
  }

 private:
  std::vector<EntityId> ids_;
  std::vector<T> data_;
};

class World {
 public:
  EntityId create() {
    const EntityId id = ++last_id_;
    alive_.push_back(id);   // növekvő, tehát rendezett marad
    return id;
  }

  bool alive(EntityId id) const { return std::binary_search(alive_.begin(), alive_.end(), id); }
  // Törlés a tick végén (a bejárások addig érintetlenek).
  void destroy(EntityId id) { if (alive(id)) pending_.push_back(id); }

  // A tick végén: a törlésre jelölt entitások és minden komponensük eltávolítása.
  void flush() {
    std::sort(pending_.begin(), pending_.end());
    pending_.erase(std::unique(pending_.begin(), pending_.end()), pending_.end());
    for (const EntityId id : pending_) {
      for (auto& p : pools_) p->remove(id);
      auto it = std::lower_bound(alive_.begin(), alive_.end(), id);
      if (it != alive_.end() && *it == id) alive_.erase(it);
    }
    pending_.clear();
  }

  template <class T>
  void register_component(const std::string& name) {
    const std::type_index key(typeid(T));
    if (index_.count(key)) return;
    index_.emplace(key, pools_.size());
    names_.push_back(name);
    pools_.push_back(std::make_unique<Pool<T>>());
  }

  template <class T>
  Pool<T>& pool() {
    return *static_cast<Pool<T>*>(pools_.at(index_.at(std::type_index(typeid(T)))).get());
  }

  template <class T>
  T& add(EntityId id, T value) { return pool<T>().add(id, std::move(value)); }
  template <class T>
  T* get(EntityId id) { return pool<T>().get(id); }

  size_t entity_count() const { return alive_.size(); }

  // Állapothash: az élő entitások és minden komponenstároló, a regisztráció sorrendjében.
  uint64_t hash() const {
    core::StateHash h;
    h.add_u64(last_id_);
    h.add_u64(alive_.size());
    for (const EntityId id : alive_) h.add_u64(id);
    for (const auto& p : pools_) p->hash(h);
    return h.value();
  }
  // Komponensenkénti hash (az eltéréskeresőhöz: melyik rendszer állapota tért el először).
  std::vector<std::pair<std::string, uint64_t>> component_hashes() const {
    std::vector<std::pair<std::string, uint64_t>> out;
    for (size_t i = 0; i < pools_.size(); ++i) {
      core::StateHash h;
      pools_[i]->hash(h);
      out.emplace_back(names_[i], h.value());
    }
    return out;
  }
  // Mentés és betöltés (a regisztrált komponenstípusoknak egyezniük kell; a függő törlés nem menthető).
  void save(core::ByteWriter& w) const {
    w.u32(last_id_);
    w.u32(static_cast<uint32_t>(alive_.size()));
    for (const EntityId id : alive_) w.u32(id);
    w.u32(static_cast<uint32_t>(pools_.size()));
    for (size_t i = 0; i < pools_.size(); ++i) { w.str(names_[i]); pools_[i]->save(w); }
  }
  bool load(core::ByteReader& r) {
    pending_.clear();
    last_id_ = r.u32();
    const uint32_t n = r.u32();
    alive_.clear();
    for (uint32_t i = 0; i < n && r.ok(); ++i) alive_.push_back(r.u32());
    if (r.u32() != pools_.size()) return false;
    for (size_t i = 0; i < pools_.size(); ++i) {
      if (r.str() != names_[i] || !pools_[i]->load(r)) return false;
    }
    return r.ok() && std::is_sorted(alive_.begin(), alive_.end());
  }
  EntityId last_id() const { return last_id_; }
  const std::vector<std::string>& component_names() const { return names_; }

 private:
  EntityId last_id_ = 0;
  std::vector<EntityId> alive_;
  std::vector<EntityId> pending_;
  std::vector<std::unique_ptr<PoolBase>> pools_;
  std::vector<std::string> names_;
  std::unordered_map<std::type_index, size_t> index_;   // csak keresésre; a bejárás a `pools_` sorrendjében
};

}  // namespace totu::sim
