#include "victory_service.h"

#include "core/event_manager.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/nation_registry.h"
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
  m_victory_state = "";
  m_elapsed_time = 0.0F;
  m_startup_delay = 0.0F;
  m_world_ptr = nullptr;
  m_victory_callback = nullptr;
  m_key_structures.clear();
  m_defeat_conditions.clear();
  m_required_key_structures = 0;
}

void VictoryService::configure(const Game::Map::VictoryConfig &config,
                               int local_owner_id) {
  reset();

  m_local_owner_id = local_owner_id;

  if (config.victoryType == "elimination") {
    m_victory_type = VictoryType::Elimination;
    m_key_structures = config.keyStructures;
  } else if (config.victoryType == "control_structures") {
    m_victory_type = VictoryType::ControlStructures;
    m_key_structures = config.keyStructures;
    m_required_key_structures = config.requiredKeyStructures;
  } else if (config.victoryType == "capture_structures") {
    m_victory_type = VictoryType::CaptureStructures;
    m_key_structures = config.keyStructures;
    m_required_key_structures = config.requiredKeyStructures;
  } else if (config.victoryType == "survive_time") {
    m_victory_type = VictoryType::SurviveTime;
    m_survive_time_duration = config.surviveTimeDuration;
  } else {
    m_victory_type = VictoryType::Elimination;
    m_key_structures = {"barracks"};
  }

  m_defeat_conditions.clear();
  for (const auto &condition : config.defeatConditions) {
    if (condition == "no_units") {
      m_defeat_conditions.push_back(DefeatCondition::NoUnits);
    } else if (condition == "no_key_structures") {
      m_defeat_conditions.push_back(DefeatCondition::NoKeyStructures);
    }
  }

  if (m_defeat_conditions.empty()) {
    m_defeat_conditions.push_back(DefeatCondition::NoKeyStructures);
  }

  m_startup_delay = k_startup_delay_seconds;
}

void VictoryService::update(Engine::Core::World &world, float delta_time) {
  if (!m_victory_state.isEmpty()) {
    return;
  }

  m_world_ptr = &world;

  if (m_startup_delay > 0.0F) {
    m_startup_delay = std::max(0.0F, m_startup_delay - delta_time);
    return;
  }

  if (m_victory_type == VictoryType::SurviveTime) {
    m_elapsed_time += delta_time;
  }

  check_victory_conditions(world);
  if (!m_victory_state.isEmpty()) {
    return;
  }

  check_defeat_conditions(world);
}

void VictoryService::on_unit_died(const Engine::Core::UnitDiedEvent &event) {}

void VictoryService::on_barrack_captured(
    const Engine::Core::BarrackCapturedEvent &) {

  if ((m_world_ptr == nullptr) || !m_victory_state.isEmpty()) {
    return;
  }

  check_victory_conditions(*m_world_ptr);
  if (!m_victory_state.isEmpty()) {
    return;
  }

  check_defeat_conditions(*m_world_ptr);
}

void VictoryService::check_victory_conditions(Engine::Core::World &world) {
  bool victory = false;

  switch (m_victory_type) {
  case VictoryType::Elimination:
    victory = check_elimination(world);
    break;
  case VictoryType::SurviveTime:
    victory = check_survive_time();
    break;
  case VictoryType::ControlStructures:
    victory = check_control_structures(world, false);
    break;
  case VictoryType::CaptureStructures:
    victory = check_control_structures(world, true);
    break;
  case VictoryType::Custom:

    break;
  }

  if (victory) {
    m_victory_state = "victory";
    qInfo() << "VICTORY! Conditions met.";

    const auto &all_owners = m_owner_registry.get_all_owners();
    for (const auto &owner : all_owners) {
      if (owner.type == Game::Systems::OwnerType::Player ||
          owner.type == Game::Systems::OwnerType::AI) {
        m_stats_registry.mark_game_end(owner.owner_id);
      }
    }

    const auto *stats = m_stats_registry.get_stats(m_local_owner_id);
    if (stats != nullptr) {
      qInfo() << "Final Stats - Troops Recruited:" << stats->troops_recruited
              << "Enemies Killed:" << stats->enemies_killed
              << "Losses:" << stats->losses
              << "Barracks Owned:" << stats->barracks_owned
              << "Play Time:" << stats->play_time_sec << "seconds";
    }

    if (m_victory_callback) {
      m_victory_callback(m_victory_state);
    }
  }
}

