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
  result["canAttack"] = true;
  result["canGuard"] = true;
  result["canHold"] = true;
  result["canPatrol"] = true;

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

  bool can_attack = true;
  bool can_guard = true;
  bool can_hold = true;
  bool can_patrol = true;

  for (auto id : sel) {
    if (!can_attack && !can_guard && !can_hold && !can_patrol) {
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

    if (can_attack && !Game::Units::can_use_attack_mode(u->spawn_type)) {
      can_attack = false;
    }
    if (can_guard && !Game::Units::can_use_guard_mode(u->spawn_type)) {
      can_guard = false;
    }
    if (can_hold && !Game::Units::can_use_hold_mode(u->spawn_type)) {
      can_hold = false;
    }
    if (can_patrol && !Game::Units::can_use_patrol_mode(u->spawn_type)) {
      can_patrol = false;
    }
  }

  result["canAttack"] = can_attack;
  result["canGuard"] = can_guard;
  result["canHold"] = can_hold;
  result["canPatrol"] = can_patrol;

  return result;
}
