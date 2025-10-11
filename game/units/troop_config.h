#pragma once

#include <string>
#include <unordered_map>

namespace Game {
namespace Units {

// Global configuration for troop types
// Maps each unit type to the number of individuals it represents
class TroopConfig {
public:
  static TroopConfig &instance() {
    static TroopConfig inst;
    return inst;
  }

  // Get the number of individuals per unit for a given troop type
  // Returns 1 if the type is not found (default for buildings, etc.)
  int getIndividualsPerUnit(const std::string &unitType) const {
    auto it = m_individualsPerUnit.find(unitType);
    if (it != m_individualsPerUnit.end()) {
      return it->second;
    }
    return 1; // Default: 1 individual per unit
  }

  // Get the maximum number of units per row for a given troop type
  // Returns 10 if the type is not found (default)
  int getMaxUnitsPerRow(const std::string &unitType) const {
    auto it = m_maxUnitsPerRow.find(unitType);
    if (it != m_maxUnitsPerRow.end()) {
      return it->second;
    }
    return 10; // Default: 10 units per row
  }

  // Register a troop type with its individual count
  void registerTroopType(const std::string &unitType, int individualsPerUnit) {
    m_individualsPerUnit[unitType] = individualsPerUnit;
  }

  // Register max units per row for a troop type
  void registerMaxUnitsPerRow(const std::string &unitType, int maxUnitsPerRow) {
    m_maxUnitsPerRow[unitType] = maxUnitsPerRow;
  }

private:
  TroopConfig() {
    // Initialize default troop types
    // Archers represent 10 individuals (2 rows x 5 cols)
    m_individualsPerUnit["archer"] = 30;
    m_maxUnitsPerRow["archer"] = 8;

    // Buildings and other non-troop units default to 1 (handled by default
    // return in getIndividualsPerUnit)
  }

  std::unordered_map<std::string, int> m_individualsPerUnit;
  std::unordered_map<std::string, int> m_maxUnitsPerRow;
};

} // namespace Units
} // namespace Game
