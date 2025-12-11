#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

namespace Game::Systems {

struct BuildingFootprint {
  float center_x;
  float center_z;
  float width;
  float depth;
  int owner_id;
  unsigned int entity_id;

  BuildingFootprint(float x, float z, float w, float d, int owner,
                    unsigned int id)
      : center_x(x), center_z(z), width(w), depth(d), owner_id(owner),
        entity_id(id) {}
};

class BuildingCollisionRegistry {
public:
  static auto instance() -> BuildingCollisionRegistry &;

  struct BuildingSize {
    float width;
    float depth;
  };

  static auto getBuildingSize(const std::string &buildingType) -> BuildingSize;

  void registerBuilding(unsigned int entity_id, const std::string &buildingType,
                        float center_x, float center_z, int owner_id);

  void unregisterBuilding(unsigned int entity_id);

  void updateBuildingPosition(unsigned int entity_id, float center_x,
                              float center_z);

  void updateBuildingOwner(unsigned int entity_id, int owner_id);

  [[nodiscard]] auto
  getAllBuildings() const -> const std::vector<BuildingFootprint> & {
    return m_buildings;
  }

  [[nodiscard]] auto
  isPointInBuilding(float x, float z,
                    unsigned int ignoreEntityId = 0) const -> bool;

  [[nodiscard]] static auto getOccupiedGridCells(
      const BuildingFootprint &footprint,
      float grid_cell_size = 1.0F) -> std::vector<std::pair<int, int>>;

  static constexpr float kDefaultGridPadding = 0.1F;
  static void setGridPadding(float padding);
  static auto getGridPadding() -> float;

  void clear();

private:
  BuildingCollisionRegistry() = default;
  ~BuildingCollisionRegistry() = default;
  BuildingCollisionRegistry(const BuildingCollisionRegistry &) = delete;
  auto operator=(const BuildingCollisionRegistry &)
      -> BuildingCollisionRegistry & = delete;

  std::vector<BuildingFootprint> m_buildings;
  std::map<unsigned int, size_t> m_entityToIndex;

  static const std::map<std::string, BuildingSize> s_buildingSizes;

  static float s_gridPadding;
};

} // namespace Game::Systems
