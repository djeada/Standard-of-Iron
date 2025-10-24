#pragma once

#include "spawn_type.h"
#include "troop_type.h"
#include <string>
#include <unordered_map>

namespace Game {
namespace Units {

class TroopConfig {
public:
  static TroopConfig &instance() {
    static TroopConfig inst;
    return inst;
  }

  int getIndividualsPerUnit(TroopType unitType) const {
    auto it = m_individualsPerUnit.find(unitType);
    if (it != m_individualsPerUnit.end()) {
      return it->second;
    }
    return 1;
  }

  int getMaxUnitsPerRow(TroopType unitType) const {
    auto it = m_maxUnitsPerRow.find(unitType);
    if (it != m_maxUnitsPerRow.end()) {
      return it->second;
    }
    return 10;
  }

  float getSelectionRingSize(TroopType unitType) const {
    auto it = m_selectionRingSize.find(unitType);
    if (it != m_selectionRingSize.end()) {
      return it->second;
    }
    return 0.5f;
  }

  float getSelectionRingYOffset(TroopType unitType) const {
    auto it = m_selectionRingYOffset.find(unitType);
    if (it != m_selectionRingYOffset.end()) {
      return it->second;
    }
    return 0.0f;
  }

  int getIndividualsPerUnit(const std::string &unitType) const {
    return getIndividualsPerUnit(troopTypeFromString(unitType));
  }

  int getIndividualsPerUnit(SpawnType spawnType) const {
    auto troopTypeOpt = spawnTypeToTroopType(spawnType);
    if (troopTypeOpt) {
      return getIndividualsPerUnit(*troopTypeOpt);
    }
    return 1;
  }

  int getMaxUnitsPerRow(const std::string &unitType) const {
    return getMaxUnitsPerRow(troopTypeFromString(unitType));
  }

  int getMaxUnitsPerRow(SpawnType spawnType) const {
    auto troopTypeOpt = spawnTypeToTroopType(spawnType);
    if (troopTypeOpt) {
      return getMaxUnitsPerRow(*troopTypeOpt);
    }
    return 10;
  }

  float getSelectionRingSize(const std::string &unitType) const {
    return getSelectionRingSize(troopTypeFromString(unitType));
  }

  float getSelectionRingSize(SpawnType spawnType) const {
    auto troopTypeOpt = spawnTypeToTroopType(spawnType);
    if (troopTypeOpt) {
      return getSelectionRingSize(*troopTypeOpt);
    }
    return 0.5f;
  }

  float getSelectionRingYOffset(const std::string &unitType) const {
    return getSelectionRingYOffset(troopTypeFromString(unitType));
  }

  float getSelectionRingYOffset(SpawnType spawnType) const {
    auto troopTypeOpt = spawnTypeToTroopType(spawnType);
    if (troopTypeOpt) {
      return getSelectionRingYOffset(*troopTypeOpt);
    }
    return 0.0f;
  }

  float getSelectionRingGroundOffset(TroopType unitType) const {
    auto it = m_selectionRingGroundOffset.find(unitType);
    if (it != m_selectionRingGroundOffset.end()) {
      return it->second;
    }
    return 0.0f;
  }

  float getSelectionRingGroundOffset(const std::string &unitType) const {
    return getSelectionRingGroundOffset(troopTypeFromString(unitType));
  }

  float getSelectionRingGroundOffset(SpawnType spawnType) const {
    auto troopTypeOpt = spawnTypeToTroopType(spawnType);
    if (troopTypeOpt) {
      return getSelectionRingGroundOffset(*troopTypeOpt);
    }
    return 0.0f;
  }

  void registerTroopType(TroopType unitType, int individualsPerUnit) {
    m_individualsPerUnit[unitType] = individualsPerUnit;
  }

  void registerMaxUnitsPerRow(TroopType unitType, int maxUnitsPerRow) {
    m_maxUnitsPerRow[unitType] = maxUnitsPerRow;
  }

  void registerSelectionRingSize(TroopType unitType, float selectionRingSize) {
    m_selectionRingSize[unitType] = selectionRingSize;
  }

  void registerSelectionRingYOffset(TroopType unitType, float offset) {
    m_selectionRingYOffset[unitType] = offset;
  }

  void registerSelectionRingGroundOffset(TroopType unitType, float offset) {
    m_selectionRingGroundOffset[unitType] = offset;
  }

private:
  TroopConfig() {
    m_individualsPerUnit[TroopType::Archer] = 20;
    m_maxUnitsPerRow[TroopType::Archer] = 5;
    m_selectionRingSize[TroopType::Archer] = 1.2f;
    m_selectionRingYOffset[TroopType::Archer] = 0.0f;
    m_selectionRingGroundOffset[TroopType::Archer] = 0.0f;

    m_individualsPerUnit[TroopType::Knight] = 15;
    m_maxUnitsPerRow[TroopType::Knight] = 5;
    m_selectionRingSize[TroopType::Knight] = 1.1f;
    m_selectionRingYOffset[TroopType::Knight] = 0.0f;
    m_selectionRingGroundOffset[TroopType::Knight] = 0.0f;

    m_individualsPerUnit[TroopType::Spearman] = 24;
    m_maxUnitsPerRow[TroopType::Spearman] = 6;
    m_selectionRingSize[TroopType::Spearman] = 1.4f;
    m_selectionRingYOffset[TroopType::Spearman] = 0.0f;
    m_selectionRingGroundOffset[TroopType::Spearman] = 0.0f;

    m_individualsPerUnit[TroopType::MountedKnight] = 9;
    m_maxUnitsPerRow[TroopType::MountedKnight] = 3;
    m_selectionRingSize[TroopType::MountedKnight] = 2.0f;
    m_selectionRingYOffset[TroopType::MountedKnight] = 0.0f;
    m_selectionRingGroundOffset[TroopType::MountedKnight] = 1.35f;
  }

  std::unordered_map<TroopType, int> m_individualsPerUnit;
  std::unordered_map<TroopType, int> m_maxUnitsPerRow;
  std::unordered_map<TroopType, float> m_selectionRingSize;
  std::unordered_map<TroopType, float> m_selectionRingYOffset;
  std::unordered_map<TroopType, float> m_selectionRingGroundOffset;
};

} // namespace Units
} // namespace Game
