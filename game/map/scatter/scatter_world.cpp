#include "game/map/scatter/scatter_world.h"

#include <utility>

namespace Game::Map {

auto ScatterWorld::capture(const TerrainService& terrain,
                           const Game::Systems::BuildingCollisionRegistry& buildings,
                           std::shared_ptr<const WorldPropClearanceIndex> clearance)
    -> std::shared_ptr<const ScatterWorld> {
  std::shared_ptr<ScatterWorld> world(new ScatterWorld());
  world->m_terrain = terrain.clone();
  world->m_buildings = buildings.footprint_snapshot();
  world->m_clearance = clearance != nullptr ? std::move(clearance)
                                            : build_world_prop_clearance_index(terrain);
  return world;
}

auto ScatterWorld::capture_live() -> std::shared_ptr<const ScatterWorld> {
  return capture(TerrainService::instance(),
                 Game::Systems::BuildingCollisionRegistry::instance(),
                 shared_world_prop_clearance_index());
}

} // namespace Game::Map
