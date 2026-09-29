#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "game/core/nav_profile.h"
#include "gate_service.h"
#include "map/forest_outline.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "pathfinding.h"
#include "systems/building_collision_registry.h"

namespace Game::Systems {

auto Pathfinding::label_at(const RegionMap& map,
                           const Point& cell) const -> std::uint32_t {
  if (cell.x < 0 || cell.x >= m_width || cell.y < 0 || cell.y >= m_height) {
    return k_unreachable_region;
  }
  auto const index = static_cast<std::size_t>(to_index(cell));
  return index < map.labels.size() ? map.labels[index] : k_unreachable_region;
}

void Pathfinding::rebuild_region_map(RegionMap& map, Passability passability) const {
  auto const cell_count = static_cast<std::size_t>(std::max(m_width, 0)) *
                          static_cast<std::size_t>(std::max(m_height, 0));
  map.labels.assign(cell_count, k_unreachable_region);
  map.sizes_valid = false;
  if (cell_count == 0U) {
    return;
  }

  auto const gates = closed_gate_cells(0, m_width - 1, 0, m_height - 1);
  auto const connects = [&](int x, int y) {
    return region_connects(x, y, passability, gates);
  };

  std::vector<int> frontier;
  std::uint32_t next_label = k_unreachable_region;
  for (int seed_y = 0; seed_y < m_height; ++seed_y) {
    for (int seed_x = 0; seed_x < m_width; ++seed_x) {
      int const seed_index = to_index(seed_x, seed_y);
      if (map.labels[static_cast<std::size_t>(seed_index)] != k_unreachable_region ||
          !connects(seed_x, seed_y)) {
        continue;
      }

      ++next_label;
      map.labels[static_cast<std::size_t>(seed_index)] = next_label;
      frontier.clear();
      frontier.push_back(seed_index);

      while (!frontier.empty()) {
        Point const current = to_point(frontier.back());
        frontier.pop_back();

        std::array<Point, 8> neighbors{};
        std::size_t const neighbor_count =
            collect_neighbors(current, neighbors, passability);
        for (std::size_t i = 0; i < neighbor_count; ++i) {
          Point const& neighbor = neighbors[i];
          if (!connects(neighbor.x, neighbor.y)) {
            continue;
          }
          auto const neighbor_index = static_cast<std::size_t>(to_index(neighbor));
          if (map.labels[neighbor_index] != k_unreachable_region) {
            continue;
          }
          map.labels[neighbor_index] = next_label;
          frontier.push_back(static_cast<int>(neighbor_index));
        }
      }
    }
  }
}

auto Pathfinding::region_connects(int x,
                                  int y,
                                  Passability passability,
                                  const std::vector<int>& gate_cells) const -> bool {
  return is_walkable(x, y, passability) ||
         std::binary_search(gate_cells.begin(), gate_cells.end(), to_index(x, y));
}

auto Pathfinding::region_map_survives(const RegionMap& map,
                                      Passability passability,
                                      std::uint64_t revision) -> bool {
  std::vector<NavChange> changes;
  if (!navigation_changes_since(map.revision, revision, changes)) {
    return false;
  }
  for (NavChange change : changes) {
    auto const gates =
        closed_gate_cells(change.min_x, change.max_x, change.min_z, change.max_z);
    if (!clamp_to_grid(change.min_x, change.max_x, change.min_z, change.max_z)) {
      continue;
    }
    for (int z = change.min_z; z <= change.max_z; ++z) {
      for (int x = change.min_x; x <= change.max_x; ++x) {
        if ((label_at(map, {x, z}) != k_unreachable_region) !=
            region_connects(x, z, passability, gates)) {
          return false;
        }
      }
    }
  }
  return true;
}

auto Pathfinding::current_region_map(Passability passability) -> const RegionMap& {
  std::uint64_t const revision = navigation_revision();
  auto& map = m_region_maps[static_cast<std::size_t>(passability)];
  if (map.revision != revision && !region_map_survives(map, passability, revision)) {
    Engine::Core::count_nav(Engine::Core::NavCounter::RegionMapRebuilds);
    rebuild_region_map(map, passability);
  }
  map.revision = revision;
  return map;
}

void Pathfinding::region_labels(const Point& first,
                                const Point& second,
                                Passability passability,
                                std::uint32_t& first_label,
                                std::uint32_t& second_label) {
  if (m_navigation_grid_dirty.load(std::memory_order_acquire)) {
    update_navigation_grid();
  }

  std::shared_lock<std::shared_mutex> const navigation_lock(m_navigation_mutex);
  std::lock_guard<std::mutex> const region_lock(m_region_mutex);
  auto const& map = current_region_map(passability);
  first_label = label_at(map, first);
  second_label = label_at(map, second);
}

auto Pathfinding::region_of(const Point& cell,
                            Passability passability) -> std::uint32_t {
  std::uint32_t label = k_unreachable_region;
  std::uint32_t ignored = k_unreachable_region;
  region_labels(cell, cell, passability, label, ignored);
  return label;
}

auto Pathfinding::find_escape_point(const Point& point,
                                    const Point& target,
                                    Passability passability) -> std::optional<Point> {
  constexpr int k_pocket_search_cells = 8;
  constexpr int k_rescue_search_cells = 128;

  if (m_navigation_grid_dirty.load(std::memory_order_acquire)) {
    update_navigation_grid();
  }

  std::shared_lock<std::shared_mutex> const navigation_lock(m_navigation_mutex);
  std::lock_guard<std::mutex> const region_lock(m_region_mutex);
  auto& map = m_region_maps[static_cast<std::size_t>(passability)];
  static_cast<void>(current_region_map(passability));

  Point const seed =
      find_nearest_walkable_point(point, k_pocket_search_cells, *this, passability);
  Point const goal =
      find_nearest_walkable_point(target, k_pocket_search_cells, *this, passability);
  std::uint32_t const own_label = is_walkable(seed.x, seed.y, passability)
                                      ? label_at(map, seed)
                                      : k_unreachable_region;
  std::uint32_t const goal_label = is_walkable(goal.x, goal.y, passability)
                                       ? label_at(map, goal)
                                       : k_unreachable_region;
  if (own_label != k_unreachable_region && own_label == goal_label) {
    return std::nullopt;
  }

  ensure_region_sizes(map);
  std::size_t const own_size = region_size(map, own_label);
  std::uint32_t destination = k_unreachable_region;
  if (region_size(map, goal_label) > own_size) {
    destination = goal_label;
  } else if (region_size(map, map.main_label) > own_size) {
    destination = map.main_label;
  }
  if (destination == k_unreachable_region) {
    return std::nullopt;
  }

  auto const terrain_line_clear = [this, &point](const Point& cell) {
    int const steps =
        std::max(std::abs(cell.x - point.x), std::abs(cell.y - point.y)) * 2;
    for (int i = 1; i <= steps; ++i) {
      float const t = static_cast<float>(i) / static_cast<float>(steps);
      int const x = static_cast<int>(
          std::lround(static_cast<float>(point.x) + (cell.x - point.x) * t));
      int const y = static_cast<int>(
          std::lround(static_cast<float>(point.y) + (cell.y - point.y) * t));
      if (!is_terrain_walkable(x, y) && is_terrain_walkable(point.x, point.y)) {
        return false;
      }
    }
    return true;
  };
  for (int radius = 1; radius <= k_rescue_search_cells; ++radius) {
    std::optional<Point> best;
    int best_distance = std::numeric_limits<int>::max();
    for_each_ring_cell(radius, [&](int dx, int dy) {
      Point const cell{point.x + dx, point.y + dy};
      int const distance = (dx * dx) + (dy * dy);
      if (distance < best_distance && label_at(map, cell) == destination &&
          is_world_position_walkable(grid_to_world(cell), passability, 0.0F) &&
          terrain_line_clear(cell)) {
        best = cell;
        best_distance = distance;
      }
    });
    if (best.has_value()) {
      return best;
    }
  }
  return std::nullopt;
}

void Pathfinding::ensure_region_sizes(RegionMap& map) {
  if (map.sizes_valid) {
    return;
  }
  map.sizes.clear();
  for (std::uint32_t const label : map.labels) {
    if (label == k_unreachable_region) {
      continue;
    }
    if (label >= map.sizes.size()) {
      map.sizes.resize(static_cast<std::size_t>(label) + 1U, 0U);
    }
    ++map.sizes[label];
  }
  map.main_label = k_unreachable_region;
  std::size_t largest = 0U;
  for (std::size_t label = 1U; label < map.sizes.size(); ++label) {
    if (map.sizes[label] > largest) {
      largest = map.sizes[label];
      map.main_label = static_cast<std::uint32_t>(label);
    }
  }
  map.sizes_valid = true;
}

auto Pathfinding::region_size(const RegionMap& map,
                              std::uint32_t label) -> std::size_t {
  return label != k_unreachable_region && label < map.sizes.size() ? map.sizes[label]
                                                                   : 0U;
}

auto Pathfinding::find_nearest_connected_point(const Point& point,
                                               const Point& target,
                                               int max_search_radius,
                                               Passability passability)
    -> std::optional<Point> {
  if (m_navigation_grid_dirty.load(std::memory_order_acquire)) {
    update_navigation_grid();
  }

  std::shared_lock<std::shared_mutex> const navigation_lock(m_navigation_mutex);
  std::lock_guard<std::mutex> const region_lock(m_region_mutex);
  auto const& map = current_region_map(passability);
  std::uint32_t const target_label = label_at(map, target);
  if (target_label == k_unreachable_region) {
    return std::nullopt;
  }
  return nearest_cell_in_region(
      map, target_label, point, point, max_search_radius, passability, 0.0F);
}

auto Pathfinding::nearest_cell_in_region(const RegionMap& map,
                                         std::uint32_t label,
                                         const Point& point,
                                         const Point& tie_break,
                                         int max_search_radius,
                                         Passability passability,
                                         float clearance_radius) const
    -> std::optional<Point> {
  for (int radius = 0; radius <= max_search_radius; ++radius) {
    std::optional<Point> best;
    std::pair<int, int> best_distance{std::numeric_limits<int>::max(), 0};
    for_each_ring_cell(radius, [&](int dx, int dy) {
      Point const cell{point.x + dx, point.y + dy};
      int const tie_x = cell.x - tie_break.x;
      int const tie_y = cell.y - tie_break.y;
      std::pair<int, int> const distance{(dx * dx) + (dy * dy),
                                         (tie_x * tie_x) + (tie_y * tie_y)};
      if (distance < best_distance && label_at(map, cell) == label &&
          is_world_position_walkable(
              grid_to_world(cell), passability, clearance_radius)) {
        best = cell;
        best_distance = distance;
      }
    });
    if (best.has_value()) {
      return best;
    }
  }
  return std::nullopt;
}

auto Pathfinding::can_reach(const Point& start,
                            const Point& end,
                            Passability passability) -> bool {
  std::uint32_t start_label = k_unreachable_region;
  std::uint32_t end_label = k_unreachable_region;
  region_labels(start, end, passability, start_label, end_label);
  return start_label != k_unreachable_region && start_label == end_label;
}

auto Pathfinding::find_nearest_walkable_point(const Point& point,
                                              int max_search_radius,
                                              const Pathfinding& pathfinder,
                                              Passability passability) -> Point {
  auto const is_walkableFunc = [&pathfinder, passability](int x, int y) -> bool {
    return pathfinder.is_walkable(x, y, passability);
  };

  if (is_walkableFunc(point.x, point.y)) {
    return point;
  }

  for (int radius = 1; radius <= max_search_radius; ++radius) {
    for (int dy = -radius; dy <= radius; ++dy) {
      for (int dx = -radius; dx <= radius; ++dx) {
        if (std::abs(dx) != radius && std::abs(dy) != radius) {
          continue;
        }

        int const check_x = point.x + dx;
        int const check_y = point.y + dy;

        if (is_walkableFunc(check_x, check_y)) {
          return {check_x, check_y};
        }
      }
    }
  }

  return point;
}

} // namespace Game::Systems
