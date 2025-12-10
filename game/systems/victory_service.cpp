#include "victory_service.h"

#include "core/event_manager.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/owner_registry.h"
#include "units/spawn_type.h"
#include <QDebug>
#include <algorithm>
#include <qglobal.h>

namespace Game::Systems {

namespace {
constexpr float k_startup_delay_seconds = 0.35F;
}

VictoryService::VictoryService()
    : m_unit_died_subscription(
          [this](const Engine::Core::UnitDiedEvent &e) { on_unit_died(e); }),
      m_barrack_captured_subscription(
          [this](const Engine::Core::BarrackCapturedEvent &e) {
            on_barrack_captured(e);
          }),
      m_stats_registry(Game::Systems::GlobalStatsRegistry::instance()),
      m_owner_registry(Game::Systems::OwnerRegistry::instance()) {}

VictoryService::~VictoryService() = default;

void VictoryService::reset() {
  m_victoryState = "";
  m_elapsedTime = 0.0F;
  m_startupDelay = 0.0F;
  m_worldPtr = nullptr;
  m_victoryCallback = nullptr;
  m_keyStructures.clear();
  m_defeatConditions.clear();
}

void VictoryService::configure(const Game::Map::VictoryConfig &config,
                               int local_owner_id) {
  reset();

  m_localOwnerId = local_owner_id;

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

  m_startupDelay = k_startup_delay_seconds;
}

void VictoryService::update(Engine::Core::World &world, float delta_time) {
  if (!m_victoryState.isEmpty()) {
    return;
  }

  m_worldPtr = &world;

  if (m_startupDelay > 0.0F) {
    m_startupDelay = std::max(0.0F, m_startupDelay - delta_time);
    return;
  }

  if (m_victoryType == VictoryType::SurviveTime) {
    m_elapsedTime += delta_time;
  }

  checkVictoryConditions(world);
  if (!m_victoryState.isEmpty()) {
    return;
  }

  checkDefeatConditions(world);
}

void VictoryService::on_unit_died(const Engine::Core::UnitDiedEvent &event) {}

void VictoryService::on_barrack_captured(
    const Engine::Core::BarrackCapturedEvent &) {

  if ((m_worldPtr == nullptr) || !m_victoryState.isEmpty()) {
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

    const auto &all_owners = m_owner_registry.getAllOwners();
    for (const auto &owner : all_owners) {
      if (owner.type == Game::Systems::OwnerType::Player ||
          owner.type == Game::Systems::OwnerType::AI) {
        m_stats_registry.mark_game_end(owner.owner_id);
      }
    }

    const auto *stats = m_stats_registry.getStats(m_localOwnerId);
    if (stats != nullptr) {
      qInfo() << "Final Stats - Troops Recruited:" << stats->troops_recruited
              << "Enemies Killed:" << stats->enemies_killed
              << "Barracks Owned:" << stats->barracks_owned
              << "Play Time:" << stats->play_time_sec << "seconds";
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

      const auto &all_owners = m_owner_registry.getAllOwners();
      for (const auto &owner : all_owners) {
        if (owner.type == Game::Systems::OwnerType::Player ||
            owner.type == Game::Systems::OwnerType::AI) {
          m_stats_registry.mark_game_end(owner.owner_id);
        }
      }

      const auto *stats = m_stats_registry.getStats(m_localOwnerId);
      if (stats != nullptr) {
        qInfo() << "Final Stats - Troops Recruited:" << stats->troops_recruited
                << "Enemies Killed:" << stats->enemies_killed
                << "Barracks Owned:" << stats->barracks_owned
                << "Play Time:" << stats->play_time_sec << "seconds";
      }

      if (m_victoryCallback) {
        m_victoryCallback(m_victoryState);
      }
      return;
    }
  }
}

auto VictoryService::checkElimination(Engine::Core::World &world) -> bool {

  bool enemy_key_structures_alive = false;

  int const local_team = m_owner_registry.getOwnerTeam(m_localOwnerId);

  auto entities = world.getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *unit = e->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->health <= 0) {
      continue;
    }

    if (unit->owner_id == m_localOwnerId) {
      continue;
    }

    if (m_owner_registry.areAllies(m_localOwnerId, unit->owner_id)) {
      continue;
    }

    QString const unit_type_str = QString::fromStdString(
        Game::Units::spawn_typeToString(unit->spawn_type));
    if (std::find(m_keyStructures.begin(), m_keyStructures.end(),
                  unit_type_str) != m_keyStructures.end()) {
      enemy_key_structures_alive = true;
      break;
    }
  }

  return !enemy_key_structures_alive;
}

auto VictoryService::checkSurviveTime() const -> bool {
  return m_elapsedTime >= m_surviveTimeDuration;
}

auto VictoryService::checkNoUnits(Engine::Core::World &world) const -> bool {

  auto entities = world.getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *unit = e->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->health <= 0) {
      continue;
    }

    if (unit->owner_id == m_localOwnerId) {
      return false;
    }
  }

  return true;
}

auto VictoryService::checkNoKeyStructures(Engine::Core::World &world) -> bool {

  auto entities = world.getEntitiesWith<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *unit = e->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->health <= 0) {
      continue;
    }

    if (unit->owner_id == m_localOwnerId) {
      QString const unit_type_str = QString::fromStdString(
          Game::Units::spawn_typeToString(unit->spawn_type));
      if (std::find(m_keyStructures.begin(), m_keyStructures.end(),
                    unit_type_str) != m_keyStructures.end()) {
        return false;
      }
    }
  }

  return true;
}

} // namespace Game::Systems
