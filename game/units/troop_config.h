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

  auto getIndividualsPerUnit(TroopType unit_type) const -> int {
    auto it = m_individuals_per_unit.find(unit_type);
    if (it != m_individuals_per_unit.end()) {
      return it->second;
    }
    return 1;
  }

  auto getProductionCost(TroopType unit_type) const -> int {
    auto it = m_productionCost.find(unit_type);
    if (it != m_productionCost.end()) {
      return it->second;
    }
    return 50;
  }

  auto getBuildTime(TroopType unit_type) const -> float {
    auto it = m_buildTime.find(unit_type);
    if (it != m_buildTime.end()) {
      return it->second;
    }
    return 5.0F;
  }

  auto getMaxUnitsPerRow(TroopType unit_type) const -> int {
    auto it = m_maxUnitsPerRow.find(unit_type);
    if (it != m_maxUnitsPerRow.end()) {
      return it->second;
    }
    return 10;
  }

  auto getSelectionRingSize(TroopType unit_type) const -> float {
    auto it = m_selectionRingSize.find(unit_type);
    if (it != m_selectionRingSize.end()) {
      return it->second;
    }
    return 0.5F;
  }

  auto getSelectionRingYOffset(TroopType unit_type) const -> float {
    auto it = m_selectionRingYOffset.find(unit_type);
    if (it != m_selectionRingYOffset.end()) {
      return it->second;
    }
    return 0.0F;
  }

  auto getIndividualsPerUnit(const std::string &unit_type) const -> int {
    return getIndividualsPerUnit(troop_typeFromString(unit_type));
  }

  auto getIndividualsPerUnit(SpawnType spawn_type) const -> int {
    auto troop_type_opt = spawn_typeToTroopType(spawn_type);
    if (troop_type_opt) {
      return getIndividualsPerUnit(*troop_type_opt);
    }
    return 1;
  }

  auto getProductionCost(const std::string &unit_type) const -> int {
    return getProductionCost(troop_typeFromString(unit_type));
  }

  auto getProductionCost(SpawnType spawn_type) const -> int {
    auto troop_type_opt = spawn_typeToTroopType(spawn_type);
    if (troop_type_opt) {
      return getProductionCost(*troop_type_opt);
    }
    return 50;
  }

  auto getBuildTime(const std::string &unit_type) const -> float {
    return getBuildTime(troop_typeFromString(unit_type));
  }

  auto getBuildTime(SpawnType spawn_type) const -> float {
    auto troop_type_opt = spawn_typeToTroopType(spawn_type);
    if (troop_type_opt) {
      return getBuildTime(*troop_type_opt);
    }
    return 5.0F;
  }

  auto getMaxUnitsPerRow(const std::string &unit_type) const -> int {
    return getMaxUnitsPerRow(troop_typeFromString(unit_type));
  }

  auto getMaxUnitsPerRow(SpawnType spawn_type) const -> int {
    auto troop_type_opt = spawn_typeToTroopType(spawn_type);
    if (troop_type_opt) {
      return getMaxUnitsPerRow(*troop_type_opt);
    }
    return 10;
  }

  auto getSelectionRingSize(const std::string &unit_type) const -> float {
    return getSelectionRingSize(troop_typeFromString(unit_type));
  }

  auto getSelectionRingSize(SpawnType spawn_type) const -> float {
    auto troop_type_opt = spawn_typeToTroopType(spawn_type);
    if (troop_type_opt) {
      return getSelectionRingSize(*troop_type_opt);
    }
    return 0.5F;
  }

  auto getSelectionRingYOffset(const std::string &unit_type) const -> float {
    return getSelectionRingYOffset(troop_typeFromString(unit_type));
  }

  auto getSelectionRingYOffset(SpawnType spawn_type) const -> float {
    auto troop_type_opt = spawn_typeToTroopType(spawn_type);
    if (troop_type_opt) {
      return getSelectionRingYOffset(*troop_type_opt);
    }
    return 0.0F;
  }

  auto getSelectionRingGroundOffset(TroopType unit_type) const -> float {
    auto it = m_selectionRingGroundOffset.find(unit_type);
    if (it != m_selectionRingGroundOffset.end()) {
      return it->second;
    }

    if (unit_type == TroopType::MountedKnight ||
        unit_type == TroopType::HorseArcher ||
        unit_type == TroopType::HorseSpearman) {
      return 1.14F;
    }
    return 0.0F;
  }

  auto
  getSelectionRingGroundOffset(const std::string &unit_type) const -> float {
    return getSelectionRingGroundOffset(troop_typeFromString(unit_type));
  }

  auto getSelectionRingGroundOffset(SpawnType spawn_type) const -> float {
    auto troop_type_opt = spawn_typeToTroopType(spawn_type);
    if (troop_type_opt) {
      return getSelectionRingGroundOffset(*troop_type_opt);
    }
    return 0.0F;
  }

  void registerTroopType(TroopType unit_type, int individuals_per_unit) {
    m_individuals_per_unit[unit_type] = individuals_per_unit;
  }

  void registerMaxUnitsPerRow(TroopType unit_type, int maxUnitsPerRow) {
    m_maxUnitsPerRow[unit_type] = maxUnitsPerRow;
  }

  void registerSelectionRingSize(TroopType unit_type, float selectionRingSize) {
    m_selectionRingSize[unit_type] = selectionRingSize;
  }

  void registerSelectionRingYOffset(TroopType unit_type, float offset) {
    m_selectionRingYOffset[unit_type] = offset;
  }

  void registerSelectionRingGroundOffset(TroopType unit_type, float offset) {
    m_selectionRingGroundOffset[unit_type] = offset;
  }

  void refresh_from_catalog() { reload_from_catalog(); }

private:
  TroopConfig() { reload_from_catalog(); }

  void reload_from_catalog() {
    m_individuals_per_unit.clear();
    m_productionCost.clear();
    m_buildTime.clear();
    m_maxUnitsPerRow.clear();
    m_selectionRingSize.clear();
    m_selectionRingYOffset.clear();
    m_selectionRingGroundOffset.clear();

    const auto &classes = TroopCatalog::instance().get_all_classes();
    for (const auto &entry : classes) {
      const auto &troop_class = entry.second;
      auto type = troop_class.unit_type;
      m_individuals_per_unit[type] = troop_class.individuals_per_unit;
      m_productionCost[type] = troop_class.production.cost;
      m_buildTime[type] = troop_class.production.build_time;
      m_maxUnitsPerRow[type] = troop_class.max_units_per_row;
      m_selectionRingSize[type] = troop_class.visuals.selection_ring_size;
      m_selectionRingYOffset[type] =
          troop_class.visuals.selection_ring_y_offset;
      m_selectionRingGroundOffset[type] =
          troop_class.visuals.selection_ring_ground_offset;
    }
  }

  std::unordered_map<TroopType, int> m_individuals_per_unit;
  std::unordered_map<TroopType, int> m_productionCost;
  std::unordered_map<TroopType, float> m_buildTime;
  std::unordered_map<TroopType, int> m_maxUnitsPerRow;
  std::unordered_map<TroopType, float> m_selectionRingSize;
  std::unordered_map<TroopType, float> m_selectionRingYOffset;
  std::unordered_map<TroopType, float> m_selectionRingGroundOffset;
};

} // namespace Game::Units
