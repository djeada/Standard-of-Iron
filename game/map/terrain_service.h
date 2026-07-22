#pragma once

#include <QVector3D>

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

#include "map_definition.h"
#include "terrain.h"

namespace Game::Map {

struct MapDefinition;
enum class SurfaceHeightKind {
  Fallback,
  Terrain,
  Road,
  Bridge,
};

struct SurfaceHeightSample {
  float world_y{0.0F};
  SurfaceHeightKind kind{SurfaceHeightKind::Fallback};
};

struct WorldPropTarget {
  std::uint64_t id = 0;
  WorldProp::Type type = WorldProp::Type::Tent;
  float x = 0.0F;
  float z = 0.0F;
};

class TerrainService {
public:
  static auto instance() -> TerrainService&;

  void initialize(const MapDefinition& map_def);

  void clear();

  [[nodiscard]] auto get_terrain_height(float world_x, float world_z) const -> float;

  [[nodiscard]] auto
  sample_surface_height(float world_x,
                        float world_z,
                        float fallback_y = 0.0F) const -> SurfaceHeightSample;

  [[nodiscard]] auto resolve_surface_world_y(float world_x,
                                             float world_z,
                                             float world_y_offset = 0.0F,
                                             float fallback_y = 0.0F) const -> float;

  [[nodiscard]] auto
  resolve_surface_world_position(float world_x,
                                 float world_z,
                                 float world_y_offset = 0.0F,
                                 float fallback_y = 0.0F) const -> QVector3D;

  [[nodiscard]] auto get_terrain_height_grid(int grid_x, int grid_z) const -> float;

  [[nodiscard]] auto is_walkable(int grid_x, int grid_z) const -> bool;

  [[nodiscard]] auto is_hill_entrance(int grid_x, int grid_z) const -> bool;

  [[nodiscard]] auto is_forbidden(int grid_x, int grid_z) const -> bool;

  [[nodiscard]] auto is_forbidden_world(float world_x, float world_z) const -> bool;

  [[nodiscard]] auto get_terrain_type(int grid_x, int grid_z) const -> TerrainType;

  [[nodiscard]] auto get_height_map() const -> const TerrainHeightMap* {
    return m_height_map.get();
  }

  [[nodiscard]] auto biome_settings() const -> const BiomeSettings& {
    return m_biome_settings;
  }
  [[nodiscard]] auto coord_system() const -> CoordSystem { return m_coord_system; }

  [[nodiscard]] auto terrain_field() const -> const TerrainField& {
    return m_terrain_field;
  }

  [[nodiscard]] auto world_props() const -> const std::vector<WorldProp>& {
    return m_world_props;
  }
  [[nodiscard]] auto world_props_revision() const -> std::uint64_t {
    return m_world_props_revision;
  }
  [[nodiscard]] auto authored_world_props() const -> const std::vector<WorldProp>& {
    return m_authored_world_props;
  }
  [[nodiscard]] auto authored_world_props_revision() const -> std::uint64_t {
    return m_authored_world_props_revision;
  }

  void remove_non_persistent_props();
  [[nodiscard]] auto
  world_prop_world_position(const WorldProp& prop,
                            float world_y_offset = 0.0F,
                            float fallback_y = 0.0F) const -> QVector3D;
  [[nodiscard]] auto find_tree_near_world(float world_x,
                                          float world_z,
                                          float max_world_distance = 2.5F) const
      -> std::optional<WorldPropTarget>;
  [[nodiscard]] auto
  find_tree_near_grid(float grid_x, float grid_z, float max_grid_distance = 3.0F) const
      -> std::optional<WorldPropTarget>;
  [[nodiscard]] auto
  find_tree_by_id(std::uint64_t tree_id) const -> std::optional<WorldPropTarget>;
  [[nodiscard]] auto find_boulder_near_world(float world_x,
                                             float world_z,
                                             float max_world_distance = 2.5F) const
      -> std::optional<WorldPropTarget>;
  [[nodiscard]] auto find_boulder_near_grid(float grid_x,
                                            float grid_z,
                                            float max_grid_distance = 3.0F) const
      -> std::optional<WorldPropTarget>;
  [[nodiscard]] auto
  find_boulder_by_id(std::uint64_t boulder_id) const -> std::optional<WorldPropTarget>;
  [[nodiscard]] auto find_iron_ore_near_world(float world_x,
                                              float world_z,
                                              float max_world_distance = 2.5F) const
      -> std::optional<WorldPropTarget>;
  [[nodiscard]] auto find_iron_ore_near_grid(float grid_x,
                                             float grid_z,
                                             float max_grid_distance = 3.0F) const
      -> std::optional<WorldPropTarget>;
  [[nodiscard]] auto find_iron_ore_by_id(std::uint64_t iron_ore_id) const
      -> std::optional<WorldPropTarget>;
  [[nodiscard]] auto find_harvestable_world_prop_by_id(
      std::uint64_t world_prop_id) const -> std::optional<WorldPropTarget>;
  [[nodiscard]] auto is_world_prop_reserved(std::uint64_t world_prop_id) const -> bool;
  [[nodiscard]] auto reserve_world_prop(std::uint64_t world_prop_id) -> bool;
  void release_world_prop(std::uint64_t world_prop_id);
  [[nodiscard]] auto harvest_world_prop(std::uint64_t world_prop_id) -> bool;

