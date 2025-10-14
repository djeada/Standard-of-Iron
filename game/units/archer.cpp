#include "archer.h"
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
    return QVector3D(0.8f, 0.9f, 1.0f);
  }
}

namespace Game {
namespace Units {

Archer::Archer(Engine::Core::World &world) : Unit(world, "archer") {}

std::unique_ptr<Archer> Archer::Create(Engine::Core::World &world,
                                       const SpawnParams &params) {
  auto unit = std::unique_ptr<Archer>(new Archer(world));
  unit->init(params);
  return unit;
}

void Archer::init(const SpawnParams &params) {

  auto *e = m_world->createEntity();
  m_id = e->getId();

  m_t = e->addComponent<Engine::Core::TransformComponent>();
  m_t->position = {params.position.x(), params.position.y(),
                   params.position.z()};
  m_t->scale = {0.5f, 0.5f, 0.5f};

  m_r = e->addComponent<Engine::Core::RenderableComponent>("", "");
  m_r->visible = true;

  m_u = e->addComponent<Engine::Core::UnitComponent>();
  m_u->unitType = m_type;
  m_u->health = 80;
  m_u->maxHealth = 80;
  m_u->speed = 3.0f;
  m_u->ownerId = params.playerId;
  m_u->visionRange = 16.0f;

  if (params.aiControlled) {
    e->addComponent<Engine::Core::AIControlledComponent>();
    std::cout << "[Archer] Created AI-controlled archer for player "
              << params.playerId << " at entity ID " << e->getId() << std::endl;
  } else {
    std::cout << "[Archer] Created player-controlled archer for player "
              << params.playerId << " at entity ID " << e->getId() << std::endl;
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

  m_atk->range = 6.0f;
  m_atk->damage = 12;
  m_atk->cooldown = 1.2f;

  m_atk->meleeRange = 1.5f;
  m_atk->meleeDamage = 5;
  m_atk->meleeCooldown = 0.8f;

  m_atk->preferredMode = Engine::Core::AttackComponent::CombatMode::Auto;
  m_atk->currentMode = Engine::Core::AttackComponent::CombatMode::Ranged;
  m_atk->canRanged = true;
  m_atk->canMelee = true;
  m_atk->maxHeightDifference = 2.0f;

  Engine::Core::EventManager::instance().publish(
      Engine::Core::UnitSpawnedEvent(m_id, m_u->ownerId, m_u->unitType));
}

} // namespace Units
} // namespace Game
