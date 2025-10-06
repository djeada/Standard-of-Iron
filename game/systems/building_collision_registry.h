#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

namespace Game::Systems {

struct BuildingFootprint {
  float centerX;
  float centerZ;
  float width;
  float depth;
  int ownerId;
  unsigned int entityId;

  BuildingFootprint(float x, float z, float w, float d, int owner,
                    unsigned int id)
      : centerX(x), centerZ(z), width(w), depth(d), ownerId(owner),
        entityId(id) {}
};

class BuildingCollisionRegistry {
public:
  static BuildingCollisionRegistry &instance();

  struct BuildingSize {
    float width;
    float depth;
  };

  static BuildingSize getBuildingSize(const std::string &buildingType);

  void registerBuilding(unsigned int entityId, const std::string &buildingType,
                        float centerX, float centerZ, int ownerId);

  void unregisterBuilding(unsigned int entityId);

  void updateBuildingPosition(unsigned int entityId, float centerX,
                              float centerZ);

  const std::vector<BuildingFootprint> &getAllBuildings() const {
    return m_buildings;
  }

  bool isPointInBuilding(float x, float z,
                         unsigned int ignoreEntityId = 0) const;

  std::vector<std::pair<int, int>>
  getOccupiedGridCells(const BuildingFootprint &footprint,
                       float gridCellSize = 1.0f) const;

  static constexpr float kDefaultGridPadding = 0.1f;
  static void setGridPadding(float padding);
  static float getGridPadding();

  void clear();

private:
  BuildingCollisionRegistry() = default;
  ~BuildingCollisionRegistry() = default;
  BuildingCollisionRegistry(const BuildingCollisionRegistry &) = delete;
  BuildingCollisionRegistry &
  operator=(const BuildingCollisionRegistry &) = delete;

  std::vector<BuildingFootprint> m_buildings;
  std::map<unsigned int, size_t> m_entityToIndex;

  static const std::map<std::string, BuildingSize> s_buildingSizes;

  static float s_gridPadding;
};

} // namespace Game::Systems
