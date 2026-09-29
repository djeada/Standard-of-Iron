#pragma once

#include <QString>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
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

struct RiverSegment {
  QVector3D start;
  QVector3D end;
  float width = 2.0F;
  WaterElevationMode elevation_mode = WaterElevationMode::Terrain;
};

struct Lake {
  QVector3D center;
  float width = 8.0F;
  float depth = 8.0F;
  float rotation_deg = 0.0F;
  WaterElevationMode elevation_mode = WaterElevationMode::Terrain;
};

[[nodiscard]] inline auto lake_boundary_scale(const Lake& lake,
                                              float local_angle) noexcept -> float {
  const float phase = lake.center.x() * 0.071F + lake.center.z() * 0.113F;
  return std::clamp(1.0F + std::sin(local_angle * 3.0F + phase) * 0.055F +
                        std::sin(local_angle * 7.0F - phase * 1.7F) * 0.025F,
                    0.90F,
                    1.10F);
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
