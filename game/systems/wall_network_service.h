#pragma once

#include <cstdint>
#include <string>
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

class WallNetworkService {
public:
  using OccupancySet = std::unordered_set<std::uint64_t>;

  static constexpr int k_segment_spacing = 2;
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
  static auto compute_connection_mask(const OccupancySet& occupancy,
                                      int grid_x,
                                      int grid_z) -> std::uint8_t;
  static auto resolve_appearance(Game::Systems::NationID nation_id,
                                 std::uint8_t mask) -> WallAppearance;

  static void refresh_world(Engine::Core::World& world);
};

} // namespace Game::Systems
