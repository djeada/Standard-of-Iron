#include "unit.h"

#include "../core/component.h"
#include "../core/world.h"
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
  return (m_world != nullptr) ? m_world->getEntity(m_id) : nullptr;
}

auto Unit::resolve_nation_id(const SpawnParams &params) -> std::string {
  if (!params.nation_id.empty()) {
    return params.nation_id;
  }

  auto &registry = Game::Systems::NationRegistry::instance();
  if (const auto *nation = registry.getNationForPlayer(params.player_id)) {
    return nation->id;
  }

  const auto &fallback_id = registry.default_nation_id();
  if (const auto *nation = registry.getNation(fallback_id)) {
    return nation->id;
  }

  return fallback_id;
}

void Unit::ensureCoreComponents() {
  if (m_world == nullptr) {
    return;
  }
  if (auto *e = entity()) {
    if (m_t == nullptr) {
      m_t = e->getComponent<Engine::Core::TransformComponent>();
    }
    if (m_r == nullptr) {
      m_r = e->getComponent<Engine::Core::RenderableComponent>();
    }
    if (m_u == nullptr) {
      m_u = e->getComponent<Engine::Core::UnitComponent>();
    }
    if (m_mv == nullptr) {
      m_mv = e->getComponent<Engine::Core::MovementComponent>();
    }
    if (m_atk == nullptr) {
      m_atk = e->getComponent<Engine::Core::AttackComponent>();
    }
  }
}

void Unit::moveTo(float x, float z) {
  ensureCoreComponents();
  if (m_mv == nullptr) {
    if (auto *e = entity()) {
      m_mv = e->addComponent<Engine::Core::MovementComponent>();
    }
  }
  if (m_mv != nullptr) {
    m_mv->target_x = x;
    m_mv->target_y = z;
    m_mv->hasTarget = true;
    m_mv->goalX = x;
    m_mv->goalY = z;
    m_mv->path.clear();
    m_mv->pathPending = false;
    m_mv->pendingRequestId = 0;
  }

  if (auto *e = entity()) {
    auto *hold_comp = e->getComponent<Engine::Core::HoldModeComponent>();
    if (hold_comp != nullptr) {
      hold_comp->active = false;
    }
  }
}

auto Unit::isAlive() const -> bool {
  if (auto *e = entity()) {
    if (auto *u = e->getComponent<Engine::Core::UnitComponent>()) {
      return u->health > 0;
    }
  }
  return false;
}

auto Unit::position() const -> QVector3D {
  if (auto *e = entity()) {
    if (auto *t = e->getComponent<Engine::Core::TransformComponent>()) {
      return {t->position.x, t->position.y, t->position.z};
    }
  }
  return {};
}

void Unit::setHoldMode(bool enabled) {
  auto *e = entity();
  if (e == nullptr) {
    return;
  }

  auto *hold_comp = e->getComponent<Engine::Core::HoldModeComponent>();

  if (enabled) {
    if (hold_comp == nullptr) {
      hold_comp = e->addComponent<Engine::Core::HoldModeComponent>();
    }
    hold_comp->active = true;
    hold_comp->exitCooldown = 0.0F;

    auto *mv = e->getComponent<Engine::Core::MovementComponent>();
    if (mv != nullptr) {
      mv->hasTarget = false;
      mv->path.clear();
      mv->pathPending = false;
    }
  } else {
    if (hold_comp != nullptr) {
      hold_comp->active = false;
      hold_comp->exitCooldown = hold_comp->standUpDuration;
    }
  }
}

auto Unit::isInHoldMode() const -> bool {
  auto *e = entity();
  if (e == nullptr) {
    return false;
  }

  auto *hold_comp = e->getComponent<Engine::Core::HoldModeComponent>();
  return (hold_comp != nullptr) && hold_comp->active;
}

} // namespace Game::Units
