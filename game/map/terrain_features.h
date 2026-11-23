#pragma once

#include <QString>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "ground_type.h"
#include "hill_shape.h"

namespace Game::Map {

struct HillEntranceCenterline {
  QVector3D start;
  QVector3D end;
};

struct HillNavigation {
  std::vector<std::uint8_t> walkable;
  std::vector<std::uint8_t> entrances;
  std::vector<HillEntranceCenterline> entrance_centerlines;

  [[nodiscard]] auto empty() const -> bool {
    return walkable.empty() && entrances.empty();
  }
};

struct TerrainFeature {
  TerrainType type;
  float center_x{};
  float center_z{};
  float radius{};
  float width{};
  float depth{};
  float height{};
  float crown = 0.0F;
  bool exact_height = false;

  std::vector<QVector3D> entrances;

  float rotation_deg = 0.0F;

  HillShape shape = HillShape::Blob;
  float thickness = 0.0F;
  float sweep_degrees = 0.0F;
  float sweep_start_degrees = 0.0F;
  float taper = 0.0F;

  bool raise_only = false;
  bool fields = false;
  float outline_seed = -1.0F;
  bool has_sweep = false;
  bool has_sweep_start = false;
  std::vector<QVector3D> shape_points;
  std::vector<QVector3D> mask_cells;
};

enum class WaterElevationMode : std::uint8_t {
  Terrain,
  Authored,
};

inline constexpr float k_default_ford_depth = 0.45F;
inline constexpr float k_min_ford_depth = 0.15F;
inline constexpr float k_max_ford_depth = 0.80F;
inline constexpr float k_default_ford_speed = 0.55F;
inline constexpr float k_min_ford_speed = 0.2F;
inline constexpr float k_max_ford_speed = 1.0F;
inline constexpr float k_default_ford_length = 14.0F;
inline constexpr float k_min_ford_length = 4.0F;
inline constexpr float k_max_ford_length = 80.0F;
inline constexpr float k_default_ford_exposure = 1.25F;
inline constexpr float k_min_ford_exposure = 1.0F;
inline constexpr float k_max_ford_exposure = 2.0F;

struct FordProfile {
  float depth = k_default_ford_depth;
  float speed = k_default_ford_speed;
  float cold = 0.0F;
  float exposure = k_default_ford_exposure;

  [[nodiscard]] auto clamped() const noexcept -> FordProfile {
    return {std::clamp(depth, k_min_ford_depth, k_max_ford_depth),
            std::clamp(speed, k_min_ford_speed, k_max_ford_speed),
            std::clamp(cold, 0.0F, 1.0F),
            std::clamp(exposure, k_min_ford_exposure, k_max_ford_exposure)};
  }

  auto operator==(const FordProfile&) const -> bool = default;
};

struct RiverSegment {
  QVector3D start;
  QVector3D end;
  float width = 2.0F;
  WaterElevationMode elevation_mode = WaterElevationMode::Terrain;

