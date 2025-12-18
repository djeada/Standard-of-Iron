#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace Game::Systems {

class BuildingCollisionRegistry;

struct Point {
  int x = 0;
  int y = 0;

  constexpr Point() = default;
  constexpr Point(int x_, int y_) : x(x_), y(y_) {}

  constexpr auto operator==(const Point &other) const -> bool {
    return x == other.x && y == other.y;
  }
};

struct DirtyRegion {
  int min_x;
  int max_x;
  int min_z;
  int max_z;

  DirtyRegion(int x1, int x2, int z1, int z2)
      : min_x(x1), max_x(x2), min_z(z1), max_z(z2) {}
};

class Pathfinding {
public:
  Pathfinding(int width, int height);
  ~Pathfinding();

  void set_grid_offset(float offset_x, float offset_z);

  auto get_grid_offset_x() const -> float { return m_gridOffsetX; }
  auto get_grid_offset_z() const -> float { return m_gridOffsetZ; }

  void set_obstacle(int x, int y, bool isObstacle);
  auto is_walkable(int x, int y) const -> bool;
  auto is_walkable_with_radius(int x, int y, float unit_radius) const -> bool;

  void update_building_obstacles();

  void mark_obstacles_dirty();

  void mark_region_dirty(int min_x, int max_x, int min_z, int max_z);

  void mark_building_region_dirty(float center_x, float center_z, float width,
                               float depth);

  auto find_path(const Point &start, const Point &end) -> std::vector<Point>;
  auto find_path(const Point &start, const Point &end, float unit_radius)
      -> std::vector<Point>;

  auto find_path_async(const Point &start,
                     const Point &end) -> std::future<std::vector<Point>>;

  void submit_path_request(std::uint64_t request_id, const Point &start,
                         const Point &end);
  void submit_path_request(std::uint64_t request_id, const Point &start,
                         const Point &end, float unit_radius);

  struct PathResult {
    std::uint64_t request_id;
    std::vector<Point> path;
  };
  auto fetch_completed_paths() -> std::vector<PathResult>;

  static auto find_nearest_walkable_point(const Point &point,
                                          int max_search_radius,
                                          const Pathfinding &pathfinder,
                                          float unit_radius = 0.0F) -> Point;

private:
  auto find_path_internal(const Point &start, const Point &end)
      -> std::vector<Point>;
  auto find_path_internal(const Point &start, const Point &end, float unit_radius)
      -> std::vector<Point>;

  static auto calculate_heuristic(const Point &a, const Point &b) -> int;

  void ensure_working_buffers();
  auto next_generation() -> std::uint32_t;
  void reset_generations();

  auto toIndex(int x, int y) const -> int { return y * m_width + x; }
  auto toIndex(const Point &p) const -> int { return toIndex(p.x, p.y); }
  auto toPoint(int index) const -> Point {
    return {index % m_width, index / m_width};
  }

  auto is_closed(int index, std::uint32_t generation) const -> bool;
  void set_closed(int index, std::uint32_t generation);

  auto get_g_cost(int index, std::uint32_t generation) const -> int;
  void set_g_cost(int index, std::uint32_t generation, int cost);

  auto has_parent(int index, std::uint32_t generation) const -> bool;
  auto get_parent(int index, std::uint32_t generation) const -> int;
  void set_parent(int index, std::uint32_t generation, int parentIndex);

  auto collect_neighbors(const Point &point,
                         std::array<Point, 8> &buffer) const -> std::size_t;
  void build_path(int start_index, int end_index, std::uint32_t generation,
                  int expected_length, std::vector<Point> &out_path) const;

  struct QueueNode {
    int index;
    int f_cost;
    int g_cost;
  };

  static auto heap_less(const QueueNode &lhs, const QueueNode &rhs) -> bool;
  void push_open_node(const QueueNode &node);
  auto pop_open_node() -> QueueNode;

  void worker_loop();

  void process_dirty_regions();

  void update_region(int min_x, int max_x, int min_z, int max_z);

  int m_width, m_height;
  std::vector<std::vector<std::uint8_t>> m_obstacles;
  float m_gridCellSize{1.0F};
  float m_gridOffsetX{0.0F}, m_gridOffsetZ{0.0F};
  std::atomic<bool> m_obstaclesDirty;
  mutable std::mutex m_mutex;
  std::atomic<bool> m_stopWorker{false};
  std::thread m_workerThread;
  std::mutex m_requestMutex;
  std::condition_variable m_requestCondition;
  struct PathRequest {
    std::uint64_t request_id{};
    Point start;
    Point end;
    float unit_radius{0.0F};
  };
  std::queue<PathRequest> m_requestQueue;
  std::mutex m_resultMutex;
  std::queue<PathResult> m_resultQueue;

  mutable std::vector<std::uint32_t> m_closedGeneration;
  mutable std::vector<std::uint32_t> m_gCostGeneration;
  mutable std::vector<int> m_gCostValues;
  mutable std::vector<std::uint32_t> m_parentGeneration;
  mutable std::vector<int> m_parentValues;
  mutable std::vector<QueueNode> m_openHeap;
  mutable std::uint32_t m_generationCounter{0};

  std::mutex m_dirtyMutex;
  std::vector<DirtyRegion> m_dirtyRegions;
  bool m_fullUpdateRequired{true};
};

} // namespace Game::Systems