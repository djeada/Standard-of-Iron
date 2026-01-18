#include "pathfinding.h"
#include "../map/terrain_service.h"
#include "building_collision_registry.h"
#include "map/terrain.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <future>
#include <limits>
#include <mutex>
#include <utility>
#include <vector>

namespace Game::Systems {

Pathfinding::Pathfinding(int width, int height)
    : m_width(width), m_height(height) {
  m_obstacles.resize(height, std::vector<std::uint8_t>(width, 0));
  ensure_working_buffers();
  m_obstacles_dirty.store(true, std::memory_order_release);
  m_full_update_required = true;
  m_worker_thread = std::thread(&Pathfinding::worker_loop, this);
}

Pathfinding::~Pathfinding() {
  m_stop_worker.store(true, std::memory_order_release);
  m_request_condition.notify_all();
  if (m_worker_thread.joinable()) {
    m_worker_thread.join();
  }
}

void Pathfinding::set_grid_offset(float offset_x, float offset_z) {
  m_grid_offset_x = offset_x;
  m_grid_offset_z = offset_z;
}

void Pathfinding::set_obstacle(int x, int y, bool is_obstacle) {
  if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
    m_obstacles[y][x] = static_cast<std::uint8_t>(is_obstacle);
  }
}

auto Pathfinding::is_walkable(int x, int y) const -> bool {
  if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
    return false;
  }
  return m_obstacles[y][x] == 0;
}

auto Pathfinding::is_walkable_with_radius(int x, int y,
                                          float unit_radius) const -> bool {
  (void)unit_radius;
  return is_walkable(x, y);
}

void Pathfinding::mark_obstacles_dirty() {
  std::lock_guard<std::mutex> const lock(m_dirty_mutex);
  m_full_update_required = true;
  m_obstacles_dirty.store(true, std::memory_order_release);
}

void Pathfinding::mark_region_dirty(int min_x, int max_x, int min_z,
                                    int max_z) {

  min_x = std::max(0, min_x);
  max_x = std::min(m_width - 1, max_x);
  min_z = std::max(0, min_z);
  max_z = std::min(m_height - 1, max_z);

  if (min_x > max_x || min_z > max_z) {
    return;
  }

  std::lock_guard<std::mutex> const lock(m_dirty_mutex);
  m_dirty_regions.emplace_back(min_x, max_x, min_z, max_z);
  m_obstacles_dirty.store(true, std::memory_order_release);
}

void Pathfinding::mark_building_region_dirty(float center_x, float center_z,
                                             float width, float depth) {
  float const padding = BuildingCollisionRegistry::get_grid_padding();
  float const half_width = width / 2.0F + padding;
  float const half_depth = depth / 2.0F + padding;

  int const min_x =
      static_cast<int>(std::floor(center_x - half_width - m_grid_offset_x));
  int const max_x =
      static_cast<int>(std::ceil(center_x + half_width - m_grid_offset_x));
  int const min_z =
      static_cast<int>(std::floor(center_z - half_depth - m_grid_offset_z));
  int const max_z =
      static_cast<int>(std::ceil(center_z + half_depth - m_grid_offset_z));

  mark_region_dirty(min_x, max_x, min_z, max_z);
}

