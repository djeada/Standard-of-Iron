#include "projectile_system.h"

#include <qvectornd.h>

#include <algorithm>
#include <cmath>

#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/world.h"
#include "arrow_projectile.h"
#include "combat_rules.h"
#include "combat_system/damage_processor.h"
#include "rpg_combat_system/rpg_commander_damage.h"
#include "stone_projectile.h"

namespace Game::Systems {

namespace {

constexpr float k_min_progress_for_impact = 0.98F;
constexpr float k_projectile_escape_radius = 1.5F;
constexpr float k_min_fire_patch_radius = 0.5F;
constexpr float k_fire_patch_ground_offset = 0.85F;
constexpr float k_initial_burning_visual_age = 0.08F;

auto projectile_owner_id(Engine::Core::World* world, Projectile* projectile) -> int {
  if (world == nullptr || projectile == nullptr || projectile->get_attacker_id() == 0) {
    return 0;
  }

  auto* attacker = world->get_entity(projectile->get_attacker_id());
  if (attacker == nullptr) {
    return 0;
  }

  auto* attacker_unit = attacker->get_component<Engine::Core::UnitComponent>();
  return (attacker_unit != nullptr) ? attacker_unit->owner_id : 0;
}

auto resolve_special_attack(Engine::Core::World* world, Projectile* projectile)
    -> const Engine::Core::SpecialAttackComponent* {
  if (world == nullptr || projectile == nullptr || projectile->get_attacker_id() == 0) {
    return nullptr;
  }

  auto* attacker = world->get_entity(projectile->get_attacker_id());
  return (attacker != nullptr)
             ? attacker->get_component<Engine::Core::SpecialAttackComponent>()
             : nullptr;
}

auto target_is_undead(const Engine::Core::Entity* entity) -> bool {
  return (entity != nullptr) && entity->has_component<Engine::Core::UndeadComponent>();
}

auto target_escaped_impact(const Engine::Core::Entity* target,
                           Projectile* projectile) -> bool {
  if (target == nullptr || projectile == nullptr) {
    return true;
  }

  const auto* target_transform =
      target->get_component<Engine::Core::TransformComponent>();
  if (target_transform == nullptr) {
    return false;
  }

  QVector3D const current_pos(target_transform->position.x,
                              target_transform->position.y,
                              target_transform->position.z);
  return (current_pos - projectile->get_target_locked_position()).length() >
         k_projectile_escape_radius;
}

auto can_affect_with_fire(int attacker_owner_id,
                          bool friendly_fire,
                          Engine::Core::EntityID attacker_id,
                          const Engine::Core::Entity* candidate,
                          const Engine::Core::UnitComponent* candidate_unit) -> bool {
  if (candidate == nullptr || candidate_unit == nullptr ||
      candidate_unit->health <= 0) {
    return false;
  }
  if (candidate->get_id() == attacker_id) {
    return false;
  }
  if (!friendly_fire && attacker_owner_id != 0 &&
      candidate_unit->owner_id == attacker_owner_id) {
    return false;
  }
  return true;
}

void apply_projectile_damage(Engine::Core::World* world,
                             Engine::Core::Entity* target,
                             int damage,
                             Engine::Core::EntityID attacker_id) {
  if (world == nullptr || target == nullptr || damage <= 0) {
    return;
  }

  if (Game::Systems::CombatRules::uses_rpg_combat_rules(target)) {
    Game::Systems::RpgCombat::deal_damage_to_rpg_commander(
        world, target, damage, attacker_id);
    return;
  }

  Game::Systems::Combat::deal_damage(world, target, damage, attacker_id);
}

void apply_cursed_status(Engine::Core::Entity* target,
                         const Engine::Core::SpecialAttackComponent* special_attack) {
  if (target == nullptr || special_attack == nullptr || target_is_undead(target) ||
      special_attack->cursed_stacks_per_hit <= 0) {
    return;
  }

  auto* target_unit = target->get_component<Engine::Core::UnitComponent>();
  if (target_unit == nullptr || target_unit->health <= 0) {
    return;
  }

  auto* morale = target->get_component<Engine::Core::MoraleComponent>();
  if (morale == nullptr) {
    morale = target->add_component<Engine::Core::MoraleComponent>();
  }
  auto* cursed = target->get_component<Engine::Core::CursedStatusComponent>();
  bool created_cursed = false;
  if (cursed == nullptr) {
    cursed = target->add_component<Engine::Core::CursedStatusComponent>();
    created_cursed = (cursed != nullptr);
  }
  if (morale == nullptr || cursed == nullptr) {
    return;
  }
  if (created_cursed) {
    cursed->stacks = 0;
  }

  cursed->morale_penalty_per_hit = special_attack->cursed_morale_penalty_per_hit;
  cursed->duration = special_attack->cursed_duration;
  cursed->remaining_duration = special_attack->cursed_duration;
  cursed->stacks += special_attack->cursed_stacks_per_hit;

  morale->morale -= special_attack->cursed_morale_penalty_per_hit *
                    static_cast<float>(special_attack->cursed_stacks_per_hit);
  Engine::Core::refresh_morale_state(*morale);
}

void refresh_burning_status(Engine::Core::Entity* target,
                            const Engine::Core::FirePatchComponent& fire_patch) {
  if (target == nullptr || fire_patch.burn_damage_per_tick <= 0 ||
      fire_patch.burn_duration <= 0.0F) {
    return;
  }

  auto* target_unit = target->get_component<Engine::Core::UnitComponent>();
  if (target_unit == nullptr || target_unit->health <= 0) {
    return;
  }

  auto* burning = target->get_component<Engine::Core::BurningStatusComponent>();
  if (burning == nullptr) {
    burning = target->add_component<Engine::Core::BurningStatusComponent>();
    if (burning != nullptr) {
      burning->duration = fire_patch.burn_duration;
      burning->remaining_duration = fire_patch.burn_duration;
      burning->ignition_elapsed = k_initial_burning_visual_age;
      burning->tick_interval = fire_patch.burn_tick_interval;
      burning->damage_per_tick = fire_patch.burn_damage_per_tick;
      burning->fire_bonus_multiplier = fire_patch.fire_bonus_multiplier;
    }
  }
  if (burning == nullptr) {
    return;
  }

  burning->duration = std::max(burning->duration, fire_patch.burn_duration);
  burning->remaining_duration =
      std::max(burning->remaining_duration, fire_patch.burn_duration);
  burning->tick_interval = fire_patch.burn_tick_interval;
  burning->damage_per_tick =
      std::max(burning->damage_per_tick, fire_patch.burn_damage_per_tick);
  burning->attacker_id = fire_patch.attacker_id;
  burning->fire_bonus_multiplier =
      std::max(burning->fire_bonus_multiplier, fire_patch.fire_bonus_multiplier);
}

void apply_fireball_burning_status(
    Engine::Core::World* world,
    Engine::Core::Entity* target,
    Projectile* projectile,
    const Engine::Core::SpecialAttackComponent* special_attack) {
  if (world == nullptr || target == nullptr || projectile == nullptr ||
      special_attack == nullptr || special_attack->burn_duration <= 0.0F ||
      special_attack->burn_damage_per_tick <= 0) {
    return;
  }

  Engine::Core::FirePatchComponent impact_fire;
  impact_fire.burn_duration = special_attack->burn_duration;
  impact_fire.burn_tick_interval = special_attack->burn_tick_interval;
  impact_fire.burn_damage_per_tick = special_attack->burn_damage_per_tick;
  impact_fire.attacker_owner_id = projectile_owner_id(world, projectile);
  impact_fire.attacker_id = projectile->get_attacker_id();
  impact_fire.friendly_fire = special_attack->friendly_fire;
  impact_fire.fire_bonus_multiplier =
      special_attack->bonus_damage_multiplier_vs_fire_vulnerable;
  refresh_burning_status(target, impact_fire);
}

void update_burning_statuses(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  for (auto* entity :
       world->get_entities_with<Engine::Core::BurningStatusComponent>()) {
    if (entity == nullptr ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto* burning = entity->get_component<Engine::Core::BurningStatusComponent>();
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (burning == nullptr || unit == nullptr || unit->health <= 0) {
      entity->remove_component<Engine::Core::BurningStatusComponent>();
      continue;
    }

    burning->remaining_duration =
        std::max(0.0F, burning->remaining_duration - delta_time);
    burning->ignition_elapsed += delta_time;
    burning->tick_accumulator += delta_time;

    float const tick_interval = std::max(0.05F, burning->tick_interval);
    while (burning->remaining_duration > 0.0F &&
           burning->tick_accumulator >= tick_interval) {
      burning->tick_accumulator -= tick_interval;

      int damage = burning->damage_per_tick;
      if (auto* undead = entity->get_component<Engine::Core::UndeadComponent>();
          undead != nullptr) {
        damage = static_cast<int>(
            std::round(static_cast<float>(damage) * undead->fire_damage_multiplier *
                       burning->fire_bonus_multiplier));
      }

      apply_projectile_damage(world, entity, std::max(1, damage), burning->attacker_id);

      unit = entity->get_component<Engine::Core::UnitComponent>();
      if (unit == nullptr || unit->health <= 0) {
        entity->remove_component<Engine::Core::BurningStatusComponent>();
        break;
      }
    }

    if (entity->get_component<Engine::Core::BurningStatusComponent>() != nullptr &&
        burning->remaining_duration <= 0.0F) {
      entity->remove_component<Engine::Core::BurningStatusComponent>();
    }
  }
}

void update_fire_patches(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  auto units = world->get_entities_with<Engine::Core::UnitComponent>();
  for (auto* entity : world->get_entities_with<Engine::Core::FirePatchComponent>()) {
    if (entity == nullptr ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto* fire_patch = entity->get_component<Engine::Core::FirePatchComponent>();
    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (fire_patch == nullptr || transform == nullptr) {
      entity->add_component<Engine::Core::PendingRemovalComponent>();
      continue;
    }

    fire_patch->remaining_duration =
        std::max(0.0F, fire_patch->remaining_duration - delta_time);
    if (fire_patch->remaining_duration <= 0.0F) {
      entity->add_component<Engine::Core::PendingRemovalComponent>();
      continue;
    }

    float const radius_sq = fire_patch->radius * fire_patch->radius;
    for (auto* candidate : units) {
      if (candidate == nullptr ||
          candidate->has_component<Engine::Core::PendingRemovalComponent>()) {
        continue;
      }

      auto* candidate_unit = candidate->get_component<Engine::Core::UnitComponent>();
      auto* candidate_transform =
          candidate->get_component<Engine::Core::TransformComponent>();
      if (candidate_transform == nullptr ||
          !can_affect_with_fire(fire_patch->attacker_owner_id,
                                fire_patch->friendly_fire,
                                fire_patch->attacker_id,
                                candidate,
                                candidate_unit)) {
        continue;
      }

      float const dx = candidate_transform->position.x - transform->position.x;
      float const dz = candidate_transform->position.z - transform->position.z;
      if ((dx * dx + dz * dz) > radius_sq) {
        continue;
      }

      refresh_burning_status(candidate, *fire_patch);
    }
  }
}

void spawn_fire_patch(Engine::Core::World* world,
                      const QVector3D& impact_pos,
                      Projectile* projectile,
                      const Engine::Core::SpecialAttackComponent* special_attack) {
  if (world == nullptr || projectile == nullptr || special_attack == nullptr ||
      special_attack->fire_patch_duration <= 0.0F ||
      special_attack->burn_duration <= 0.0F ||
      special_attack->burn_damage_per_tick <= 0) {
    return;
  }

  auto* fire_patch_entity = world->create_entity();
  if (fire_patch_entity == nullptr) {
    return;
  }

  fire_patch_entity->add_component<Engine::Core::TransformComponent>(
      impact_pos.x(), impact_pos.y(), impact_pos.z());
  auto* fire_patch =
      fire_patch_entity->add_component<Engine::Core::FirePatchComponent>();
  if (fire_patch == nullptr) {
    fire_patch_entity->add_component<Engine::Core::PendingRemovalComponent>();
    return;
  }

  float const ground_y = std::max(0.0F, impact_pos.y() - k_fire_patch_ground_offset);
  auto* fire_patch_transform =
      fire_patch_entity->get_component<Engine::Core::TransformComponent>();
  if (fire_patch_transform != nullptr) {
    fire_patch_transform->position.y = ground_y;
  }

  fire_patch->radius = std::max(k_min_fire_patch_radius,
                                special_attack->fire_patch_radius > 0.0F
                                    ? special_attack->fire_patch_radius
                                    : special_attack->splash_radius);
  fire_patch->duration = special_attack->fire_patch_duration;
  fire_patch->remaining_duration = special_attack->fire_patch_duration;
  fire_patch->burn_duration = special_attack->burn_duration;
  fire_patch->burn_tick_interval = special_attack->burn_tick_interval;
  fire_patch->burn_damage_per_tick = special_attack->burn_damage_per_tick;
  fire_patch->attacker_owner_id = projectile_owner_id(world, projectile);
  fire_patch->attacker_id = projectile->get_attacker_id();
  fire_patch->friendly_fire = special_attack->friendly_fire;
  fire_patch->fire_bonus_multiplier =
      special_attack->bonus_damage_multiplier_vs_fire_vulnerable;
}

void apply_arrow_like_impact(Engine::Core::World* world,
                             Projectile* projectile,
                             ArrowProjectile* arrow_projectile) {
  if (world == nullptr || projectile == nullptr || arrow_projectile == nullptr) {
    return;
  }

  auto* target = world->get_entity(projectile->get_target_id());
  if (target == nullptr ||
      target->has_component<Engine::Core::PendingRemovalComponent>() ||
      target_escaped_impact(target, projectile)) {
    projectile->deactivate();
    return;
  }

  auto* target_unit = target->get_component<Engine::Core::UnitComponent>();
  if (target_unit == nullptr || target_unit->health <= 0) {
    projectile->deactivate();
    return;
  }

  const auto* special_attack = resolve_special_attack(world, projectile);
  switch (arrow_projectile->get_kind()) {
  case ProjectileKind::Fireball: {
    QVector3D const impact_pos = projectile->get_target_locked_position();
    float const radius = std::max(0.1F, arrow_projectile->impact_radius());
    float const radius_sq = radius * radius;
    int const attacker_owner = projectile_owner_id(world, projectile);
    float const fire_bonus =
        (special_attack != nullptr)
            ? special_attack->bonus_damage_multiplier_vs_fire_vulnerable
            : 1.0F;

    for (auto* candidate : world->get_entities_with<Engine::Core::UnitComponent>()) {
      if (candidate == nullptr ||
          candidate->has_component<Engine::Core::PendingRemovalComponent>()) {
        continue;
      }

      auto* candidate_unit = candidate->get_component<Engine::Core::UnitComponent>();
      auto* candidate_transform =
          candidate->get_component<Engine::Core::TransformComponent>();
      if (candidate_unit == nullptr || candidate_transform == nullptr ||
          candidate_unit->health <= 0) {
        continue;
      }
      if (!can_affect_with_fire(attacker_owner,
                                arrow_projectile->friendly_fire(),
                                projectile->get_attacker_id(),
                                candidate,
                                candidate_unit)) {
        continue;
      }

      const float dx = candidate_transform->position.x - impact_pos.x();
      const float dz = candidate_transform->position.z - impact_pos.z();
      const float distance_sq = dx * dx + dz * dz;
      if (distance_sq > radius_sq) {
        continue;
      }

      int damage = projectile->get_damage();
      if (candidate->get_id() != projectile->get_target_id()) {
        damage = static_cast<int>(std::round(
            static_cast<float>(damage) * arrow_projectile->splash_damage_multiplier()));
      }
      if (auto* undead = candidate->get_component<Engine::Core::UndeadComponent>();
          undead != nullptr) {
        damage = static_cast<int>(std::round(
            static_cast<float>(damage) * undead->fire_damage_multiplier * fire_bonus));
      }

      apply_projectile_damage(
          world, candidate, std::max(1, damage), projectile->get_attacker_id());

      apply_fireball_burning_status(world, candidate, projectile, special_attack);
    }

    spawn_fire_patch(world, impact_pos, projectile, special_attack);
    projectile->deactivate();
    return;
  }
  case ProjectileKind::CursedArrow:
    apply_projectile_damage(
        world, target, projectile->get_damage(), projectile->get_attacker_id());
    apply_cursed_status(target, special_attack);
    projectile->deactivate();
    return;
  case ProjectileKind::Arrow:
  case ProjectileKind::Stone:
  default:
    apply_projectile_damage(
        world, target, projectile->get_damage(), projectile->get_attacker_id());
    projectile->deactivate();
    return;
  }
}

} // namespace

ProjectileSystem::ProjectileSystem()
    : m_arrow_config(GameConfig::instance().arrow()) {
}

void ProjectileSystem::spawn_arrow(const QVector3D& start,
                                   const QVector3D& end,
                                   const QVector3D& color,
                                   float speed,
                                   bool is_ballista_bolt,
                                   ProjectileKind kind,
                                   bool should_apply_damage,
                                   int damage,
                                   Engine::Core::EntityID attacker_id,
                                   Engine::Core::EntityID target_id,
                                   float impact_radius,
                                   float splash_damage_multiplier,
                                   bool friendly_fire) {
  QVector3D const delta = end - start;
  float const dist = delta.length();

  float arc_height = NAN;
  if (is_ballista_bolt) {
    arc_height = std::clamp(m_arrow_config.arc_height_multiplier * dist * 0.4F,
                            m_arrow_config.arc_height_min * 0.5F,
                            m_arrow_config.arc_height_max * 0.6F);
  } else {
    arc_height = std::clamp(m_arrow_config.arc_height_multiplier * dist,
                            m_arrow_config.arc_height_min,
                            m_arrow_config.arc_height_max);
  }
  float const inv_dist = (dist > 0.001F) ? (1.0F / dist) : 1.0F;

  m_projectiles.push_back(std::make_unique<ArrowProjectile>(start,
                                                            end,
                                                            color,
                                                            speed,
                                                            arc_height,
                                                            inv_dist,
                                                            is_ballista_bolt,
                                                            kind,
                                                            should_apply_damage,
                                                            damage,
                                                            attacker_id,
                                                            target_id,
                                                            impact_radius,
                                                            splash_damage_multiplier,
                                                            friendly_fire));
}

void ProjectileSystem::spawn_stone(const QVector3D& start,
                                   const QVector3D& end,
                                   const QVector3D& color,
                                   float speed,
                                   float scale,
                                   bool should_apply_damage,
                                   int damage,
                                   Engine::Core::EntityID attacker_id,
                                   Engine::Core::EntityID target_id) {
  QVector3D const delta = end - start;
  float const dist = delta.length();

  constexpr float k_stone_arc_multiplier = 0.35F;
  constexpr float k_stone_arc_min = 1.0F;
  constexpr float k_stone_arc_max = 4.0F;
  float const arc_height =
      std::clamp(k_stone_arc_multiplier * dist, k_stone_arc_min, k_stone_arc_max);
  float const inv_dist = (dist > 0.001F) ? (1.0F / dist) : 1.0F;

  m_projectiles.push_back(std::make_unique<StoneProjectile>(start,
                                                            end,
                                                            color,
                                                            speed,
                                                            arc_height,
                                                            inv_dist,
                                                            scale,
                                                            should_apply_damage,
                                                            damage,
                                                            attacker_id,
                                                            target_id));
}

void ProjectileSystem::update(Engine::Core::World* world, float delta_time) {
  if (world != nullptr) {
    for (auto* entity :
         world->get_entities_with<Engine::Core::CursedStatusComponent>()) {
      if (entity == nullptr) {
        continue;
      }
      auto* cursed = entity->get_component<Engine::Core::CursedStatusComponent>();
      if (cursed == nullptr) {
        continue;
      }
      cursed->remaining_duration =
          std::max(0.0F, cursed->remaining_duration - delta_time);
      if (cursed->remaining_duration <= 0.0F) {
        entity->remove_component<Engine::Core::CursedStatusComponent>();
      }
    }

    update_burning_statuses(world, delta_time);
    update_fire_patches(world, delta_time);
  }

  for (auto& projectile : m_projectiles) {
    projectile->update(delta_time);

    if (projectile->should_apply_damage() && world != nullptr) {
      apply_impact_damage(world, projectile.get());
    }
  }

  m_projectiles.erase(
      std::remove_if(m_projectiles.begin(),
                     m_projectiles.end(),
                     [](const ProjectilePtr& p) { return !p->is_active(); }),
      m_projectiles.end());
}

void ProjectileSystem::apply_impact_damage(Engine::Core::World* world,
                                           Projectile* projectile) {
  if (projectile == nullptr) {
    return;
  }

  if (projectile->get_progress() < k_min_progress_for_impact) {
    return;
  }

  if (projectile->get_target_id() == 0) {
    return;
  }

  if (auto* arrow_projectile = dynamic_cast<ArrowProjectile*>(projectile);
      arrow_projectile != nullptr) {
    apply_arrow_like_impact(world, projectile, arrow_projectile);
    return;
  }

  auto* target = world->get_entity(projectile->get_target_id());
  if (target == nullptr ||
      target->has_component<Engine::Core::PendingRemovalComponent>() ||
      target_escaped_impact(target, projectile)) {
    projectile->deactivate();
    return;
  }

  auto* target_unit = target->get_component<Engine::Core::UnitComponent>();
  if (target_unit == nullptr || target_unit->health <= 0) {
    projectile->deactivate();
    return;
  }

  target_unit->health -= projectile->get_damage();
  projectile->deactivate();
  if (target_unit->health <= 0) {
    target_unit->health = 0;

    Engine::Core::EventManager::instance().publish(
        Engine::Core::UnitDiedEvent(projectile->get_target_id(),
                                    target_unit->owner_id,
                                    target_unit->spawn_type,
                                    projectile->get_attacker_id(),
                                    projectile_owner_id(world, projectile)));
  }
}

} // namespace Game::Systems