  std::optional<FordProfile> ford;
  // Sheltered water (a harbour ring, a canal): drawn with slow, small ripples.
  bool calm = false;
};

struct FordCrossing {
  QString id;
  QVector3D position;
  float length = k_default_ford_length;
  FordProfile profile;
};

// How far a lake's bed lies under its water level.
inline constexpr float k_lake_bed_depth = 0.10F;

// The outline of a lake inside its width and depth.
enum class LakeShape : std::uint8_t {
  // An ellipse with a wandering shore: a lake, a bay, an open coast.
  Natural,
  // A true ellipse: a dug pool, a round harbour.
  Round,
  // A rectangle with eased corners: a harbour basin, a dock, a canal reach.
  Basin,
};

struct Lake {
  QVector3D center;
  float width = 8.0F;
  float depth = 8.0F;
  float rotation_deg = 0.0F;
  WaterElevationMode elevation_mode = WaterElevationMode::Terrain;
  LakeShape shape = LakeShape::Natural;
  // Sheltered water (a harbour basin, a garden pool): slow, small ripples.
  bool calm = false;
};

inline constexpr std::string_view k_lake_shape_natural = "natural";
inline constexpr std::string_view k_lake_shape_round = "round";
inline constexpr std::string_view k_lake_shape_basin = "basin";

[[nodiscard]] constexpr auto
lake_shape_name(LakeShape shape) noexcept -> std::string_view {
  switch (shape) {
  case LakeShape::Round:
    return k_lake_shape_round;
  case LakeShape::Basin:
    return k_lake_shape_basin;
  case LakeShape::Natural:
    break;
  }
  return k_lake_shape_natural;
}

[[nodiscard]] constexpr auto
lake_shape_from_name(std::string_view name) noexcept -> std::optional<LakeShape> {
  if (name.empty() || name == k_lake_shape_natural) {
    return LakeShape::Natural;
  }
  if (name == k_lake_shape_round) {
    return LakeShape::Round;
  }
  if (name == k_lake_shape_basin) {
    return LakeShape::Basin;
  }
  return std::nullopt;
}

// How far the shore stands from the centre at an angle, as a multiple of the
// ellipse through the lake's width and depth.
[[nodiscard]] inline auto lake_boundary_scale(const Lake& lake,
                                              float local_angle) noexcept -> float {
  if (lake.shape == LakeShape::Round) {
    return 1.0F;
  }
  if (lake.shape == LakeShape::Basin) {
    // A superellipse: straight sides out to the full width and depth, and
    // corners eased enough to be drawn and walked round.
    constexpr float k_exponent = 12.0F;
    const float c = std::abs(std::cos(local_angle));
    const float s = std::abs(std::sin(local_angle));
    return std::pow(std::pow(c, k_exponent) + std::pow(s, k_exponent),
                    -1.0F / k_exponent);
  }
  const float phase = lake.center.x() * 0.071F + lake.center.z() * 0.113F;
  return std::clamp(1.0F + std::sin(local_angle * 3.0F + phase) * 0.055F +
                        std::sin(local_angle * 7.0F - phase * 1.7F) * 0.025F,
                    0.90F,
                    1.10F);
}

// How far in from the shore a lake's shallows reach, in world units: the
// stretch over which the bed shows and the water's edge breaks up. Zero keeps
// them in proportion to the lake, which suits a pond and ruins a sea: there
// the mud flats would run out for a bowshot.
[[nodiscard]] inline auto lake_shallows_reach(const Lake& lake) noexcept -> float {
  constexpr float k_open_water_half_extent = 34.0F;
  constexpr float k_open_water_reach = 20.0F;
  constexpr float k_built_edge_reach = 4.0F;
  if (lake.shape != LakeShape::Natural) {
    return k_built_edge_reach;
  }
  return std::min(lake.width, lake.depth) * 0.5F > k_open_water_half_extent
             ? k_open_water_reach
             : 0.0F;
}

[[nodiscard]] inline auto point_in_lake(const Lake& lake,
                                        float world_x,
                                        float world_z,
                                        float padding = 0.0F) noexcept -> bool {
  constexpr float deg_to_rad = 0.01745329251994329577F;
  const float angle = -lake.rotation_deg * deg_to_rad;
  const float cos_a = std::cos(angle);
  const float sin_a = std::sin(angle);
  const float dx = world_x - lake.center.x();
  const float dz = world_z - lake.center.z();
  const float local_x = dx * cos_a - dz * sin_a;
  const float local_z = dx * sin_a + dz * cos_a;
  const float half_width = std::max(lake.width * 0.5F + padding, 0.0001F);
  const float half_depth = std::max(lake.depth * 0.5F + padding, 0.0001F);
  const float normalized_x = local_x / half_width;
  const float normalized_z = local_z / half_depth;
  const float radial = std::hypot(normalized_x, normalized_z);
  const float local_angle = std::atan2(normalized_z, normalized_x);
  return radial <= lake_boundary_scale(lake, local_angle);
}

[[nodiscard]] inline auto point_on_lake_boundary(const Lake& lake,
                                                 float world_x,
                                                 float world_z,
                                                 float tolerance) noexcept -> bool {
  const float shoreline_band = std::max(tolerance, 0.0F);
  return point_in_lake(lake, world_x, world_z, shoreline_band) &&
         !point_in_lake(lake, world_x, world_z, -shoreline_band);
}

[[nodiscard]] inline auto
lake_boundary_intersection(const Lake& lake,
                           QVector3D dry_point,
                           QVector3D wet_point) noexcept -> std::optional<QVector3D> {
  if (point_in_lake(lake, dry_point.x(), dry_point.z()) ||
      !point_in_lake(lake, wet_point.x(), wet_point.z())) {
    return std::nullopt;
  }

  for (int iteration = 0; iteration < 28; ++iteration) {
    const QVector3D midpoint = (dry_point + wet_point) * 0.5F;
    if (point_in_lake(lake, midpoint.x(), midpoint.z())) {
      wet_point = midpoint;
    } else {
      dry_point = midpoint;
    }
  }
  return (dry_point + wet_point) * 0.5F;
}

struct RoadSegment {
  QVector3D start;
  QVector3D end;
  float width = 3.0F;
  QString style = QStringLiteral("default");
};

inline constexpr float k_road_surface_y_offset = 0.02F;

inline constexpr float k_road_surface_envelope_tiles = 0.35F;
inline constexpr int k_road_surface_envelope_taps = 4;

[[nodiscard]] inline auto road_surface_world_y(float terrain_height) -> float {
  return terrain_height + k_road_surface_y_offset;
}

} // namespace Game::Map