void Pathfinding::process_dirty_regions() {
  std::vector<DirtyRegion> regions_to_process;

  {
    std::lock_guard<std::mutex> const lock(m_dirty_mutex);
    if (m_full_update_required) {

      m_dirty_regions.clear();
      m_full_update_required = false;

      for (auto &row : m_obstacles) {
        std::fill(row.begin(), row.end(), static_cast<std::uint8_t>(0));
      }

      auto &terrain_service = Game::Map::TerrainService::instance();
      if (terrain_service.is_initialized()) {
        const Game::Map::TerrainHeightMap *height_map =
            terrain_service.get_height_map();
        const int terrain_width =
            (height_map != nullptr) ? height_map->getWidth() : 0;
        const int terrain_height =
            (height_map != nullptr) ? height_map->getHeight() : 0;

        for (int z = 0; z < m_height; ++z) {
          for (int x = 0; x < m_width; ++x) {
            bool blocked = false;
            if (x < terrain_width && z < terrain_height) {
              blocked = !terrain_service.is_walkable(x, z);
            } else {
              blocked = true;
            }

            if (blocked) {
              m_obstacles[z][x] = static_cast<std::uint8_t>(1);
            }
          }
        }
      }

      auto &registry = BuildingCollisionRegistry::instance();
      const auto &buildings = registry.get_all_buildings();

      for (const auto &building : buildings) {
        auto cells =
            Game::Systems::BuildingCollisionRegistry::get_occupied_grid_cells(
                building, m_grid_cell_size);
        for (const auto &cell : cells) {
          int const grid_x =
              static_cast<int>(std::round(cell.first - m_grid_offset_x));
          int const grid_z =
              static_cast<int>(std::round(cell.second - m_grid_offset_z));

          if (grid_x >= 0 && grid_x < m_width && grid_z >= 0 &&
              grid_z < m_height) {
            m_obstacles[grid_z][grid_x] = static_cast<std::uint8_t>(1);
          }
        }
      }

      return;
    }

    regions_to_process = std::move(m_dirty_regions);
    m_dirty_regions.clear();
  }

  if (regions_to_process.empty()) {
    return;
  }

  for (const auto &region : regions_to_process) {
    update_region(region.min_x, region.max_x, region.min_z, region.max_z);
  }
}

void Pathfinding::update_region(int min_x, int max_x, int min_z, int max_z) {
  auto &terrain_service = Game::Map::TerrainService::instance();
  const Game::Map::TerrainHeightMap *height_map = nullptr;
  int terrain_width = 0;
  int terrain_height = 0;

  if (terrain_service.is_initialized()) {
    height_map = terrain_service.get_height_map();
    terrain_width = (height_map != nullptr) ? height_map->getWidth() : 0;
    terrain_height = (height_map != nullptr) ? height_map->getHeight() : 0;
  }

  for (int z = min_z; z <= max_z; ++z) {
    for (int x = min_x; x <= max_x; ++x) {
      bool blocked = false;
      if (x >= 0 && x < terrain_width && z >= 0 && z < terrain_height) {
        blocked = !terrain_service.is_walkable(x, z);
      } else if (terrain_service.is_initialized()) {

        blocked = true;
      }
      m_obstacles[z][x] = static_cast<std::uint8_t>(blocked ? 1 : 0);
    }
  }

  auto &registry = BuildingCollisionRegistry::instance();
  const auto &buildings = registry.get_all_buildings();

  for (const auto &building : buildings) {
    auto cells =
        Game::Systems::BuildingCollisionRegistry::get_occupied_grid_cells(
            building, m_grid_cell_size);
    for (const auto &cell : cells) {
      int const grid_x =
          static_cast<int>(std::round(cell.first - m_grid_offset_x));
      int const grid_z =
          static_cast<int>(std::round(cell.second - m_grid_offset_z));

      if (grid_x >= min_x && grid_x <= max_x && grid_z >= min_z &&
          grid_z <= max_z && grid_x >= 0 && grid_x < m_width && grid_z >= 0 &&
          grid_z < m_height) {
        m_obstacles[grid_z][grid_x] = static_cast<std::uint8_t>(1);
      }
    }
  }
}

void Pathfinding::update_building_obstacles() {

  if (!m_obstacles_dirty.load(std::memory_order_acquire)) {
    return;
  }

  std::lock_guard<std::mutex> const lock(m_mutex);

  if (!m_obstacles_dirty.load(std::memory_order_acquire)) {
    return;
  }

  process_dirty_regions();

  m_obstacles_dirty.store(false, std::memory_order_release);
}

auto Pathfinding::find_path(const Point &start,
                            const Point &end) -> std::vector<Point> {

  if (m_obstacles_dirty.load(std::memory_order_acquire)) {
    update_building_obstacles();
  }

  std::lock_guard<std::mutex> const lock(m_mutex);
  return find_path_internal(start, end);
}

