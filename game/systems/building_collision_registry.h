#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace Engine::Core {
using EntityID = std::uint64_t;
}

namespace Game::Systems {

inline constexpr float k_default_building_grid_padding = 1.0F;
inline constexpr float k_wall_segment_grid_padding = 0.0F;

inline constexpr float k_wall_link_spacing = 2.0F;

[[nodiscard]] inline auto
is_wall_link_building_type(const std::string& building_type) -> bool {
  return building_type == "wall_segment" || building_type == "wall_gate";
}

[[nodiscard]] inline auto
is_wall_tower_building_type(const std::string& building_type) -> bool {
  return building_type == "defense_tower";
}

struct BuildingFootprint {
  float center_x;
  float center_z;
  float width;
  float depth;
  int owner_id;
  Engine::Core::EntityID entity_id;

  float grid_padding{k_default_building_grid_padding};
  bool blocks_navigation{true};

  bool wall_link{false};

  bool wall_tower{false};

  float body_center_x{0.0F};
  float body_center_z{0.0F};
  float body_width{0.0F};
  float body_depth{0.0F};

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
      , grid_padding(padding)
      , body_center_x(x)
      , body_center_z(z)
      , body_width(w)
      , body_depth(d) {}
};

struct NavigationPassage {
  float center_x{0.0F};
  float center_z{0.0F};
  float width{2.0F};
  float depth{2.0F};
  Engine::Core::EntityID source_entity_id{0};
};

class BuildingCollisionRegistry {
public:
  BuildingCollisionRegistry();
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

  using RegionDirtyHook = void (*)(float center_x,
                                   float center_z,
                                   float width,
                                   float depth);
  using GridDirtyHook = void (*)();

  struct ObstructionRelease {
    float center_x{0.0F};
    float center_z{0.0F};
    bool located{false};
  };

  using ObstructionReleasedHook = void (*)(const ObstructionRelease& release);
  static void set_region_dirty_hook(RegionDirtyHook hook);
  static void set_grid_dirty_hook(GridDirtyHook hook);
  static void set_obstruction_released_hook(ObstructionReleasedHook hook);

  static auto get_building_size(const std::string& building_type) -> BuildingSize;

  struct BuildingBody {
    float width;
    float depth;
    float offset_x;
    float offset_z;
  };

  static auto get_building_body(const std::string& building_type) -> BuildingBody;

  static auto axis_aligned_size(BuildingSize size,
                                float facing_degrees) -> BuildingSize;

  static auto rotate_body_offset(BuildingBody body,
                                 float facing_degrees) -> BuildingBody;

  static auto get_building_grid_padding(const std::string& building_type) -> float;

  void register_building(Engine::Core::EntityID entity_id,
                         const std::string& building_type,
                         float center_x,
                         float center_z,
                         int owner_id,
                         float facing_degrees = 0.0F);

  void register_building(Engine::Core::EntityID entity_id,
                         const std::string& building_type,
                         float center_x,
                         float center_z,
                         int owner_id,
                         BuildingSize size);

  void resize_building(Engine::Core::EntityID entity_id, BuildingSize size);

  void unregister_building(Engine::Core::EntityID entity_id);

  void set_authored_obstacles(std::vector<BuildingFootprint> obstacles);
  void clear_authored_obstacles();

  [[nodiscard]] auto
  authored_obstacles() const -> const std::vector<BuildingFootprint>& {
    return m_authored_obstacles;
  }

  void apply_building_body(Engine::Core::EntityID entity_id,
                           const std::string& building_type,
                           float facing_degrees);

  void update_building_position(Engine::Core::EntityID entity_id,
                                float center_x,
                                float center_z);

  void update_building_owner(Engine::Core::EntityID entity_id, int owner_id);

  void set_building_navigation_blocking(Engine::Core::EntityID entity_id,
                                        bool blocks_navigation);

  [[nodiscard]] auto
  find_building(Engine::Core::EntityID entity_id) const -> const BuildingFootprint*;

