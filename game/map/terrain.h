#pragma once

#include <QVector3D>

#include <cstdint>
#include <optional>
#include <vector>

#include "bridge_geometry.h"
#include "ground_type.h"
#include "terrain_features.h"

namespace Game::Map {

struct BiomeSettings;
struct TerrainSurfaceProfile;

struct TerrainField {
  int width = 0;
  int height = 0;
  float tile_size = 1.0F;
  std::vector<float> heights;
  std::vector<float> slopes;
  std::vector<float> curvature;

  void clear();

  [[nodiscard]] auto empty() const -> bool;

  [[nodiscard]] auto sample_height_at(float gx, float gz) const -> float;

  [[nodiscard]] auto sample_slope_at(int grid_x, int grid_z) const -> float;

  [[nodiscard]] auto sample_curvature_at(int grid_x, int grid_z) const -> float;
};

class TerrainHeightMap {
public:
  TerrainHeightMap(int width, int height, float tile_size);

  void build_from_features(const std::vector<TerrainFeature>& features);

  void add_river_segments(const std::vector<RiverSegment>& river_segments);

  void add_lakes(const std::vector<Lake>& lakes);

  // Marks the authored ford crossings (and every river segment flagged as a
  // ford) as wadeable and lowers their beds so troops stand waist-deep.
  // Call after add_river_segments and before add_bridges.
  void add_fords(const std::vector<FordCrossing>& fords);

  [[nodiscard]] auto get_fords() const -> const std::vector<FordCrossing>& {
    return m_fords;
  }

  [[nodiscard]] auto is_ford_cell(int grid_x, int grid_z) const -> bool;

  // The ford profile of the cell under a world point, or null on dry land
  // and on unfordable water.
  [[nodiscard]] auto ford_profile_at(float world_x,
                                     float world_z) const -> const FordProfile*;

  [[nodiscard]] auto ford_profile_at_grid(int grid_x,
                                          int grid_z) const -> const FordProfile*;

  // Metres of water over the ground at a world point inside a ford; zero on
  // dry land, on the bank, and anywhere that is not a ford.
  [[nodiscard]] auto ford_water_depth_at(float world_x, float world_z) const -> float;

  // The water surface height of the ford under a world point.
  [[nodiscard]] auto ford_water_level_at(float world_x,
                                         float world_z) const -> std::optional<float>;

  [[nodiscard]] auto has_fords() const -> bool { return !m_ford_table.empty(); }

  [[nodiscard]] auto get_height_at(float world_x, float world_z) const -> float;

  [[nodiscard]] auto get_base_height_at(float world_x, float world_z) const -> float;

  [[nodiscard]] auto get_height_at_grid(int grid_x, int grid_z) const -> float;

  [[nodiscard]] auto is_walkable(int grid_x, int grid_z) const -> bool;

  [[nodiscard]] auto isHillEntrance(int grid_x, int grid_z) const -> bool;

  [[nodiscard]] auto
  getHillEntranceTraversalPosition(float world_x,
                                   float world_z) const -> std::optional<QVector3D>;

  [[nodiscard]] auto getTerrainType(int grid_x, int grid_z) const -> TerrainType;

  [[nodiscard]] auto is_fields(int grid_x, int grid_z) const -> bool;

  [[nodiscard]] auto
  isRiverOrNearby(int grid_x, int grid_z, int margin = 1) const -> bool;

  [[nodiscard]] auto get_width() const -> int { return m_width; }
  [[nodiscard]] auto get_height() const -> int { return m_height; }
  [[nodiscard]] auto get_tile_size() const -> float { return m_tile_size; }

  [[nodiscard]] auto get_height_data() const -> const std::vector<float>& {
    return m_heights;
  }
  [[nodiscard]] auto getTerrainTypes() const -> const std::vector<TerrainType>& {
    return m_terrain_types;
  }
  [[nodiscard]] auto getHillEntrances() const -> const std::vector<bool>& {
    return m_hill_entrances;
  }
  [[nodiscard]] auto get_river_segments() const -> const std::vector<RiverSegment>& {
    return m_river_segments;
  }
  [[nodiscard]] auto get_lakes() const -> const std::vector<Lake>& { return m_lakes; }

  void add_bridges(const std::vector<Bridge>& bridges);
  [[nodiscard]] auto get_bridges() const -> const std::vector<Bridge>& {
    return m_bridges;
  }

  [[nodiscard]] auto isOnBridge(float world_x, float world_z) const -> bool;

  [[nodiscard]] auto isBridgeCell(int grid_x, int grid_z) const -> bool;

  [[nodiscard]] auto isBridgeCenterline(int grid_x, int grid_z) const -> bool;

  [[nodiscard]] auto getBridgeCenterPosition(float world_x, float world_z) const
      -> std::optional<QVector3D>;

  [[nodiscard]] auto getBridgeTraversalPosition(float world_x, float world_z) const
      -> std::optional<QVector3D>;

  [[nodiscard]] auto getBridgeDeckHeight(float world_x,
                                         float world_z) const -> std::optional<float>;

  void apply_biome_variation(const BiomeSettings& settings);

  [[nodiscard]] auto hill_navigation() const -> HillNavigation;

  void restore_from_data(const std::vector<float>& heights,
                         const std::vector<TerrainType>& terrain_types,
                         const std::vector<RiverSegment>& rivers,
                         const std::vector<Bridge>& bridges,
                         const std::vector<Lake>& lakes = {},
                         const HillNavigation& hills = {},
                         const std::vector<FordCrossing>& fords = {});

private:
  int m_width;
  int m_height;
  float m_tile_size;

  std::vector<float> m_heights;
  std::vector<TerrainType> m_terrain_types;
  std::vector<bool> m_hill_entrances;
  std::vector<bool> m_hill_walkable;
  std::vector<std::uint8_t> m_fields;
  std::vector<HillEntranceCenterline> m_hill_entrance_centerlines;
  std::vector<RiverSegment> m_river_segments;
  std::vector<Lake> m_lakes;
  std::vector<Bridge> m_bridges;

  std::vector<bool> m_on_bridge;
  std::vector<bool> m_bridge_walkable;
  std::vector<bool> m_water_blocked;
  std::vector<bool> m_bridge_centerline;
  std::vector<QVector3D> m_bridge_centers;

  struct FordCell {
    FordProfile profile;
    float water_y = 0.0F;
  };
  std::vector<FordCrossing> m_fords;
  std::vector<FordCell> m_ford_table;
  std::vector<std::uint8_t> m_ford_cells;

  [[nodiscard]] auto indexAt(int x, int z) const -> int;
  [[nodiscard]] auto in_bounds(int x, int z) const -> bool;

  void precompute_bridge_data();

  void stamp_bridge_cells(const Bridge& bridge);

  void carve_river_channels();

  void carve_river_channel(const RiverSegment& river,
                           const std::vector<float>& land_before_rivers,
                           float campaign_bank_blend_cells);

  void apply_ground_irregularity(const TerrainSurfaceProfile& surface_profile);

  void apply_legacy_height_noise(const TerrainSurfaceProfile& surface_profile);

  void precompute_water_blocked();

  void precompute_ford_cells(bool lower_beds);
  auto ford_table_index(const FordProfile& profile, float water_y) -> std::uint8_t;
  void stamp_ford_cell(int x, int z, std::uint8_t entry);
  [[nodiscard]] auto ford_entry_at(float world_x,
                                   float world_z) const -> const FordCell*;

  [[nodiscard]] static auto calculateFeatureHeight(const TerrainFeature& feature,
                                                   float world_x,
                                                   float world_z) -> float;
};

} // namespace Game::Map