auto Pathfinding::find_path(const Point &start, const Point &end,
                            float unit_radius) -> std::vector<Point> {

  if (m_obstacles_dirty.load(std::memory_order_acquire)) {
    update_building_obstacles();
  }

  std::lock_guard<std::mutex> const lock(m_mutex);
  return find_path_internal(start, end, unit_radius);
}

auto Pathfinding::find_path_async(const Point &start, const Point &end)
    -> std::future<std::vector<Point>> {
  return std::async(std::launch::async,
                    [this, start, end]() { return find_path(start, end); });
}

void Pathfinding::submit_path_request(std::uint64_t request_id,
                                      const Point &start, const Point &end) {
  {
    std::lock_guard<std::mutex> const lock(m_request_mutex);
    m_request_queue.push({request_id, start, end, 0.0F});
  }
  m_request_condition.notify_one();
}

void Pathfinding::submit_path_request(std::uint64_t request_id,
                                      const Point &start, const Point &end,
                                      float unit_radius) {
  {
    std::lock_guard<std::mutex> const lock(m_request_mutex);
    m_request_queue.push({request_id, start, end, unit_radius});
  }
  m_request_condition.notify_one();
}

auto Pathfinding::fetch_completed_paths()
    -> std::vector<Pathfinding::PathResult> {
  std::vector<PathResult> results;
  std::lock_guard<std::mutex> const lock(m_result_mutex);
  while (!m_result_queue.empty()) {
    results.push_back(std::move(m_result_queue.front()));
    m_result_queue.pop();
  }
  return results;
}

auto Pathfinding::find_path_internal(const Point &start,
                                     const Point &end) -> std::vector<Point> {
  return find_path_internal(start, end, 0.0F);
}

auto Pathfinding::find_path_internal(const Point &start, const Point &end,
                                     float unit_radius) -> std::vector<Point> {
  ensure_working_buffers();

  auto const is_walkableFunc = [this, unit_radius](int x, int y) -> bool {
    if (unit_radius <= 0.5F) {
      return is_walkable(x, y);
    }
    return is_walkable_with_radius(x, y, unit_radius);
  };

  if (!is_walkableFunc(start.x, start.y) || !is_walkableFunc(end.x, end.y)) {
    return {};
  }

  const int start_idx = to_index(start);
  const int end_idx = to_index(end);

  if (start_idx == end_idx) {
    return {start};
  }

  const std::uint32_t generation = next_generation();

  m_open_heap.clear();

  set_g_cost(start_idx, generation, 0);
  set_parent(start_idx, generation, start_idx);

  push_open_node({start_idx, calculate_heuristic(start, end), 0});

  const int max_iterations = std::max(m_width * m_height, 1);
  int iterations = 0;

  int final_cost = -1;

  while (!m_open_heap.empty() && iterations < max_iterations) {
    ++iterations;

    QueueNode const current = pop_open_node();

    if (current.g_cost > get_g_cost(current.index, generation)) {
      continue;
    }

    if (is_closed(current.index, generation)) {
      continue;
    }

    set_closed(current.index, generation);

    if (current.index == end_idx) {
      final_cost = current.g_cost;
      break;
    }

    const Point current_point = to_point(current.index);
    std::array<Point, 8> neighbors{};
    const std::size_t neighbor_count =
        collect_neighbors(current_point, neighbors);

    for (std::size_t i = 0; i < neighbor_count; ++i) {
      const Point &neighbor = neighbors[i];
      if (!is_walkableFunc(neighbor.x, neighbor.y)) {
        continue;
      }

      const int neighbor_idx = to_index(neighbor);
      if (is_closed(neighbor_idx, generation)) {
        continue;
      }

      const int tentative_gcost = current.g_cost + 1;
      if (tentative_gcost >= get_g_cost(neighbor_idx, generation)) {
        continue;
      }

      set_g_cost(neighbor_idx, generation, tentative_gcost);
      set_parent(neighbor_idx, generation, current.index);

      const int h_cost = calculate_heuristic(neighbor, end);
      push_open_node({neighbor_idx, tentative_gcost + h_cost, tentative_gcost});
    }
  }

  if (final_cost < 0) {
    return {};
  }

  std::vector<Point> path;
  path.reserve(final_cost + 1);
  build_path(start_idx, end_idx, generation, final_cost + 1, path);
  return path;
}

