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

auto Pathfinding::find_path(const Point& start,
                            const Point& end,
                            Passability passability,
                            float clearance_radius) -> std::vector<Point> {
  Engine::Core::NavScope const scope(Engine::Core::NavCounter::IndividualRoutes);

  if (m_navigation_grid_dirty.load(std::memory_order_acquire)) {
    update_navigation_grid();
  }

  std::uint64_t const revision = navigation_revision();
  int const clearance_quarters =
      static_cast<int>(std::ceil(std::max(0.0F, clearance_radius) * 4.0F));
  PathCacheKey const key{
      start.x, start.y, end.x, end.y, passability, clearance_quarters};
  {
    std::lock_guard<std::mutex> const cache_lock(m_path_cache_mutex);
    if (m_path_cache_revision != revision) {
      drop_paths_crossing_changes(m_path_cache_revision, revision);
      m_path_cache_revision = revision;
    }
    if (auto const cached = m_path_cache.find(key); cached != m_path_cache.end()) {
      cached->second.last_used = ++m_path_cache_clock;
      Engine::Core::count_nav(Engine::Core::NavCounter::RouteCacheHits);
      return cached->second.path;
    }
  }
  Engine::Core::count_nav(Engine::Core::NavCounter::RouteCacheMisses);

  std::shared_lock<std::shared_mutex> const navigation_lock(m_navigation_mutex);
  auto path = find_path_internal(
      start, end, passability, static_cast<float>(clearance_quarters) * 0.25F);

  std::lock_guard<std::mutex> const cache_lock(m_path_cache_mutex);
  if (m_path_cache_revision != revision || navigation_revision() != revision) {
    return path;
  }
  evict_cold_paths();
  CachedPath entry;
  entry.path = path;
  entry.last_used = ++m_path_cache_clock;
  entry.min_x = std::min(start.x, end.x);
  entry.max_x = std::max(start.x, end.x);
  entry.min_z = std::min(start.y, end.y);
  entry.max_z = std::max(start.y, end.y);
  for (const Point& point : entry.path) {
    entry.min_x = std::min(entry.min_x, point.x);
    entry.max_x = std::max(entry.max_x, point.x);
    entry.min_z = std::min(entry.min_z, point.y);
    entry.max_z = std::max(entry.max_z, point.y);
  }
  m_path_cache.emplace(key, std::move(entry));
  return path;
}

auto Pathfinding::PathCacheKeyHash::operator()(const PathCacheKey& key) const noexcept
    -> std::size_t {
  std::size_t result = std::hash<int>{}(key.start_x);
  auto combine = [&result](int value) {
    result ^= std::hash<int>{}(value) + 0x9e3779b9U + (result << 6U) + (result >> 2U);
  };
  combine(key.start_y);
  combine(key.end_x);
  combine(key.end_y);
  combine(static_cast<int>(key.passability));
  combine(key.clearance_quarters);
  return result;
}

auto Pathfinding::make_search_request(const Point& start,
                                      const Point& goal,
                                      Passability passability,
                                      float clearance_radius) const -> SearchRequest {
  SearchRequest request;
  request.start = start;
  request.goal = goal;
  request.passability = passability;
  request.one_man = routing_clearance(clearance_radius);
  request.centre_clear_of_neighbours =
      request.one_man < 1.0F - (m_grid_cell_size * 0.5F) - 1.0e-4F;
  return request;
}

auto Pathfinding::search_cell_walkable(const SearchRequest& request,
                                       int x,
                                       int y) const -> bool {
  if (request.centre_clear_of_neighbours) {
    return is_walkable(x, y, request.passability);
  }
  return is_world_position_walkable(
      grid_to_world({x, y}), request.passability, request.one_man);
}

auto Pathfinding::search_clearance_cost(int x, int y) const -> int {
  int const reach = clearance_penalty(x, y);

  return reach == 1 ? k_edge_step_penalty : 0;
}

auto Pathfinding::goal_in_start_region(const Point& start,
                                       const Point& end,
                                       Passability passability,
                                       float one_man) -> Point {
  std::lock_guard<std::mutex> const region_lock(m_region_mutex);
  auto const& map = current_region_map(passability);
  std::uint32_t const start_region = label_at(map, start);
  if (label_at(map, end) == start_region) {
    return end;
  }
  int const reach = std::max(std::abs(end.x - start.x), std::abs(end.y - start.y));
  return nearest_cell_in_region(
             map, start_region, end, start, reach, passability, one_man)
      .value_or(start);
}

