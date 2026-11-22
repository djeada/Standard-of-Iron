#pragma once

#include <cstddef>
#include <limits>
#include <vector>

#include "game/map/biome_settings.h"
#include "game/map/map_definition.h"
#include "game/map/terrain.h"

namespace Game::Map {
class TerrainService;
}

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

// What a scatter generator reads. A generator is the first of a scatter
// layer's three stages (generate, sync, submit): a GL-free, deterministic
// function from these inputs to the layer's instances. The seed is
// `biome.seed`; the same request in the same session yields the same instances
// in the same order.
//
// Not everything comes through the request yet. The spawn validator behind
// grass, stones and plants also asks process-wide state: building footprints
// (BuildingCollisionRegistry::instance()), roads, bridges and water
// (TerrainService::instance()) and the world-prop clearance index. Those are
// written by the simulation without a lock, and TerrainService fills a mutable
// prop-surface cache from const lookups. So a generator must run where the
// simulation is not mutating the map: at map load (the loader thread, before
// the renderer resumes) or on the render thread under the scatter lock. Moving
// generation onto a worker first needs those inputs snapshotted into the
// request.
struct ScatterRequest {
  ScatterGrid grid;
  // The map's ground-ecology field (forest, margin, talus...); null when the
  // generator does not use it or none was built.
  const GroundEcologyField* ecology;
  const Game::Map::BiomeSettings& biome;
  // Resolves world-prop footprints and surface heights.
  const Game::Map::TerrainService& terrain;
  const std::vector<Game::Map::WorldProp>& world_props;
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
