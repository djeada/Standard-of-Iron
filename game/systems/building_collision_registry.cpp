#include "building_collision_registry.h"
#include "command_service.h"
#include "pathfinding.h"
#include <algorithm>
#include <cmath>

namespace Game::Systems {

const std::map<std::string, BuildingCollisionRegistry::BuildingSize>
    BuildingCollisionRegistry::s_buildingSizes = {
        {"barracks", {4.f, 4.f}},

};

float BuildingCollisionRegistry::s_gridPadding =
    BuildingCollisionRegistry::kDefaultGridPadding;

BuildingCollisionRegistry &BuildingCollisionRegistry::instance() {
  static BuildingCollisionRegistry instance;
  return instance;
}

BuildingCollisionRegistry::BuildingSize
BuildingCollisionRegistry::getBuildingSize(const std::string &buildingType) {
  auto it = s_buildingSizes.find(buildingType);
  if (it != s_buildingSizes.end()) {
    return it->second;
  }

  return {2.0f, 2.0f};
}

void BuildingCollisionRegistry::registerBuilding(
    unsigned int entityId, const std::string &buildingType, float centerX,
    float centerZ, int ownerId) {

  if (m_entityToIndex.find(entityId) != m_entityToIndex.end()) {

    updateBuildingPosition(entityId, centerX, centerZ);
    return;
  }

  BuildingSize size = getBuildingSize(buildingType);
  BuildingFootprint footprint(centerX, centerZ, size.width, size.depth, ownerId,
                              entityId);

  m_buildings.push_back(footprint);
  m_entityToIndex[entityId] = m_buildings.size() - 1;

  if (auto *pf = CommandService::getPathfinder()) {
    pf->markObstaclesDirty();
  }
}

void BuildingCollisionRegistry::unregisterBuilding(unsigned int entityId) {
  auto it = m_entityToIndex.find(entityId);
  if (it == m_entityToIndex.end()) {
    return;
  }

  size_t index = it->second;

  if (index != m_buildings.size() - 1) {
    std::swap(m_buildings[index], m_buildings.back());

    m_entityToIndex[m_buildings[index].entityId] = index;
  }

  m_buildings.pop_back();
  m_entityToIndex.erase(entityId);

  if (auto *pf = CommandService::getPathfinder()) {
    pf->markObstaclesDirty();
  }
}

void BuildingCollisionRegistry::updateBuildingPosition(unsigned int entityId,
                                                       float centerX,
                                                       float centerZ) {
  auto it = m_entityToIndex.find(entityId);
  if (it == m_entityToIndex.end()) {
    return;
  }

  size_t index = it->second;
  m_buildings[index].centerX = centerX;
  m_buildings[index].centerZ = centerZ;

  if (auto *pf = CommandService::getPathfinder()) {
    pf->markObstaclesDirty();
  }
}

void BuildingCollisionRegistry::updateBuildingOwner(unsigned int entityId,
                                                    int ownerId) {
  auto it = m_entityToIndex.find(entityId);
  if (it == m_entityToIndex.end()) {
    return;
  }

  size_t index = it->second;
  m_buildings[index].ownerId = ownerId;
}

bool BuildingCollisionRegistry::isPointInBuilding(
    float x, float z, unsigned int ignoreEntityId) const {
  for (const auto &building : m_buildings) {
    if (ignoreEntityId != 0 && building.entityId == ignoreEntityId) {
      continue;
    }

    float halfWidth = building.width / 2.0f;
    float halfDepth = building.depth / 2.0f;

    float minX = building.centerX - halfWidth;
    float maxX = building.centerX + halfWidth;
    float minZ = building.centerZ - halfDepth;
    float maxZ = building.centerZ + halfDepth;

    if (x >= minX && x <= maxX && z >= minZ && z <= maxZ) {
      return true;
    }
  }
  return false;
}

std::vector<std::pair<int, int>>
BuildingCollisionRegistry::getOccupiedGridCells(
    const BuildingFootprint &footprint, float gridCellSize) const {
  std::vector<std::pair<int, int>> cells;

  float halfWidth = footprint.width / 2.0f;
  float halfDepth = footprint.depth / 2.0f;

  float padding = s_gridPadding;
  int minGridX = static_cast<int>(
      std::floor((footprint.centerX - halfWidth - padding) / gridCellSize));
  int maxGridX = static_cast<int>(
      std::ceil((footprint.centerX + halfWidth + padding) / gridCellSize));
  int minGridZ = static_cast<int>(
      std::floor((footprint.centerZ - halfDepth - padding) / gridCellSize));
  int maxGridZ = static_cast<int>(
      std::ceil((footprint.centerZ + halfDepth + padding) / gridCellSize));

  for (int gx = minGridX; gx < maxGridX; ++gx) {
    for (int gz = minGridZ; gz < maxGridZ; ++gz) {
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

float BuildingCollisionRegistry::getGridPadding() { return s_gridPadding; }

} // namespace Game::Systems
