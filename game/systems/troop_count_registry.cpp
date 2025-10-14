#include "troop_count_registry.h"
#include "../core/component.h"
#include "../core/world.h"
#include "../units/troop_config.h"

namespace Game::Systems {

TroopCountRegistry &TroopCountRegistry::instance() {
  static TroopCountRegistry inst;
  return inst;
}

void TroopCountRegistry::initialize() {
  m_unitSpawnedSubscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitSpawnedEvent>(
          [this](const Engine::Core::UnitSpawnedEvent &e) {
            onUnitSpawned(e);
          });

  m_unitDiedSubscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>(
          [this](const Engine::Core::UnitDiedEvent &e) { onUnitDied(e); });
}

void TroopCountRegistry::clear() { m_troopCounts.clear(); }

int TroopCountRegistry::getTroopCount(int ownerId) const {
  auto it = m_troopCounts.find(ownerId);
  if (it != m_troopCounts.end()) {
    return it->second;
  }
  return 0;
}

void TroopCountRegistry::onUnitSpawned(
    const Engine::Core::UnitSpawnedEvent &event) {
  if (event.unitType == "barracks")
    return;

  int individualsPerUnit =
      Game::Units::TroopConfig::instance().getIndividualsPerUnit(
          event.unitType);
  m_troopCounts[event.ownerId] += individualsPerUnit;
}

void TroopCountRegistry::onUnitDied(
    const Engine::Core::UnitDiedEvent &event) {
  if (event.unitType == "barracks")
    return;

  int individualsPerUnit =
      Game::Units::TroopConfig::instance().getIndividualsPerUnit(
          event.unitType);
  m_troopCounts[event.ownerId] -= individualsPerUnit;
  if (m_troopCounts[event.ownerId] < 0) {
    m_troopCounts[event.ownerId] = 0;
  }
}

void TroopCountRegistry::rebuildFromWorld(Engine::Core::World &world) {
  m_troopCounts.clear();

  auto entities = world.getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *unit = e->getComponent<Engine::Core::UnitComponent>();
    if (!unit || unit->health <= 0)
      continue;

    if (unit->unitType == "barracks")
      continue;

    int individualsPerUnit =
        Game::Units::TroopConfig::instance().getIndividualsPerUnit(
            unit->unitType);
    m_troopCounts[unit->ownerId] += individualsPerUnit;
  }
}

} // namespace Game::Systems
