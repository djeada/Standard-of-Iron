#pragma once

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <vector>

#include "river_ribbon.h"
#include "terrain_features.h"

namespace Game::Map {

inline constexpr float k_min_bridge_width = 8.0F;

struct Bridge {
  QVector3D start;
  QVector3D end;
  float width = k_min_bridge_width;
  float height = 0.5F;
};

[[nodiscard]] inline auto xz_cross(const QVector3D& a,
                                   const QVector3D& b) noexcept -> float {
  return a.x() * b.z() - a.z() * b.x();
}

inline constexpr float k_max_oblique_bridge_span = 3.0F;

[[nodiscard]] inline auto bridge_required_half_length_for_river(
    const Bridge& bridge, const RiverSegment& river) noexcept -> std::optional<float> {
  QVector3D const bridge_vec = bridge.end - bridge.start;
  QVector3D const river_vec = river.end - river.start;
  float const bridge_len = std::hypot(bridge_vec.x(), bridge_vec.z());
  float const river_len = std::hypot(river_vec.x(), river_vec.z());
  if (bridge_len < 1.0e-4F || river_len < 1.0e-4F) {
    return std::nullopt;
  }

  float const cross = xz_cross(bridge_vec, river_vec);
  float const sin_angle = std::abs(cross) / (bridge_len * river_len);
  if (sin_angle < 1.0e-4F) {
    return std::nullopt;
  }

  QVector3D const diff = river.start - bridge.start;
  float const t = xz_cross(diff, river_vec) / cross;
  float const s = xz_cross(diff, bridge_vec) / cross;
  if (t < 0.0F || t > 1.0F || s < 0.0F || s > 1.0F) {
    return std::nullopt;
  }

  float const perpendicular_half = river.width * 0.5F;
  float const oblique_half = perpendicular_half / sin_angle;
  return std::min(oblique_half, perpendicular_half * k_max_oblique_bridge_span);
}

[[nodiscard]] inline constexpr auto bridge_abutment_reach(float bridge_width) -> float {
  return std::clamp(bridge_width * 0.14F, 0.7F, 1.3F);
}

inline constexpr float k_river_drawn_edge_scale = 1.30F;

inline constexpr float k_river_drawn_meander_reach = 0.16F;

inline constexpr float k_water_bank_clearance = 0.6F;

[[nodiscard]] inline constexpr auto river_drawn_half_width(float river_width) -> float {
  return river_width *
         ((0.5F * k_river_drawn_edge_scale) + k_river_drawn_meander_reach);
}

[[nodiscard]] inline constexpr auto
river_bank_standing_half_width(float river_width) -> float {
  return river_drawn_half_width(river_width) + k_water_bank_clearance;
}

[[nodiscard]] inline constexpr auto
bridge_walkable_half_width(float bridge_width) -> float {
  return std::max((bridge_width * 0.5F) - k_water_bank_clearance, bridge_width * 0.25F);
}

inline constexpr float k_bridge_min_bank_landing = 1.0F;

inline constexpr float k_bridge_max_bank_landing = 1.5F;

[[nodiscard]] inline constexpr auto bridge_bank_landing(float bridge_width,
                                                        float river_width) -> float {
  return std::clamp(std::max(bridge_width * 0.06F, river_width * 0.03F),
                    k_bridge_min_bank_landing,
                    k_bridge_max_bank_landing);
}

[[nodiscard]] inline constexpr auto
bridge_visual_landing_run(float bridge_width) -> float {
  return bridge_abutment_reach(bridge_width);
}

[[nodiscard]] inline auto river_drawn_cross_section(const RiverSegment& river,
                                                    float t) -> RibbonCrossSection {
  return sample_ribbon_cross_section(
      river.start, river.end, river.width, t, k_river_ribbon_shape);
}

[[nodiscard]] inline constexpr auto drawn_lake_padding(float tile_size) -> float {
  return std::max(tile_size, 0.15F);
}

[[nodiscard]] inline auto drawn_water_level_at(const std::vector<RiverSegment>& rivers,
                                               const std::vector<Lake>& lakes,
                                               float lake_padding,
                                               float world_x,
                                               float world_z) -> std::optional<float> {
  std::optional<float> level;
  const auto raise = [&level](float candidate) {
    level = level.has_value() ? std::max(*level, candidate) : candidate;
  };

  for (const RiverSegment& river : rivers) {
    float const delta_x = river.end.x() - river.start.x();
    float const delta_z = river.end.z() - river.start.z();
    float const length_sq = (delta_x * delta_x) + (delta_z * delta_z);
    float const t = length_sq < 1.0e-8F
                        ? 0.0F
                        : std::clamp((((world_x - river.start.x()) * delta_x) +
                                      ((world_z - river.start.z()) * delta_z)) /
                                         length_sq,
                                     0.0F,
                                     1.0F);
    if (std::hypot(world_x - (river.start.x() + delta_x * t),
                   world_z - (river.start.z() + delta_z * t)) >
        river_drawn_half_width(river.width)) {
      continue;
    }
    RibbonCrossSection const section = river_drawn_cross_section(river, t);
    if (std::hypot(world_x - section.center.x(), world_z - section.center.z()) >
        section.half_width) {
      continue;
    }
    raise(river.start.y() + (river.end.y() - river.start.y()) * t);
  }

  for (const Lake& lake : lakes) {
    if (point_in_lake(lake, world_x, world_z, lake_padding)) {
      raise(lake.center.y());
    }
  }
  return level;
}

inline constexpr float k_bridge_deck_flare = 1.25F;

[[nodiscard]] inline auto
drawn_water_level_under_bridge(const Bridge& bridge,
                               const std::vector<RiverSegment>& rivers,
                               const std::vector<Lake>& lakes,
                               float lake_padding,
                               float along_from,
                               float along_to) -> std::optional<float> {
  QVector3D direction = bridge.end - bridge.start;
  direction.setY(0.0F);
  float const length = direction.length();
  if (length < 1.0e-4F || along_to < along_from) {
    return std::nullopt;
  }
  direction /= length;
  QVector3D const perpendicular(-direction.z(), 0.0F, direction.x());
  float const half_width =
      std::max(bridge.width, k_min_bridge_width) * 0.5F * k_bridge_deck_flare;

  constexpr float k_sample_spacing = 0.5F;
  int const along_samples = std::max(
      1, static_cast<int>(std::ceil((along_to - along_from) / k_sample_spacing)));
  int const lateral_samples =
      std::max(2, static_cast<int>(std::ceil(half_width * 2.0F / k_sample_spacing)));

  std::optional<float> level;
  for (int along_index = 0; along_index <= along_samples; ++along_index) {
    float const along = along_from + (along_to - along_from) *
                                         static_cast<float>(along_index) /
                                         static_cast<float>(along_samples);
    for (int lateral_index = 0; lateral_index <= lateral_samples; ++lateral_index) {
      float const lateral =
          half_width *
          ((static_cast<float>(lateral_index) / static_cast<float>(lateral_samples)) *
               2.0F -
           1.0F);
      QVector3D const point =
          bridge.start + direction * along + perpendicular * lateral;
      if (auto const water =
              drawn_water_level_at(rivers, lakes, lake_padding, point.x(), point.z());
          water.has_value() && (!level.has_value() || *water > *level)) {
        level = water;
      }
    }
  }
  return level;
}

struct RiverWaterReach {
  float behind = 0.0F;
  float ahead = 0.0F;
};

[[nodiscard]] inline auto
drawn_water_reach_across(const RiverSegment& river,
                         float crossing_t,
                         const QVector3D& crossing,
                         const QVector3D& span_dir,
                         float deck_half_width) -> RiverWaterReach {
  QVector3D const river_vec = river.end - river.start;
  float const river_len = std::hypot(river_vec.x(), river_vec.z());
  float const channel_half = river.width * 0.5F;
  RiverWaterReach reach{channel_half, channel_half};
  if (river_len < 1.0e-4F) {
    return reach;
  }

  constexpr float k_sample_spacing = 0.5F;
  int const samples = std::max(
      2, static_cast<int>(std::ceil(deck_half_width * 2.0F / k_sample_spacing)));
  float const t_half = deck_half_width / river_len;
  for (int index = 0; index <= samples; ++index) {
    float const lateral =
        (static_cast<float>(index) / static_cast<float>(samples)) * 2.0F - 1.0F;
    float const t = std::clamp(crossing_t + t_half * lateral, 0.0F, 1.0F);
    RibbonCrossSection const section = river_drawn_cross_section(river, t);
    float const offset = QVector3D::dotProduct(section.center - crossing, span_dir);
    reach.behind = std::max(reach.behind, section.half_width - offset);
    reach.ahead = std::max(reach.ahead, section.half_width + offset);
  }
  return reach;
}

[[nodiscard]] inline auto closest_point_on_segment(const QVector3D& point,
                                                   const QVector3D& start,
                                                   const QVector3D& end) -> QVector3D {
  QVector3D const segment = end - start;
  float const length_sq = (segment.x() * segment.x()) + (segment.z() * segment.z());
  if (length_sq < 1.0e-6F) {
    return start;
  }
  float const t = std::clamp((((point.x() - start.x()) * segment.x()) +
                              ((point.z() - start.z()) * segment.z())) /
                                 length_sq,
                             0.0F,
                             1.0F);
  return start + segment * t;
}

[[nodiscard]] inline auto bridge_oblique_half_for(const QVector3D& bridge_dir,
                                                  const RiverSegment& river,
                                                  float river_len) -> float {
  float const perpendicular_half = river.width * 0.5F;
  QVector3D const river_vec = river.end - river.start;
  float const sin_angle = river_len < 1.0e-4F
                              ? 1.0F
                              : std::abs(xz_cross(bridge_dir, river_vec)) / river_len;
  if (sin_angle < 1.0e-4F) {
    return perpendicular_half * k_max_oblique_bridge_span;
  }
  return std::min(perpendicular_half / sin_angle,
                  perpendicular_half * k_max_oblique_bridge_span);
}

inline void fit_bridge_span_to_riverbanks(Bridge& bridge,
                                          const std::vector<RiverSegment>& rivers) {
  QVector3D const bridge_vec = bridge.end - bridge.start;
  float const bridge_len = std::hypot(bridge_vec.x(), bridge_vec.z());
  if (bridge_len < 1.0e-4F) {
    return;
  }
  QVector3D const dir(bridge_vec.x() / bridge_len, 0.0F, bridge_vec.z() / bridge_len);

  auto apply = [&](const RiverSegment& river,
                   float river_t,
                   const QVector3D& crossing) {
    QVector3D const river_vec = river.end - river.start;
    float const river_len = std::hypot(river_vec.x(), river_vec.z());
    QVector3D span_dir = dir;
    if (river_len >= 1.0e-4F) {
      span_dir = QVector3D(-river_vec.z() / river_len, 0.0F, river_vec.x() / river_len);
      if (QVector3D::dotProduct(span_dir, dir) < 0.0F) {
        span_dir = -span_dir;
      }
    }

    float const deck_half_width = std::max(bridge.width, k_min_bridge_width) * 0.5F;
    RiverWaterReach const water =
        drawn_water_reach_across(river, river_t, crossing, span_dir, deck_half_width);
    float const past_water =
        k_water_bank_clearance + bridge_bank_landing(bridge.width, river.width);
    float behind = water.behind + past_water;
    float ahead = water.ahead + past_water;

    float const shortfall = (deck_half_width * 2.0F) + 0.01F - (behind + ahead);
    if (shortfall > 0.0F) {
      behind += shortfall * 0.5F;
      ahead += shortfall * 0.5F;
    }

    bridge.start = crossing - span_dir * behind;
    bridge.end = crossing + span_dir * ahead;
  };

  for (const RiverSegment& river : rivers) {
    if (!bridge_required_half_length_for_river(bridge, river).has_value()) {
      continue;
    }

    QVector3D const river_vec = river.end - river.start;
    float const cross = xz_cross(bridge_vec, river_vec);
    QVector3D const diff = river.start - bridge.start;
    float const t = xz_cross(diff, river_vec) / cross;
    float const s = xz_cross(diff, bridge_vec) / cross;

    apply(river, s, bridge.start + dir * (t * bridge_len));
    return;
  }

  QVector3D const midpoint = (bridge.start + bridge.end) * 0.5F;
  const RiverSegment* nearest = nullptr;
  QVector3D nearest_point;
  float nearest_distance = 0.0F;

  for (const RiverSegment& river : rivers) {
    QVector3D const on_river =
        closest_point_on_segment(midpoint, river.start, river.end);
    float const distance =
        std::hypot(on_river.x() - midpoint.x(), on_river.z() - midpoint.z());
    if (distance > river.width * k_max_oblique_bridge_span) {
      continue;
    }
    if (nearest == nullptr || distance < nearest_distance) {
      nearest = &river;
      nearest_point = on_river;
      nearest_distance = distance;
    }
  }

  if (nearest == nullptr) {
    return;
  }

  QVector3D const river_vec = nearest->end - nearest->start;
  float const river_len_sq =
      (river_vec.x() * river_vec.x()) + (river_vec.z() * river_vec.z());
  float const river_t =
      river_len_sq < 1.0e-8F
          ? 0.0F
          : (((nearest_point.x() - nearest->start.x()) * river_vec.x()) +
             ((nearest_point.z() - nearest->start.z()) * river_vec.z())) /
                river_len_sq;
  apply(*nearest, river_t, nearest_point);
}

inline void extend_bridge_to_span_riverbanks(Bridge& bridge,
                                             const std::vector<RiverSegment>& rivers) {
  fit_bridge_span_to_riverbanks(bridge, rivers);
}

[[nodiscard]] inline auto bridge_arch_curve(float t) -> float {

  float const clamped_t = std::clamp(t, 0.0F, 1.0F);
  float const s = std::sin(std::numbers::pi_v<float> * clamped_t);
  return s * s;
}

inline constexpr float k_min_bridge_deck_rise = 0.72F;

inline constexpr float k_bridge_rise_per_width = 0.075F;

inline constexpr float k_river_bank_max_grade = 0.35F;

inline constexpr float k_river_bank_max_blend_cells = 26.0F;

inline constexpr float k_bridge_deck_visual_lift = 0.12F;

inline constexpr float k_bridge_water_clearance = 0.10F;

[[nodiscard]] inline constexpr auto
bridge_abutment_floor_over_water(float water_level) -> float {
  return water_level + k_bridge_water_clearance - k_bridge_deck_visual_lift;
}

inline constexpr float k_bridge_landing_thickness = 0.06F;

inline constexpr float k_bridge_max_entry_grade = 0.11F;

[[nodiscard]] inline auto bridge_max_rise_for_span(float span) -> float {
  float const safe_span = std::max(span, 0.5F);
  float const angular = std::numbers::pi_v<float> / safe_span;
  float const entry_slope =
      safe_span > 4.0F ? angular * std::sin(2.0F * angular) : angular;
  return k_bridge_max_entry_grade / entry_slope;
}

[[nodiscard]] inline auto bridge_effective_height(const Bridge& bridge) -> float {
  float const span =
      std::hypot(bridge.end.x() - bridge.start.x(), bridge.end.z() - bridge.start.z());
  return std::min(std::max({bridge.height,
                            k_min_bridge_deck_rise,
                            bridge.width * k_bridge_rise_per_width}),
                  bridge_max_rise_for_span(span));
}

[[nodiscard]] inline auto bridge_deck_world_y(const Bridge& bridge, float t) -> float {
  float const clamped_t = std::clamp(t, 0.0F, 1.0F);
  float const base_y =
      bridge.start.y() * (1.0F - clamped_t) + bridge.end.y() * clamped_t;
  return base_y + k_bridge_deck_visual_lift +
         bridge_effective_height(bridge) * bridge_arch_curve(clamped_t);
}

inline constexpr float k_bridge_entry_margin_tiles = 1.0F;
inline constexpr float k_bridge_cell_half_span_tiles = 0.5F;

[[nodiscard]] inline auto bridge_crossing_entry_margin(float bridge_width,
                                                       float tile_size) -> float {
  return std::max(tile_size * 2.0F, bridge_width);
}

[[nodiscard]] inline auto
bridge_crossing_alignment_half_width(float bridge_width, float tile_size) -> float {
  return std::max(bridge_width * 0.5F + tile_size, tile_size * 1.5F);
}

} // namespace Game::Map
