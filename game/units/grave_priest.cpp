#include "grave_priest.h"

#include <qvectornd.h>

#include <memory>

#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/world.h"
#include "../systems/troop_profile_service.h"
#include "spawn_type.h"
#include "units/troop_type.h"
#include "units/unit.h"

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
    return {0.8F, 0.9F, 1.0F};
  }
}

namespace Game::Units {

GravePriest::GravePriest(Engine::Core::World& world)
    : Unit(world, TroopType::GravePriest) {
}

auto GravePriest::Create(Engine::Core::World& world,
                         const SpawnParams& params) -> std::unique_ptr<GravePriest> {
  auto unit = std::unique_ptr<GravePriest>(new GravePriest(world));
  unit->init(params);
  return unit;
}

void GravePriest::init(const SpawnParams& params) {
  auto* e = m_world->create_entity();
  m_id = e->get_id();

  const auto nation_id = resolve_nation_id(params);
  const auto troop_type =
      spawn_typeToTroopType(params.spawn_type).value_or(TroopType::GravePriest);
  auto profile =
      Game::Systems::TroopProfileService::instance().get_profile(nation_id, troop_type);

  m_t = e->add_component<Engine::Core::TransformComponent>();
  m_t->position = {params.position.x(), params.position.y(), params.position.z()};
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

  e->add_component<Engine::Core::UndeadComponent>();

  if (params.ai_controlled) {
    e->add_component<Engine::Core::AIControlledComponent>();
  }

  QVector3D const tc = team_color(m_u->owner_id);
  m_r->color[0] = tc.x();
  m_r->color[1] = tc.y();
  m_r->color[2] = tc.z();

  m_mv = e->add_component<Engine::Core::MovementComponent>();
  if (m_mv != nullptr) {
    m_mv->set_rest_position(params.position.x(), params.position.z());
  }

  m_atk = e->add_component<Engine::Core::AttackComponent>();
  if (m_atk != nullptr) {
    m_atk->range = profile.combat.ranged_range;
    m_atk->damage = profile.combat.ranged_damage;
    m_atk->cooldown = profile.combat.ranged_cooldown;
    m_atk->melee_range = profile.combat.melee_range;
    m_atk->melee_damage = profile.combat.melee_damage;
    m_atk->melee_cooldown = profile.combat.melee_cooldown;
    m_atk->preferred_mode = profile.combat.can_ranged
                                ? Engine::Core::AttackComponent::CombatMode::Ranged
                                : Engine::Core::AttackComponent::CombatMode::Melee;
    m_atk->current_mode = profile.combat.can_ranged
                              ? Engine::Core::AttackComponent::CombatMode::Ranged
                              : Engine::Core::AttackComponent::CombatMode::Melee;
    m_atk->can_ranged = profile.combat.can_ranged;
    m_atk->can_melee = profile.combat.can_melee;
    m_atk->time_since_last = m_atk->cooldown;
  }

  auto* healer_comp = e->add_component<Engine::Core::HealerComponent>();
  if (healer_comp != nullptr) {
    healer_comp->healing_range = std::max(9.0F, profile.combat.vision_range * 0.58F);
    healer_comp->healing_amount = std::max(
        24, static_cast<int>(std::round(profile.combat.ranged_damage * 0.85F)));
    healer_comp->healing_cooldown =
        std::max(2.45F, profile.combat.ranged_cooldown + 0.2F);
    healer_comp->target_affinity =
        Engine::Core::HealerComponent::TargetAffinity::UndeadAllies;
    healer_comp->suppress_attack_while_healing = true;
    healer_comp->time_since_last_heal = healer_comp->healing_cooldown;
  }

  if (profile.has_ability("fireball")) {
    auto* special_attack = e->add_component<Engine::Core::SpecialAttackComponent>();
    if (special_attack != nullptr) {
      special_attack->projectile_kind = Game::Systems::ProjectileKind::Fireball;
      special_attack->use_projectile_system = true;
      special_attack->friendly_fire = false;
      special_attack->splash_radius = 2.4F;
      special_attack->splash_damage_multiplier = 0.7F;
      special_attack->bonus_damage_multiplier_vs_fire_vulnerable = 1.7F;
      special_attack->fire_patch_duration = 4.0F;
      special_attack->fire_patch_radius = 1.9F;
      special_attack->burn_duration = 1.5F;
      special_attack->burn_tick_interval = 0.5F;
      special_attack->burn_damage_per_tick = 2;
    }
  }

  Engine::Core::EventManager::instance().publish(Engine::Core::UnitSpawnedEvent(
      m_id, m_u->owner_id, m_u->spawn_type, params.is_initial_spawn));
}

} // namespace Game::Units