void VictoryService::check_defeat_conditions(Engine::Core::World &world) {
  for (const auto &condition : m_defeat_conditions) {
    bool defeat = false;

    switch (condition) {
    case DefeatCondition::NoUnits:
      defeat = check_no_units(world);
      break;
    case DefeatCondition::NoKeyStructures:
      defeat = check_no_key_structures(world);
      break;
    case DefeatCondition::TimeExpired:

      break;
    }

    if (defeat) {
      m_victory_state = "defeat";
      qInfo() << "DEFEAT! Condition met.";

      const auto &all_owners = m_owner_registry.get_all_owners();
      for (const auto &owner : all_owners) {
        if (owner.type == Game::Systems::OwnerType::Player ||
            owner.type == Game::Systems::OwnerType::AI) {
          m_stats_registry.mark_game_end(owner.owner_id);
        }
      }

      const auto *stats = m_stats_registry.get_stats(m_local_owner_id);
      if (stats != nullptr) {
        qInfo() << "Final Stats - Troops Recruited:" << stats->troops_recruited
                << "Enemies Killed:" << stats->enemies_killed
                << "Losses:" << stats->losses
                << "Barracks Owned:" << stats->barracks_owned
                << "Play Time:" << stats->play_time_sec << "seconds";
      }

      if (m_victory_callback) {
        m_victory_callback(m_victory_state);
      }
      return;
    }
  }
}

auto VictoryService::check_elimination(Engine::Core::World &world) -> bool {

  bool enemy_key_structures_alive = false;

  int const local_team = m_owner_registry.get_owner_team(m_local_owner_id);

  auto entities = world.get_entities_with<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *unit = e->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->health <= 0) {
      continue;
    }

    if (unit->owner_id == m_local_owner_id) {
      continue;
    }

    if (m_owner_registry.are_allies(m_local_owner_id, unit->owner_id)) {
      continue;
    }

    QString const unit_type_str = QString::fromStdString(
        Game::Units::spawn_typeToString(unit->spawn_type));
    if (std::find(m_key_structures.begin(), m_key_structures.end(),
                  unit_type_str) != m_key_structures.end()) {
      enemy_key_structures_alive = true;
      break;
    }
  }

  return !enemy_key_structures_alive;
}

auto VictoryService::check_survive_time() const -> bool {
  return m_elapsed_time >= m_survive_time_duration;
}

auto VictoryService::check_control_structures(
    Engine::Core::World &world, bool require_captured) const -> bool {
  if (m_key_structures.empty()) {
    return false;
  }

  int required = m_required_key_structures;
  if (required <= 0) {
    required = 1;
  }

  auto &nation_registry = Game::Systems::NationRegistry::instance();
  const auto *local_nation =
      nation_registry.get_nation_for_player(m_local_owner_id);
  const bool has_local_nation = (local_nation != nullptr);
  const auto local_nation_id =
      has_local_nation ? local_nation->id : nation_registry.default_nation_id();

  int captured_count = 0;

  auto entities = world.get_entities_with<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *unit = e->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->health <= 0) {
      continue;
    }

    if (unit->owner_id != m_local_owner_id) {
      continue;
    }

    const QString unit_type = QString::fromStdString(
        Game::Units::spawn_typeToString(unit->spawn_type));
    if (std::find(m_key_structures.begin(), m_key_structures.end(), unit_type) ==
        m_key_structures.end()) {
      continue;
    }

    if (require_captured && has_local_nation) {
      auto *building = e->get_component<Engine::Core::BuildingComponent>();
      if (building == nullptr ||
          building->original_nation_id == local_nation_id) {
        continue;
      }
    }

    captured_count++;
    if (captured_count >= required) {
      return true;
    }
  }

  return false;
}

auto VictoryService::check_no_units(Engine::Core::World &world) const -> bool {

  auto entities = world.get_entities_with<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *unit = e->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->health <= 0) {
      continue;
    }

    if (unit->owner_id == m_local_owner_id) {
      return false;
    }
  }

  return true;
}

auto VictoryService::check_no_key_structures(Engine::Core::World &world)
    -> bool {

  auto entities = world.get_entities_with<Engine::Core::UnitComponent>();
  for (auto *e : entities) {
    auto *unit = e->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->health <= 0) {
      continue;
    }

    if (unit->owner_id == m_local_owner_id) {
      QString const unit_type_str = QString::fromStdString(
          Game::Units::spawn_typeToString(unit->spawn_type));
      if (std::find(m_key_structures.begin(), m_key_structures.end(),
                    unit_type_str) != m_key_structures.end()) {
        return false;
      }
    }
  }

  return true;
}

} // namespace Game::Systems
