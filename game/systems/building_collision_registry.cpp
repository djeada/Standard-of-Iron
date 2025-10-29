#include "building_collision_registry.h"
#include "command_service.h"
#include "pathfinding.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace Game::Systems {

const std::map<std::string, BuildingCollisionRegistry::BuildingSize>
    BuildingCollisionRegistry::s_buildingSizes = {
        {"barracks", {4.F, 4.F}},

};

float BuildingCollisionRegistry::s_gridPadding =
    BuildingCollisionRegistry::kDefaultGridPadding;

auto BuildingCollisionRegistry::instance() -> BuildingCollisionRegistry & {
  static BuildingCollisionRegistry instance;
  return instance;
}

auto BuildingCollisionRegistry::getBuildingSize(const std::string &buildingType)
    -> BuildingCollisionRegistry::BuildingSize {
  auto it = s_buildingSizes.find(buildingType);
  if (it != s_buildingSizes.end()) {
    return it->second;
  }

  return {2.0F, 2.0F};
}

void BuildingCollisionRegistry::registerBuilding(
    unsigned int entity_id, const std::string &buildingType, float center_x,
    float center_z, int owner_id) {

  if (m_entityToIndex.find(entity_id) != m_entityToIndex.end()) {

    updateBuildingPosition(entity_id, center_x, center_z);
    return;
  }

  BuildingSize const size = getBuildingSize(buildingType);
  BuildingFootprint const footprint(center_x, center_z, size.width, size.depth,
                                    owner_id, entity_id);

  m_buildings.push_back(footprint);
  m_entityToIndex[entity_id] = m_buildings.size() - 1;

  if (auto *pf = CommandService::getPathfinder()) {
    pf->markObstaclesDirty();
  }
}

void BuildingCollisionRegistry::unregisterBuilding(unsigned int entity_id) {
  auto it = m_entityToIndex.find(entity_id);
  if (it == m_entityToIndex.end()) {
    return;
  }

  size_t const index = it->second;

  if (index != m_buildings.size() - 1) {
    std::swap(m_buildings[index], m_buildings.back());

    m_entityToIndex[m_buildings[index].entity_id] = index;
  }

  m_buildings.pop_back();
  m_entityToIndex.erase(entity_id);

  if (auto *pf = CommandService::getPathfinder()) {
    pf->markObstaclesDirty();
  }
}

void BuildingCollisionRegistry::updateBuildingPosition(unsigned int entity_id,
                                                       float center_x,
                                                       float center_z) {
  auto it = m_entityToIndex.find(entity_id);
  if (it == m_entityToIndex.end()) {
    return;
  }

  size_t const index = it->second;
  m_buildings[index].center_x = center_x;
  m_buildings[index].center_z = center_z;

  if (auto *pf = CommandService::getPathfinder()) {
    pf->markObstaclesDirty();
  }
}

void BuildingCollisionRegistry::updateBuildingOwner(unsigned int entity_id,
                                                    int owner_id) {
  auto it = m_entityToIndex.find(entity_id);
  if (it == m_entityToIndex.end()) {
    return;
  }

  size_t const index = it->second;
  m_buildings[index].owner_id = owner_id;
}

auto BuildingCollisionRegistry::isPointInBuilding(
    float x, float z, unsigned int ignoreEntityId) const -> bool {
  for (const auto &building : m_buildings) {
    if (ignoreEntityId != 0 && building.entity_id == ignoreEntityId) {
      continue;
    }

    float const half_width = building.width / 2.0F;
    float const half_depth = building.depth / 2.0F;

    float const min_x = building.center_x - half_width;
    float const max_x = building.center_x + half_width;
    float const min_z = building.center_z - half_depth;
    float const max_z = building.center_z + half_depth;

    if (x >= min_x && x <= max_x && z >= min_z && z <= max_z) {
      return true;
    }
  }
  return false;
}

auto BuildingCollisionRegistry::getOccupiedGridCells(
    const BuildingFootprint &footprint,
    float gridCellSize) -> std::vector<std::pair<int, int>> {
  std::vector<std::pair<int, int>> cells;

  float const half_width = footprint.width / 2.0F;
  float const half_depth = footprint.depth / 2.0F;

  float const padding = s_gridPadding;
  int const min_grid_x = static_cast<int>(
      std::floor((footprint.center_x - half_width - padding) / gridCellSize));
  int const max_grid_x = static_cast<int>(
      std::ceil((footprint.center_x + half_width + padding) / gridCellSize));
  int const min_grid_z = static_cast<int>(
      std::floor((footprint.center_z - half_depth - padding) / gridCellSize));
  int const max_grid_z = static_cast<int>(
      std::ceil((footprint.center_z + half_depth + padding) / gridCellSize));

  for (int gx = min_grid_x; gx < max_grid_x; ++gx) {
    for (int gz = min_grid_z; gz < max_grid_z; ++gz) {
      cells.emplace_back(gx, gz);
    }
  }

  return cells;
}

void BuildingCollisionRegistry::clear() {
  m_buildings.clear();
  m_entityToIndex.clear();
}

void BuildingCollisionRegistry::setGridPadding(float padding) {
  s_gridPadding = padding;

  if (auto *pf = CommandService::getPathfinder()) {
    pf->markObstaclesDirty();
  }
}

auto BuildingCollisionRegistry::getGridPadding() -> float {
  return s_gridPadding;
}

} // namespace Game::Systems
