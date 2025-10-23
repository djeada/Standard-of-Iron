#pragma once

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

  int getIndividualsPerUnit(const std::string &unitType) const {
    return getIndividualsPerUnit(troopTypeFromString(unitType));
  }

  int getMaxUnitsPerRow(const std::string &unitType) const {
    return getMaxUnitsPerRow(troopTypeFromString(unitType));
  }

  float getSelectionRingSize(const std::string &unitType) const {
    return getSelectionRingSize(troopTypeFromString(unitType));
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

private:
  TroopConfig() {
    m_individualsPerUnit[TroopType::Archer] = 20;
    m_maxUnitsPerRow[TroopType::Archer] = 5;
    m_selectionRingSize[TroopType::Archer] = 1.2f;

    m_individualsPerUnit[TroopType::Knight] = 15;
    m_maxUnitsPerRow[TroopType::Knight] = 5;
    m_selectionRingSize[TroopType::Knight] = 1.1f;

    m_individualsPerUnit[TroopType::Spearman] = 24;
    m_maxUnitsPerRow[TroopType::Spearman] = 6;
    m_selectionRingSize[TroopType::Spearman] = 1.4f;

    m_individualsPerUnit[TroopType::MountedKnight] = 9;
    m_maxUnitsPerRow[TroopType::MountedKnight] = 3;
    m_selectionRingSize[TroopType::MountedKnight] = 2.0f;
  }

  std::unordered_map<TroopType, int> m_individualsPerUnit;
  std::unordered_map<TroopType, int> m_maxUnitsPerRow;
  std::unordered_map<TroopType, float> m_selectionRingSize;
};

} // namespace Units
} // namespace Game
