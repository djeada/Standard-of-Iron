#include "mounted_knight.h"
#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/world.h"
#include <iostream>

static inline QVector3D teamColor(int ownerId) {
  switch (ownerId) {
  case 1:
    return QVector3D(0.20f, 0.55f, 1.00f);
  case 2:
    return QVector3D(1.00f, 0.30f, 0.30f);
  case 3:
    return QVector3D(0.20f, 0.80f, 0.40f);
  case 4:
    return QVector3D(1.00f, 0.80f, 0.20f);
  default:
    return QVector3D(0.8f, 0.8f, 0.8f);
  }
}

namespace Game {
namespace Units {

MountedKnight::MountedKnight(Engine::Core::World &world)
    : Unit(world, TroopType::MountedKnight) {}

std::unique_ptr<MountedKnight>
MountedKnight::Create(Engine::Core::World &world, const SpawnParams &params) {
  auto unit = std::unique_ptr<MountedKnight>(new MountedKnight(world));
  unit->init(params);
  return unit;
}

void MountedKnight::init(const SpawnParams &params) {

  auto *e = m_world->createEntity();
  m_id = e->getId();

  m_t = e->addComponent<Engine::Core::TransformComponent>();
  m_t->position = {params.position.x(), params.position.y(),
                   params.position.z()};
  m_t->scale = {0.8f, 0.8f, 0.8f};

  m_r = e->addComponent<Engine::Core::RenderableComponent>("", "");
  m_r->visible = true;

  m_u = e->addComponent<Engine::Core::UnitComponent>();
  m_u->unitType = m_typeString;
  m_u->spawnTypeEnum = params.spawnType;
  m_u->health = 200;
  m_u->maxHealth = 200;
  m_u->speed = 8.0f;
  m_u->ownerId = params.playerId;
  m_u->visionRange = 16.0f;

  if (params.aiControlled) {
    e->addComponent<Engine::Core::AIControlledComponent>();
  } else {
  }

  QVector3D tc = teamColor(m_u->ownerId);
  m_r->color[0] = tc.x();
  m_r->color[1] = tc.y();
  m_r->color[2] = tc.z();

  m_mv = e->addComponent<Engine::Core::MovementComponent>();
  if (m_mv) {
    m_mv->goalX = params.position.x();
    m_mv->goalY = params.position.z();
    m_mv->targetX = params.position.x();
    m_mv->targetY = params.position.z();
  }

  m_atk = e->addComponent<Engine::Core::AttackComponent>();

  m_atk->range = 1.5f;
  m_atk->damage = 5;
  m_atk->cooldown = 2.0f;

  m_atk->meleeRange = 2.0f;
  m_atk->meleeDamage = 25;
  m_atk->meleeCooldown = 0.8f;

  m_atk->preferredMode = Engine::Core::AttackComponent::CombatMode::Melee;
  m_atk->currentMode = Engine::Core::AttackComponent::CombatMode::Melee;
  m_atk->canRanged = false;
  m_atk->canMelee = true;
  m_atk->maxHeightDifference = 2.0f;

  Engine::Core::EventManager::instance().publish(
      Engine::Core::UnitSpawnedEvent(m_id, m_u->ownerId, m_u->unitType));
}

} // namespace Units
} // namespace Game
