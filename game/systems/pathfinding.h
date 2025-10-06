#pragma once

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
  int x, y;
  Point(int x = 0, int y = 0) : x(x), y(y) {}
  bool operator==(const Point &other) const {
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
  int m_width, m_height;
  std::vector<std::vector<bool>> m_obstacles;
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

  std::vector<Point> findPathInternal(const Point &start,
                                      const Point &end) const;

  int calculateHeuristic(const Point &a, const Point &b) const;
  std::vector<Point> getNeighbors(const Point &point) const;
  std::vector<Point>
  reconstructPath(const Point &start, const Point &end,
                  const std::vector<std::vector<Point>> &parents) const;

  void workerLoop();
};

} // namespace Game::Systems