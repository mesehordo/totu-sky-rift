// SPDX-License-Identifier: GPL-3.0-or-later
// Entitás–komponens tár (UJ_MOTOR_TERV 3., ADR-005). Determinisztikus szabályok:
//   - az entitás-azonosító egy növekvő 32 bites szám, soha nem használódik újra (0 = nincs);
//   - minden komponenstípus egy tömör, azonosító szerint RENDEZETT tárolóban él – a bejárás mindig azonosító szerint
//     növekvő, a beszúrás sorrendjétől és a memóriacímektől függetlenül;
//   - a törlés a tick végén (`flush`) történik, így bejárás közben biztonságos.
// A komponenstípusokat név szerint kell regisztrálni (a regisztráció sorrendje az állapothash sorrendje).
#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace totu::sim {

using EntityId = uint32_t;
constexpr EntityId kNoEntity = 0;

class PoolBase {
 public:
  virtual ~PoolBase() = default;
  virtual bool remove(EntityId id) = 0;
  virtual size_t size() const = 0;
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
