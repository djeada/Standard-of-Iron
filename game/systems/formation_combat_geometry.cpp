#include "formation_combat_geometry.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

#include "../core/component.h"
#include "../units/spawn_type.h"
#include "../units/troop_config.h"
#include "formation_system.h"
#include "troop_profile_service.h"

namespace Game::Systems::FormationCombat {
namespace {

// The elephant's trample radius is an area-of-effect gameplay radius, not the
// visible animal's collision radius.  Treating the full 2.5 m trample reach as
// body geometry makes melee lock several metres before the head reaches a
// soldier.  This radius follows the authored mesh's roughly 1.1 m half-length.
constexpr float k_elephant_visual_body_radius = 1.15F;
// Drive through roughly one infantry rank before locking.  The chase target is
// intentionally deeper than the accepted contact depth so movement's arrival
// tolerance cannot turn this back into a shallow front-line touch.
constexpr float k_elephant_chase_penetration = 1.50F;
constexpr float k_elephant_contact_penetration = 1.10F;

auto is_mounted(Game::Units::SpawnType spawn_type) noexcept -> bool {
  using Game::Units::SpawnType;
  return spawn_type == SpawnType::MountedKnight ||
         spawn_type == SpawnType::HorseArcher || spawn_type == SpawnType::HorseSpearman;
}

auto formation_category(const Engine::Core::UnitComponent& unit) noexcept
    -> FormationUnitCategory {

  return is_mounted(unit.spawn_type) ? FormationUnitCategory::Cavalry
                                     : FormationUnitCategory::Infantry;
}

auto world_slot(const Engine::Core::TransformComponent& transform,
                float local_x,
                float local_z) noexcept -> std::pair<float, float> {
  float const yaw = transform.rotation.y * std::numbers::pi_v<float> / 180.0F;
  float const sin_yaw = std::sin(yaw);
  float const cos_yaw = std::cos(yaw);
  float const world_x = transform.position.x + cos_yaw * local_x + sin_yaw * local_z;
  float const world_z = transform.position.z - sin_yaw * local_x + cos_yaw * local_z;
  return {world_x, world_z};
}

auto slot_distance(const SoldierSlot& lhs, const SoldierSlot& rhs) noexcept -> float {
  float const dx = rhs.world_x - lhs.world_x;
  float const dz = rhs.world_z - lhs.world_z;
  return std::sqrt(dx * dx + dz * dz);
}

} // namespace

auto formation_seed(const Engine::Core::Entity& entity) noexcept -> std::uint32_t {
  auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
  std::uint32_t seed = entity.get_id() * 0x9E3779B9U;
  if (unit != nullptr) {
    seed ^= static_cast<std::uint32_t>(unit->owner_id) * 0x85EBCA6BU;
  }
  return seed;
}

auto resolve_definition(const Engine::Core::UnitComponent& unit)
    -> FormationDefinition {
  FormationDefinition definition;
  definition.total_count =
      Game::Units::TroopConfig::instance().get_individuals_per_unit(unit.spawn_type);
  definition.max_per_row =
      Game::Units::TroopConfig::instance().get_max_units_per_row(unit.spawn_type);
  definition.spacing =
      Game::Units::TroopConfig::instance().get_formation_spacing(unit.spawn_type);
  definition.category = formation_category(unit);

  if (auto troop_type = Game::Units::spawn_typeToTroopType(unit.spawn_type);
      unit.uses_nation_formation_profile && troop_type) {
    auto const profile =
        TroopProfileService::instance().get_profile(unit.nation_id, *troop_type);
    return resolve_definition(unit, profile);
  }

  if (unit.render_individuals_per_unit_override > 0) {
    definition.total_count = unit.render_individuals_per_unit_override;
  }
  definition.total_count = std::max(1, definition.total_count);
  definition.max_per_row =
      std::clamp(definition.max_per_row, 1, definition.total_count);
  definition.spacing = std::max(0.1F, definition.spacing);
  return definition;
}

auto resolve_definition(const Engine::Core::UnitComponent& unit,
                        const TroopProfile& profile) -> FormationDefinition {
  FormationDefinition definition;
  definition.total_count = profile.individuals_per_unit;
  definition.max_per_row = profile.max_units_per_row;
  definition.spacing = profile.visuals.formation_spacing;
  definition.type = profile.formation_type;
  definition.category = formation_category(unit);
  if (unit.render_individuals_per_unit_override > 0) {
    definition.total_count = unit.render_individuals_per_unit_override;
  }
  definition.total_count = std::max(1, definition.total_count);
  definition.max_per_row =
      std::clamp(definition.max_per_row, 1, definition.total_count);
  definition.spacing = std::max(0.1F, definition.spacing);
  return definition;
}

auto resolve_layout(const Engine::Core::Entity& entity) -> FormationLayout {
  FormationLayout result;
  auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
  auto const* transform = entity.get_component<Engine::Core::TransformComponent>();
  if (unit == nullptr) {
    return result;
  }

  Engine::Core::TransformComponent const identity_transform{};
  auto const& resolved_transform =
      transform != nullptr ? *transform : identity_transform;

  bool const rigid_body = entity.has_component<Engine::Core::BuildingComponent>() ||
                          entity.has_component<Engine::Core::ElephantComponent>();
  if (rigid_body) {
    result.total_count = 1;
    result.live_count = unit->health > 0 ? 1 : 0;
    result.rows = 1;
    result.cols = 1;
    result.body_radius = std::max(
        0.05F, std::max(resolved_transform.scale.x, resolved_transform.scale.z) * 0.5F);
    if (entity.has_component<Engine::Core::ElephantComponent>()) {
      float const visual_scale = std::max(
          0.05F,
          std::max(std::abs(resolved_transform.scale.x),
                   std::abs(resolved_transform.scale.z)));
      result.body_radius =
          std::max(result.body_radius, k_elephant_visual_body_radius * visual_scale);
    }
    result.seed = formation_seed(entity);
    if (result.live_count > 0) {
      SoldierSlot const slot{0U,
                             0U,
                             0U,
                             0.0F,
                             0.0F,
                             0.0F,
                             resolved_transform.position.x,
                             resolved_transform.position.z};
      result.all_slots.push_back(slot);
      result.live_slots.push_back(slot);
      result.occupied_slots.push_back(slot);
    }
    return result;
  }

  auto const definition = resolve_definition(*unit);
  result.total_count = definition.total_count;
  result.cols = definition.max_per_row;
  result.rows = std::max(1, (result.total_count + result.cols - 1) / result.cols);
  result.live_count = Engine::Core::resolve_surviving_individual_count(
      unit->health, unit->max_health, result.total_count);
  result.spacing = definition.spacing;
  result.body_radius = std::max(
      0.05F, std::max(resolved_transform.scale.x, resolved_transform.scale.z) * 0.5F);
  result.seed = formation_seed(entity);

  result.all_slots.reserve(static_cast<std::size_t>(result.total_count));
  result.live_slots.reserve(static_cast<std::size_t>(result.live_count));
  result.occupied_slots.reserve(static_cast<std::size_t>(result.total_count));
  auto resolve_slot = [&](int idx) {
    int const row = result.rows - 1 - idx / result.cols;
    int const col = idx % result.cols;
    auto const offset =
        FormationSystem::instance().get_local_offset(definition.type,
                                                     definition.category,
                                                     idx,
                                                     row,
                                                     col,
                                                     result.rows,
                                                     result.cols,
                                                     result.spacing,
                                                     result.seed);
    auto const [world_x, world_z] =
        world_slot(resolved_transform, offset.offset_x, offset.offset_z);
    return SoldierSlot{static_cast<std::uint16_t>(idx),
                       static_cast<std::uint16_t>(row),
                       static_cast<std::uint16_t>(col),
                       offset.offset_x,
                       offset.offset_z,
                       offset.yaw_offset,
                       world_x,
                       world_z};
  };
  for (int idx = 0; idx < result.total_count; ++idx) {
    result.all_slots.push_back(resolve_slot(idx));
  }
  int const retired_count = result.total_count - result.live_count;
  std::vector<bool> active_casualty_slots(static_cast<std::size_t>(result.total_count),
                                          false);
  if (auto const* casualties =
          entity.get_component<Engine::Core::SoldierCasualtyAnimationComponent>()) {
    for (auto const& casualty : casualties->entries) {
      int const idx = static_cast<int>(casualty.slot_index);
      if (idx >= 0 && idx < result.total_count) {
        active_casualty_slots[static_cast<std::size_t>(idx)] = true;
      }
    }
  }

  for (int idx = retired_count; idx < result.total_count; ++idx) {
    auto const& slot = result.all_slots[static_cast<std::size_t>(idx)];
    result.live_slots.push_back(slot);
    result.occupied_slots.push_back(slot);
  }
  for (int idx = 0; idx < retired_count; ++idx) {
    if (active_casualty_slots[static_cast<std::size_t>(idx)]) {
      result.occupied_slots.push_back(result.all_slots[static_cast<std::size_t>(idx)]);
    }
  }
  return result;
}

auto has_formation_slots(const Engine::Core::Entity& entity) -> bool {
  if (entity.has_component<Engine::Core::BuildingComponent>() ||
      entity.has_component<Engine::Core::ElephantComponent>()) {
    return false;
  }
  auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
  return unit != nullptr && resolve_definition(*unit).total_count > 1;
}

auto contact_geometry(const Engine::Core::Entity& attacker,
                      const Engine::Core::Entity& target) -> ContactGeometry {
  ContactGeometry result;
  auto const* attacker_transform =
      attacker.get_component<Engine::Core::TransformComponent>();
  auto const* target_transform =
      target.get_component<Engine::Core::TransformComponent>();
  if (attacker_transform == nullptr || target_transform == nullptr) {
    result.surface_gap = std::numeric_limits<float>::infinity();
    return result;
  }

  float const dx = target_transform->position.x - attacker_transform->position.x;
  float const dz = target_transform->position.z - attacker_transform->position.z;
  result.center_distance = std::sqrt(dx * dx + dz * dz);
  result.uses_formation_slots =
      has_formation_slots(attacker) || has_formation_slots(target);
  if (!result.uses_formation_slots) {
    result.surface_gap = result.center_distance;
    result.contact_center_distance = 0.0F;
    result.engagement_center_distance = 0.0F;
    return result;
  }

  auto const attacker_layout = resolve_layout(attacker);
  auto const target_layout = resolve_layout(target);
  result.formation_overlap_required =
      has_formation_slots(attacker) && has_formation_slots(target);

  result.contact_tolerance =
      std::min(attacker_layout.body_radius, target_layout.body_radius) * 0.15F;
  float nearest = std::numeric_limits<float>::infinity();
  for (auto const& attacker_slot : attacker_layout.occupied_slots) {
    for (auto const& target_slot : target_layout.occupied_slots) {
      nearest = std::min(nearest, slot_distance(attacker_slot, target_slot));
    }
  }
  result.surface_gap =
      nearest - attacker_layout.body_radius - target_layout.body_radius;

  if (result.center_distance > 0.0001F) {
    float const dir_x = dx / result.center_distance;
    float const dir_z = dz / result.center_distance;
    float const contact_radius =
        attacker_layout.body_radius + target_layout.body_radius;
    float const contact_radius_sq = contact_radius * contact_radius;

    for (auto const& attacker_slot : attacker_layout.occupied_slots) {
      float const attacker_offset_x =
          attacker_slot.world_x - attacker_transform->position.x;
      float const attacker_offset_z =
          attacker_slot.world_z - attacker_transform->position.z;
      for (auto const& target_slot : target_layout.occupied_slots) {
        float const relative_x =
            (target_slot.world_x - target_transform->position.x) - attacker_offset_x;
        float const relative_z =
            (target_slot.world_z - target_transform->position.z) - attacker_offset_z;
        float const parallel = relative_x * dir_x + relative_z * dir_z;
        float const relative_sq = relative_x * relative_x + relative_z * relative_z;
        float const lateral_sq = std::max(0.0F, relative_sq - parallel * parallel);
        if (lateral_sq > contact_radius_sq) {
          continue;
        }
        float const candidate = -parallel + std::sqrt(contact_radius_sq - lateral_sq);
        result.contact_center_distance =
            std::max(result.contact_center_distance, candidate);
      }
    }
  }
  if (result.formation_overlap_required) {

    float const rank_spacing = std::min(attacker_layout.spacing, target_layout.spacing);
    float const body_radius =
        std::max(attacker_layout.body_radius, target_layout.body_radius);
    result.engagement_center_distance =
        std::min(result.contact_center_distance,
                 std::max(rank_spacing * 0.30F, body_radius * 0.5F));
  } else if (attacker.has_component<Engine::Core::ElephantComponent>() &&
             has_formation_slots(target)) {
    // A war elephant must drive into the infantry footprint before combat
    // locks it in place.  Crossing a full front rank gives the charge the
    // intended mass instead of merely touching the leading soldier.
    result.engagement_center_distance = std::max(
        0.0F,
        result.center_distance -
            (result.surface_gap + k_elephant_chase_penetration));
  } else {
    result.engagement_center_distance = result.contact_center_distance;
  }
  return result;
}

auto contact_is_active(const Engine::Core::Entity& attacker,
                       const Engine::Core::Entity& target,
                       const ContactGeometry& geometry) -> bool {
  if (!geometry.uses_formation_slots) {
    return false;
  }
  constexpr float k_contact_numeric_epsilon = 0.001F;
  auto const* previous =
      attacker.get_component<Engine::Core::FormationContactComponent>();
  if (previous != nullptr && previous->in_contact &&
      previous->target_id == target.get_id()) {
    return true;
  }
  if (geometry.formation_overlap_required) {
    bool const deep_front_rank_overlap =
        geometry.center_distance <=
        geometry.engagement_center_distance + k_contact_numeric_epsilon;
    auto const* attacker_attack =
        attacker.get_component<Engine::Core::AttackComponent>();
    // A mounted charge can establish the melee lock while the two formation
    // roots are still slightly outside the normal deep-overlap center. Once
    // their visible soldier footprints overlap, that committed lock is valid
    // contact and must be allowed to transition into ordinary melee attacks.
    bool const locked_visible_overlap =
        attacker_attack != nullptr && attacker_attack->in_melee_lock &&
        attacker_attack->melee_lock_target_id == target.get_id() &&
        geometry.surface_gap <= k_contact_numeric_epsilon;
    auto const* target_attack = target.get_component<Engine::Core::AttackComponent>();
    bool const occupied_battle_edge =
        target_attack != nullptr && target_attack->in_melee_lock &&
        target_attack->melee_lock_target_id != 0U &&
        target_attack->melee_lock_target_id != attacker.get_id();
    bool const reinforcement_has_visible_overlap =
        occupied_battle_edge && geometry.surface_gap <= -(geometry.contact_tolerance +
                                                          k_contact_numeric_epsilon);
    return deep_front_rank_overlap || locked_visible_overlap ||
           reinforcement_has_visible_overlap;
  }
  if (attacker.has_component<Engine::Core::ElephantComponent>() &&
      has_formation_slots(target)) {
    return geometry.surface_gap <=
           -k_elephant_contact_penetration + k_contact_numeric_epsilon;
  }
  return geometry.surface_gap <= k_contact_numeric_epsilon;
}

auto engaged_soldiers(const Engine::Core::Entity& attacker,
                      const Engine::Core::Entity& target)
    -> std::vector<std::uint16_t> {
  std::vector<std::uint16_t> engaged;
  for (auto const& pair : engagement_pairs(attacker, target)) {
    engaged.push_back(pair.attacker_slot);
  }
  return engaged;
}

auto engagement_pairs(const Engine::Core::Entity& attacker,
                      const Engine::Core::Entity& target)
    -> std::vector<Engine::Core::FormationEngagementPair> {
  using Pair = Engine::Core::FormationEngagementPair;
  std::vector<Pair> result;
  auto const geometry = contact_geometry(attacker, target);
  if (!geometry.uses_formation_slots) {
    return result;
  }

  auto const attacker_layout = resolve_layout(attacker);
  auto const target_layout = resolve_layout(target);
  auto const* attacker_transform =
      attacker.get_component<Engine::Core::TransformComponent>();
  auto const* target_transform =
      target.get_component<Engine::Core::TransformComponent>();
  if (attacker_transform == nullptr || target_transform == nullptr ||
      attacker_layout.live_slots.empty() || target_layout.live_slots.empty()) {
    return result;
  }

  float const contact_distance =
      attacker_layout.body_radius + target_layout.body_radius;
  for (auto const& attacker_slot : attacker_layout.live_slots) {
    auto const* closest_target = &target_layout.live_slots.front();
    float closest_distance = slot_distance(attacker_slot, *closest_target);
    for (auto const& target_slot : target_layout.live_slots) {
      float const distance = slot_distance(attacker_slot, target_slot);
      if (distance < closest_distance ||
          (distance == closest_distance && target_slot.index < closest_target->index)) {
        closest_target = &target_slot;
        closest_distance = distance;
      }
    }
    result.push_back({attacker_slot.index,
                      closest_target->index,
                      closest_distance,
                      closest_distance - contact_distance});
  }
  std::sort(result.begin(), result.end(), [](auto const& lhs, auto const& rhs) {
    return lhs.attacker_slot < rhs.attacker_slot;
  });
  return result;
}

} // namespace Game::Systems::FormationCombat