  [[nodiscard]] auto
  get_all_buildings() const -> const std::vector<BuildingFootprint>& {
    return m_buildings;
  }

  template <typename Fn>
  void for_each_building_in_region(
      float min_x, float max_x, float min_z, float max_z, Fn&& fn) const {
    float const expansion = m_max_half_extent + s_grid_padding;
    int const min_bucket_x = bucket_coord(min_x - expansion);
    int const max_bucket_x = bucket_coord(max_x + expansion);
    int const min_bucket_z = bucket_coord(min_z - expansion);
    int const max_bucket_z = bucket_coord(max_z + expansion);
    for (int bucket_x = min_bucket_x; bucket_x <= max_bucket_x; ++bucket_x) {
      for (int bucket_z = min_bucket_z; bucket_z <= max_bucket_z; ++bucket_z) {
        auto const bucket = m_spatial_buckets.find(bucket_key(bucket_x, bucket_z));
        if (bucket == m_spatial_buckets.end()) {
          continue;
        }
        for (Engine::Core::EntityID const id : bucket->second) {
          auto const index = m_entity_to_index.find(id);
          if (index == m_entity_to_index.end()) {
            continue;
          }
          BuildingFootprint const& footprint = m_buildings[index->second];
          float const half_width = footprint.width * 0.5F + footprint.grid_padding;
          float const half_depth = footprint.depth * 0.5F + footprint.grid_padding;
          if (footprint.center_x + half_width < min_x ||
              footprint.center_x - half_width > max_x ||
              footprint.center_z + half_depth < min_z ||
              footprint.center_z - half_depth > max_z) {
            continue;
          }
          fn(footprint);
        }
      }
    }
  }

  [[nodiscard]] auto is_point_in_building(
      float x, float z, Engine::Core::EntityID ignore_entity_id = 0) const -> bool;

  [[nodiscard]] auto is_point_in_blocking_building(float x, float z) const -> bool;

  [[nodiscard]] auto is_rect_overlapping_blocking_building(
      float min_x,
      float max_x,
      float min_z,
      float max_z,
      Engine::Core::EntityID ignore_entity_id = 0) const -> bool;

  [[nodiscard]] auto blocking_penetration_depth(float x, float z) const -> float;

  [[nodiscard]] auto segment_crosses_blocking_building(float start_x,
                                                       float start_z,
                                                       float end_x,
                                                       float end_z) const -> bool;

  [[nodiscard]] auto is_circle_overlapping_building(
      float x,
      float z,
      float radius,
      Engine::Core::EntityID ignore_entity_id = 0) const -> bool;

  void set_navigation_passages(std::vector<NavigationPassage> passages);

  [[nodiscard]] auto
  navigation_passages() const -> const std::vector<NavigationPassage>& {
    return m_navigation_passages;
  }

  static constexpr float k_default_grid_padding = k_default_building_grid_padding;
  static void set_grid_padding(float padding);
  static auto get_grid_padding() -> float;

  void clear();

private:
  void release_authored_obstacles_within(float center_x,
                                         float center_z,
                                         float width,
                                         float depth);
  static constexpr float k_spatial_bucket_size = 16.0F;
  static auto bucket_coord(float coordinate) -> int;
  static auto bucket_key(int bucket_x, int bucket_z) -> std::int64_t;
  void add_to_spatial_index(const BuildingFootprint& footprint);
  void remove_from_spatial_index(const BuildingFootprint& footprint);

  std::vector<BuildingFootprint> m_buildings;

  std::vector<BuildingFootprint> m_authored_obstacles;
  std::vector<NavigationPassage> m_navigation_passages;
  std::map<Engine::Core::EntityID, size_t> m_entity_to_index;
  std::unordered_map<std::int64_t, std::vector<Engine::Core::EntityID>>
      m_spatial_buckets;
  float m_max_half_extent{0.0F};

  static const std::map<std::string, BuildingSize> s_building_sizes;
  static const std::map<std::string, BuildingBody> s_building_bodies;

  static float s_grid_padding;
};

} // namespace Game::Systems
