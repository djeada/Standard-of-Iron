#pragma once

#include <QVector3D>

#include <memory>
#include <optional>
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

  [[nodiscard]] auto terrain_field() const -> const TerrainField& {
    return m_terrain_field;
  }

  [[nodiscard]] auto world_props() const -> const std::vector<WorldProp>& {
    return m_world_props;
  }

  void remove_non_persistent_props();

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
                               const std::vector<WorldProp>& world_props = {});

private:
  TerrainService() = default;
  ~TerrainService() = default;

  TerrainService(const TerrainService&) = delete;
  auto operator=(const TerrainService&) -> TerrainService& = delete;

  void rebuild_terrain_field();

  std::unique_ptr<TerrainHeightMap> m_height_map;
  TerrainField m_terrain_field;
  BiomeSettings m_biome_settings;
  std::vector<WorldProp> m_world_props;
  std::vector<RoadSegment> m_road_segments;
};

} // namespace Game::Map
