#include "horse_archer.h"
#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/world.h"
#include "../systems/troop_profile_service.h"
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

HorseArcher::HorseArcher(Engine::Core::World &world)
    : Unit(world, TroopType::HorseArcher) {}

auto HorseArcher::Create(Engine::Core::World &world, const SpawnParams &params)
    -> std::unique_ptr<HorseArcher> {
  auto unit = std::unique_ptr<HorseArcher>(new HorseArcher(world));
  unit->init(params);
  return unit;
}

void HorseArcher::init(const SpawnParams &params) {

  auto *e = m_world->createEntity();
  m_id = e->getId();

  const auto nation_id = resolve_nation_id(params);
  auto profile = Game::Systems::TroopProfileService::instance().get_profile(
      nation_id, TroopType::HorseArcher);

  m_t = e->addComponent<Engine::Core::TransformComponent>();
  m_t->position = {params.position.x(), params.position.y(),
                   params.position.z()};
  float const scale = profile.visuals.render_scale;
  m_t->scale = {scale, scale, scale};

  m_r = e->addComponent<Engine::Core::RenderableComponent>("", "");
  m_r->visible = true;
  m_r->rendererId = profile.visuals.renderer_id;

  m_u = e->addComponent<Engine::Core::UnitComponent>();
  m_u->spawn_type = params.spawn_type;
  m_u->health = profile.combat.health;
  m_u->max_health = profile.combat.max_health;
  m_u->speed = profile.combat.speed;
  m_u->owner_id = params.player_id;
  m_u->vision_range = profile.combat.vision_range;
  m_u->nation_id = nation_id;

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

  m_atk->range = profile.combat.ranged_range;
  m_atk->damage = profile.combat.ranged_damage;
  m_atk->cooldown = profile.combat.ranged_cooldown;

  m_atk->meleeRange = profile.combat.melee_range;
  m_atk->meleeDamage = profile.combat.melee_damage;
  m_atk->meleeCooldown = profile.combat.melee_cooldown;

  m_atk->preferredMode = profile.combat.can_ranged
                             ? Engine::Core::AttackComponent::CombatMode::Auto
                             : Engine::Core::AttackComponent::CombatMode::Melee;
  m_atk->currentMode = profile.combat.can_ranged
                           ? Engine::Core::AttackComponent::CombatMode::Ranged
                           : Engine::Core::AttackComponent::CombatMode::Melee;
  m_atk->canRanged = profile.combat.can_ranged;
  m_atk->canMelee = profile.combat.can_melee;
  m_atk->max_heightDifference = 2.0F;

  Engine::Core::EventManager::instance().publish(
      Engine::Core::UnitSpawnedEvent(m_id, m_u->owner_id, m_u->spawn_type));
}

} // namespace Game::Units