auto Pathfinding::run_search(SearchBuffers& buffers,
                             const SearchRequest& request,
                             std::uint32_t generation) const -> SearchOutcome {
  Point const& start = request.start;
  Point const& goal = request.goal;
  int const start_idx = to_index(start);
  int const end_idx = to_index(goal);

  buffers.open_heap.clear();

  set_g_cost(buffers, start_idx, generation, 0);
  set_parent(buffers, start_idx, generation, start_idx);

  push_open_node(buffers, {start_idx, calculate_heuristic(start, goal), 0});

  SearchOutcome outcome;
  outcome.best_idx = start_idx;
  outcome.best_h = calculate_heuristic(start, goal);
  outcome.best_g = 0;

  while (!buffers.open_heap.empty()) {
    QueueNode const current = pop_open_node(buffers);

    if (current.g_cost > get_g_cost(buffers, current.index, generation)) {
      continue;
    }

    if (is_closed(buffers, current.index, generation)) {
      continue;
    }

    Engine::Core::count_nav(Engine::Core::NavCounter::CellsExpanded);
    set_closed(buffers, current.index, generation);

    Point const current_point = to_point(current.index);
    int const current_h = calculate_heuristic(current_point, goal);
    if (current_h < outcome.best_h ||
        (current_h == outcome.best_h && current.g_cost < outcome.best_g)) {
      outcome.best_idx = current.index;
      outcome.best_h = current_h;
      outcome.best_g = current.g_cost;
    }

    if (current.index == end_idx) {
      outcome.final_cost = current.g_cost;
      break;
    }

    relax_neighbors(buffers, request, current, current_point, start_idx, generation);
  }
  return outcome;
}

void Pathfinding::relax_neighbors(SearchBuffers& buffers,
                                  const SearchRequest& request,
                                  const QueueNode& current,
                                  const Point& current_point,
                                  int start_idx,
                                  std::uint32_t generation) const {
  int arrival_x = 0;
  int arrival_z = 0;
  if (current.index != start_idx) {
    Point const came_from = to_point(get_parent(buffers, current.index, generation));
    arrival_x = current_point.x - came_from.x;
    arrival_z = current_point.y - came_from.y;
  }

  std::array<Point, 8> neighbors{};
  const std::size_t neighbor_count =
      collect_neighbors(current_point, neighbors, request.passability);

  for (std::size_t i = 0; i < neighbor_count; ++i) {
    const Point& neighbor = neighbors[i];
    if (!search_cell_walkable(request, neighbor.x, neighbor.y)) {
      continue;
    }

    const int neighbor_idx = to_index(neighbor);
    if (is_closed(buffers, neighbor_idx, generation)) {
      continue;
    }

    const int step_x = neighbor.x - current_point.x;
    const int step_z = neighbor.y - current_point.y;
    if (step_x != 0 && step_z != 0 &&
        (!search_cell_walkable(request, current_point.x + step_x, current_point.y) ||
         !search_cell_walkable(request, current_point.x, current_point.y + step_z))) {
      continue;
    }
    const bool turns = (arrival_x != 0 || arrival_z != 0) &&
                       (step_x != arrival_x || step_z != arrival_z);
    const int tentative_gcost =
        current.g_cost +
        ((step_x != 0 && step_z != 0) ? k_diagonal_step_cost : k_straight_step_cost) +
        search_clearance_cost(neighbor.x, neighbor.y) +
        climb_penalty(current.index, neighbor_idx) + wade_penalty(neighbor_idx) +
        (turns ? k_turn_penalty : 0);
    if (tentative_gcost >= get_g_cost(buffers, neighbor_idx, generation)) {
      continue;
    }

    set_g_cost(buffers, neighbor_idx, generation, tentative_gcost);
    set_parent(buffers, neighbor_idx, generation, current.index);

    const int h_cost = calculate_heuristic(neighbor, request.goal);
    push_open_node(buffers, {neighbor_idx, tentative_gcost + h_cost, tentative_gcost});
  }
}

