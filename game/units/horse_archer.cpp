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

  auto *e = m_world->create_entity();
  m_id = e->get_id();

  const auto nation_id = resolve_nation_id(params);
  auto profile = Game::Systems::TroopProfileService::instance().get_profile(
      nation_id, TroopType::HorseArcher);

  m_t = e->add_component<Engine::Core::TransformComponent>();
  m_t->position = {params.position.x(), params.position.y(),
                   params.position.z()};
  float const scale = profile.visuals.render_scale;
  m_t->scale = {scale, scale, scale};

  m_r = e->add_component<Engine::Core::RenderableComponent>("", "");
  m_r->visible = true;
  m_r->renderer_id = profile.visuals.renderer_id;

  m_u = e->add_component<Engine::Core::UnitComponent>();
  m_u->spawn_type = params.spawn_type;
  m_u->health = profile.combat.health;
  m_u->max_health = profile.combat.max_health;
  m_u->speed = profile.combat.speed;
  m_u->owner_id = params.player_id;
  m_u->vision_range = profile.combat.vision_range;
  m_u->nation_id = nation_id;

  if (params.aiControlled) {
    e->add_component<Engine::Core::AIControlledComponent>();
  } else {
  }

  QVector3D const tc = team_color(m_u->owner_id);
  m_r->color[0] = tc.x();
  m_r->color[1] = tc.y();
  m_r->color[2] = tc.z();

  m_mv = e->add_component<Engine::Core::MovementComponent>();
  if (m_mv != nullptr) {
    m_mv->goal_x = params.position.x();
    m_mv->goal_y = params.position.z();
    m_mv->target_x = params.position.x();
    m_mv->target_y = params.position.z();
  }

  m_atk = e->add_component<Engine::Core::AttackComponent>();

  m_atk->range = profile.combat.ranged_range;
  m_atk->damage = profile.combat.ranged_damage;
  m_atk->cooldown = profile.combat.ranged_cooldown;

  m_atk->melee_range = profile.combat.melee_range;
  m_atk->melee_damage = profile.combat.melee_damage;
  m_atk->melee_cooldown = profile.combat.melee_cooldown;

  m_atk->preferred_mode = profile.combat.can_ranged
                             ? Engine::Core::AttackComponent::CombatMode::Auto
                             : Engine::Core::AttackComponent::CombatMode::Melee;
  m_atk->current_mode = profile.combat.can_ranged
                           ? Engine::Core::AttackComponent::CombatMode::Ranged
                           : Engine::Core::AttackComponent::CombatMode::Melee;
  m_atk->can_ranged = profile.combat.can_ranged;
  m_atk->can_melee = profile.combat.can_melee;
  m_atk->max_height_difference = 2.0F;

  Engine::Core::EventManager::instance().publish(
      Engine::Core::UnitSpawnedEvent(m_id, m_u->owner_id, m_u->spawn_type));
}

} // namespace Game::Units
