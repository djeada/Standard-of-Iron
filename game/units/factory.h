#pragma once

#include "../units/troop_type.h"
#include "unit.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace Game {
namespace Units {

class UnitFactoryRegistry {
public:
  using Factory = std::function<std::unique_ptr<Unit>(Engine::Core::World &,
                                                      const SpawnParams &)>;

  void registerFactory(TroopType type, Factory f) {
    m_enumMap[type] = std::move(f);
  }

  void registerFactory(const std::string &type, Factory f) {
    m_stringMap[type] = std::move(f);
  }

  std::unique_ptr<Unit> create(TroopType type, Engine::Core::World &world,
                               const SpawnParams &params) const {
    auto it = m_enumMap.find(type);
    if (it == m_enumMap.end())
      return nullptr;
    return it->second(world, params);
  }

  std::unique_ptr<Unit> create(const std::string &type,
                               Engine::Core::World &world,
                               const SpawnParams &params) const {
    auto it = m_stringMap.find(type);
    if (it == m_stringMap.end())
      return nullptr;
    return it->second(world, params);
  }

private:
  std::unordered_map<TroopType, Factory> m_enumMap;
  std::unordered_map<std::string, Factory> m_stringMap;
};

void registerBuiltInUnits(UnitFactoryRegistry &reg);

} // namespace Units
} // namespace Game