auto Pathfinding::calculate_heuristic(const Point &a, const Point &b) -> int {
  return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

void Pathfinding::ensure_working_buffers() {
  const std::size_t total_cells =
      static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);

  if (m_closed_generation.size() != total_cells) {
    m_closed_generation.assign(total_cells, 0);
    m_g_cost_generation.assign(total_cells, 0);
    m_g_cost_values.assign(total_cells, std::numeric_limits<int>::max());
    m_parent_generation.assign(total_cells, 0);
    m_parent_values.assign(total_cells, -1);
  }

  const std::size_t min_open_capacity =
      std::max<std::size_t>(total_cells / 8, 64);
  if (m_open_heap.capacity() < min_open_capacity) {
    m_open_heap.reserve(min_open_capacity);
  }
}

auto Pathfinding::next_generation() -> std::uint32_t {
  auto next = ++m_generation_counter;
  if (next == 0) {
    reset_generations();
    next = ++m_generation_counter;
  }
  return next;
}

void Pathfinding::reset_generations() {
  std::fill(m_closed_generation.begin(), m_closed_generation.end(), 0);
  std::fill(m_g_cost_generation.begin(), m_g_cost_generation.end(), 0);
  std::fill(m_parent_generation.begin(), m_parent_generation.end(), 0);
  std::fill(m_g_cost_values.begin(), m_g_cost_values.end(),
            std::numeric_limits<int>::max());
  std::fill(m_parent_values.begin(), m_parent_values.end(), -1);
  m_generation_counter = 0;
}

auto Pathfinding::is_closed(int index, std::uint32_t generation) const -> bool {
  return index >= 0 &&
         static_cast<std::size_t>(index) < m_closed_generation.size() &&
         m_closed_generation[static_cast<std::size_t>(index)] == generation;
}

void Pathfinding::set_closed(int index, std::uint32_t generation) {
  if (index >= 0 &&
      static_cast<std::size_t>(index) < m_closed_generation.size()) {
    m_closed_generation[static_cast<std::size_t>(index)] = generation;
  }
}

auto Pathfinding::get_g_cost(int index, std::uint32_t generation) const -> int {
  if (index < 0 ||
      static_cast<std::size_t>(index) >= m_g_cost_generation.size()) {
    return std::numeric_limits<int>::max();
  }
  if (m_g_cost_generation[static_cast<std::size_t>(index)] == generation) {
    return m_g_cost_values[static_cast<std::size_t>(index)];
  }
  return std::numeric_limits<int>::max();
}

void Pathfinding::set_g_cost(int index, std::uint32_t generation, int cost) {
  if (index >= 0 &&
      static_cast<std::size_t>(index) < m_g_cost_generation.size()) {
    const auto idx = static_cast<std::size_t>(index);
    m_g_cost_generation[idx] = generation;
    m_g_cost_values[idx] = cost;
  }
}

auto Pathfinding::has_parent(int index,
                             std::uint32_t generation) const -> bool {
  return index >= 0 &&
         static_cast<std::size_t>(index) < m_parent_generation.size() &&
         m_parent_generation[static_cast<std::size_t>(index)] == generation;
}

auto Pathfinding::get_parent(int index, std::uint32_t generation) const -> int {
  if (has_parent(index, generation)) {
    return m_parent_values[static_cast<std::size_t>(index)];
  }
  return -1;
}