auto Pathfinding::path_from_outcome(const SearchBuffers& buffers,
                                    const SearchRequest& request,
                                    const SearchOutcome& outcome,
                                    std::uint32_t generation) const
    -> std::vector<Point> {
  int const start_idx = to_index(request.start);
  std::vector<Point> path;
  if (outcome.final_cost < 0) {
    if (outcome.best_idx == start_idx) {
      return {request.start};
    }
    int const partial_cells = (outcome.best_g / k_straight_step_cost) + 1;
    path.reserve(static_cast<std::size_t>(partial_cells));
    build_path(start_idx, outcome.best_idx, generation, partial_cells, buffers, path);
    return path;
  }
  int const path_cells = (outcome.final_cost / k_straight_step_cost) + 1;
  path.reserve(static_cast<std::size_t>(path_cells));
  build_path(start_idx, to_index(request.goal), generation, path_cells, buffers, path);
  return path;
}

auto Pathfinding::find_path_internal(const Point& start,
                                     const Point& end,
                                     Passability passability,
                                     float clearance_radius) -> std::vector<Point> {
  SearchBuffers& buffers = search_buffers_for(this);
  ensure_working_buffers(buffers);

  SearchRequest request =
      make_search_request(start, end, passability, clearance_radius);

  if (!search_cell_walkable(request, start.x, start.y) ||
      !search_cell_walkable(request, end.x, end.y)) {
    Point resolved_start = start;
    Point resolved_end = end;

    if ((!search_cell_walkable(request, start.x, start.y) &&
         !resolve_walkable_endpoint(
             start, resolved_start, passability, request.one_man)) ||
        (!search_cell_walkable(request, end.x, end.y) &&
         !resolve_walkable_endpoint(end, resolved_end, passability, request.one_man))) {
      return {};
    }

    if (resolved_start == start && resolved_end == end) {
      return {};
    }

    return find_path_internal(
        resolved_start, resolved_end, passability, clearance_radius);
  }

  request.goal = goal_in_start_region(start, end, passability, request.one_man);

  if (to_index(start) == to_index(request.goal)) {
    return {start};
  }

  std::uint32_t const generation = next_generation(buffers);
  SearchOutcome const outcome = run_search(buffers, request, generation);
  return path_from_outcome(buffers, request, outcome, generation);
}

auto Pathfinding::resolve_walkable_endpoint(const Point& requested,
                                            Point& resolved,
                                            Passability passability,
                                            float clearance_radius) const -> bool {
  auto const is_walkable_func = [this, passability, clearance_radius](int x,
                                                                      int y) -> bool {
    return is_world_position_walkable(
        grid_to_world({x, y}), passability, clearance_radius);
  };

  if (is_walkable_func(requested.x, requested.y)) {
    resolved = requested;
    return true;
  }

  Point const clamped_origin{
      std::clamp(requested.x, 0, std::max(m_width - 1, 0)),
      std::clamp(requested.y, 0, std::max(m_height - 1, 0)),
  };

  if (is_walkable_func(clamped_origin.x, clamped_origin.y)) {
    resolved = clamped_origin;
    return true;
  }

  int const max_search_radius =
      std::max({clamped_origin.x,
                std::max(0, m_width - 1 - clamped_origin.x),
                clamped_origin.y,
                std::max(0, m_height - 1 - clamped_origin.y)});

  for (int radius = 1; radius <= max_search_radius; ++radius) {
    bool found_candidate = false;
    int best_distance_sq = std::numeric_limits<int>::max();
    Point best_candidate{};

    for_each_ring_cell(radius, [&](int dx, int dy) {
      int const check_x = clamped_origin.x + dx;
      int const check_y = clamped_origin.y + dy;
      if (!is_walkable_func(check_x, check_y)) {
        return;
      }

      int const requested_dx = check_x - requested.x;
      int const requested_dy = check_y - requested.y;
      int const distance_sq = requested_dx * requested_dx + requested_dy * requested_dy;
      if (distance_sq < best_distance_sq) {
        best_distance_sq = distance_sq;
        best_candidate = {check_x, check_y};
        found_candidate = true;
      }
    });

    if (found_candidate) {
      resolved = best_candidate;
      return true;
    }
  }

  return false;
}

auto Pathfinding::calculate_heuristic(const Point& a, const Point& b) -> int {

  int const dx = std::abs(a.x - b.x);
  int const dy = std::abs(a.y - b.y);
  int const diagonal = std::min(dx, dy);
  int const straight = std::max(dx, dy) - diagonal;
  int const octile =
      (diagonal * k_diagonal_step_cost) + (straight * k_straight_step_cost);
  return octile * k_heuristic_weight_numerator / k_heuristic_weight_denominator;
}

