#include "unit.h"
#include "../core/component.h"
#include "../core/world.h"

namespace Game {
namespace Units {

Unit::Unit(Engine::Core::World &world, const std::string &type)
    : m_world(&world), m_type(type) {}

Engine::Core::Entity *Unit::entity() const {
  return m_world ? m_world->getEntity(m_id) : nullptr;
}

void Unit::ensureCoreComponents() {
  if (!m_world)
    return;
  if (auto *e = entity()) {
    if (!m_t)
      m_t = e->getComponent<Engine::Core::TransformComponent>();
    if (!m_r)
      m_r = e->getComponent<Engine::Core::RenderableComponent>();
    if (!m_u)
      m_u = e->getComponent<Engine::Core::UnitComponent>();
    if (!m_mv)
      m_mv = e->getComponent<Engine::Core::MovementComponent>();
    if (!m_atk)
      m_atk = e->getComponent<Engine::Core::AttackComponent>();
  }
}

void Unit::moveTo(float x, float z) {
  ensureCoreComponents();
  if (!m_mv) {
    if (auto *e = entity())
      m_mv = e->addComponent<Engine::Core::MovementComponent>();
  }
  if (m_mv) {
    m_mv->targetX = x;
    m_mv->targetY = z;
    m_mv->hasTarget = true;
    m_mv->goalX = x;
    m_mv->goalY = z;
    m_mv->path.clear();
    m_mv->pathPending = false;
    m_mv->pendingRequestId = 0;
  }
}

bool Unit::isAlive() const {
  if (auto *e = entity()) {
    if (auto *u = e->getComponent<Engine::Core::UnitComponent>())
      return u->health > 0;
  }
  return false;
}

QVector3D Unit::position() const {
  if (auto *e = entity()) {
    if (auto *t = e->getComponent<Engine::Core::TransformComponent>())
      return QVector3D(t->position.x, t->position.y, t->position.z);
  }
  return QVector3D();
}

} // namespace Units
} // namespace Game
