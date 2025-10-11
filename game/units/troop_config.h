#pragma once

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

  int getIndividualsPerUnit(const std::string &unitType) const {
    auto it = m_individualsPerUnit.find(unitType);
    if (it != m_individualsPerUnit.end()) {
      return it->second;
    }
    return 1;
  }

  int getMaxUnitsPerRow(const std::string &unitType) const {
    auto it = m_maxUnitsPerRow.find(unitType);
    if (it != m_maxUnitsPerRow.end()) {
      return it->second;
    }
    return 10;
  }

  float getSelectionRingSize(const std::string &unitType) const {
    auto it = m_selectionRingSize.find(unitType);
    if (it != m_selectionRingSize.end()) {
      return it->second;
    }
    return 0.5f;
  }

  void registerTroopType(const std::string &unitType, int individualsPerUnit) {
    m_individualsPerUnit[unitType] = individualsPerUnit;
  }

  void registerMaxUnitsPerRow(const std::string &unitType, int maxUnitsPerRow) {
    m_maxUnitsPerRow[unitType] = maxUnitsPerRow;
  }

  void registerSelectionRingSize(const std::string &unitType,
                                  float selectionRingSize) {
    m_selectionRingSize[unitType] = selectionRingSize;
  }

private:
  TroopConfig() {
    m_individualsPerUnit["archer"] = 30;
    m_maxUnitsPerRow["archer"] = 6;
    m_selectionRingSize["archer"] = 1.5f;
  }

  std::unordered_map<std::string, int> m_individualsPerUnit;
  std::unordered_map<std::string, int> m_maxUnitsPerRow;
  std::unordered_map<std::string, float> m_selectionRingSize;
};

} // namespace Units
} // namespace Game
