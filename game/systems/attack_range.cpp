#include "attack_range.h"

#include <algorithm>
#include <cmath>

#include "../core/component_gameplay.h"
#include "../core/world.h"
#include "combat_system/combat_types.h"
#include "combat_system/combat_utils.h"

namespace Game::Systems {

namespace {

auto ring_is_redundant(const std::vector<AttackRangeRing>& kept,
                       const AttackRangeRing& candidate) -> bool {
  for (const auto& ring : kept) {
    if (ring.weapon_class != candidate.weapon_class) {
      continue;
    }
    float const radius_delta = std::abs(ring.max_radius - candidate.max_radius);
    if (radius_delta >
        candidate.max_radius * k_attack_range_duplicate_radius_tolerance) {
      continue;
    }
    float const dx = ring.world_x - candidate.world_x;
    float const dz = ring.world_z - candidate.world_z;
    float const limit = candidate.max_radius * k_attack_range_duplicate_center_factor;
    if (dx * dx + dz * dz <= limit * limit) {
      return true;
    }
  }
  return false;
}

auto planar_distance(const Engine::Core::TransformComponent& from,
                     const Engine::Core::TransformComponent& to) -> float {
  float const dx = to.position.x - from.position.x;
  float const dz = to.position.z - from.position.z;
  return std::hypot(dx, dz);
}

auto verdict_rank(RangeVerdict verdict) -> int {
  switch (verdict) {
  case RangeVerdict::InRange:
    return 4;
  case RangeVerdict::Blocked:
    return 3;
  case RangeVerdict::TooClose:
    return 2;
  case RangeVerdict::OutOfRange:
    return 1;
  case RangeVerdict::None:
    break;
  }
  return 0;
}

} // namespace

auto hold_mode_range_multiplier(const Engine::Core::Entity& entity,
                                Game::Units::SpawnType spawn_type) -> float {
  const auto* hold_mode = entity.get_component<Engine::Core::HoldModeComponent>();
  if ((hold_mode == nullptr) || !hold_mode->active) {
    return 1.0F;
  }
  if (spawn_type == Game::Units::SpawnType::Archer) {
    return Combat::Constants::k_range_multiplier_hold;
  }
  if (spawn_type == Game::Units::SpawnType::Spearman) {
    return Combat::Constants::k_range_multiplier_spearman_hold;
  }
  return 1.0F;
}

auto range_weapon_class(Game::Units::SpawnType spawn_type) -> RangeWeaponClass {
  switch (spawn_type) {
  case Game::Units::SpawnType::Archer:
  case Game::Units::SpawnType::HorseArcher:
  case Game::Units::SpawnType::SkeletonArcher:
  case Game::Units::SpawnType::RomanFieldCommander:
  case Game::Units::SpawnType::CarthageBowCommander:
    return RangeWeaponClass::Bow;
  case Game::Units::SpawnType::Catapult:
  case Game::Units::SpawnType::Ballista:
  case Game::Units::SpawnType::DefenseTower:
    return RangeWeaponClass::Siege;
  case Game::Units::SpawnType::GravePriest:
    return RangeWeaponClass::Arcane;
  default:
    break;
  }
  return RangeWeaponClass::None;
}

auto range_weapon_class_key(RangeWeaponClass weapon_class) -> std::string_view {
  switch (weapon_class) {
  case RangeWeaponClass::Bow:
    return "bow";
  case RangeWeaponClass::Siege:
    return "siege";
  case RangeWeaponClass::Arcane:
    return "arcane";
  case RangeWeaponClass::None:
    break;
  }
  return "none";
}

auto range_verdict_key(RangeVerdict verdict) -> std::string_view {
  switch (verdict) {
  case RangeVerdict::InRange:
    return "in_range";
  case RangeVerdict::TooClose:
    return "too_close";
  case RangeVerdict::OutOfRange:
    return "out_of_range";
  case RangeVerdict::Blocked:
    return "blocked";
  case RangeVerdict::None:
    break;
  }
  return "none";
}

auto resolve_attack_range(const Engine::Core::Entity& entity) -> AttackRangeProfile {
  AttackRangeProfile profile;

  const auto* attack = entity.get_component<Engine::Core::AttackComponent>();
  const auto* unit = entity.get_component<Engine::Core::UnitComponent>();
  if ((attack == nullptr) || (unit == nullptr) || !attack->can_ranged) {
    return profile;
  }

  float const multiplier = hold_mode_range_multiplier(entity, unit->spawn_type);
  profile.ranged = true;
  profile.max_range = attack->range * multiplier;
  profile.current_range = attack->get_current_range() * multiplier;
  profile.min_range = std::min(attack->min_range, profile.max_range);
  profile.weapon_class = range_weapon_class(unit->spawn_type);
  return profile;
}

auto collect_attack_range_rings(const AttackRangeRingRequest& request)
    -> std::vector<AttackRangeRing> {
  std::vector<AttackRangeRing> rings;
  if (request.world == nullptr || request.selection.empty() || request.max_rings == 0) {
    return rings;
  }

  std::vector<AttackRangeRing> candidates;
  candidates.reserve(request.selection.size());

  for (const auto entity_id : request.selection) {
    auto* entity = request.world->get_entity(entity_id);
    if (entity == nullptr ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->health <= 0 ||
        unit->owner_id != request.local_owner_id) {
      continue;
    }

    const auto profile = resolve_attack_range(*entity);
    if (!profile.ranged || profile.max_range <= 0.0F) {
      continue;
    }

    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      continue;
    }

    AttackRangeRing ring;
    ring.entity_id = entity_id;
    ring.world_x = transform->position.x;
    ring.world_y = transform->position.y;
    ring.world_z = transform->position.z;
    ring.max_radius = profile.max_range;
    ring.min_radius = profile.min_range;
    ring.weapon_class = profile.weapon_class;
    ring.focused = entity_id == request.focus_entity_id;
    candidates.push_back(ring);
  }

