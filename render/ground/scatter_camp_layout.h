#pragma once

#include <cmath>
#include <cstdint>

#include "ground_utils.h"
#include "scatter_composition_context.h"
#include "scatter_rules.h"
#include "spawn_validator.h"

namespace Render::Ground {

[[nodiscard]] inline auto
camp_prop_slot_count(CampPropRole role,
                     const ScatterCompositionSample& camp_sample,
                     float camp_radius,
                     std::uint32_t& state) -> int {
  float const size_factor = smoothstep(1.8F, 4.2F, camp_radius);
  switch (role) {
  case CampPropRole::Tent: {
    int count = 1 + (size_factor > 0.35F ? 1 : 0);
    if (size_factor > 0.80F && rand_01(state) < 0.45F) {
      ++count;
    }
    return count;
  }
  case CampPropRole::SupplyCart: {
    int count = (camp_radius > 1.7F || camp_sample.road_influence > 0.25F) ? 1 : 0;
    if (count > 0 && size_factor > 0.85F && rand_01(state) < 0.35F) {
      ++count;
    }
    return count;
  }
  case CampPropRole::WeaponRack: {
    if (camp_sample.fertility > 0.75F && camp_sample.dryness < 0.35F) {
      return 0;
    }
    int count = 1;
    if (size_factor > 0.45F && rand_01(state) < 0.60F) {
      ++count;
    }
    return count;
  }
  case CampPropRole::DeadTree: {
    if (camp_sample.dryness < 0.45F && camp_sample.rockiness < 0.55F) {
      return 0;
    }
    int count = 1;
    if (size_factor > 0.70F && rand_01(state) < 0.30F) {
      ++count;
    }
    return count;
  }
  }
  return 0;
}

inline auto
find_contextual_camp_prop_spawn_position(const SpawnValidator& validator,
                                         const ScatterCompositionContext& context,
                                         CampPropRole role,
                                         float center_world_x,
                                         float center_world_z,
                                         float preferred_distance,
                                         const ScatterCompositionSample& camp_sample,
                                         int slot_index,
                                         int slot_count,
                                         std::uint32_t& state,
                                         float& out_world_x,
                                         float& out_world_z) -> bool {
  float const base_angle = Detail::camp_layout_angle(center_world_x, center_world_z) +
                           Detail::role_angle_offset(role, slot_index, slot_count);
  auto const [min_scale, max_scale] = Detail::role_distance_range(role, camp_sample);

  for (int attempt = 0; attempt < 12; ++attempt) {
    float const angle = base_angle + remap(rand_01(state), -0.22F, 0.22F) +
                        static_cast<float>(attempt) * 0.08F;
    float const distance_scale = remap(rand_01(state), min_scale, max_scale) +
                                 static_cast<float>(attempt) * 0.03F;
    float const distance = preferred_distance * distance_scale;
    float const candidate_x = center_world_x + std::cos(angle) * distance;
    float const candidate_z = center_world_z + std::sin(angle) * distance;
    if (!validator.can_spawn_at_world(candidate_x, candidate_z)) {
      continue;
    }

    auto const sample =
        context.sample_world(candidate_x, candidate_z, state ^ 0x4D92F1B7U);
    if (rand_01(state) > scatter_spawn_chance(Detail::to_species(role), sample)) {
      continue;
    }
    out_world_x = candidate_x;
    out_world_z = candidate_z;
    return true;
  }
  return false;
}

} // namespace Render::Ground
