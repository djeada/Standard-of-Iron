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

  void registerTroopType(const std::string &unitType, int individualsPerUnit) {
    m_individualsPerUnit[unitType] = individualsPerUnit;
  }

  void registerMaxUnitsPerRow(const std::string &unitType, int maxUnitsPerRow) {
    m_maxUnitsPerRow[unitType] = maxUnitsPerRow;
  }

private:
  TroopConfig() {

    m_individualsPerUnit["archer"] = 30;
    m_maxUnitsPerRow["archer"] = 8;
  }

  std::unordered_map<std::string, int> m_individualsPerUnit;
  std::unordered_map<std::string, int> m_maxUnitsPerRow;
};

} // namespace Units
} // namespace Game
