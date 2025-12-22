#include "selection_query_service.h"

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/systems/selection_system.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_config.h"

SelectionQueryService::SelectionQueryService(Engine::Core::World *world,
                                             QObject *parent)
    : QObject(parent), m_world(world) {}

auto SelectionQueryService::get_selected_units_command_mode() const -> QString {
  if (!m_world) {
    return "normal";
  }
  auto *selection_system =
      m_world->get_system<Game::Systems::SelectionSystem>();
  if (!selection_system) {
    return "normal";
  }

  const auto &sel = selection_system->get_selected_units();
  if (sel.empty()) {
    return "normal";
  }

  int attacking_count = 0;
  int patrolling_count = 0;
  int guarding_count = 0;
  int total_units = 0;

  for (auto id : sel) {
    auto *e = m_world->get_entity(id);
    if (!e) {
      continue;
    }

    auto *u = e->get_component<Engine::Core::UnitComponent>();
    if (!u || u->spawn_type == Game::Units::SpawnType::Barracks) {
      continue;
    }

    total_units++;

    if (e->get_component<Engine::Core::AttackTargetComponent>()) {
      attacking_count++;
    }

    auto *patrol = e->get_component<Engine::Core::PatrolComponent>();
    if (patrol && patrol->patrolling) {
      patrolling_count++;
    }

    auto *guard = e->get_component<Engine::Core::GuardModeComponent>();
    if (guard && guard->active) {
      guarding_count++;
    }
  }

  if (total_units == 0) {
    return "normal";
  }

  if (patrolling_count == total_units) {
    return "patrol";
  }
  if (attacking_count == total_units) {
    return "attack";
  }
  if (guarding_count == total_units) {
    return "guard";
  }

  return "normal";
}

auto SelectionQueryService::get_selected_units_mode_availability() const
    -> QVariantMap {
  QVariantMap result;
  result["canAttack"] = false;
  result["canGuard"] = false;
  result["canHold"] = false;
  result["canPatrol"] = false;
  result["canHeal"] = false;
  result["canBuild"] = false;

  if (!m_world) {
    return result;
  }
  auto *selection_system =
      m_world->get_system<Game::Systems::SelectionSystem>();
  if (!selection_system) {
    return result;
  }

  const auto &sel = selection_system->get_selected_units();
  if (sel.empty()) {
    return result;
  }

  // Use superset logic: show mode if ANY selected unit supports it
  bool can_attack = false;
  bool can_guard = false;
  bool can_hold = false;
  bool can_patrol = false;
  bool can_heal = false;
  bool can_build = false;

  for (auto id : sel) {
    // Early exit if all modes are already available
    if (can_attack && can_guard && can_hold && can_patrol && can_heal &&
        can_build) {
      break;
    }

    auto *e = m_world->get_entity(id);
    if (!e) {
      continue;
    }

    auto *u = e->get_component<Engine::Core::UnitComponent>();
    if (!u || u->spawn_type == Game::Units::SpawnType::Barracks) {
      continue;
    }

    if (!can_attack && Game::Units::can_use_attack_mode(u->spawn_type)) {
      can_attack = true;
    }
    if (!can_guard && Game::Units::can_use_guard_mode(u->spawn_type)) {
      can_guard = true;
    }
    if (!can_hold && Game::Units::can_use_hold_mode(u->spawn_type)) {
      can_hold = true;
    }
    if (!can_patrol && Game::Units::can_use_patrol_mode(u->spawn_type)) {
      can_patrol = true;
    }
    if (!can_heal && u->spawn_type == Game::Units::SpawnType::Healer) {
      can_heal = true;
    }
    if (!can_build && u->spawn_type == Game::Units::SpawnType::Builder) {
      can_build = true;
    }
  }

  result["canAttack"] = can_attack;
  result["canGuard"] = can_guard;
  result["canHold"] = can_hold;
  result["canPatrol"] = can_patrol;
  result["canHeal"] = can_heal;
  result["canBuild"] = can_build;

  return result;
}
