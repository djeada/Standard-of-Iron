#pragma once

#include "../../game/map/terrain.h"
#include <QVector3D>
#include <cstdint>
#include <vector>

namespace Render::Ground {

struct SpawnValidationConfig {

  int grid_width = 0;
  int grid_height = 0;
  float tile_size = 1.0F;

  float edge_padding = 0.08F;

  float max_slope = 0.65F;

  int river_margin = 1;

  bool allow_flat = true;
  bool allow_hill = false;
  bool allow_mountain = false;
  bool allow_river = false;

  bool check_buildings = true;
  bool check_roads = true;
  bool check_bridges = true;
  bool check_slope = true;
  bool check_river_margin = true;
};

struct SpawnTerrainCache {
  std::vector<QVector3D> normals;
  std::vector<float> heights;
  std::vector<Game::Map::TerrainType> terrain_types;
  int width = 0;
  int height = 0;
  float tile_size = 1.0F;

  void build_from_height_map(const std::vector<float> &height_data,
                             const std::vector<Game::Map::TerrainType> &types,
                             int w, int h, float ts);

  [[nodiscard]] auto sample_height_at(float gx, float gz) const -> float;

  [[nodiscard]] auto get_slope_at(int grid_x, int grid_z) const -> float;

  [[nodiscard]] auto
  get_terrain_type_at(int grid_x, int grid_z) const -> Game::Map::TerrainType;
};

class SpawnValidator {
public:
  SpawnValidator(const SpawnTerrainCache &cache,
                 const SpawnValidationConfig &config);

  [[nodiscard]] auto can_spawn_at_grid(float gx, float gz) const -> bool;

  [[nodiscard]] auto can_spawn_at_world(float world_x,
                                        float world_z) const -> bool;

  void grid_to_world(float gx, float gz, float &out_world_x,
                     float &out_world_z) const;

  void world_to_grid(float world_x, float world_z, float &out_gx,
                     float &out_gz) const;

private:
  const SpawnTerrainCache &m_cache;
  SpawnValidationConfig m_config;

  float m_edge_margin_x = 0.0F;
  float m_edge_margin_z = 0.0F;
  float m_half_width = 0.0F;
  float m_half_height = 0.0F;

  [[nodiscard]] auto check_edge_padding(float gx, float gz) const -> bool;

  [[nodiscard]] auto check_terrain_type(int grid_x, int grid_z) const -> bool;

  [[nodiscard]] auto check_river_margin(int grid_x, int grid_z) const -> bool;

  [[nodiscard]] auto check_slope(int grid_x, int grid_z) const -> bool;

  [[nodiscard]] auto check_building_collision(float world_x,
                                              float world_z) const -> bool;

  [[nodiscard]] auto check_road_collision(float world_x,
                                          float world_z) const -> bool;

  [[nodiscard]] auto check_bridge_collision(float world_x,
                                            float world_z) const -> bool;
};

[[nodiscard]] auto make_plant_spawn_config() -> SpawnValidationConfig;

[[nodiscard]] auto make_stone_spawn_config() -> SpawnValidationConfig;

[[nodiscard]] auto make_tree_spawn_config() -> SpawnValidationConfig;

[[nodiscard]] auto make_firecamp_spawn_config() -> SpawnValidationConfig;

[[nodiscard]] auto make_grass_spawn_config() -> SpawnValidationConfig;

} // namespace Render::Ground
