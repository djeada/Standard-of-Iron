#include "pathfinding.h"
#include "../map/terrain_service.h"
#include "building_collision_registry.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

namespace Game::Systems {

Pathfinding::Pathfinding(int width, int height)
    : m_width(width), m_height(height), m_gridCellSize(1.0f),
      m_gridOffsetX(0.0f), m_gridOffsetZ(0.0f) {
  m_obstacles.resize(height, std::vector<bool>(width, false));
  m_obstaclesDirty.store(true, std::memory_order_release);
  m_workerThread = std::thread(&Pathfinding::workerLoop, this);
}

Pathfinding::~Pathfinding() {
  m_stopWorker.store(true, std::memory_order_release);
  m_requestCondition.notify_all();
  if (m_workerThread.joinable()) {
    m_workerThread.join();
  }
}

void Pathfinding::setGridOffset(float offsetX, float offsetZ) {
  m_gridOffsetX = offsetX;
  m_gridOffsetZ = offsetZ;
}

void Pathfinding::setObstacle(int x, int y, bool isObstacle) {
  if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
    m_obstacles[y][x] = isObstacle;
  }
}

bool Pathfinding::isWalkable(int x, int y) const {
  if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
    return false;
  }
  return !m_obstacles[y][x];
}

void Pathfinding::markObstaclesDirty() {
  m_obstaclesDirty.store(true, std::memory_order_release);
}

void Pathfinding::updateBuildingObstacles() {

  if (!m_obstaclesDirty.load(std::memory_order_acquire)) {
    return;
  }

  std::lock_guard<std::mutex> lock(m_mutex);

  if (!m_obstaclesDirty.load(std::memory_order_acquire)) {
    return;
  }

  for (auto &row : m_obstacles) {
    std::fill(row.begin(), row.end(), false);
  }

  auto &terrainService = Game::Map::TerrainService::instance();
  if (terrainService.isInitialized()) {
    const Game::Map::TerrainHeightMap *heightMap =
        terrainService.getHeightMap();
    const int terrainWidth = heightMap ? heightMap->getWidth() : 0;
    const int terrainHeight = heightMap ? heightMap->getHeight() : 0;

    for (int z = 0; z < m_height; ++z) {
      for (int x = 0; x < m_width; ++x) {
        bool blocked = false;
        if (x < terrainWidth && z < terrainHeight) {
          blocked = !terrainService.isWalkable(x, z);
        } else {
          blocked = true;
        }

        if (blocked) {
          m_obstacles[z][x] = true;
        }
      }
    }
  }

  auto &registry = BuildingCollisionRegistry::instance();
  const auto &buildings = registry.getAllBuildings();

  for (const auto &building : buildings) {
    auto cells = registry.getOccupiedGridCells(building, m_gridCellSize);
    for (const auto &cell : cells) {
      int gridX = static_cast<int>(std::round(cell.first - m_gridOffsetX));
      int gridZ = static_cast<int>(std::round(cell.second - m_gridOffsetZ));

      if (gridX >= 0 && gridX < m_width && gridZ >= 0 && gridZ < m_height) {
        m_obstacles[gridZ][gridX] = true;
      }
    }
  }

  m_obstaclesDirty.store(false, std::memory_order_release);
}

std::vector<Point> Pathfinding::findPath(const Point &start, const Point &end) {

  if (m_obstaclesDirty.load(std::memory_order_acquire)) {
    updateBuildingObstacles();
  }

  std::lock_guard<std::mutex> lock(m_mutex);
  return findPathInternal(start, end);
}

std::future<std::vector<Point>> Pathfinding::findPathAsync(const Point &start,
                                                           const Point &end) {
  return std::async(std::launch::async,
                    [this, start, end]() { return findPath(start, end); });
}

void Pathfinding::submitPathRequest(std::uint64_t requestId, const Point &start,
                                    const Point &end) {
  {
    std::lock_guard<std::mutex> lock(m_requestMutex);
    m_requestQueue.push({requestId, start, end});
  }
  m_requestCondition.notify_one();
}

std::vector<Pathfinding::PathResult> Pathfinding::fetchCompletedPaths() {
  std::vector<PathResult> results;
  std::lock_guard<std::mutex> lock(m_resultMutex);
  while (!m_resultQueue.empty()) {
    results.push_back(std::move(m_resultQueue.front()));
    m_resultQueue.pop();
  }
  return results;
}

