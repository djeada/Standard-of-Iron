#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include "../core/component.h"
#include "formation_combat_geometry.h"
#include "formation_geometry_internal.h"

namespace Game::Systems::FormationCombat {
namespace {

auto slot_distance(const SoldierSlot& lhs, const SoldierSlot& rhs) noexcept -> float {
  float const dx = rhs.world_x - lhs.world_x;
  float const dz = rhs.world_z - lhs.world_z;
  return std::sqrt(dx * dx + dz * dz);
}

} // namespace

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
  thread_local FormationLayout attacker_layout;
  thread_local FormationLayout target_layout;
  resolve_layout_into(attacker, attacker_layout);
  resolve_layout_into(target, target_layout);
  return engagement_pairs(attacker, target, attacker_layout, target_layout);
}

auto engagement_pairs(const Engine::Core::Entity& attacker,
                      const Engine::Core::Entity& target,
                      const FormationLayout& attacker_layout,
                      const FormationLayout& target_layout)
    -> std::vector<Engine::Core::FormationEngagementPair> {
  using Pair = Engine::Core::FormationEngagementPair;
  std::vector<Pair> result;
  if (!has_formation_slots(attacker) && !has_formation_slots(target)) {
    return result;
  }
  auto const* attacker_transform =
      attacker.get_component<Engine::Core::TransformComponent>();
  auto const* target_transform =
      target.get_component<Engine::Core::TransformComponent>();
  if (attacker_transform == nullptr || target_transform == nullptr ||
      attacker_layout.live_slots.empty() || target_layout.live_slots.empty()) {
    return result;
  }

  FormationLayout spatial_attacker_layout;
  FormationLayout spatial_target_layout;
  Detail::spatialize_layout_into(attacker, attacker_layout, spatial_attacker_layout);
  Detail::spatialize_layout_into(target, target_layout, spatial_target_layout);
  float const contact_distance =
      spatial_attacker_layout.body_radius + spatial_target_layout.body_radius;
  for (auto const& attacker_slot : spatial_attacker_layout.live_slots) {
    auto const* closest_target = &spatial_target_layout.live_slots.front();
    float closest_distance = slot_distance(attacker_slot, *closest_target);
    for (auto const& target_slot : spatial_target_layout.live_slots) {
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

auto select_damage_engagement_pair(
    const Engine::Core::Entity& attacker,
    Engine::Core::EntityID opponent_id,
    const std::vector<Engine::Core::FormationEngagementPair>& pairs)
    -> std::optional<Engine::Core::FormationEngagementPair> {
  if (pairs.empty()) {
    return std::nullopt;
  }

  auto const closest = std::min_element(
      pairs.begin(), pairs.end(), [](auto const& lhs, auto const& rhs) {
        if (lhs.surface_gap != rhs.surface_gap) {
          return lhs.surface_gap < rhs.surface_gap;
        }
        return lhs.attacker_slot < rhs.attacker_slot;
      });
  float const spacing = resolve_layout_spacing(attacker);
  float const equivalent_contact_band = std::max(0.05F, spacing * 0.18F);
  std::size_t contact_candidate_count = 0U;
  for (auto const& pair : pairs) {
    if (pair.surface_gap <= closest->surface_gap + equivalent_contact_band) {
      ++contact_candidate_count;
    }
  }

  std::uint32_t seed = attacker.get_id() * 0x9e3779b9U;
  seed ^= opponent_id * 0x85ebca6bU;
  if (auto const* state =
          attacker.get_component<Engine::Core::CombatStateComponent>()) {
    seed ^= static_cast<std::uint32_t>(state->attack_variant) * 0xc2b2ae35U;
  }
  if (auto const* action =
          attacker.get_component<Engine::Core::RpgCommanderActionComponent>()) {
    seed ^= static_cast<std::uint32_t>(action->combat_action_id) * 0x27d4eb2dU;
    seed ^= static_cast<std::uint32_t>(action->melee_attack_sequence) * 0x165667b1U;
  }
  seed ^= seed >> 16U;
  seed *= 0x7feb352dU;
  seed ^= seed >> 15U;
  std::size_t selected_index = seed % contact_candidate_count;
  for (auto const& pair : pairs) {
    if (pair.surface_gap > closest->surface_gap + equivalent_contact_band) {
      continue;
    }
    if (selected_index == 0U) {
      return pair;
    }
    --selected_index;
  }
  return std::nullopt;
}

} // namespace Game::Systems::FormationCombat
