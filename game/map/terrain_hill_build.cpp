#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "terrain_build_internal.h"
#include "terrain_hill_stamp.h"

namespace Game::Map::terrain_build {

namespace {

constexpr float k_hill_fallback_entrance_margin_cells = 2.0F;

} // namespace

HillStamp::HillStamp(BuildContext& ctx,
                     const TerrainFeature& feature_in,
                     const FeatureFrame& frame)
    : L(ctx.layers)
    , base_heights(ctx.base_heights)
    , erosion_strength(ctx.erosion_strength)
    , erosion_protected(ctx.erosion_protected)
    , feature(feature_in)
    , grid_center_x(frame.grid_center_x)
    , grid_center_z(frame.grid_center_z)
    , grid_half_width(ctx.layers.half_width())
    , grid_half_height(ctx.layers.half_height())
    , walkable_mask(static_cast<std::size_t>(ctx.layers.cell_count()), 0)
    , entrance_line_mask(static_cast<std::size_t>(ctx.layers.cell_count()), 0) {
  place_footprint();
  build_shape();
  measure_extents();
  configure_landform();
}

void HillStamp::place_footprint() {
  campaign_landform_scale = Game::Map::is_campaign_landform_scale(L.width, L.height);
  const bool shaped_hill = feature.shape != Game::Map::HillShape::Blob;
  footprint =
      Game::Map::hill_footprint_cells({.width = feature.width,
                                       .depth = feature.depth,
                                       .radius = feature.radius,
                                       .rotation_deg = feature.rotation_deg,
                                       .tile_size = L.tile_size,
                                       .grid_center_x = grid_center_x,
                                       .grid_center_z = grid_center_z,
                                       .campaign_scale = campaign_landform_scale,
                                       .shaped = shaped_hill});
  float const hill_rotation_deg =
      feature.shape == Game::Map::HillShape::Mask ? 0.0F : footprint.rotation_deg;
  const float angle_rad = hill_rotation_deg * k_deg_to_rad;
  cos_a = std::cos(angle_rad);
  sin_a = std::sin(angle_rad);

  crown_profile = Game::Map::hill_crown_profile(footprint,
                                                feature.height,
                                                L.tile_size,
                                                campaign_landform_scale,
                                                feature.crown,
                                                feature.exact_height);
  hill_height = crown_profile.height;
}

void HillStamp::build_shape() {
  Game::Map::HillShapeAuthoring authoring;
  authoring.shape = feature.shape;
  authoring.thickness = feature.thickness;
  authoring.sweep_degrees = feature.sweep_degrees;
  authoring.sweep_start_degrees = feature.sweep_start_degrees;
  authoring.taper = feature.taper;
  authoring.has_sweep = feature.has_sweep;
  authoring.has_sweep_start = feature.has_sweep_start;
  authoring.local_points.reserve(feature.shape_points.size());
  for (const auto& point : feature.shape_points) {
    const float point_dx = (point.x() / L.tile_size) + grid_half_width - grid_center_x;
    const float point_dz = (point.z() / L.tile_size) + grid_half_height - grid_center_z;
    authoring.local_points.push_back(
        {point_dx * cos_a + point_dz * sin_a, -point_dx * sin_a + point_dz * cos_a});
  }

  auto shape_params = Game::Map::hill_shape_params(footprint, authoring, L.tile_size);
  shape_params.mask_center_x = grid_center_x;
  shape_params.mask_center_z = grid_center_z;
  const float mask_reference = std::min(footprint.half_width, footprint.half_depth);
  shape_params.mask_inset_cells =
      std::max(mask_reference -
                   Game::Map::hill_crown_extent_cells(crown_profile, mask_reference),
               1.0F);
  shape_params.mask_cells.reserve(feature.mask_cells.size());
  for (const auto& cell : feature.mask_cells) {
    shape_params.mask_cells.push_back(
        {int(std::lround((cell.x() / L.tile_size) + grid_half_width)),
         int(std::lround((cell.z() / L.tile_size) + grid_half_height))});
  }
  shape_geometry = Game::Map::build_hill_shape(shape_params);
  shaped_geometry = shape_geometry.is_shaped();

  crown_thickness =
      Game::Map::hill_crown_extent_cells(crown_profile, shape_geometry.half_thickness);

  crown.height = hill_height;
  if (shaped_geometry) {
    crown.half_width = crown_thickness;
    crown.half_depth = crown_thickness;
  } else {
    crown.half_width =
        Game::Map::hill_crown_extent_cells(crown_profile, footprint.half_width);
    crown.half_depth =
        Game::Map::hill_crown_extent_cells(crown_profile, footprint.half_depth);
  }
}

void HillStamp::measure_extents() {
  slope_width = footprint.half_width;
  slope_depth = footprint.half_depth;
  const float max_extent = (shaped_geometry ? std::max(shape_geometry.bound_half_x,
                                                       shape_geometry.bound_half_z)
                                            : std::max(slope_width, slope_depth)) *
                           1.18F;
  min_x = std::max(0, int(std::floor(grid_center_x - max_extent - 1.0F)));
  max_x = std::min(L.width - 1, int(std::ceil(grid_center_x + max_extent + 1.0F)));
  min_z = std::max(0, int(std::floor(grid_center_z - max_extent - 1.0F)));
  max_z = std::min(L.height - 1, int(std::ceil(grid_center_z + max_extent + 1.0F)));
}

void HillStamp::configure_landform() {
  const float plateau_width = crown.half_width;
  const float plateau_depth = crown.half_depth;
  const float feature_phase =
      grid_center_x * 0.083F + grid_center_z * 0.127F + hill_height * 0.31F;
  const auto hill_seed =
      0x6A09E667U ^ static_cast<std::uint32_t>(
                        std::abs(grid_center_x * 29.0F + grid_center_z * 43.0F));
  hill_config = Landform::HillConfig{
      .outer_radius_x = slope_width,
      .outer_radius_z = slope_depth,
      .crown_radius_x = plateau_width,
      .crown_radius_z = plateau_depth,
      .height = hill_height,
      .phase = feature_phase,
      .seed = hill_seed,
      .rounded_crown = campaign_landform_scale,
      .shape = &shape_geometry,
      .crown_thickness = crown_thickness,
  };
}

auto HillStamp::local_to_world(float local_x, float local_z) const -> QVector3D {
  const float grid_x = grid_center_x + local_x * cos_a - local_z * sin_a;
  const float grid_z = grid_center_z + local_x * sin_a + local_z * cos_a;
  return QVector3D((grid_x - grid_half_width) * L.tile_size,
                   0.0F,
                   (grid_z - grid_half_height) * L.tile_size);
}

auto HillStamp::slope_distance(float local_x, float local_z) const -> float {
  return Landform::sample_hill(local_x, local_z, hill_config).outer_distance;
}

auto HillStamp::crown_distance(float local_x, float local_z) const -> float {
  return Landform::sample_hill(local_x, local_z, hill_config).crown_distance;
}

auto HillStamp::entrance_outside_hill(float local_x,
                                      float local_z,
                                      float dir_local_x,
                                      float dir_local_z,
                                      float start_reach) const -> QVector3D {
  const float limit = std::max(slope_width, slope_depth) * 1.5F + 8.0F;
  float reach = start_reach;
  while (reach < limit && slope_distance(local_x + dir_local_x * reach,
                                         local_z + dir_local_z * reach) <= 1.0F) {
    reach += 1.0F;
  }
  reach += k_hill_fallback_entrance_margin_cells;
  return local_to_world(local_x + dir_local_x * reach, local_z + dir_local_z * reach);
}

auto HillStamp::resolve_entrances() const -> std::vector<QVector3D> {
  std::vector<QVector3D> hill_entrances = feature.entrances;
  if (hill_entrances.empty()) {
    if (shape_geometry.is_spine()) {
      const auto pose = Game::Map::hill_shape_pose_at(shape_geometry, 0.5F);
      hill_entrances.push_back(entrance_outside_hill(pose.position.x,
                                                     pose.position.z,
                                                     pose.tangent.z,
                                                     -pose.tangent.x,
                                                     shape_geometry.half_thickness));
    } else if (shape_geometry.is_mask()) {
      hill_entrances.push_back(
          entrance_outside_hill(0.0F, 0.0F, -1.0F, 0.0F, shape_geometry.bound_half_x));
    } else {
      hill_entrances.push_back(
          entrance_outside_hill(0.0F, 0.0F, -1.0F, 0.0F, slope_width * 0.98F));
    }
  }
  return hill_entrances;
}

void HillStamp::raise_body() {
  ground_before_hill = L.heights;

  for (int z = min_z; z <= max_z; ++z) {
    for (int x = min_x; x <= max_x; ++x) {
      const float dx = float(x) - grid_center_x;
      const float dz = float(z) - grid_center_z;

      const float rotated_x = dx * cos_a + dz * sin_a;
      const float rotated_z = -dx * sin_a + dz * cos_a;

      const auto landform = Landform::sample_hill(rotated_x, rotated_z, hill_config);
      const float norm_plateau_dist = landform.crown_distance;
      const float norm_slope_dist = landform.outer_distance;

      if (norm_slope_dist > 1.0F) {
        continue;
      }

      const int idx = L.index_at(x, z);

      const float height = hill_height * landform.elevation_fraction;

      float const surface_height = base_heights[idx] + height;
      if (surface_height > L.heights[idx]) {
        L.heights[idx] = surface_height;
        L.types[idx] = TerrainType::Hill;
        erosion_strength[idx] =
            std::max(erosion_strength[idx],
                     0.50F + 0.18F * (1.0F - height / std::max(hill_height, 0.001F)));
      }
      if (norm_plateau_dist <= 1.0F && L.types[idx] == TerrainType::Hill) {
        walkable_mask[idx] = 1;
        erosion_protected[idx] = 1;
      }
    }
  }
}

auto HillStamp::cluster_entrances(const std::vector<QVector3D>& hill_entrances) const
    -> std::vector<EntranceCluster> {
  std::vector<EntranceCluster> entrance_clusters;
  entrance_clusters.reserve(hill_entrances.size());
  constexpr float k_entrance_cluster_distance = 5.0F;
  for (const auto& entrance : hill_entrances) {
    const float entrance_gx = (entrance.x() / L.tile_size) + grid_half_width;
    const float entrance_gz = (entrance.z() / L.tile_size) + grid_half_height;
    auto cluster_it = std::find_if(entrance_clusters.begin(),
                                   entrance_clusters.end(),
                                   [&](const EntranceCluster& cluster) {
                                     return std::hypot(entrance_gx - cluster.grid_x,
                                                       entrance_gz - cluster.grid_z) <=
                                            k_entrance_cluster_distance;
                                   });
    if (cluster_it == entrance_clusters.end()) {
      entrance_clusters.push_back(
          {.grid_x = entrance_gx, .grid_z = entrance_gz, .radius = 0.0F, .samples = 1});
      continue;
    }

    const float previous_x = cluster_it->grid_x;
    const float previous_z = cluster_it->grid_z;
    cluster_it->samples += 1;
    const float weight = 1.0F / float(cluster_it->samples);
    cluster_it->grid_x += (entrance_gx - cluster_it->grid_x) * weight;
    cluster_it->grid_z += (entrance_gz - cluster_it->grid_z) * weight;
    const float center_shift =
        std::hypot(cluster_it->grid_x - previous_x, cluster_it->grid_z - previous_z);
    cluster_it->radius = std::max(
        cluster_it->radius + center_shift,
        std::hypot(entrance_gx - cluster_it->grid_x, entrance_gz - cluster_it->grid_z));
  }
  return entrance_clusters;
}

void HillStamp::trim_walkable_to_crown() {
  for (int z = min_z; z <= max_z; ++z) {
    for (int x = min_x; x <= max_x; ++x) {
      int const idx = L.index_at(x, z);
      if (L.types[idx] != TerrainType::Hill) {
        continue;
      }

      if (entrance_line_mask[idx] == 1) {
        continue;
      }

      const float dx = float(x) - grid_center_x;
      const float dz = float(z) - grid_center_z;
      const float rotated_x = dx * cos_a + dz * sin_a;
      const float rotated_z = -dx * sin_a + dz * cos_a;
      const float norm_plateau_dist = crown_distance(rotated_x, rotated_z);

      if (norm_plateau_dist > 1.0F) {
        walkable_mask[idx] = 0;
      }

      if (norm_plateau_dist > 0.85F) {
        bool adjacent_to_non_hill = false;
        constexpr int k_dirs[8][2] = {
            {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
        for (const auto& dir : k_dirs) {
          int const nx = x + dir[0];
          int const nz = z + dir[1];
          if (!L.in_bounds(nx, nz)) {
            adjacent_to_non_hill = true;
            break;
          }
          int const n_idx = L.index_at(nx, nz);
          if (L.types[n_idx] != TerrainType::Hill) {
            adjacent_to_non_hill = true;
            break;
          }
        }
        if (adjacent_to_non_hill) {
          walkable_mask[idx] = 0;
        }
      }
    }
  }
}

void HillStamp::trim_walkable_edges() {
  if (!entrance_line_mask.empty()) {
    for (int z = min_z; z <= max_z; ++z) {
      for (int x = min_x; x <= max_x; ++x) {
        int const idx = L.index_at(x, z);
        if (walkable_mask[idx] == 0 || entrance_line_mask[idx] == 1 ||
            L.types[idx] != TerrainType::Hill) {
          continue;
        }

        bool on_edge = false;
        constexpr int k_dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& dir : k_dirs) {
          int const nx = x + dir[0];
          int const nz = z + dir[1];
          if (!L.in_bounds(nx, nz)) {
            on_edge = true;
            break;
          }
          int const n_idx = L.index_at(nx, nz);
          if (L.types[n_idx] != TerrainType::Hill) {
            on_edge = true;
            break;
          }
        }
        if (on_edge) {
          walkable_mask[idx] = 0;
        }
      }
    }
  }
}

void HillStamp::flood_walkable_from_entrances() {
  const int map_cell_count = L.cell_count();
  if (!entrance_indices.empty()) {
    std::vector<std::uint8_t> visited(map_cell_count, 0);
    std::vector<int> queue;
    queue.reserve(entrance_indices.size());

    for (int const entrance_idx : entrance_indices) {
      if ((visited[entrance_idx] != 0U) || walkable_mask[entrance_idx] == 0) {
        continue;
      }
      visited[entrance_idx] = 1;
      L.hill_walkable[entrance_idx] = true;
      queue.push_back(entrance_idx);

      while (!queue.empty()) {
        int const idx = queue.back();
        queue.pop_back();

        int const cx = idx % L.width;
        int const cz = idx / L.width;

        constexpr int k_dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& dir : k_dirs) {
          int const nx = cx + dir[0];
          int const nz = cz + dir[1];
          if (!L.in_bounds(nx, nz)) {
            continue;
          }
          int const n_idx = L.index_at(nx, nz);
          if ((visited[n_idx] != 0U) || walkable_mask[n_idx] == 0) {
            continue;
          }

          visited[n_idx] = 1;
          L.hill_walkable[n_idx] = true;
          queue.push_back(n_idx);
        }
      }
    }
  }
}

void HillStamp::run() {
  const std::vector<QVector3D> hill_entrances = resolve_entrances();
  raise_body();
  for (const auto& entrance : cluster_entrances(hill_entrances)) {
    carve_entrance(entrance);
  }
  trim_walkable_to_crown();
  trim_walkable_edges();
  flood_walkable_from_entrances();
}

void stamp_hill(BuildContext& ctx,
                const TerrainFeature& feature,
                const FeatureFrame& frame) {
  HillStamp stamp(ctx, feature, frame);
  stamp.run();
}

} // namespace Game::Map::terrain_build
