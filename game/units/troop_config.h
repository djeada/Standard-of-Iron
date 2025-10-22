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
    m_individualsPerUnit["archer"] = 20;
    m_maxUnitsPerRow["archer"] = 5;
    m_selectionRingSize["archer"] = 1.2f;

    m_individualsPerUnit["knight"] = 15;
    m_maxUnitsPerRow["knight"] = 5;
    m_selectionRingSize["knight"] = 1.1f;

    m_individualsPerUnit["spearman"] = 24;
    m_maxUnitsPerRow["spearman"] = 6;
    m_selectionRingSize["spearman"] = 1.4f;

    m_individualsPerUnit["mounted_knight"] = 10;
    m_maxUnitsPerRow["mounted_knight"] = 5;
    m_selectionRingSize["mounted_knight"] = 1.0f;
  }

  std::unordered_map<std::string, int> m_individualsPerUnit;
  std::unordered_map<std::string, int> m_maxUnitsPerRow;
  std::unordered_map<std::string, float> m_selectionRingSize;
};

} // namespace Units
} // namespace Game
