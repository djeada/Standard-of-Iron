#pragma once

#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <vector>

#include "game/map/biome_settings.h"
#include "game/map/map_definition.h"
#include "game/map/terrain.h"

namespace Game::Map {
class ScatterWorld;
class TerrainService;
} // namespace Game::Map

namespace Render::Ground {
struct GroundEcologyField;
}

namespace Render::Ground::Scatter {

inline constexpr std::size_t k_unbounded_instances =
    std::numeric_limits<std::size_t>::max();

struct ScatterGrid {
  int width = 0;
  int height = 0;
  float tile_size = 1.0F;
  const std::vector<float>* heights = nullptr;
  const std::vector<Game::Map::TerrainType>* types = nullptr;

  [[nodiscard]] auto empty() const noexcept -> bool {
    return heights == nullptr || types == nullptr;
  }
};

struct ScatterGridData {
  int width = 0;
  int height = 0;
  float tile_size = 1.0F;
  std::vector<float> heights;
  std::vector<Game::Map::TerrainType> types;

  [[nodiscard]] auto view() const noexcept -> ScatterGrid {
    return {width, height, tile_size, &heights, &types};
  }

  [[nodiscard]] static auto copy_of(const Game::Map::TerrainHeightMap& height_map)
      -> std::shared_ptr<const ScatterGridData> {
    auto data = std::make_shared<ScatterGridData>();
    data->width = height_map.get_width();
    data->height = height_map.get_height();
    data->tile_size = height_map.get_tile_size();
    data->heights = height_map.get_height_data();
    data->types = height_map.getTerrainTypes();
    return data;
  }
};

using ScatterAdoption = std::function<void()>;

using ScatterJob = std::function<ScatterAdoption()>;

struct ScatterRequest {
  ScatterGrid grid;

  const GroundEcologyField* ecology;
  const Game::Map::BiomeSettings& biome;

  const Game::Map::TerrainService& terrain;
  const std::vector<Game::Map::WorldProp>& world_props;

  const Game::Map::ScatterWorld* world = nullptr;
};

template <typename Instance>
auto apply_instance_budget(std::vector<Instance>& instances,
                           std::size_t budget) -> bool {
  if (instances.size() <= budget) {
    return false;
  }
  instances.resize(budget);
  return true;
}

} // namespace Render::Ground::Scatter
