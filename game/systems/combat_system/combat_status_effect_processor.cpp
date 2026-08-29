#include "combat_status_effect_processor.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "../../core/component.h"
#include "../../core/world.h"
#include "../combat_actions/combat_action_definition.h"
#include "../combat_rules.h"
#include "../projectile_kind.h"
#include "combat_hit_resolver.h"
#include "structure_fire.h"

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
  std::vector<Engine::Core::EntityID> expired;
  for (auto [entity_id, cursed] : world->view<Engine::Core::CursedStatusComponent>()) {
    if (world->has<Engine::Core::PendingRemovalComponent>(entity_id)) {
      continue;
    }

    cursed.remaining_duration = std::max(0.0F, cursed.remaining_duration - delta_time);
    if (cursed.remaining_duration <= 0.0F) {
      expired.push_back(entity_id);
    }
  }
  for (const Engine::Core::EntityID entity_id : expired) {
    world->remove<Engine::Core::CursedStatusComponent>(entity_id);
    ++result.expired_curses;
  }
}

auto process_burning_statuses(Engine::Core::World* world,
                              float delta_time,
                              CombatStatusEffectUpdateResult& result) -> void {
  std::vector<Engine::Core::EntityID> expired;
  for (auto [entity, burning] :
       world->entity_view<Engine::Core::BurningStatusComponent>()) {
    const Engine::Core::EntityID entity_id = entity.get_id();
    if (world->has<Engine::Core::PendingRemovalComponent>(entity_id)) {
      continue;
    }

    auto* unit = world->try_get<Engine::Core::UnitComponent>(entity_id);
    if (unit == nullptr || unit->health <= 0) {
      expired.push_back(entity_id);
      continue;
    }

    burning.remaining_duration =
        std::max(0.0F, burning.remaining_duration - delta_time);
    burning.ignition_elapsed += delta_time;
    burning.tick_accumulator += delta_time;

    float const tick_interval = std::max(0.05F, burning.tick_interval);
    bool extinguished = false;
    while (burning.remaining_duration > 0.0F &&
           burning.tick_accumulator >= tick_interval) {
      burning.tick_accumulator -= tick_interval;

      int damage = burning.damage_per_tick;
      if (auto* undead = world->try_get<Engine::Core::UndeadComponent>(entity_id);
          undead != nullptr) {
        damage = static_cast<int>(
            std::round(static_cast<float>(damage) * undead->fire_damage_multiplier *
                       burning.fire_bonus_multiplier));
      }

      if (apply_burning_tick_damage(
              world, entity, std::max(1, damage), burning.attacker_id)) {
        ++result.burning_ticks;
      }

      unit = world->try_get<Engine::Core::UnitComponent>(entity_id);
      if (unit == nullptr || unit->health <= 0) {
        extinguished = true;
        break;
      }
    }

    if (extinguished || burning.remaining_duration <= 0.0F) {
      expired.push_back(entity_id);
    }
  }

  for (const Engine::Core::EntityID entity_id : expired) {
    if (world->remove<Engine::Core::BurningStatusComponent>(entity_id)) {
      ++result.expired_burning_statuses;
    }
  }
}

auto process_fire_patches(Engine::Core::World* world,
                          float delta_time,
                          CombatStatusEffectUpdateResult& result) -> void {
  if (world->entities_with<Engine::Core::FirePatchComponent>().empty()) {
    return;
  }

  auto units = world->collect_entities_with<Engine::Core::UnitComponent>();
  for (auto [entity, fire_patch, transform] :
       world->entity_view<Engine::Core::FirePatchComponent,
                          Engine::Core::TransformComponent>()) {
    (void)transform;
    const Engine::Core::EntityID entity_id = entity.get_id();
    if (world->has<Engine::Core::PendingRemovalComponent>(entity_id)) {
      continue;
    }

    fire_patch.remaining_duration =
        std::max(0.0F, fire_patch.remaining_duration - delta_time);
    if (fire_patch.remaining_duration <= 0.0F) {
      world->emplace<Engine::Core::PendingRemovalComponent>(entity_id);
      ++result.expired_fire_patches;
      continue;
    }

    for (auto* candidate : units) {
      if (candidate == nullptr ||
          candidate->has_component<Engine::Core::PendingRemovalComponent>()) {
        continue;
      }

      if (apply_fire_patch_contact_effect(world, entity, *candidate)) {
        ++result.fire_patch_contacts;
      }
    }
  }
}

} // namespace

void process_stagger_recovery(Engine::Core::World* world, float delta_time) {
  std::vector<Engine::Core::EntityID> recovered;
  for (auto [entity, stagger] : world->entity_view<Engine::Core::StaggerComponent>()) {
    if (Game::Systems::CombatRules::uses_rpg_combat_rules(&entity)) {
      continue;
    }
    stagger.remaining -= delta_time;
    if (stagger.remaining <= 0.0F) {
      recovered.push_back(entity.get_id());
    }
  }
  for (const Engine::Core::EntityID entity_id : recovered) {
    world->remove<Engine::Core::StaggerComponent>(entity_id);
  }
}

void process_signature_presentations(Engine::Core::World* world, float delta_time) {
  for (auto [entity_id, presentation] :
       world->view<Engine::Core::CommanderSignaturePresentationComponent>()) {
    (void)entity_id;
    for (auto& entry : presentation.entries) {
      entry.age += delta_time;
    }
    std::erase_if(presentation.entries,
                  [](auto const& entry) { return entry.age >= entry.lifetime; });
  }
}

auto process_combat_status_effects(Engine::Core::World* world,
                                   float delta_time) -> CombatStatusEffectUpdateResult {
  CombatStatusEffectUpdateResult result;
  if (world == nullptr) {
    return result;
  }

  float const clamped_delta_time = std::max(0.0F, delta_time);
  process_stagger_recovery(world, clamped_delta_time);
  process_signature_presentations(world, clamped_delta_time);
  process_cursed_statuses(world, clamped_delta_time, result);
  process_burning_statuses(world, clamped_delta_time, result);
  process_fire_patches(world, clamped_delta_time, result);

  auto const structure_fires = process_structure_fires(world, clamped_delta_time);
  result.burning_structures = structure_fires.burning_structures;
  result.structure_fire_ticks = structure_fires.fire_ticks;
  result.extinguished_structure_fires = structure_fires.extinguished_fires;
  return result;
}

} // namespace Game::Systems::Combat