  if (candidates.empty()) {
    return rings;
  }

  if (candidates.size() == 1) {
    candidates.front().focused = true;
  }

  std::stable_sort(candidates.begin(),
                   candidates.end(),
                   [](const AttackRangeRing& lhs, const AttackRangeRing& rhs) {
                     if (lhs.focused != rhs.focused) {
                       return lhs.focused;
                     }
                     return lhs.max_radius > rhs.max_radius;
                   });

  rings.reserve(std::min(candidates.size(), request.max_rings));
  for (const auto& candidate : candidates) {
    if (rings.size() >= request.max_rings) {
      break;
    }
    if (!candidate.focused && ring_is_redundant(rings, candidate)) {
      continue;
    }
    rings.push_back(candidate);
  }
  return rings;
}

auto classify_range_to_target(Engine::Core::World* world,
                              std::span<const Engine::Core::EntityID> attackers,
                              Engine::Core::EntityID target_id) -> RangeVerdict {
  if (world == nullptr || target_id == 0 || attackers.empty()) {
    return RangeVerdict::None;
  }

  auto* target = world->get_entity(target_id);
  if (target == nullptr) {
    return RangeVerdict::None;
  }
  const auto* target_transform =
      target->get_component<Engine::Core::TransformComponent>();
  if (target_transform == nullptr) {
    return RangeVerdict::None;
  }

  RangeVerdict best = RangeVerdict::None;
  for (const auto attacker_id : attackers) {
    auto* attacker = world->get_entity(attacker_id);
    if (attacker == nullptr || attacker == target) {
      continue;
    }
    const auto profile = resolve_attack_range(*attacker);
    if (!profile.ranged) {
      continue;
    }
    const auto* attacker_transform =
        attacker->get_component<Engine::Core::TransformComponent>();
    if (attacker_transform == nullptr) {
      continue;
    }

    RangeVerdict verdict = RangeVerdict::OutOfRange;
    if (Combat::is_in_range(attacker, target, profile.current_range)) {
      verdict = RangeVerdict::InRange;
    } else {
      float const distance = planar_distance(*attacker_transform, *target_transform);
      float const reach = profile.max_range + Combat::combat_radius(target);
      if (profile.min_range > 0.0F && distance < profile.min_range) {
        verdict = RangeVerdict::TooClose;
      } else if (distance <= reach) {
        verdict = RangeVerdict::Blocked;
      }
    }

    if (verdict_rank(verdict) > verdict_rank(best)) {
      best = verdict;
    }
    if (best == RangeVerdict::InRange) {
      break;
    }
  }

  return best;
}

} // namespace Game::Systems
