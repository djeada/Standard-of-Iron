#pragma once

#include "../../game/map/terrain.h"
#include <QVector3D>
#include <cstdint>
#include <vector>

namespace Render::Ground {

/**
 * @brief Configuration for spawn validation checks.
 *
 * This struct contains all the parameters needed to validate whether
 * a position is suitable for spawning objects (plants, stones, trees,
 * fire camps, etc.). Use this to configure which checks should be
 * performed and with what thresholds.
 */
struct SpawnValidationConfig {
  // Grid dimensions and tile size
  int grid_width = 0;
  int grid_height = 0;
  float tile_size = 1.0F;

  // Edge padding (fraction of map size to exclude from edges)
  float edge_padding = 0.08F;

  // Slope threshold (0-1, higher means steeper slopes are allowed)
  float max_slope = 0.65F;

  // River margin (number of tiles around rivers to exclude)
  int river_margin = 1;

  // Terrain type restrictions
  bool allow_flat = true;
  bool allow_hill = false;
  bool allow_mountain = false;
  bool allow_river = false;

  // Check flags
  bool check_buildings = true;
  bool check_roads = true;
  bool check_slope = true;
  bool check_river_margin = true;
};

/**
 * @brief Cached terrain data for efficient spawn validation.
 *
 * This struct holds precomputed terrain data (normals, heights) to avoid
 * redundant calculations during spawn validation. The data should be
 * computed once and reused for all spawn checks in a renderer.
 */
struct SpawnTerrainCache {
  std::vector<QVector3D> normals;
  std::vector<float> heights;
  std::vector<Game::Map::TerrainType> terrain_types;
  int width = 0;
  int height = 0;
  float tile_size = 1.0F;

  /**
   * @brief Build the terrain cache from height map data.
   *
   * @param height_data The height data from TerrainHeightMap
   * @param types The terrain types from TerrainHeightMap
   * @param w Grid width
   * @param h Grid height
   * @param ts Tile size
   */
  void build_from_height_map(const std::vector<float> &height_data,
                             const std::vector<Game::Map::TerrainType> &types,
                             int w, int h, float ts);

  /**
   * @brief Sample interpolated height at grid coordinates.
   *
   * @param gx Grid x coordinate (can be fractional)
   * @param gz Grid z coordinate (can be fractional)
   * @return Interpolated height value
   */
  [[nodiscard]] auto sample_height_at(float gx, float gz) const -> float;

  /**
   * @brief Get the slope at a grid position (0 = flat, 1 = vertical).
   *
   * @param grid_x Grid x coordinate (integer)
   * @param grid_z Grid z coordinate (integer)
   * @return Slope value between 0 and 1
   */
  [[nodiscard]] auto get_slope_at(int grid_x, int grid_z) const -> float;

  /**
   * @brief Get the terrain type at a grid position.
   *
   * @param grid_x Grid x coordinate
   * @param grid_z Grid z coordinate
   * @return Terrain type at the position
   */
  [[nodiscard]] auto get_terrain_type_at(int grid_x,
                                         int grid_z) const -> Game::Map::TerrainType;
};

/**
 * @brief Unified spawn validator for random object placement.
 *
 * This class provides a centralized, efficient way to validate whether
 * a position is suitable for spawning objects. It consolidates all the
 * spawn validation logic that was previously duplicated across multiple
 * renderer files.
 *
 * Usage:
 * 1. Create a SpawnValidator with the terrain cache and configuration
 * 2. Call can_spawn_at_grid() or can_spawn_at_world() to check positions
 * 3. Reuse the same validator for all spawn checks in a renderer
 */
class SpawnValidator {
public:
  /**
   * @brief Construct a spawn validator.
   *
   * @param cache Reference to the terrain cache (must outlive the validator)
   * @param config Validation configuration
   */
  SpawnValidator(const SpawnTerrainCache &cache,
                 const SpawnValidationConfig &config);

  /**
   * @brief Check if an object can be spawned at grid coordinates.
   *
   * This method performs all configured checks to determine if a position
   * is valid for spawning. It checks: edge padding, terrain type,
   * river margin, slope, buildings, and roads.
   *
   * @param gx Grid x coordinate (can be fractional)
   * @param gz Grid z coordinate (can be fractional)
   * @return true if the position is valid for spawning
   */
  [[nodiscard]] auto can_spawn_at_grid(float gx, float gz) const -> bool;

  /**
   * @brief Check if an object can be spawned at world coordinates.
   *
   * @param world_x World x coordinate
   * @param world_z World z coordinate
   * @return true if the position is valid for spawning
   */
  [[nodiscard]] auto can_spawn_at_world(float world_x,
                                        float world_z) const -> bool;

  /**
   * @brief Convert grid coordinates to world coordinates.
   *
   * @param gx Grid x coordinate
   * @param gz Grid z coordinate
   * @param out_world_x Output world x coordinate
   * @param out_world_z Output world z coordinate
   */
  void grid_to_world(float gx, float gz, float &out_world_x,
                     float &out_world_z) const;

  /**
   * @brief Convert world coordinates to grid coordinates.
   *
   * @param world_x World x coordinate
   * @param world_z World z coordinate
   * @param out_gx Output grid x coordinate
   * @param out_gz Output grid z coordinate
   */
  void world_to_grid(float world_x, float world_z, float &out_gx,
                     float &out_gz) const;

private:
  const SpawnTerrainCache &m_cache;
  SpawnValidationConfig m_config;

  // Precomputed edge margins
  float m_edge_margin_x = 0.0F;
  float m_edge_margin_z = 0.0F;
  float m_half_width = 0.0F;
  float m_half_height = 0.0F;

  /**
   * @brief Check if position is within edge padding bounds.
   */
  [[nodiscard]] auto check_edge_padding(float gx, float gz) const -> bool;

  /**
   * @brief Check if terrain type is allowed at position.
   */
  [[nodiscard]] auto check_terrain_type(int grid_x, int grid_z) const -> bool;

  /**
   * @brief Check if position is far enough from rivers.
   */
  [[nodiscard]] auto check_river_margin(int grid_x, int grid_z) const -> bool;

  /**
   * @brief Check if slope is acceptable at position.
   */
  [[nodiscard]] auto check_slope(int grid_x, int grid_z) const -> bool;

  /**
   * @brief Check if position collides with buildings.
   */
  [[nodiscard]] auto check_building_collision(float world_x,
                                              float world_z) const -> bool;

  /**
   * @brief Check if position is on a road.
   */
  [[nodiscard]] auto check_road_collision(float world_x,
                                          float world_z) const -> bool;
};

/**
 * @brief Create a default spawn config for plants/grass.
 */
[[nodiscard]] auto make_plant_spawn_config() -> SpawnValidationConfig;

/**
 * @brief Create a default spawn config for stones.
 */
[[nodiscard]] auto make_stone_spawn_config() -> SpawnValidationConfig;

/**
 * @brief Create a default spawn config for trees (pine, olive).
 */
[[nodiscard]] auto make_tree_spawn_config() -> SpawnValidationConfig;

/**
 * @brief Create a default spawn config for fire camps.
 */
[[nodiscard]] auto make_firecamp_spawn_config() -> SpawnValidationConfig;

/**
 * @brief Create a default spawn config for grass blades.
 */
[[nodiscard]] auto make_grass_spawn_config() -> SpawnValidationConfig;

} // namespace Render::Ground
