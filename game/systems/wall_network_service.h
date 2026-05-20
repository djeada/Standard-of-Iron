#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "nation_id.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

struct WallGridPosition {
  int x{0};
  int z{0};
};

struct WallAppearance {
  std::string renderer_id;
  float rotation_y{0.0F};
  std::uint8_t connection_mask{0};
};

struct WallPlacementValidation {
  bool valid{false};
  std::string failure_reason;
};

class WallNetworkService {
public:
  using OccupancySet = std::unordered_set<std::uint64_t>;
  using OwnerOccupancyMap = std::unordered_map<int, OccupancySet>;

  static constexpr int k_segment_spacing = 2;
  static constexpr int k_wall_segment_wood_cost = 25;
  static constexpr std::uint8_t k_connection_north = 1U << 0U;
  static constexpr std::uint8_t k_connection_east = 1U << 1U;
  static constexpr std::uint8_t k_connection_south = 1U << 2U;
  static constexpr std::uint8_t k_connection_west = 1U << 3U;

  static auto snap_grid_coordinate(int grid_value) -> int;
  static auto snap_world_position(float world_x, float world_z) -> WallGridPosition;
  static auto build_axis_aligned_chain(const WallGridPosition& start,
                                       const WallGridPosition& end)
      -> std::vector<WallGridPosition>;

  static auto encode_key(int grid_x, int grid_z) -> std::uint64_t;
  static void add_world_occupancy(Engine::Core::World& world,
                                  OccupancySet& out,
                                  bool include_construction_sites = true);
  static void build_connection_occupancy(Engine::Core::World& world,
                                         OwnerOccupancyMap& out,
                                         bool include_construction_sites = true,
                                         bool include_towers = true);
  static auto compute_connection_mask(const OccupancySet& occupancy,
                                      int grid_x,
                                      int grid_z) -> std::uint8_t;
  static auto validate_wall_segment_placement(Engine::Core::World& world,
                                              const WallGridPosition& position,
                                              bool include_construction_sites = true,
                                              unsigned int ignore_entity_id = 0)
      -> WallPlacementValidation;
  static auto find_tower_snap_socket(Engine::Core::World& world,
                                     int owner_id,
                                     float world_x,
                                     float world_z,
                                     float max_snap_distance = 1.5F)
      -> std::optional<WallGridPosition>;
  static auto resolve_appearance(Game::Systems::NationID nation_id,
                                 std::uint8_t mask) -> WallAppearance;

  static void refresh_world(Engine::Core::World& world);
};

} // namespace Game::Systems
