#pragma once

#include "terrain.h"
#include <memory>
#include <vector>

namespace Game::Map {

struct MapDefinition;
struct FireCamp;

class TerrainService {
public:
  static auto instance() -> TerrainService &;

  void initialize(const MapDefinition &mapDef);

  void clear();

  [[nodiscard]] auto getTerrainHeight(float world_x,
                                      float world_z) const -> float;

  [[nodiscard]] auto getTerrainHeightGrid(int grid_x,
                                          int grid_z) const -> float;

  [[nodiscard]] auto isWalkable(int grid_x, int grid_z) const -> bool;

  [[nodiscard]] auto isHillEntrance(int grid_x, int grid_z) const -> bool;

  [[nodiscard]] auto isForbidden(int grid_x, int grid_z) const -> bool;

  [[nodiscard]] auto isForbiddenWorld(float world_x,
                                      float world_z) const -> bool;

  [[nodiscard]] auto getTerrainType(int grid_x,
                                    int grid_z) const -> TerrainType;

  [[nodiscard]] auto getHeightMap() const -> const TerrainHeightMap * {
    return m_height_map.get();
  }

  [[nodiscard]] auto biomeSettings() const -> const BiomeSettings & {
    return m_biomeSettings;
  }

  [[nodiscard]] auto fire_camps() const -> const std::vector<FireCamp> & {
    return m_fire_camps;
  }

  [[nodiscard]] auto road_segments() const -> const std::vector<RoadSegment> & {
    return m_road_segments;
  }

  [[nodiscard]] auto is_point_on_road(float world_x,
                                      float world_z) const -> bool;

  [[nodiscard]] auto isInitialized() const -> bool {
    return m_height_map != nullptr;
  }

  void restoreFromSerialized(int width, int height, float tile_size,
                             const std::vector<float> &heights,
                             const std::vector<TerrainType> &terrain_types,
                             const std::vector<RiverSegment> &rivers,
                             const std::vector<RoadSegment> &roads,
                             const std::vector<Bridge> &bridges,
                             const BiomeSettings &biome);

private:
  TerrainService() = default;
  ~TerrainService() = default;

  TerrainService(const TerrainService &) = delete;
  auto operator=(const TerrainService &) -> TerrainService & = delete;

  std::unique_ptr<TerrainHeightMap> m_height_map;
  BiomeSettings m_biomeSettings;
  std::vector<FireCamp> m_fire_camps;
  std::vector<RoadSegment> m_road_segments;
};

} // namespace Game::Map
