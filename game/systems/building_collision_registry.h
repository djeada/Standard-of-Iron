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

  static auto
  get_building_size(const std::string &building_type) -> BuildingSize;

  void register_building(unsigned int entity_id,
                         const std::string &building_type, float center_x,
                         float center_z, int owner_id);

  void unregister_building(unsigned int entity_id);

  void update_building_position(unsigned int entity_id, float center_x,
                                float center_z);

  void update_building_owner(unsigned int entity_id, int owner_id);

  [[nodiscard]] auto
  get_all_buildings() const -> const std::vector<BuildingFootprint> & {
    return m_buildings;
  }

  [[nodiscard]] auto
  is_point_in_building(float x, float z,
                       unsigned int ignore_entity_id = 0) const -> bool;

  [[nodiscard]] auto is_circle_overlapping_building(
      float x, float z, float radius,
      unsigned int ignore_entity_id = 0) const -> bool;

  [[nodiscard]] static auto get_occupied_grid_cells(
      const BuildingFootprint &footprint,
      float grid_cell_size = 1.0F) -> std::vector<std::pair<int, int>>;

  static constexpr float k_default_grid_padding = 1.0F;
  static void set_grid_padding(float padding);
  static auto get_grid_padding() -> float;

  void clear();

private:
  BuildingCollisionRegistry() = default;
  ~BuildingCollisionRegistry() = default;
  BuildingCollisionRegistry(const BuildingCollisionRegistry &) = delete;
  auto operator=(const BuildingCollisionRegistry &)
      -> BuildingCollisionRegistry & = delete;

  std::vector<BuildingFootprint> m_buildings;
  std::map<unsigned int, size_t> m_entity_to_index;

  static const std::map<std::string, BuildingSize> s_building_sizes;

  static float s_grid_padding;
};

} // namespace Game::Systems