auto Pathfinding::clamp_to_grid(int& min_x,
                                int& max_x,
                                int& min_z,
                                int& max_z) const -> bool {
  min_x = std::max(0, min_x);
  max_x = std::min(m_width - 1, max_x);
  min_z = std::max(0, min_z);
  max_z = std::min(m_height - 1, max_z);
  return min_x <= max_x && min_z <= max_z;
}

auto Pathfinding::search_buffers_by_grid()
    -> std::unordered_map<const Pathfinding*, SearchBuffers>& {
  thread_local std::unordered_map<const Pathfinding*, SearchBuffers> buffers_by_grid;
  return buffers_by_grid;
}

auto Pathfinding::search_buffers_for(const Pathfinding* pathfinding) -> SearchBuffers& {
  return search_buffers_by_grid()[pathfinding];
}

void Pathfinding::release_search_buffers(const Pathfinding* pathfinding) {
  search_buffers_by_grid().erase(pathfinding);
}

void Pathfinding::ensure_working_buffers(SearchBuffers& buffers) const {
  const std::size_t total_cells =
      static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);

  if (buffers.closed_generation.size() != total_cells) {
    buffers.closed_generation.assign(total_cells, 0);
    buffers.g_cost_generation.assign(total_cells, 0);
    buffers.g_cost_values.assign(total_cells, std::numeric_limits<int>::max());
    buffers.parent_generation.assign(total_cells, 0);
    buffers.parent_values.assign(total_cells, -1);
    buffers.generation_counter = 0;
  }

  const std::size_t min_open_capacity = std::max<std::size_t>(total_cells / 8, 64);
  if (buffers.open_heap.capacity() < min_open_capacity) {
    buffers.open_heap.reserve(min_open_capacity);
  }
}

auto Pathfinding::next_generation(SearchBuffers& buffers) -> std::uint32_t {
  auto next = ++buffers.generation_counter;
  if (next == 0) {
    reset_generations(buffers);
    next = ++buffers.generation_counter;
  }
  return next;
}

void Pathfinding::reset_generations(SearchBuffers& buffers) {
  std::fill(buffers.closed_generation.begin(), buffers.closed_generation.end(), 0);
  std::fill(buffers.g_cost_generation.begin(), buffers.g_cost_generation.end(), 0);
  std::fill(buffers.parent_generation.begin(), buffers.parent_generation.end(), 0);
  std::fill(buffers.g_cost_values.begin(),
            buffers.g_cost_values.end(),
            std::numeric_limits<int>::max());
  std::fill(buffers.parent_values.begin(), buffers.parent_values.end(), -1);
  buffers.generation_counter = 0;
}

auto Pathfinding::is_closed(const SearchBuffers& buffers,
                            int index,
                            std::uint32_t generation) -> bool {
  return index >= 0 &&
         static_cast<std::size_t>(index) < buffers.closed_generation.size() &&
         buffers.closed_generation[static_cast<std::size_t>(index)] == generation;
}

void Pathfinding::set_closed(SearchBuffers& buffers,
                             int index,
                             std::uint32_t generation) {
  if (index >= 0 &&
      static_cast<std::size_t>(index) < buffers.closed_generation.size()) {
    buffers.closed_generation[static_cast<std::size_t>(index)] = generation;
  }
}

auto Pathfinding::get_g_cost(const SearchBuffers& buffers,
                             int index,
                             std::uint32_t generation) -> int {
  if (index < 0 ||
      static_cast<std::size_t>(index) >= buffers.g_cost_generation.size()) {
    return std::numeric_limits<int>::max();
  }
  if (buffers.g_cost_generation[static_cast<std::size_t>(index)] == generation) {
    return buffers.g_cost_values[static_cast<std::size_t>(index)];
  }
  return std::numeric_limits<int>::max();
}

void Pathfinding::set_g_cost(SearchBuffers& buffers,
                             int index,
                             std::uint32_t generation,
                             int cost) {
  if (index >= 0 &&
      static_cast<std::size_t>(index) < buffers.g_cost_generation.size()) {
    const auto idx = static_cast<std::size_t>(index);
    buffers.g_cost_generation[idx] = generation;
    buffers.g_cost_values[idx] = cost;
  }
}

