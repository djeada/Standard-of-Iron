#include "unit.h"

#include "../core/component.h"
#include "../core/world.h"
#include "../systems/nation_id.h"
#include "../systems/nation_registry.h"
#include "units/troop_type.h"
#include <qvectornd.h>
#include <string>
#include <utility>

namespace Game::Units {

Unit::Unit(Engine::Core::World &world, TroopType type)
    : m_world(&world), m_type_string(troop_typeToString(type)) {}

Unit::Unit(Engine::Core::World &world, std::string type)
    : m_world(&world), m_type_string(std::move(type)) {}

auto Unit::entity() const -> Engine::Core::Entity * {
  return (m_world != nullptr) ? m_world->get_entity(m_id) : nullptr;
}

auto Unit::resolve_nation_id(const SpawnParams &params)
    -> Game::Systems::NationID {

  return params.nation_id;
}

void Unit::ensureCoreComponents() {
  if (m_world == nullptr) {
    return;
  }
  if (auto *e = entity()) {
    if (m_t == nullptr) {
      m_t = e->get_component<Engine::Core::TransformComponent>();
    }
    if (m_r == nullptr) {
      m_r = e->get_component<Engine::Core::RenderableComponent>();
    }
    if (m_u == nullptr) {
      m_u = e->get_component<Engine::Core::UnitComponent>();
    }
    if (m_mv == nullptr) {
      m_mv = e->get_component<Engine::Core::MovementComponent>();
    }
    if (m_atk == nullptr) {
      m_atk = e->get_component<Engine::Core::AttackComponent>();
    }
  }
}

void Unit::move_to(float x, float z) {
  ensureCoreComponents();
  if (m_mv == nullptr) {
    if (auto *e = entity()) {
      m_mv = e->add_component<Engine::Core::MovementComponent>();
    }
  }
  if (m_mv != nullptr) {
    m_mv->target_x = x;
    m_mv->target_y = z;
    m_mv->has_target = true;
    m_mv->goal_x = x;
    m_mv->goal_y = z;
    m_mv->path.clear();
    m_mv->path_pending = false;
    m_mv->pending_request_id = 0;
  }

  if (auto *e = entity()) {
    auto *hold_comp = e->get_component<Engine::Core::HoldModeComponent>();
    if (hold_comp != nullptr) {
      hold_comp->active = false;
    }
    auto *guard_comp = e->get_component<Engine::Core::GuardModeComponent>();
    if (guard_comp != nullptr) {
      guard_comp->active = false;
    }
  }
}

auto Unit::is_alive() const -> bool {
  if (auto *e = entity()) {
    if (auto *u = e->get_component<Engine::Core::UnitComponent>()) {
      return u->health > 0;
    }
  }
  return false;
}

auto Unit::position() const -> QVector3D {
  if (auto *e = entity()) {
    if (auto *t = e->get_component<Engine::Core::TransformComponent>()) {
      return {t->position.x, t->position.y, t->position.z};
    }
  }
  return {};
}

void Unit::set_hold_mode(bool enabled) {
  auto *e = entity();
  if (e == nullptr) {
    return;
  }

  auto *hold_comp = e->get_component<Engine::Core::HoldModeComponent>();

  if (enabled) {
    if (hold_comp == nullptr) {
      hold_comp = e->add_component<Engine::Core::HoldModeComponent>();
    }
    hold_comp->active = true;
    hold_comp->exit_cooldown = 0.0F;

    auto *guard_comp = e->get_component<Engine::Core::GuardModeComponent>();
    if (guard_comp != nullptr) {
      guard_comp->active = false;
    }

    auto *mv = e->get_component<Engine::Core::MovementComponent>();
    if (mv != nullptr) {
      mv->has_target = false;
      mv->path.clear();
      mv->path_pending = false;
    }
  } else {
    if (hold_comp != nullptr) {
      hold_comp->active = false;
      hold_comp->exit_cooldown = hold_comp->stand_up_duration;
    }
  }
}

auto Unit::is_in_hold_mode() const -> bool {
  auto *e = entity();
  if (e == nullptr) {
    return false;
  }

  auto *hold_comp = e->get_component<Engine::Core::HoldModeComponent>();
  return (hold_comp != nullptr) && hold_comp->active;
}

void Unit::set_guard_mode(bool enabled) {
  auto *e = entity();
  if (e == nullptr) {
    return;
  }

  auto *guard_comp = e->get_component<Engine::Core::GuardModeComponent>();

  if (enabled) {
    if (guard_comp == nullptr) {
      guard_comp = e->add_component<Engine::Core::GuardModeComponent>();
    }
    guard_comp->active = true;
    guard_comp->returning_to_guard_position = false;

    auto *hold_comp = e->get_component<Engine::Core::HoldModeComponent>();
    if (hold_comp != nullptr) {
      hold_comp->active = false;
    }

    if (!guard_comp->has_guard_target) {
      auto *transform = e->get_component<Engine::Core::TransformComponent>();
      if (transform != nullptr) {
        guard_comp->guard_position_x = transform->position.x;
        guard_comp->guard_position_z = transform->position.z;
        guard_comp->has_guard_target = true;
      }
    }
  } else {
    if (guard_comp != nullptr) {
      guard_comp->active = false;
    }
  }
}

void Unit::set_guard_target(Engine::Core::EntityID target_id) {
  auto *e = entity();
  if (e == nullptr) {
    return;
  }

  auto *guard_comp = e->get_component<Engine::Core::GuardModeComponent>();
  if (guard_comp == nullptr) {
    guard_comp = e->add_component<Engine::Core::GuardModeComponent>();
  }

  guard_comp->guarded_entity_id = target_id;
  guard_comp->guard_position_x = 0.0F;
  guard_comp->guard_position_z = 0.0F;
  guard_comp->active = true;
  guard_comp->returning_to_guard_position = false;
  guard_comp->has_guard_target = true;

  auto *hold_comp = e->get_component<Engine::Core::HoldModeComponent>();
  if (hold_comp != nullptr) {
    hold_comp->active = false;
  }
}

void Unit::set_guard_position(float x, float z) {
  auto *e = entity();
  if (e == nullptr) {
    return;
  }

  auto *guard_comp = e->get_component<Engine::Core::GuardModeComponent>();
  if (guard_comp == nullptr) {
    guard_comp = e->add_component<Engine::Core::GuardModeComponent>();
  }

  guard_comp->guarded_entity_id = 0;
  guard_comp->guard_position_x = x;
  guard_comp->guard_position_z = z;
  guard_comp->active = true;
  guard_comp->returning_to_guard_position = false;
  guard_comp->has_guard_target = true;

  auto *hold_comp = e->get_component<Engine::Core::HoldModeComponent>();
  if (hold_comp != nullptr) {
    hold_comp->active = false;
  }
}

auto Unit::is_in_guard_mode() const -> bool {
  auto *e = entity();
  if (e == nullptr) {
    return false;
  }

  auto *guard_comp = e->get_component<Engine::Core::GuardModeComponent>();
  return (guard_comp != nullptr) && guard_comp->active;
}

void Unit::clear_guard_mode() {
  auto *e = entity();
  if (e == nullptr) {
    return;
  }

  auto *guard_comp = e->get_component<Engine::Core::GuardModeComponent>();
  if (guard_comp != nullptr) {
    guard_comp->active = false;
    guard_comp->guarded_entity_id = 0;
    guard_comp->guard_position_x = 0.0F;
    guard_comp->guard_position_z = 0.0F;
    guard_comp->returning_to_guard_position = false;
    guard_comp->has_guard_target = false;
  }
}

void Unit::set_run_mode(bool enabled) {
  auto *e = entity();
  if (e == nullptr) {
    return;
  }

  const auto *unit_comp = e->get_component<Engine::Core::UnitComponent>();
  if (unit_comp == nullptr ||
      !Game::Units::can_use_run_mode(unit_comp->spawn_type)) {
    return;
  }

  auto *stamina_comp = e->get_component<Engine::Core::StaminaComponent>();
  if (stamina_comp == nullptr) {
    if (!enabled) {
      return;
    }
    stamina_comp = e->add_component<Engine::Core::StaminaComponent>();
  }

  stamina_comp->run_requested = enabled;
}

auto Unit::is_running() const -> bool {
  const auto *e = entity();
  if (e == nullptr) {
    return false;
  }

  const auto *stamina_comp =
      e->get_component<Engine::Core::StaminaComponent>();
  return stamina_comp != nullptr && stamina_comp->is_running;
}

auto Unit::can_run() const -> bool {
  const auto *e = entity();
  if (e == nullptr) {
    return false;
  }

  const auto *unit_comp = e->get_component<Engine::Core::UnitComponent>();
  return unit_comp != nullptr &&
         Game::Units::can_use_run_mode(unit_comp->spawn_type);
}

} // namespace Game::Units
