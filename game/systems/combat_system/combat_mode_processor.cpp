#include "combat_mode_processor.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "../../core/ambient_session.h"
#include "../../core/component_structures.h"
#include "../../core/world.h"
#include "../formation_combat_geometry.h"
#include "../owner_registry.h"
#include "combat_utils.h"

namespace Game::Systems::Combat {

void update_combat_mode(Engine::Core::Entity* attacker,
                        Engine::Core::World* world,
                        Engine::Core::AttackComponent* attack_comp) {
  if (attack_comp == nullptr) {
    return;
  }

  bool const in_melee_combat = in_rts_melee_lock(attacker);
  if (in_melee_combat) {

    attack_comp->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
    return;
  }

  if (attack_comp->preferred_mode != Engine::Core::AttackComponent::CombatMode::Auto) {
    attack_comp->current_mode = attack_comp->preferred_mode;
    return;
  }

  bool const has_attack_target =
      attacker->has_component<Engine::Core::AttackTargetComponent>();

  if (!in_melee_combat && !has_attack_target) {

    if (attack_comp->can_ranged) {
      attack_comp->current_mode = Engine::Core::AttackComponent::CombatMode::Ranged;
    } else {
      attack_comp->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
    }
    return;
  }

  if (!attack_comp->can_ranged) {
    attack_comp->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
    return;
  }
  if (!attack_comp->can_melee) {
    attack_comp->current_mode = Engine::Core::AttackComponent::CombatMode::Ranged;
    return;
  }

  auto* attacker_transform =
      attacker->get_component<Engine::Core::TransformComponent>();
  if (attacker_transform == nullptr) {
    return;
  }

  auto* attacker_unit = attacker->get_component<Engine::Core::UnitComponent>();
  if (attacker_unit == nullptr) {
    return;
  }

  auto& owner_registry = *Game::Session::services_for(*world).owners;

  const float reach = std::max(attack_comp->range, attack_comp->melee_range);
  static thread_local std::vector<Engine::Core::EntityID> nearby;
  collect_unit_ids_near(*world,
                        attacker_transform->position.x,
                        attacker_transform->position.z,
                        reach + 2.0F * FormationCombat::max_contact_extent(),
                        nearby);

  struct Candidate {
    Engine::Core::Entity* entity{nullptr};
    float center_distance{0.0F};
    float height_diff{0.0F};
    float extent{0.0F};
  };
  static thread_local std::vector<Candidate> candidates;
  candidates.clear();
  for (const Engine::Core::EntityID target_id : nearby) {
    auto* target = world->get_entity(target_id);
    if (target == nullptr || target == attacker) {
      continue;
    }

    auto* target_unit = target->get_component<Engine::Core::UnitComponent>();
    if ((target_unit == nullptr) || target_unit->health <= 0) {
      continue;
    }

    if (owner_registry.are_allies(attacker_unit->owner_id, target_unit->owner_id)) {
      continue;
    }

    if (is_building(target)) {
      continue;
    }

    auto* target_transform = target->get_component<Engine::Core::TransformComponent>();
    if (target_transform == nullptr) {
      continue;
    }

    float const dx = target_transform->position.x - attacker_transform->position.x;
    float const dz = target_transform->position.z - attacker_transform->position.z;
    float const dy = target_transform->position.y - attacker_transform->position.y;
    candidates.push_back({.entity = target,
                          .center_distance = std::sqrt(dx * dx + dz * dz),
                          .height_diff = std::abs(dy),
                          .extent = std::max(combat_radius(target),
                                             FormationCombat::max_contact_extent())});
  }
  std::sort(candidates.begin(),
            candidates.end(),
            [](const Candidate& lhs, const Candidate& rhs) {
              if (lhs.center_distance != rhs.center_distance) {
                return lhs.center_distance < rhs.center_distance;
              }
              return lhs.entity->get_id() < rhs.entity->get_id();
            });

  float const attacker_extent = FormationCombat::max_contact_extent();
  float closest_enemy_dist_sq = std::numeric_limits<float>::max();
  float closest_surface_dist = std::numeric_limits<float>::max();
  float closest_height_diff = 0.0F;

  for (const Candidate& candidate : candidates) {
    float const nearest_possible =
        candidate.center_distance - attacker_extent - candidate.extent;
    if (nearest_possible >= closest_surface_dist) {
      break;
    }
    auto const geometry =
        FormationCombat::contact_geometry(*attacker, *candidate.entity);
    float const surface_dist =
        geometry.uses_formation_slots
            ? std::max(0.0F, geometry.surface_gap)
            : std::max(0.0F,
                       candidate.center_distance - combat_radius(candidate.entity));
    float const dist_sq = surface_dist * surface_dist;

    if (dist_sq < closest_enemy_dist_sq) {
      closest_enemy_dist_sq = dist_sq;
      closest_surface_dist = surface_dist;
      closest_height_diff = candidate.height_diff;
    }
  }

  if (closest_enemy_dist_sq == std::numeric_limits<float>::max()) {
    if (attack_comp->can_ranged) {
      attack_comp->current_mode = Engine::Core::AttackComponent::CombatMode::Ranged;
    } else {
      attack_comp->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
    }
    return;
  }

  float const closest_dist = std::sqrt(closest_enemy_dist_sq);

  bool const in_melee_range =
      attack_comp->is_in_melee_range(closest_dist, closest_height_diff);
  bool const in_ranged_range = attack_comp->is_in_ranged_range(closest_dist);

  if (in_melee_range && attack_comp->can_melee) {
    attack_comp->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
  } else if (in_ranged_range && attack_comp->can_ranged) {
    attack_comp->current_mode = Engine::Core::AttackComponent::CombatMode::Ranged;
  } else if (attack_comp->can_ranged) {
    attack_comp->current_mode = Engine::Core::AttackComponent::CombatMode::Ranged;
  } else {
    attack_comp->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
  }
}

} // namespace Game::Systems::Combat