  [[nodiscard]] auto road_segments() const -> const std::vector<RoadSegment>& {
    return m_road_segments;
  }

  [[nodiscard]] auto is_point_on_road(float world_x, float world_z) const -> bool;
  [[nodiscard]] auto
  is_point_near_road(float world_x, float world_z, float clearance) const -> bool;

  [[nodiscard]] auto is_on_bridge(float world_x, float world_z) const -> bool;
  [[nodiscard]] auto
  is_point_near_bridge(float world_x, float world_z, float clearance) const -> bool;
  [[nodiscard]] auto
  is_point_near_river(float world_x, float world_z, float clearance) const -> bool;
  [[nodiscard]] auto
  is_point_near_water(float world_x, float world_z, float clearance) const -> bool;

  [[nodiscard]] auto get_bridge_center_position(float world_x, float world_z) const
      -> std::optional<QVector3D>;

  [[nodiscard]] auto get_bridge_traversal_position(float world_x, float world_z) const
      -> std::optional<QVector3D>;

  [[nodiscard]] auto is_initialized() const -> bool { return m_height_map != nullptr; }

  void restore_from_serialized(int width,
                               int height,
                               float tile_size,
                               const std::vector<float>& heights,
                               const std::vector<TerrainType>& terrain_types,
                               const std::vector<RiverSegment>& rivers,
                               const std::vector<RoadSegment>& roads,
                               const std::vector<Bridge>& bridges,
                               const BiomeSettings& biome,
                               const std::vector<WorldProp>& world_props = {},
                               const std::vector<WorldProp>& authored_world_props = {},
                               const std::vector<Lake>& lakes = {});

private:
  TerrainService() = default;
  ~TerrainService() = default;

  TerrainService(const TerrainService&) = delete;
  auto operator=(const TerrainService&) -> TerrainService& = delete;

  void rebuild_terrain_field();
  void rebuild_road_spatial_index();
  [[nodiscard]] auto is_point_near_indexed_road(float world_x,
                                                float world_z,
                                                float clearance) const -> bool;
  [[nodiscard]] auto sample_surface_base_height(
      float world_x, float world_z, float fallback_y) const -> SurfaceHeightSample;
  void normalize_world_props(std::vector<WorldProp>& world_props);
  void sync_world_prop_identity_state();
  void bump_world_props_revision();
  void bump_authored_world_props_revision();

  std::unique_ptr<TerrainHeightMap> m_height_map;
  TerrainField m_terrain_field;
  BiomeSettings m_biome_settings;
  CoordSystem m_coord_system{CoordSystem::Grid};
  std::vector<WorldProp> m_authored_world_props;
  std::vector<WorldProp> m_world_props;
  std::vector<RoadSegment> m_road_segments;
  struct RoadQuerySegment {
    float start_x{0.0F};
    float start_z{0.0F};
    float delta_x{0.0F};
    float delta_z{0.0F};
    float inverse_length_sq{0.0F};
    float half_width{0.0F};
    float min_x{0.0F};
    float max_x{0.0F};
    float min_z{0.0F};
    float max_z{0.0F};
  };
  std::vector<RoadQuerySegment> m_road_query_segments;
  float m_road_index_origin_x{0.0F};
  float m_road_index_origin_z{0.0F};
  float m_road_index_cell_size{1.0F};
  int m_road_index_columns{0};
  int m_road_index_rows{0};
  std::vector<std::uint32_t> m_road_index_offsets;
  std::vector<std::uint32_t> m_road_index_segment_ids;
  std::unordered_set<std::uint64_t> m_reserved_world_prop_ids;
  std::uint64_t m_next_world_prop_id{1};
  std::uint64_t m_authored_world_props_revision{0};
  std::uint64_t m_world_props_revision{0};
};

} // namespace Game::Map