std::vector<Point> Pathfinding::findPathInternal(const Point &start,
                                                 const Point &end) const {
  if (!isWalkable(start.x, start.y) || !isWalkable(end.x, end.y)) {
    return {};
  }

  if (start == end) {
    return {start};
  }

  struct QueueNode {
    Point position;
    int fCost;
    int gCost;
  };

  auto compare = [](const QueueNode &a, const QueueNode &b) {
    return a.fCost > b.fCost;
  };

  std::priority_queue<QueueNode, std::vector<QueueNode>, decltype(compare)>
      openQueue(compare);

  std::vector<std::vector<bool>> closedList(m_height,
                                            std::vector<bool>(m_width, false));
  std::vector<std::vector<int>> gCosts(
      m_height, std::vector<int>(m_width, std::numeric_limits<int>::max()));
  std::vector<std::vector<Point>> parents(
      m_height, std::vector<Point>(m_width, Point(-1, -1)));

  gCosts[start.y][start.x] = 0;
  parents[start.y][start.x] = start;
  openQueue.push({start, calculateHeuristic(start, end), 0});

  const int maxIterations = std::max(m_width * m_height, 1);
  int iterations = 0;
  bool pathFound = false;

  while (!openQueue.empty() && iterations < maxIterations) {
    iterations++;

    QueueNode current = openQueue.top();
    openQueue.pop();

    const Point &currentPos = current.position;

    if (current.gCost > gCosts[currentPos.y][currentPos.x]) {
      continue;
    }

    if (closedList[currentPos.y][currentPos.x]) {
      continue;
    }

    closedList[currentPos.y][currentPos.x] = true;

    if (currentPos == end) {
      pathFound = true;
      break;
    }

    for (const auto &neighborPos : getNeighbors(currentPos)) {
      if (!isWalkable(neighborPos.x, neighborPos.y) ||
          closedList[neighborPos.y][neighborPos.x]) {
        continue;
      }

      int tentativeGCost = current.gCost + 1;

      if (tentativeGCost < gCosts[neighborPos.y][neighborPos.x]) {
        gCosts[neighborPos.y][neighborPos.x] = tentativeGCost;
        parents[neighborPos.y][neighborPos.x] = currentPos;

        int hCost = calculateHeuristic(neighborPos, end);
        openQueue.push({neighborPos, tentativeGCost + hCost, tentativeGCost});
      }
    }
  }

  if (!pathFound) {
    return {};
  }

  return reconstructPath(start, end, parents);
}

int Pathfinding::calculateHeuristic(const Point &a, const Point &b) const {

  return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

std::vector<Point> Pathfinding::getNeighbors(const Point &point) const {
  std::vector<Point> neighbors;

  for (int dx = -1; dx <= 1; ++dx) {
    for (int dy = -1; dy <= 1; ++dy) {
      if (dx == 0 && dy == 0)
        continue;

      int x = point.x + dx;
      int y = point.y + dy;

      if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
        if (dx != 0 && dy != 0) {

          if (!isWalkable(point.x + dx, point.y) ||
              !isWalkable(point.x, point.y + dy)) {
            continue;
          }
        }
        neighbors.emplace_back(x, y);
      }
    }
  }

  return neighbors;
}

std::vector<Point> Pathfinding::reconstructPath(
    const Point &start, const Point &end,
    const std::vector<std::vector<Point>> &parents) const {
  std::vector<Point> path;
  Point current = end;
  path.push_back(current);

  while (!(current == start)) {
    const Point &parent = parents[current.y][current.x];
    if (parent.x == -1 && parent.y == -1) {
      return {};
    }
    current = parent;
    path.push_back(current);
  }

  std::reverse(path.begin(), path.end());
  return path;
}

void Pathfinding::workerLoop() {
  while (true) {
    PathRequest request;
    {
      std::unique_lock<std::mutex> lock(m_requestMutex);
      m_requestCondition.wait(lock, [this]() {
        return m_stopWorker.load(std::memory_order_acquire) ||
               !m_requestQueue.empty();
      });

      if (m_stopWorker.load(std::memory_order_acquire) &&
          m_requestQueue.empty()) {
        break;
      }

      request = m_requestQueue.front();
      m_requestQueue.pop();
    }

    auto path = findPath(request.start, request.end);

    {
      std::lock_guard<std::mutex> lock(m_resultMutex);
      m_resultQueue.push({request.requestId, std::move(path)});
    }
  }
}

} // namespace Game::Systems