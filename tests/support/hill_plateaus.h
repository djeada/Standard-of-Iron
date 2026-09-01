#pragma once

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
#include "game/systems/nav_grid.h"
#include "game/systems/pathfinding.h"

namespace TestSupport {

[[nodiscard]] inline auto on_walkable_hill(Game::Systems::Point cell,
                                           int grid) -> bool {
  if (cell.x < 0 || cell.x >= grid || cell.y < 0 || cell.y >= grid) {
    return false;
  }
  auto const& terrain = Game::Map::TerrainService::instance();
  auto const* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  return terrain.get_terrain_type(cell.x, cell.y) == Game::Map::TerrainType::Hill &&
         terrain.is_walkable(cell.x, cell.y) && pathfinder != nullptr &&
         pathfinder->is_walkable(cell.x, cell.y);
}

[[nodiscard]] inline auto
hill_plateaus(int grid) -> std::vector<std::vector<Game::Systems::Point>> {
  using Game::Systems::Point;

  std::vector<std::uint8_t> seen(static_cast<std::size_t>(grid) * grid, 0U);
  std::vector<std::vector<Point>> found;
  for (int z = 0; z < grid; ++z) {
    for (int x = 0; x < grid; ++x) {
      Point const seed{x, z};
      auto const seed_index = static_cast<std::size_t>(z) * grid + x;
      if (seen[seed_index] != 0U || !on_walkable_hill(seed, grid)) {
        continue;
      }
      std::vector<Point> cells{seed};
      std::vector<Point> queue{seed};
      seen[seed_index] = 1U;
      while (!queue.empty()) {
        Point const cell = queue.back();
        queue.pop_back();
        constexpr int k_dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (auto const& dir : k_dirs) {
          Point const next{cell.x + dir[0], cell.y + dir[1]};
          if (!on_walkable_hill(next, grid)) {
            continue;
          }
          auto& mark = seen[static_cast<std::size_t>(next.y) * grid + next.x];
          if (mark != 0U) {
            continue;
          }
          mark = 1U;
          cells.push_back(next);
          queue.push_back(next);
        }
      }
      found.push_back(std::move(cells));
    }
  }
  std::sort(found.begin(), found.end(), [](auto const& a, auto const& b) {
    return a.size() > b.size();
  });
  return found;
}

[[nodiscard]] inline auto
crown_of(const std::vector<Game::Systems::Point>& plateau,
         const Game::Map::TerrainHeightMap& heights,
         float fraction_of_top = 0.85F) -> std::vector<Game::Systems::Point> {
  float top = 0.0F;
  for (auto const& cell : plateau) {
    top = std::max(top, heights.get_height_at_grid(cell.x, cell.y));
  }
  std::vector<Game::Systems::Point> crown;
  for (auto const& cell : plateau) {
    if (heights.get_height_at_grid(cell.x, cell.y) >= top * fraction_of_top) {
      crown.push_back(cell);
    }
  }
  return crown;
}

} // namespace TestSupport
