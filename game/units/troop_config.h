#pragma once

#include "spawn_type.h"
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

private:
  TroopConfig() {
    m_individuals_per_unit[TroopType::Archer] = 20;
    m_maxUnitsPerRow[TroopType::Archer] = 5;
    m_selectionRingSize[TroopType::Archer] = 1.2F;
    m_selectionRingYOffset[TroopType::Archer] = 0.0F;
    m_selectionRingGroundOffset[TroopType::Archer] = 0.0F;

    m_individuals_per_unit[TroopType::Knight] = 15;
    m_maxUnitsPerRow[TroopType::Knight] = 5;
    m_selectionRingSize[TroopType::Knight] = 1.1F;
    m_selectionRingYOffset[TroopType::Knight] = 0.0F;
    m_selectionRingGroundOffset[TroopType::Knight] = 0.0F;

    m_individuals_per_unit[TroopType::Spearman] = 24;
    m_maxUnitsPerRow[TroopType::Spearman] = 6;
    m_selectionRingSize[TroopType::Spearman] = 1.4F;
    m_selectionRingYOffset[TroopType::Spearman] = 0.0F;
    m_selectionRingGroundOffset[TroopType::Spearman] = 0.0F;

    m_individuals_per_unit[TroopType::MountedKnight] = 9;
    m_maxUnitsPerRow[TroopType::MountedKnight] = 3;
    m_selectionRingSize[TroopType::MountedKnight] = 2.0F;
    m_selectionRingYOffset[TroopType::MountedKnight] = 0.0F;
    m_selectionRingGroundOffset[TroopType::MountedKnight] = 1.35F;
  }

  std::unordered_map<TroopType, int> m_individuals_per_unit;
  std::unordered_map<TroopType, int> m_maxUnitsPerRow;
  std::unordered_map<TroopType, float> m_selectionRingSize;
  std::unordered_map<TroopType, float> m_selectionRingYOffset;
  std::unordered_map<TroopType, float> m_selectionRingGroundOffset;
};

} // namespace Game::Units
