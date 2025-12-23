#include "spawn_validator.h"
#include "../../game/map/terrain_service.h"
#include "../../game/systems/building_collision_registry.h"
#include <algorithm>
#include <cmath>

namespace Render::Ground {

void SpawnTerrainCache::build_from_height_map(
    const std::vector<float> &height_data,
    const std::vector<Game::Map::TerrainType> &types, int w, int h, float ts) {
  width = w;
  height = h;
  tile_size = ts;
  heights = height_data;
  terrain_types = types;

  normals.resize(static_cast<size_t>(width * height),
                 QVector3D(0.0F, 1.0F, 0.0F));

  if (width < 2 || height < 2 || heights.empty()) {
    return;
  }

  for (int z = 0; z < height; ++z) {
    for (int x = 0; x < width; ++x) {
      int const idx = z * width + x;
      float const gx0 = std::clamp(static_cast<float>(x) - 1.0F, 0.0F,
                                   static_cast<float>(width - 1));
      float const gx1 = std::clamp(static_cast<float>(x) + 1.0F, 0.0F,
                                   static_cast<float>(width - 1));
      float const gz0 = std::clamp(static_cast<float>(z) - 1.0F, 0.0F,
                                   static_cast<float>(height - 1));
      float const gz1 = std::clamp(static_cast<float>(z) + 1.0F, 0.0F,
                                   static_cast<float>(height - 1));

      float const h_l = sample_height_at(gx0, static_cast<float>(z));
      float const h_r = sample_height_at(gx1, static_cast<float>(z));
      float const h_d = sample_height_at(static_cast<float>(x), gz0);
      float const h_u = sample_height_at(static_cast<float>(x), gz1);

      QVector3D const dx(2.0F * tile_size, h_r - h_l, 0.0F);
      QVector3D const dz(0.0F, h_u - h_d, 2.0F * tile_size);
      QVector3D n = QVector3D::crossProduct(dz, dx);
      if (n.lengthSquared() > 0.0F) {
        n.normalize();
      } else {
        n = QVector3D(0, 1, 0);
      }
      normals[static_cast<size_t>(idx)] = n;
    }
  }
}

auto SpawnTerrainCache::sample_height_at(float gx, float gz) const -> float {
  if (heights.empty() || width < 1 || height < 1) {
    return 0.0F;
  }

  gx = std::clamp(gx, 0.0F, static_cast<float>(width - 1));
  gz = std::clamp(gz, 0.0F, static_cast<float>(height - 1));

  int const x0 = static_cast<int>(std::floor(gx));
  int const z0 = static_cast<int>(std::floor(gz));
  int const x1 = std::min(x0 + 1, width - 1);
  int const z1 = std::min(z0 + 1, height - 1);

  float const tx = gx - static_cast<float>(x0);
  float const tz = gz - static_cast<float>(z0);

  float const h00 = heights[static_cast<size_t>(z0 * width + x0)];
  float const h10 = heights[static_cast<size_t>(z0 * width + x1)];
  float const h01 = heights[static_cast<size_t>(z1 * width + x0)];
  float const h11 = heights[static_cast<size_t>(z1 * width + x1)];

  float const h0 = h00 * (1.0F - tx) + h10 * tx;
  float const h1 = h01 * (1.0F - tx) + h11 * tx;
  return h0 * (1.0F - tz) + h1 * tz;
}

auto SpawnTerrainCache::get_slope_at(int grid_x, int grid_z) const -> float {
  if (normals.empty() || grid_x < 0 || grid_x >= width || grid_z < 0 ||
      grid_z >= height) {
    return 0.0F;
  }

  int const idx = grid_z * width + grid_x;
  QVector3D const normal = normals[static_cast<size_t>(idx)];
  return 1.0F - std::clamp(normal.y(), 0.0F, 1.0F);
}

auto SpawnTerrainCache::get_terrain_type_at(int grid_x, int grid_z) const
    -> Game::Map::TerrainType {
  if (terrain_types.empty() || grid_x < 0 || grid_x >= width || grid_z < 0 ||
      grid_z >= height) {
    return Game::Map::TerrainType::Flat;
  }

  int const idx = grid_z * width + grid_x;
  return terrain_types[static_cast<size_t>(idx)];
}

SpawnValidator::SpawnValidator(const SpawnTerrainCache &cache,
                               const SpawnValidationConfig &config)
    : m_cache(cache), m_config(config) {

  float const edge_padding = std::clamp(m_config.edge_padding, 0.0F, 0.5F);
  m_edge_margin_x = static_cast<float>(m_config.grid_width) * edge_padding;
  m_edge_margin_z = static_cast<float>(m_config.grid_height) * edge_padding;

  m_half_width = static_cast<float>(m_config.grid_width) * 0.5F - 0.5F;
  m_half_height = static_cast<float>(m_config.grid_height) * 0.5F - 0.5F;
}

auto SpawnValidator::can_spawn_at_grid(float gx, float gz) const -> bool {

  if (!check_edge_padding(gx, gz)) {
    return false;
  }

  float const sgx =
      std::clamp(gx, 0.0F, static_cast<float>(m_config.grid_width - 1));
  float const sgz =
      std::clamp(gz, 0.0F, static_cast<float>(m_config.grid_height - 1));

  int const grid_x = std::clamp(static_cast<int>(std::floor(sgx + 0.5F)), 0,
                                m_config.grid_width - 1);
  int const grid_z = std::clamp(static_cast<int>(std::floor(sgz + 0.5F)), 0,
                                m_config.grid_height - 1);

  if (!check_terrain_type(grid_x, grid_z)) {
    return false;
  }

  if (m_config.check_river_margin && !check_river_margin(grid_x, grid_z)) {
    return false;
  }

  if (m_config.check_slope && !check_slope(grid_x, grid_z)) {
    return false;
  }

  float world_x = 0.0F;
  float world_z = 0.0F;
  grid_to_world(gx, gz, world_x, world_z);

  if (m_config.check_buildings && !check_building_collision(world_x, world_z)) {
    return false;
  }

  if (m_config.check_roads && !check_road_collision(world_x, world_z)) {
    return false;
  }

  if (m_config.check_bridges && !check_bridge_collision(world_x, world_z)) {
    return false;
  }

  return true;
}

auto SpawnValidator::can_spawn_at_world(float world_x,
                                        float world_z) const -> bool {
  float gx = 0.0F;
  float gz = 0.0F;
  world_to_grid(world_x, world_z, gx, gz);
  return can_spawn_at_grid(gx, gz);
}

void SpawnValidator::grid_to_world(float gx, float gz, float &out_world_x,
                                   float &out_world_z) const {
  out_world_x = (gx - m_half_width) * m_config.tile_size;
  out_world_z = (gz - m_half_height) * m_config.tile_size;
}

void SpawnValidator::world_to_grid(float world_x, float world_z, float &out_gx,
                                   float &out_gz) const {
  out_gx = world_x / m_config.tile_size + m_half_width;
  out_gz = world_z / m_config.tile_size + m_half_height;
}

auto SpawnValidator::check_edge_padding(float gx, float gz) const -> bool {
  if (gx < m_edge_margin_x ||
      gx > static_cast<float>(m_config.grid_width - 1) - m_edge_margin_x) {
    return false;
  }
  if (gz < m_edge_margin_z ||
      gz > static_cast<float>(m_config.grid_height - 1) - m_edge_margin_z) {
    return false;
  }
  return true;
}

auto SpawnValidator::check_terrain_type(int grid_x, int grid_z) const -> bool {
  Game::Map::TerrainType const terrain_type =
      m_cache.get_terrain_type_at(grid_x, grid_z);

  switch (terrain_type) {
  case Game::Map::TerrainType::Flat:
    return m_config.allow_flat;
  case Game::Map::TerrainType::Hill:
    return m_config.allow_hill;
  case Game::Map::TerrainType::Mountain:
    return m_config.allow_mountain;
  case Game::Map::TerrainType::River:
    return m_config.allow_river;
  case Game::Map::TerrainType::Forest:
    return m_config.allow_flat; // Treat forest like flat terrain for spawning
  }
  return m_config.allow_flat;
}

auto SpawnValidator::check_river_margin(int grid_x, int grid_z) const -> bool {
  int const margin = m_config.river_margin;

  for (int dz = -margin; dz <= margin; ++dz) {
    for (int dx = -margin; dx <= margin; ++dx) {
      if (dx == 0 && dz == 0) {
        continue;
      }
      int const nx = grid_x + dx;
      int const nz = grid_z + dz;
      if (nx >= 0 && nx < m_config.grid_width && nz >= 0 &&
          nz < m_config.grid_height) {
        if (m_cache.get_terrain_type_at(nx, nz) ==
            Game::Map::TerrainType::River) {
          return false;
        }
      }
    }
  }
  return true;
}

auto SpawnValidator::check_slope(int grid_x, int grid_z) const -> bool {
  float const slope = m_cache.get_slope_at(grid_x, grid_z);
  return slope <= m_config.max_slope;
}

auto SpawnValidator::check_building_collision(float world_x,
                                              float world_z) const -> bool {
  auto &building_registry =
      Game::Systems::BuildingCollisionRegistry::instance();
  return !building_registry.is_point_in_building(world_x, world_z);
}

auto SpawnValidator::check_road_collision(float world_x,
                                          float world_z) const -> bool {
  auto &terrain_service = Game::Map::TerrainService::instance();
  return !terrain_service.is_point_on_road(world_x, world_z);
}

auto SpawnValidator::check_bridge_collision(float world_x,
                                            float world_z) const -> bool {
  auto &terrain_service = Game::Map::TerrainService::instance();
  return !terrain_service.is_on_bridge(world_x, world_z);
}

auto make_plant_spawn_config() -> SpawnValidationConfig {
  SpawnValidationConfig config;
  config.edge_padding = 0.08F;
  config.max_slope = 0.65F;
  config.river_margin = 1;
  config.allow_flat = true;
  config.allow_hill = false;
  config.allow_mountain = false;
  config.allow_river = false;
  config.check_buildings = true;
  config.check_roads = true;
  config.check_slope = true;
  config.check_river_margin = true;
  return config;
}

auto make_stone_spawn_config() -> SpawnValidationConfig {
  SpawnValidationConfig config;
  config.edge_padding = 0.08F;
  config.max_slope = 0.15F;
  config.river_margin = 1;
  config.allow_flat = true;
  config.allow_hill = false;
  config.allow_mountain = false;
  config.allow_river = false;
  config.check_buildings = true;
  config.check_roads = false;
  config.check_slope = true;
  config.check_river_margin = true;
  return config;
}

auto make_tree_spawn_config() -> SpawnValidationConfig {
  SpawnValidationConfig config;
  config.edge_padding = 0.08F;
  config.max_slope = 0.75F;
  config.river_margin = 1;
  config.allow_flat = true;
  config.allow_hill = false;
  config.allow_mountain = false;
  config.allow_river = false;
  config.check_buildings = true;
  config.check_roads = true;
  config.check_slope = true;
  config.check_river_margin = true;
  return config;
}

auto make_firecamp_spawn_config() -> SpawnValidationConfig {
  SpawnValidationConfig config;
  config.edge_padding = 0.08F;
  config.max_slope = 0.30F;
  config.river_margin = 0;
  config.allow_flat = true;
  config.allow_hill = true;
  config.allow_mountain = false;
  config.allow_river = false;
  config.check_buildings = true;
  config.check_roads = true;
  config.check_slope = true;
  config.check_river_margin = false;
  return config;
}

auto make_grass_spawn_config() -> SpawnValidationConfig {
  SpawnValidationConfig config;
  config.edge_padding = 0.08F;
  config.max_slope = 0.92F;
  config.river_margin = 1;
  config.allow_flat = true;
  config.allow_hill = false;
  config.allow_mountain = false;
  config.allow_river = false;
  config.check_buildings = true;
  config.check_roads = true;
  config.check_slope = true;
  config.check_river_margin = true;
  return config;
}

} // namespace Render::Ground
