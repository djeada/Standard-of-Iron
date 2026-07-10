#include "combat_status_effect_processor.h"

#include <algorithm>
#include <cmath>

#include "../../core/component.h"
#include "../../core/world.h"
#include "../combat_actions/combat_action_definition.h"
#include "../projectile_kind.h"
#include "combat_hit_resolver.h"

namespace Game::Systems::Combat {

namespace {

[[nodiscard]] auto entity_position(const Engine::Core::Entity& entity) -> QVector3D {
  auto const* transform = entity.get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return {0.0F, 0.0F, 0.0F};
  }
  return {transform->position.x, transform->position.y, transform->position.z};
}

auto apply_burning_tick_damage(Engine::Core::World* world,
                               Engine::Core::Entity& target,
                               int damage,
                               Engine::Core::EntityID attacker_id) -> bool {
  if (world == nullptr || damage <= 0) {
    return false;
  }

  auto const result = resolve_projectile_impact_hit(
      world,
      {.contact = {.attacker_id = attacker_id,
                   .target_id = target.get_id(),
                   .weapon_family = Game::Systems::CombatActions::WeaponFamily::Bow,
                   .attack_family = Engine::Core::CombatAttackFamily::Bow,
                   .attack_direction = Engine::Core::AttackDirection::Thrust,
                   .contact_point = entity_position(target),
                   .from_projectile = true,
                   .projectile_kind = Game::Systems::ProjectileKind::Arrow},
       .explicit_raw_damage = damage});
  return result.applied;
}

auto process_cursed_statuses(Engine::Core::World* world,
                             float delta_time,
                             CombatStatusEffectUpdateResult& result) -> void {
  for (auto* entity : world->get_entities_with<Engine::Core::CursedStatusComponent>()) {
    if (entity == nullptr ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {
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
      ++result.expired_curses;
    }
  }
}

auto process_burning_statuses(Engine::Core::World* world,
                              float delta_time,
                              CombatStatusEffectUpdateResult& result) -> void {
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
      ++result.expired_burning_statuses;
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

      if (apply_burning_tick_damage(
              world, *entity, std::max(1, damage), burning->attacker_id)) {
        ++result.burning_ticks;
      }

      unit = entity->get_component<Engine::Core::UnitComponent>();
      if (unit == nullptr || unit->health <= 0) {
        entity->remove_component<Engine::Core::BurningStatusComponent>();
        ++result.expired_burning_statuses;
        break;
      }
    }

    if (entity->get_component<Engine::Core::BurningStatusComponent>() != nullptr &&
        burning->remaining_duration <= 0.0F) {
      entity->remove_component<Engine::Core::BurningStatusComponent>();
      ++result.expired_burning_statuses;
    }
  }
}

auto process_fire_patches(Engine::Core::World* world,
                          float delta_time,
                          CombatStatusEffectUpdateResult& result) -> void {
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
      ++result.expired_fire_patches;
      continue;
    }

    fire_patch->remaining_duration =
        std::max(0.0F, fire_patch->remaining_duration - delta_time);
    if (fire_patch->remaining_duration <= 0.0F) {
      entity->add_component<Engine::Core::PendingRemovalComponent>();
      ++result.expired_fire_patches;
      continue;
    }

    for (auto* candidate : units) {
      if (candidate == nullptr ||
          candidate->has_component<Engine::Core::PendingRemovalComponent>()) {
        continue;
      }

      if (apply_fire_patch_contact_effect(world, *entity, *candidate)) {
        ++result.fire_patch_contacts;
      }
    }
  }
}

} // namespace

auto process_combat_status_effects(Engine::Core::World* world,
                                   float delta_time) -> CombatStatusEffectUpdateResult {
  CombatStatusEffectUpdateResult result;
  if (world == nullptr) {
    return result;
  }

  float const clamped_delta_time = std::max(0.0F, delta_time);
  process_cursed_statuses(world, clamped_delta_time, result);
  process_burning_statuses(world, clamped_delta_time, result);
  process_fire_patches(world, clamped_delta_time, result);
  return result;
}

} // namespace Game::Systems::Combat
