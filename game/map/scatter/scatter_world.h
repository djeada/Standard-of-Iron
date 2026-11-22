#pragma once

#include <cstdint>
#include <memory>

#include "game/map/scatter/world_prop_clearance_index.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"

namespace Game::Map {

// Everything scatter generation reads from the live map, frozen at one moment:
// a copy of the terrain service (heights, roads, bridges, water, fords and the
// world props with their surface lookups), the building and authored-obstacle
// footprints, and the world-prop clearance index.
//
// The simulation writes the live services without a lock, so a generator that
// reads them must run where the simulation is not mutating the map. A snapshot
// lets it run anywhere: it is captured where the map is quiet (under the
// GameEngine frame lock at the scatter sync point in update_presentation, or on
// the loader thread) and then handed to a worker. The copy's prop-surface
// cache is private to the snapshot; jobs that share one snapshot run one at a
// time.
class ScatterWorld {
public:
  // Captures from the given live services. `clearance` is their clearance
  // index; pass null to build it from `terrain`.
  [[nodiscard]] static auto
  capture(const TerrainService& terrain,
          const Game::Systems::BuildingCollisionRegistry& buildings,
          std::shared_ptr<const WorldPropClearanceIndex> clearance = nullptr)
      -> std::shared_ptr<const ScatterWorld>;

  // Captures from the session's live services (TerrainService::instance(),
  // BuildingCollisionRegistry::instance() and the shared clearance index).
  [[nodiscard]] static auto capture_live() -> std::shared_ptr<const ScatterWorld>;

  [[nodiscard]] auto terrain() const noexcept -> const TerrainService& {
    return *m_terrain;
  }
  [[nodiscard]] auto
  buildings() const noexcept -> const Game::Systems::BuildingFootprintSnapshot& {
    return m_buildings;
  }
  [[nodiscard]] auto clearance() const noexcept -> const WorldPropClearanceIndex& {
    return *m_clearance;
  }

  // The live world-prop revision this snapshot was taken at.
  [[nodiscard]] auto world_props_revision() const noexcept -> std::uint64_t {
    return m_terrain->world_props_revision();
  }

private:
  ScatterWorld() = default;

  std::unique_ptr<TerrainService> m_terrain;
  Game::Systems::BuildingFootprintSnapshot m_buildings;
  std::shared_ptr<const WorldPropClearanceIndex> m_clearance;
};

} // namespace Game::Map
