#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace Engine::Core {
using EntityID = std::uint64_t;
}

namespace Game::Systems {

inline constexpr float k_default_building_grid_padding = 1.0F;

inline constexpr float k_wall_segment_grid_padding = 0.0F;

struct BuildingFootprint {
  float center_x;
  float center_z;
  float width;
  float depth;
  int owner_id;
  Engine::Core::EntityID entity_id;

  float grid_padding;

  BuildingFootprint(float x,
                    float z,
                    float w,
                    float d,
                    int owner,
                    Engine::Core::EntityID id,
                    float padding = k_default_building_grid_padding)
      : center_x(x)
      , center_z(z)
      , width(w)
      , depth(d)
      , owner_id(owner)
      , entity_id(id)
      , grid_padding(padding) {}
};

class BuildingCollisionRegistry {
public:
  BuildingCollisionRegistry() = default;
  ~BuildingCollisionRegistry() = default;
  BuildingCollisionRegistry(const BuildingCollisionRegistry&) = delete;
  BuildingCollisionRegistry(BuildingCollisionRegistry&&) = delete;
  auto
  operator=(const BuildingCollisionRegistry&) -> BuildingCollisionRegistry& = delete;
  auto operator=(BuildingCollisionRegistry&&) -> BuildingCollisionRegistry& = delete;

  static auto instance() -> BuildingCollisionRegistry&;

  struct BuildingSize {
    float width;
    float depth;
  };

  static auto get_building_size(const std::string& building_type) -> BuildingSize;

  static auto get_building_grid_padding(const std::string& building_type) -> float;

  void register_building(Engine::Core::EntityID entity_id,
                         const std::string& building_type,
                         float center_x,
                         float center_z,
                         int owner_id);

  void unregister_building(Engine::Core::EntityID entity_id);

  void set_authored_obstacles(std::vector<BuildingFootprint> obstacles);
  void clear_authored_obstacles();

  [[nodiscard]] auto
  authored_obstacles() const -> const std::vector<BuildingFootprint>& {
    return m_authored_obstacles;
  }

  void update_building_position(Engine::Core::EntityID entity_id,
                                float center_x,
                                float center_z);

  void update_building_owner(Engine::Core::EntityID entity_id, int owner_id);

  [[nodiscard]] auto
  get_all_buildings() const -> const std::vector<BuildingFootprint>& {
    return m_buildings;
  }

  [[nodiscard]] auto is_point_in_building(
      float x, float z, Engine::Core::EntityID ignore_entity_id = 0) const -> bool;

  [[nodiscard]] auto is_circle_overlapping_building(
      float x,
      float z,
      float radius,
      Engine::Core::EntityID ignore_entity_id = 0) const -> bool;

  [[nodiscard]] static auto get_occupied_grid_cells(const BuildingFootprint& footprint,
                                                    float grid_cell_size = 1.0F)
      -> std::vector<std::pair<int, int>>;

  static constexpr float k_default_grid_padding = k_default_building_grid_padding;
  static void set_grid_padding(float padding);
  static auto get_grid_padding() -> float;

  void clear();

private:
  void release_authored_obstacles_within(float center_x,
                                         float center_z,
                                         float width,
                                         float depth);

  std::vector<BuildingFootprint> m_buildings;

  std::vector<BuildingFootprint> m_authored_obstacles;
  std::map<Engine::Core::EntityID, size_t> m_entity_to_index;

  static const std::map<std::string, BuildingSize> s_building_sizes;

  static float s_grid_padding;
};

} // namespace Game::Systems
