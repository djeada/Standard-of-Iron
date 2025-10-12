#include "victory_service.h"

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/systems/owner_registry.h"
#include <QDebug>

namespace Game {
namespace Systems {

VictoryService::VictoryService()
    : m_unitDiedSubscription([this](const Engine::Core::UnitDiedEvent &e) {
        onUnitDied(e);
      }) {}

VictoryService::~VictoryService() = default;

void VictoryService::configure(const Game::Map::VictoryConfig &config,
                                int localOwnerId) {
  m_localOwnerId = localOwnerId;
  m_victoryState = "";
  m_elapsedTime = 0.0f;

  // Parse victory type
  if (config.victoryType == "elimination") {
    m_victoryType = VictoryType::Elimination;
    m_keyStructures = config.keyStructures;
  } else if (config.victoryType == "survive_time") {
    m_victoryType = VictoryType::SurviveTime;
    m_surviveTimeDuration = config.surviveTimeDuration;
  } else {
    m_victoryType = VictoryType::Elimination;
    m_keyStructures = {"barracks"};
  }

  // Parse defeat conditions
  m_defeatConditions.clear();
  for (const auto &condition : config.defeatConditions) {
    if (condition == "no_units") {
      m_defeatConditions.push_back(DefeatCondition::NoUnits);
    } else if (condition == "no_key_structures") {
      m_defeatConditions.push_back(DefeatCondition::NoKeyStructures);
    }
  }

  // Default defeat condition if none specified
  if (m_defeatConditions.empty()) {
    m_defeatConditions.push_back(DefeatCondition::NoKeyStructures);
  }
}

void VictoryService::update(Engine::Core::World &world, float deltaTime) {
  if (!m_victoryState.isEmpty()) {
    return;
  }

  if (m_victoryType == VictoryType::SurviveTime) {
    m_elapsedTime += deltaTime;
  }

  checkVictoryConditions(world);
  if (!m_victoryState.isEmpty()) {
    return;
  }

  checkDefeatConditions(world);
}

void VictoryService::onUnitDied(const Engine::Core::UnitDiedEvent &event) {
  // Event is handled; actual victory/defeat checks happen in update()
  // This allows us to react to unit deaths if needed in the future
}

void VictoryService::checkVictoryConditions(Engine::Core::World &world) {
  bool victory = false;

  switch (m_victoryType) {
  case VictoryType::Elimination:
    victory = checkElimination(world);
    break;
  case VictoryType::SurviveTime:
    victory = checkSurviveTime();
    break;
  case VictoryType::Custom:
    // Future expansion point
    break;
  }

  if (victory) {
    m_victoryState = "victory";
    qInfo() << "VICTORY! Conditions met.";
    if (m_victoryCallback) {
      m_victoryCallback(m_victoryState);
    }
  }
}

void VictoryService::checkDefeatConditions(Engine::Core::World &world) {
  for (const auto &condition : m_defeatConditions) {
    bool defeat = false;

    switch (condition) {
    case DefeatCondition::NoUnits:
      defeat = checkNoUnits(world);
      break;
    case DefeatCondition::NoKeyStructures:
      defeat = checkNoKeyStructures(world);
      break;
    case DefeatCondition::TimeExpired:
      // Future expansion
      break;
    }

    if (defeat) {
      m_victoryState = "defeat";
      qInfo() << "DEFEAT! Condition met.";
      if (m_victoryCallback) {
        m_victoryCallback(m_victoryState);
      }
      return;
    }
  }
}

bool VictoryService::checkElimination(Engine::Core::World &world) {
  // Check if all enemy key structures are destroyed
  bool enemyKeyStructuresAlive = false;

  auto entities = world.getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *unit = e->getComponent<Engine::Core::UnitComponent>();
    if (!unit || unit->health <= 0)
      continue;

    // Check if this is an enemy key structure
    if (OwnerRegistry::instance().isAI(unit->ownerId)) {
      QString unitType = QString::fromStdString(unit->unitType);
      if (m_keyStructures.contains(unitType)) {
        enemyKeyStructuresAlive = true;
        break;
      }
    }
  }

  return !enemyKeyStructuresAlive;
}

bool VictoryService::checkSurviveTime() {
  return m_elapsedTime >= m_surviveTimeDuration;
}

bool VictoryService::checkNoUnits(Engine::Core::World &world) {
  // Check if player has no units left
  auto entities = world.getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *unit = e->getComponent<Engine::Core::UnitComponent>();
    if (!unit || unit->health <= 0)
      continue;

    if (unit->ownerId == m_localOwnerId) {
      return false; // Player has at least one unit
    }
  }

  return true; // No player units alive
}

bool VictoryService::checkNoKeyStructures(Engine::Core::World &world) {
  // Check if player has no key structures left
  auto entities = world.getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *unit = e->getComponent<Engine::Core::UnitComponent>();
    if (!unit || unit->health <= 0)
      continue;

    if (unit->ownerId == m_localOwnerId) {
      QString unitType = QString::fromStdString(unit->unitType);
      if (m_keyStructures.contains(unitType)) {
        return false; // Player still has a key structure
      }
    }
  }

  return true; // No player key structures alive
}

} // namespace Systems
} // namespace Game
