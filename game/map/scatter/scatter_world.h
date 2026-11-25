#pragma once

#include <cstdint>
#include <memory>

#include "game/map/scatter/world_prop_clearance_index.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"

namespace Game::Map {

class ScatterWorld {
public:
  [[nodiscard]] static auto
  capture(const TerrainService& terrain,
          const Game::Systems::BuildingCollisionRegistry& buildings,
          std::shared_ptr<const WorldPropClearanceIndex> clearance = nullptr)
      -> std::shared_ptr<const ScatterWorld>;

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