auto Pathfinding::has_parent(const SearchBuffers& buffers,
                             int index,
                             std::uint32_t generation) -> bool {
  return index >= 0 &&
         static_cast<std::size_t>(index) < buffers.parent_generation.size() &&
         buffers.parent_generation[static_cast<std::size_t>(index)] == generation;
}

auto Pathfinding::get_parent(const SearchBuffers& buffers,
                             int index,
                             std::uint32_t generation) -> int {
  if (has_parent(buffers, index, generation)) {
    return buffers.parent_values[static_cast<std::size_t>(index)];
  }
  return -1;
}

void Pathfinding::set_parent(SearchBuffers& buffers,
                             int index,
                             std::uint32_t generation,
                             int parent_index) {
  if (index >= 0 &&
      static_cast<std::size_t>(index) < buffers.parent_generation.size()) {
    const auto idx = static_cast<std::size_t>(index);
    buffers.parent_generation[idx] = generation;
    buffers.parent_values[idx] = parent_index;
  }
}

auto Pathfinding::collect_neighbors(const Point& point,
                                    std::array<Point, 8>& buffer,
                                    Passability passability) const -> std::size_t {
  std::size_t count = 0;
  for (int dx = -1; dx <= 1; ++dx) {
    for (int dy = -1; dy <= 1; ++dy) {
      if (dx == 0 && dy == 0) {
        continue;
      }

      const int x = point.x + dx;
      const int y = point.y + dy;

      if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        continue;
      }

      if (dx != 0 && dy != 0) {
        if (!is_walkable(point.x + dx, point.y, passability) ||
            !is_walkable(point.x, point.y + dy, passability)) {
          continue;
        }
      }

      buffer[count++] = Point{x, y};
    }
  }
  return count;
}

void Pathfinding::build_path(int start_index,
                             int end_index,
                             std::uint32_t generation,
                             int expected_length,
                             const SearchBuffers& buffers,
                             std::vector<Point>& out_path) const {
  out_path.clear();
  if (expected_length > 0) {
    out_path.reserve(static_cast<std::size_t>(expected_length));
  }
  int current = end_index;

  while (current >= 0) {
    out_path.push_back(to_point(current));
    if (current == start_index) {
      std::reverse(out_path.begin(), out_path.end());
      return;
    }

    if (!has_parent(buffers, current, generation)) {
      out_path.clear();
      return;
    }

    const int parent = get_parent(buffers, current, generation);
    if (parent == current || parent < 0) {
      out_path.clear();
      return;
    }
    current = parent;
  }

  out_path.clear();
}

auto Pathfinding::heap_less(const QueueNode& lhs, const QueueNode& rhs) -> bool {
  if (lhs.f_cost != rhs.f_cost) {
    return lhs.f_cost < rhs.f_cost;
  }
  return lhs.g_cost < rhs.g_cost;
}

void Pathfinding::push_open_node(SearchBuffers& buffers, const QueueNode& node) {
  Engine::Core::count_nav(Engine::Core::NavCounter::HeapOperations);
  auto& heap = buffers.open_heap;
  heap.push_back(node);
  std::size_t index = heap.size() - 1;
  while (index > 0) {
    std::size_t const parent = (index - 1) / 2;
    if (heap_less(heap[parent], heap[index])) {
      break;
    }
    std::swap(heap[parent], heap[index]);
    index = parent;
  }
}

auto Pathfinding::pop_open_node(SearchBuffers& buffers) -> Pathfinding::QueueNode {
  Engine::Core::count_nav(Engine::Core::NavCounter::HeapOperations);
  auto& heap = buffers.open_heap;
  QueueNode top = heap.front();
  QueueNode const last = heap.back();
  heap.pop_back();
  if (!heap.empty()) {
    heap[0] = last;
    std::size_t index = 0;
    const std::size_t size = heap.size();
    while (true) {
      std::size_t const left = index * 2 + 1;
      std::size_t const right = left + 1;
      std::size_t smallest = index;

      if (left < size && !heap_less(heap[smallest], heap[left])) {
        smallest = left;
      }
      if (right < size && !heap_less(heap[smallest], heap[right])) {
        smallest = right;
      }
      if (smallest == index) {
        break;
      }
      std::swap(heap[index], heap[smallest]);
      index = smallest;
    }
  }
  return top;
}

} // namespace Game::Systems
