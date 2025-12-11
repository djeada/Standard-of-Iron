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

class Pathfinding {
public:
  Pathfinding(int width, int height);
  ~Pathfinding();

  void setGridOffset(float offset_x, float offset_z);

  auto getGridOffsetX() const -> float { return m_gridOffsetX; }
  auto getGridOffsetZ() const -> float { return m_gridOffsetZ; }

  void setObstacle(int x, int y, bool isObstacle);
  auto isWalkable(int x, int y) const -> bool;

  void updateBuildingObstacles();

  void markObstaclesDirty();

  auto findPath(const Point &start, const Point &end) -> std::vector<Point>;

  auto findPathAsync(const Point &start,
                     const Point &end) -> std::future<std::vector<Point>>;

  void submitPathRequest(std::uint64_t request_id, const Point &start,
                         const Point &end);

  struct PathResult {
    std::uint64_t request_id;
    std::vector<Point> path;
  };
  auto fetchCompletedPaths() -> std::vector<PathResult>;

private:
  auto findPathInternal(const Point &start,
                        const Point &end) -> std::vector<Point>;

  static auto calculateHeuristic(const Point &a, const Point &b) -> int;

  void ensureWorkingBuffers();
  auto nextGeneration() -> std::uint32_t;
  void resetGenerations();

  auto toIndex(int x, int y) const -> int { return y * m_width + x; }
  auto toIndex(const Point &p) const -> int { return toIndex(p.x, p.y); }
  auto toPoint(int index) const -> Point {
    return {index % m_width, index / m_width};
  }

  auto isClosed(int index, std::uint32_t generation) const -> bool;
  void setClosed(int index, std::uint32_t generation);

  auto getGCost(int index, std::uint32_t generation) const -> int;
  void setGCost(int index, std::uint32_t generation, int cost);

  auto hasParent(int index, std::uint32_t generation) const -> bool;
  auto getParent(int index, std::uint32_t generation) const -> int;
  void setParent(int index, std::uint32_t generation, int parentIndex);

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

  void workerLoop();

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
};

} // namespace Game::Systems