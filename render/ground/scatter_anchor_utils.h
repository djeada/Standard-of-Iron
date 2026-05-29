#pragma once

#include <QVector2D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

#include "ground_utils.h"
#include "scatter_composition_types.h"

namespace Render::Ground {

namespace Detail {

struct PointAnchor {
  QVector2D position{0.0F, 0.0F};
  float radius = 1.0F;
};

struct LinearAnchor {
  QVector2D start{0.0F, 0.0F};
  QVector2D end{0.0F, 0.0F};
  float width = 1.0F;
};

inline auto distance_to_segment(const QVector2D& point,
                                const LinearAnchor& anchor) -> float {
  QVector2D const segment = anchor.end - anchor.start;
  float const length_sq = QVector2D::dotProduct(segment, segment);
  if (length_sq < 1e-4F) {
    return (point - anchor.start).length();
  }
  float const t = std::clamp(
      QVector2D::dotProduct(point - anchor.start, segment) / length_sq, 0.0F, 1.0F);
  QVector2D const closest = anchor.start + segment * t;
  return (point - closest).length();
}

inline auto distance_to_point(const QVector2D& point,
                              const PointAnchor& anchor) -> float {
  return (point - anchor.position).length();
}

inline auto point_influence(const QVector2D& point,
                            const std::vector<PointAnchor>& anchors,
                            float inner_scale,
                            float outer_scale) -> float {
  float strongest = 0.0F;
  for (const auto& anchor : anchors) {
    float const inner = std::max(0.0F, anchor.radius * inner_scale);
    float const outer = std::max(inner + 0.1F, anchor.radius * outer_scale);
    float const influence =
        1.0F - smoothstep(inner, outer, distance_to_point(point, anchor));
    strongest = std::max(strongest, influence);
  }
  return strongest;
}

inline auto line_influence(const QVector2D& point,
                           const std::vector<LinearAnchor>& anchors,
                           float inner_extra,
                           float outer_extra) -> float {
  float strongest = 0.0F;
  for (const auto& anchor : anchors) {
    float const inner = std::max(0.0F, anchor.width * 0.5F + inner_extra);
    float const outer = std::max(inner + 0.1F, inner + outer_extra);
    float const influence =
        1.0F - smoothstep(inner, outer, distance_to_segment(point, anchor));
    strongest = std::max(strongest, influence);
  }
  return strongest;
}

inline auto camp_layout_seed(float center_world_x,
                             float center_world_z) -> std::uint32_t {
  return hash_coords(static_cast<int>(std::floor(center_world_x * 4.0F)),
                     static_cast<int>(std::floor(center_world_z * 4.0F)),
                     0x6CB5E33AU);
}

inline auto camp_layout_angle(float center_world_x, float center_world_z) -> float {
  std::uint32_t state = camp_layout_seed(center_world_x, center_world_z);
  return rand_01(state) * MathConstants::k_two_pi;
}

inline auto
role_angle_offset(CampPropRole role, int slot_index, int slot_count) -> float {
  float const slot_center =
      slot_count > 1
          ? static_cast<float>(slot_index) - static_cast<float>(slot_count - 1) * 0.5F
          : 0.0F;
  switch (role) {
  case CampPropRole::Tent:
    return 0.65F + slot_center * 0.74F;
  case CampPropRole::SupplyCart:
    return MathConstants::k_two_pi * 0.5F + slot_center * 0.38F;
  case CampPropRole::WeaponRack:
    return 0.18F + slot_center * 0.52F;
  case CampPropRole::DeadTree:
    return -1.10F + slot_center * 0.62F;
  }
  return 0.0F;
}

inline auto
role_distance_range(CampPropRole role,
                    const ScatterCompositionSample& sample) -> std::pair<float, float> {
  switch (role) {
  case CampPropRole::Tent:
    return {0.84F, 0.98F + sample.dryness * 0.06F};
  case CampPropRole::SupplyCart:
    return {1.00F + sample.road_influence * 0.06F,
            1.18F + sample.road_influence * 0.10F};
  case CampPropRole::WeaponRack:
    return {0.68F, 0.82F + sample.cluster_bias * 0.06F};
  case CampPropRole::DeadTree:
    return {1.22F + sample.dryness * 0.08F, 1.52F + sample.rockiness * 0.10F};
  }
  return {0.9F, 1.1F};
}

inline auto to_species(CampPropRole role) -> ScatterRuleSpecies {
  switch (role) {
  case CampPropRole::Tent:
    return ScatterRuleSpecies::Tent;
  case CampPropRole::SupplyCart:
    return ScatterRuleSpecies::SupplyCart;
  case CampPropRole::WeaponRack:
    return ScatterRuleSpecies::WeaponRack;
  case CampPropRole::DeadTree:
    return ScatterRuleSpecies::DeadTree;
  }
  return ScatterRuleSpecies::Tent;
}

} // namespace Detail

} // namespace Render::Ground
