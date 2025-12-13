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

} // namespace Game::Units
