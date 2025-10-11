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

  constexpr bool operator==(const Point &other) const {
    return x == other.x && y == other.y;
  }
};

class Pathfinding {
public:
  Pathfinding(int width, int height);
  ~Pathfinding();

  void setGridOffset(float offsetX, float offsetZ);

  float getGridOffsetX() const { return m_gridOffsetX; }
  float getGridOffsetZ() const { return m_gridOffsetZ; }

  void setObstacle(int x, int y, bool isObstacle);
  bool isWalkable(int x, int y) const;

  void updateBuildingObstacles();

  void markObstaclesDirty();

  std::vector<Point> findPath(const Point &start, const Point &end);

  std::future<std::vector<Point>> findPathAsync(const Point &start,
                                                const Point &end);

  void submitPathRequest(std::uint64_t requestId, const Point &start,
                         const Point &end);

  struct PathResult {
    std::uint64_t requestId;
    std::vector<Point> path;
  };
  std::vector<PathResult> fetchCompletedPaths();

private:
  std::vector<Point> findPathInternal(const Point &start, const Point &end);

  int calculateHeuristic(const Point &a, const Point &b) const;

  void ensureWorkingBuffers();
  std::uint32_t nextGeneration();
  void resetGenerations();

  inline int toIndex(int x, int y) const { return y * m_width + x; }
  inline int toIndex(const Point &p) const { return toIndex(p.x, p.y); }
  inline Point toPoint(int index) const {
    return Point(index % m_width, index / m_width);
  }

  bool isClosed(int index, std::uint32_t generation) const;
  void setClosed(int index, std::uint32_t generation);

  int getGCost(int index, std::uint32_t generation) const;
  void setGCost(int index, std::uint32_t generation, int cost);

  bool hasParent(int index, std::uint32_t generation) const;
  int getParent(int index, std::uint32_t generation) const;
  void setParent(int index, std::uint32_t generation, int parentIndex);

  std::size_t collectNeighbors(const Point &point,
                               std::array<Point, 8> &buffer) const;
  void buildPath(int startIndex, int endIndex, std::uint32_t generation,
                 int expectedLength, std::vector<Point> &outPath) const;

  struct QueueNode {
    int index;
    int fCost;
    int gCost;
  };

  bool heapLess(const QueueNode &lhs, const QueueNode &rhs) const;
  void pushOpenNode(const QueueNode &node);
  QueueNode popOpenNode();

  void workerLoop();

  int m_width, m_height;
  std::vector<std::vector<std::uint8_t>> m_obstacles;
  float m_gridCellSize;
  float m_gridOffsetX, m_gridOffsetZ;
  std::atomic<bool> m_obstaclesDirty;
  mutable std::mutex m_mutex;
  std::atomic<bool> m_stopWorker{false};
  std::thread m_workerThread;
  std::mutex m_requestMutex;
  std::condition_variable m_requestCondition;
  struct PathRequest {
    std::uint64_t requestId;
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