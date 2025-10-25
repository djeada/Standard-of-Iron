#include "victory_service.h"

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/owner_registry.h"
#include <QDebug>
#include <algorithm>

namespace Game {
namespace Systems {

VictoryService::VictoryService()
    : m_unitDiedSubscription(
          [this](const Engine::Core::UnitDiedEvent &e) { onUnitDied(e); }),
      m_barrackCapturedSubscription(
          [this](const Engine::Core::BarrackCapturedEvent &e) {
            onBarrackCaptured(e);
          }),
      m_statsRegistry(Game::Systems::GlobalStatsRegistry::instance()),
      m_ownerRegistry(Game::Systems::OwnerRegistry::instance()) {}

VictoryService::~VictoryService() = default;

void VictoryService::reset() {
  m_victoryState = "";
  m_elapsedTime = 0.0f;
  m_worldPtr = nullptr;
  m_victoryCallback = nullptr;
}

void VictoryService::configure(const Game::Map::VictoryConfig &config,
                               int localOwnerId) {
  reset();

  m_localOwnerId = localOwnerId;

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

  m_defeatConditions.clear();
  for (const auto &condition : config.defeatConditions) {
    if (condition == "no_units") {
      m_defeatConditions.push_back(DefeatCondition::NoUnits);
    } else if (condition == "no_key_structures") {
      m_defeatConditions.push_back(DefeatCondition::NoKeyStructures);
    }
  }

  if (m_defeatConditions.empty()) {
    m_defeatConditions.push_back(DefeatCondition::NoKeyStructures);
  }
}

void VictoryService::update(Engine::Core::World &world, float deltaTime) {
  if (!m_victoryState.isEmpty()) {
    return;
  }

  m_worldPtr = &world;

  if (m_victoryType == VictoryType::SurviveTime) {
    m_elapsedTime += deltaTime;
  }

  checkVictoryConditions(world);
  if (!m_victoryState.isEmpty()) {
    return;
  }

  checkDefeatConditions(world);
}

void VictoryService::onUnitDied(const Engine::Core::UnitDiedEvent &event) {}

void VictoryService::onBarrackCaptured(
    const Engine::Core::BarrackCapturedEvent &event) {

  if (!m_worldPtr || !m_victoryState.isEmpty()) {
    return;
  }

  checkVictoryConditions(*m_worldPtr);
  if (!m_victoryState.isEmpty()) {
    return;
  }

  checkDefeatConditions(*m_worldPtr);
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

    break;
  }

  if (victory) {
    m_victoryState = "victory";
    qInfo() << "VICTORY! Conditions met.";

    const auto &allOwners = m_ownerRegistry.getAllOwners();
    for (const auto &owner : allOwners) {
      if (owner.type == Game::Systems::OwnerType::Player ||
          owner.type == Game::Systems::OwnerType::AI) {
        m_statsRegistry.markGameEnd(owner.ownerId);
      }
    }

    const auto *stats = m_statsRegistry.getStats(m_localOwnerId);
    if (stats) {
      qInfo() << "Final Stats - Troops Recruited:" << stats->troopsRecruited
              << "Enemies Killed:" << stats->enemiesKilled
              << "Barracks Owned:" << stats->barracksOwned
              << "Play Time:" << stats->playTimeSec << "seconds";
    }

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

      break;
    }

    if (defeat) {
      m_victoryState = "defeat";
      qInfo() << "DEFEAT! Condition met.";

      const auto &allOwners = m_ownerRegistry.getAllOwners();
      for (const auto &owner : allOwners) {
        if (owner.type == Game::Systems::OwnerType::Player ||
            owner.type == Game::Systems::OwnerType::AI) {
          m_statsRegistry.markGameEnd(owner.ownerId);
        }
      }

      const auto *stats = m_statsRegistry.getStats(m_localOwnerId);
      if (stats) {
        qInfo() << "Final Stats - Troops Recruited:" << stats->troopsRecruited
                << "Enemies Killed:" << stats->enemiesKilled
                << "Barracks Owned:" << stats->barracksOwned
                << "Play Time:" << stats->playTimeSec << "seconds";
      }

      if (m_victoryCallback) {
        m_victoryCallback(m_victoryState);
      }
      return;
    }
  }
}

bool VictoryService::checkElimination(Engine::Core::World &world) {

  bool enemyKeyStructuresAlive = false;

  int localTeam = m_ownerRegistry.getOwnerTeam(m_localOwnerId);

  auto entities = world.getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *unit = e->getComponent<Engine::Core::UnitComponent>();
    if (!unit || unit->health <= 0) {
      continue;
    }

    if (unit->ownerId == m_localOwnerId) {
      continue;
    }

    if (m_ownerRegistry.areAllies(m_localOwnerId, unit->ownerId)) {
      continue;
    }

    QString unitTypeStr =
        QString::fromStdString(Game::Units::spawnTypeToString(unit->spawnType));
    if (std::find(m_keyStructures.begin(), m_keyStructures.end(),
                  unitTypeStr) != m_keyStructures.end()) {
      enemyKeyStructuresAlive = true;
      break;
    }
  }

  return !enemyKeyStructuresAlive;
}

bool VictoryService::checkSurviveTime() {
  return m_elapsedTime >= m_surviveTimeDuration;
}

bool VictoryService::checkNoUnits(Engine::Core::World &world) {

  auto entities = world.getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *unit = e->getComponent<Engine::Core::UnitComponent>();
    if (!unit || unit->health <= 0) {
      continue;
    }

    if (unit->ownerId == m_localOwnerId) {
      return false;
    }
  }

  return true;
}

bool VictoryService::checkNoKeyStructures(Engine::Core::World &world) {

  auto entities = world.getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *unit = e->getComponent<Engine::Core::UnitComponent>();
    if (!unit || unit->health <= 0) {
      continue;
    }

    if (unit->ownerId == m_localOwnerId) {
      QString unitTypeStr = QString::fromStdString(
          Game::Units::spawnTypeToString(unit->spawnType));
      if (std::find(m_keyStructures.begin(), m_keyStructures.end(),
                    unitTypeStr) != m_keyStructures.end()) {
        return false;
      }
    }
  }

  return true;
}

} // namespace Systems
} // namespace Game
