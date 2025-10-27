#include "mounted_knight.h"
#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/world.h"
#include "units/troop_type.h"
#include "units/unit.h"
#include <memory>
#include <qvectornd.h>

static inline auto team_color(int owner_id) -> QVector3D {
  switch (owner_id) {
  case 1:
    return {0.20F, 0.55F, 1.00F};
  case 2:
    return {1.00F, 0.30F, 0.30F};
  case 3:
    return {0.20F, 0.80F, 0.40F};
  case 4:
    return {1.00F, 0.80F, 0.20F};
  default:
    return {0.8F, 0.8F, 0.8F};
  }
}

namespace Game::Units {

MountedKnight::MountedKnight(Engine::Core::World &world)
    : Unit(world, TroopType::MountedKnight) {}

auto MountedKnight::Create(Engine::Core::World &world,
                           const SpawnParams &params)
    -> std::unique_ptr<MountedKnight> {
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
  m_t->scale = {0.8F, 0.8F, 0.8F};

  m_r = e->addComponent<Engine::Core::RenderableComponent>("", "");
  m_r->visible = true;

  m_u = e->addComponent<Engine::Core::UnitComponent>();
  m_u->spawn_type = params.spawn_type;
  m_u->health = 200;
  m_u->max_health = 200;
  m_u->speed = 8.0F;
  m_u->owner_id = params.player_id;
  m_u->vision_range = 16.0F;

  if (params.aiControlled) {
    e->addComponent<Engine::Core::AIControlledComponent>();
  } else {
  }

  QVector3D const tc = team_color(m_u->owner_id);
  m_r->color[0] = tc.x();
  m_r->color[1] = tc.y();
  m_r->color[2] = tc.z();

  m_mv = e->addComponent<Engine::Core::MovementComponent>();
  if (m_mv != nullptr) {
    m_mv->goalX = params.position.x();
    m_mv->goalY = params.position.z();
    m_mv->target_x = params.position.x();
    m_mv->target_y = params.position.z();
  }

  m_atk = e->addComponent<Engine::Core::AttackComponent>();

  m_atk->range = 1.5F;
  m_atk->damage = 5;
  m_atk->cooldown = 2.0F;

  m_atk->meleeRange = 2.0F;
  m_atk->meleeDamage = 25;
  m_atk->meleeCooldown = 0.8F;

  m_atk->preferredMode = Engine::Core::AttackComponent::CombatMode::Melee;
  m_atk->currentMode = Engine::Core::AttackComponent::CombatMode::Melee;
  m_atk->canRanged = false;
  m_atk->canMelee = true;
  m_atk->max_heightDifference = 2.0F;

  Engine::Core::EventManager::instance().publish(
      Engine::Core::UnitSpawnedEvent(m_id, m_u->owner_id, m_u->spawn_type));
}

} // namespace Game::Units
