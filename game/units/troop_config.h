#pragma once

#include "spawn_type.h"
#include "troop_catalog.h"
#include "troop_type.h"
#include <string>
#include <unordered_map>

namespace Game::Units {

class TroopConfig {
public:
  static auto instance() -> TroopConfig & {
    static TroopConfig inst;
    return inst;
  }

  auto get_individuals_per_unit(TroopType unit_type) const -> int {
    auto it = m_individuals_per_unit.find(unit_type);
    if (it != m_individuals_per_unit.end()) {
      return it->second;
    }
    return 1;
  }

  auto get_production_cost(TroopType unit_type) const -> int {
    auto it = m_production_cost.find(unit_type);
    if (it != m_production_cost.end()) {
      return it->second;
    }
    return 50;
  }

  auto get_build_time(TroopType unit_type) const -> float {
    auto it = m_build_time.find(unit_type);
    if (it != m_build_time.end()) {
      return it->second;
    }
    return 5.0F;
  }

  auto get_max_units_per_row(TroopType unit_type) const -> int {
    auto it = m_max_units_per_row.find(unit_type);
    if (it != m_max_units_per_row.end()) {
      return it->second;
    }
    return 10;
  }

  auto get_selection_ring_size(TroopType unit_type) const -> float {
    auto it = m_selection_ring_size.find(unit_type);
    if (it != m_selection_ring_size.end()) {
      return it->second;
    }
    return 0.5F;
  }

  auto get_individuals_per_unit(const std::string &unit_type) const -> int {
    return get_individuals_per_unit(troop_typeFromString(unit_type));
  }

  auto get_individuals_per_unit(SpawnType spawn_type) const -> int {
    auto troop_type_opt = spawn_typeToTroopType(spawn_type);
    if (troop_type_opt) {
      return get_individuals_per_unit(*troop_type_opt);
    }
    return 1;
  }

  auto get_production_cost(const std::string &unit_type) const -> int {
    return get_production_cost(troop_typeFromString(unit_type));
  }

  auto get_production_cost(SpawnType spawn_type) const -> int {
    auto troop_type_opt = spawn_typeToTroopType(spawn_type);
    if (troop_type_opt) {
      return get_production_cost(*troop_type_opt);
    }
    return 50;
  }

  auto get_build_time(const std::string &unit_type) const -> float {
    return get_build_time(troop_typeFromString(unit_type));
  }

  auto get_build_time(SpawnType spawn_type) const -> float {
    auto troop_type_opt = spawn_typeToTroopType(spawn_type);
    if (troop_type_opt) {
      return get_build_time(*troop_type_opt);
    }
    return 5.0F;
  }

  auto get_max_units_per_row(const std::string &unit_type) const -> int {
    return get_max_units_per_row(troop_typeFromString(unit_type));
  }

  auto get_max_units_per_row(SpawnType spawn_type) const -> int {
    auto troop_type_opt = spawn_typeToTroopType(spawn_type);
    if (troop_type_opt) {
      return get_max_units_per_row(*troop_type_opt);
    }
    return 10;
  }

  auto get_selection_ring_size(const std::string &unit_type) const -> float {
    return get_selection_ring_size(troop_typeFromString(unit_type));
  }

  auto get_selection_ring_size(SpawnType spawn_type) const -> float {
    auto troop_type_opt = spawn_typeToTroopType(spawn_type);
    if (troop_type_opt) {
      return get_selection_ring_size(*troop_type_opt);
    }
    return 0.5F;
  }

  auto get_selection_ring_ground_offset(TroopType unit_type) const -> float {
    auto it = m_selection_ring_ground_offset.find(unit_type);
    if (it != m_selection_ring_ground_offset.end()) {
      return it->second;
    }

    if (unit_type == TroopType::MountedKnight ||
        unit_type == TroopType::HorseArcher ||
        unit_type == TroopType::HorseSpearman) {
      return 1.14F;
    }
    return 0.0F;
  }

  auto get_selection_ring_ground_offset(const std::string &unit_type) const
      -> float {
    return get_selection_ring_ground_offset(troop_typeFromString(unit_type));
  }

  auto get_selection_ring_ground_offset(SpawnType spawn_type) const -> float {
    auto troop_type_opt = spawn_typeToTroopType(spawn_type);
    if (troop_type_opt) {
      return get_selection_ring_ground_offset(*troop_type_opt);
    }
    return 0.0F;
  }

  void register_troop_type(TroopType unit_type, int individuals_per_unit) {
    m_individuals_per_unit[unit_type] = individuals_per_unit;
  }

  void register_max_units_per_row(TroopType unit_type, int max_units_per_row) {
    m_max_units_per_row[unit_type] = max_units_per_row;
  }

  void register_selection_ring_size(TroopType unit_type,
                                    float selection_ring_size) {
    m_selection_ring_size[unit_type] = selection_ring_size;
  }

  void register_selection_ring_ground_offset(TroopType unit_type,
                                             float offset) {
    m_selection_ring_ground_offset[unit_type] = offset;
  }

  void refresh_from_catalog() { reload_from_catalog(); }

private:
  TroopConfig() { reload_from_catalog(); }

  void reload_from_catalog() {
    m_individuals_per_unit.clear();
    m_production_cost.clear();
    m_build_time.clear();
    m_max_units_per_row.clear();
    m_selection_ring_size.clear();
    m_selection_ring_ground_offset.clear();

    const auto &classes = TroopCatalog::instance().get_all_classes();
    for (const auto &entry : classes) {
      const auto &troop_class = entry.second;
      auto type = troop_class.unit_type;
      m_individuals_per_unit[type] = troop_class.individuals_per_unit;
      m_production_cost[type] = troop_class.production.cost;
      m_build_time[type] = troop_class.production.build_time;
      m_max_units_per_row[type] = troop_class.max_units_per_row;
      m_selection_ring_size[type] = troop_class.visuals.selection_ring_size;
      m_selection_ring_ground_offset[type] =
          troop_class.visuals.selection_ring_ground_offset;
    }
  }

  std::unordered_map<TroopType, int> m_individuals_per_unit;
  std::unordered_map<TroopType, int> m_production_cost;
  std::unordered_map<TroopType, float> m_build_time;
  std::unordered_map<TroopType, int> m_max_units_per_row;
  std::unordered_map<TroopType, float> m_selection_ring_size;
  std::unordered_map<TroopType, float> m_selection_ring_ground_offset;
};

} // namespace Game::Units