void Pathfinding::set_parent(int index, std::uint32_t generation,
                             int parent_index) {
  if (index >= 0 &&
      static_cast<std::size_t>(index) < m_parent_generation.size()) {
    const auto idx = static_cast<std::size_t>(index);
    m_parent_generation[idx] = generation;
    m_parent_values[idx] = parent_index;
  }
}

auto Pathfinding::collect_neighbors(
    const Point &point, std::array<Point, 8> &buffer) const -> std::size_t {
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
        if (!is_walkable(point.x + dx, point.y) ||
            !is_walkable(point.x, point.y + dy)) {
          continue;
        }
      }

      buffer[count++] = Point{x, y};
    }
  }
  return count;
}

void Pathfinding::build_path(int start_index, int end_index,
                             std::uint32_t generation, int expected_length,
                             std::vector<Point> &out_path) const {
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

    if (!has_parent(current, generation)) {
      out_path.clear();
      return;
    }

    const int parent = get_parent(current, generation);
    if (parent == current || parent < 0) {
      out_path.clear();
      return;
    }
    current = parent;
  }

  out_path.clear();
}

auto Pathfinding::heap_less(const QueueNode &lhs,
                            const QueueNode &rhs) -> bool {
  if (lhs.f_cost != rhs.f_cost) {
    return lhs.f_cost < rhs.f_cost;
  }
  return lhs.g_cost < rhs.g_cost;
}

void Pathfinding::push_open_node(const QueueNode &node) {
  m_open_heap.push_back(node);
  std::size_t index = m_open_heap.size() - 1;
  while (index > 0) {
    std::size_t const parent = (index - 1) / 2;
    if (heap_less(m_open_heap[parent], m_open_heap[index])) {
      break;
    }
    std::swap(m_open_heap[parent], m_open_heap[index]);
    index = parent;
  }
}

auto Pathfinding::pop_open_node() -> Pathfinding::QueueNode {
  QueueNode top = m_open_heap.front();
  QueueNode const last = m_open_heap.back();
  m_open_heap.pop_back();
  if (!m_open_heap.empty()) {
    m_open_heap[0] = last;
    std::size_t index = 0;
    const std::size_t size = m_open_heap.size();
    while (true) {
      std::size_t const left = index * 2 + 1;
      std::size_t const right = left + 1;
      std::size_t smallest = index;

      if (left < size && !heap_less(m_open_heap[smallest], m_open_heap[left])) {
        smallest = left;
      }
      if (right < size &&
          !heap_less(m_open_heap[smallest], m_open_heap[right])) {
        smallest = right;
      }
      if (smallest == index) {
        break;
      }
      std::swap(m_open_heap[index], m_open_heap[smallest]);
      index = smallest;
    }
  }
  return top;
}

void Pathfinding::worker_loop() {
  while (true) {
    PathRequest request;
    {
      std::unique_lock<std::mutex> lock(m_request_mutex);
      m_request_condition.wait(lock, [this]() {
        return m_stop_worker.load(std::memory_order_acquire) ||
               !m_request_queue.empty();
      });

      if (m_stop_worker.load(std::memory_order_acquire) &&
          m_request_queue.empty()) {
        break;
      }

      request = m_request_queue.front();
      m_request_queue.pop();
    }

    auto path = (request.unit_radius > 0.0F)
                    ? find_path(request.start, request.end, request.unit_radius)
                    : find_path(request.start, request.end);

    {
      std::lock_guard<std::mutex> const lock(m_result_mutex);
      m_result_queue.push({request.request_id, std::move(path)});
    }
  }
}

auto Pathfinding::find_nearest_walkable_point(const Point &point,
                                              int max_search_radius,
                                              const Pathfinding &pathfinder,
                                              float unit_radius) -> Point {
  auto const is_walkableFunc = [&pathfinder, unit_radius](int x,
                                                          int y) -> bool {
    if (unit_radius <= 0.5F) {
      return pathfinder.is_walkable(x, y);
    }
    return pathfinder.is_walkable_with_radius(x, y, unit_radius);
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
