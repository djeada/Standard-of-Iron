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

// The terrain grid a generator samples: size, tile size, heights and terrain
// types. A view; the layer that calls the generator owns the vectors.
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

// A copy of the terrain grid a layer generates from. Shared, so a regeneration
// running on a worker keeps it alive after the layer moves on.
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

// The render-thread half of a regeneration: swaps finished instances into the
// layer that planned it.
using ScatterAdoption = std::function<void()>;
// The worker half: reads only what it captured (a snapshot, copies of the
// layer's inputs) and returns the adoption.
using ScatterJob = std::function<ScatterAdoption()>;

// What a scatter generator reads. A generator is the first of a scatter
// layer's three stages (generate, sync, submit): a GL-free, deterministic
// function from these inputs to the layer's instances. The seed is
// `biome.seed`; the same request yields the same instances in the same order.
//
// With `world` set the request is self-contained: `terrain` is the snapshot's
// copy of the terrain service, and the spawn validator behind grass, stones and
// plants reads building footprints, roads, bridges, water and the world-prop
// clearance index from the snapshot. Such a request can be generated on a
// worker while the simulation keeps writing the live map; that is how the
// runtime refreshes (world props changing, anchored trees, grass after a
// graphics-profile change) run. Without `world` the validator reads the live
// services, which is only safe where the simulation is not mutating the map:
// at map load (the loader thread) or a synchronous refresh on the thread that
// owns the map.
struct ScatterRequest {
  ScatterGrid grid;
  // The map's ground-ecology field (forest, margin, talus...); null when the
  // generator does not use it or none was built.
  const GroundEcologyField* ecology;
  const Game::Map::BiomeSettings& biome;
  // Resolves world-prop footprints and surface heights; the snapshot's copy
  // when `world` is set.
  const Game::Map::TerrainService& terrain;
  const std::vector<Game::Map::WorldProp>& world_props;
  // The frozen inputs, or null to read the live services.
  const Game::Map::ScatterWorld* world = nullptr;
};

// Keeps at most `budget` instances, in generation order. True when the budget
// cut anything.
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
