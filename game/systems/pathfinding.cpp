#include "pathfinding.h"
#include "../map/terrain_service.h"
#include "building_collision_registry.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace Game::Systems {

Pathfinding::Pathfinding(int width, int height)
    : m_width(width), m_height(height), m_gridCellSize(1.0f),
      m_gridOffsetX(0.0f), m_gridOffsetZ(0.0f) {
  m_obstacles.resize(height, std::vector<std::uint8_t>(width, 0));
  ensureWorkingBuffers();
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
    m_obstacles[y][x] = static_cast<std::uint8_t>(isObstacle);
  }
}

bool Pathfinding::isWalkable(int x, int y) const {
  if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
    return false;
  }
  return m_obstacles[y][x] == 0;
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
    std::fill(row.begin(), row.end(), static_cast<std::uint8_t>(0));
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
          m_obstacles[z][x] = static_cast<std::uint8_t>(1);
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
        m_obstacles[gridZ][gridX] = static_cast<std::uint8_t>(1);
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
                                                 const Point &end) {
  ensureWorkingBuffers();

  if (!isWalkable(start.x, start.y) || !isWalkable(end.x, end.y)) {
    return {};
  }

  const int startIdx = toIndex(start);
  const int endIdx = toIndex(end);

  if (startIdx == endIdx) {
    return {start};
  }

  const std::uint32_t generation = nextGeneration();

  m_openHeap.clear();

  setGCost(startIdx, generation, 0);
  setParent(startIdx, generation, startIdx);

  pushOpenNode({startIdx, calculateHeuristic(start, end), 0});

  const int maxIterations = std::max(m_width * m_height, 1);
  int iterations = 0;

  int finalCost = -1;

  while (!m_openHeap.empty() && iterations < maxIterations) {
    ++iterations;

    QueueNode current = popOpenNode();

    if (current.gCost > getGCost(current.index, generation)) {
      continue;
    }

    if (isClosed(current.index, generation)) {
      continue;
    }

    setClosed(current.index, generation);

    if (current.index == endIdx) {
      finalCost = current.gCost;
      break;
    }

    const Point currentPoint = toPoint(current.index);
    std::array<Point, 8> neighbors{};
    const std::size_t neighborCount = collectNeighbors(currentPoint, neighbors);

    for (std::size_t i = 0; i < neighborCount; ++i) {
      const Point &neighbor = neighbors[i];
      if (!isWalkable(neighbor.x, neighbor.y)) {
        continue;
      }

      const int neighborIdx = toIndex(neighbor);
      if (isClosed(neighborIdx, generation)) {
        continue;
      }

      const int tentativeGCost = current.gCost + 1;
      if (tentativeGCost >= getGCost(neighborIdx, generation)) {
        continue;
      }

      setGCost(neighborIdx, generation, tentativeGCost);
      setParent(neighborIdx, generation, current.index);

      const int hCost = calculateHeuristic(neighbor, end);
      pushOpenNode({neighborIdx, tentativeGCost + hCost, tentativeGCost});
    }
  }

  if (finalCost < 0) {
    return {};
  }

  std::vector<Point> path;
  path.reserve(finalCost + 1);
  buildPath(startIdx, endIdx, generation, finalCost + 1, path);
  return path;
}

int Pathfinding::calculateHeuristic(const Point &a, const Point &b) const {
  return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

void Pathfinding::ensureWorkingBuffers() {
  const std::size_t totalCells =
      static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);

  if (m_closedGeneration.size() != totalCells) {
    m_closedGeneration.assign(totalCells, 0);
    m_gCostGeneration.assign(totalCells, 0);
    m_gCostValues.assign(totalCells, std::numeric_limits<int>::max());
    m_parentGeneration.assign(totalCells, 0);
    m_parentValues.assign(totalCells, -1);
  }

  const std::size_t minOpenCapacity = std::max<std::size_t>(totalCells / 8, 64);
  if (m_openHeap.capacity() < minOpenCapacity) {
    m_openHeap.reserve(minOpenCapacity);
  }
}

std::uint32_t Pathfinding::nextGeneration() {
  auto next = ++m_generationCounter;
  if (next == 0) {
    resetGenerations();
    next = ++m_generationCounter;
  }
  return next;
}

void Pathfinding::resetGenerations() {
  std::fill(m_closedGeneration.begin(), m_closedGeneration.end(), 0);
  std::fill(m_gCostGeneration.begin(), m_gCostGeneration.end(), 0);
  std::fill(m_parentGeneration.begin(), m_parentGeneration.end(), 0);
  std::fill(m_gCostValues.begin(), m_gCostValues.end(),
            std::numeric_limits<int>::max());
  std::fill(m_parentValues.begin(), m_parentValues.end(), -1);
  m_generationCounter = 0;
}

bool Pathfinding::isClosed(int index, std::uint32_t generation) const {
  return index >= 0 &&
         static_cast<std::size_t>(index) < m_closedGeneration.size() &&
         m_closedGeneration[static_cast<std::size_t>(index)] == generation;
}

void Pathfinding::setClosed(int index, std::uint32_t generation) {
  if (index >= 0 &&
      static_cast<std::size_t>(index) < m_closedGeneration.size()) {
    m_closedGeneration[static_cast<std::size_t>(index)] = generation;
  }
}

int Pathfinding::getGCost(int index, std::uint32_t generation) const {
  if (index < 0 ||
      static_cast<std::size_t>(index) >= m_gCostGeneration.size()) {
    return std::numeric_limits<int>::max();
  }
  if (m_gCostGeneration[static_cast<std::size_t>(index)] == generation) {
    return m_gCostValues[static_cast<std::size_t>(index)];
  }
  return std::numeric_limits<int>::max();
}

void Pathfinding::setGCost(int index, std::uint32_t generation, int cost) {
  if (index >= 0 &&
      static_cast<std::size_t>(index) < m_gCostGeneration.size()) {
    const auto idx = static_cast<std::size_t>(index);
    m_gCostGeneration[idx] = generation;
    m_gCostValues[idx] = cost;
  }
}

bool Pathfinding::hasParent(int index, std::uint32_t generation) const {
  return index >= 0 &&
         static_cast<std::size_t>(index) < m_parentGeneration.size() &&
         m_parentGeneration[static_cast<std::size_t>(index)] == generation;
}

int Pathfinding::getParent(int index, std::uint32_t generation) const {
  if (hasParent(index, generation)) {
    return m_parentValues[static_cast<std::size_t>(index)];
  }
  return -1;
}

void Pathfinding::setParent(int index, std::uint32_t generation,
                            int parentIndex) {
  if (index >= 0 &&
      static_cast<std::size_t>(index) < m_parentGeneration.size()) {
    const auto idx = static_cast<std::size_t>(index);
    m_parentGeneration[idx] = generation;
    m_parentValues[idx] = parentIndex;
  }
}

std::size_t Pathfinding::collectNeighbors(const Point &point,
                                          std::array<Point, 8> &buffer) const {
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
        if (!isWalkable(point.x + dx, point.y) ||
            !isWalkable(point.x, point.y + dy)) {
          continue;
        }
      }

      buffer[count++] = Point{x, y};
    }
  }
  return count;
}

void Pathfinding::buildPath(int startIndex, int endIndex,
                            std::uint32_t generation, int expectedLength,
                            std::vector<Point> &outPath) const {
  outPath.clear();
  if (expectedLength > 0) {
    outPath.reserve(static_cast<std::size_t>(expectedLength));
  }
  int current = endIndex;

  while (current >= 0) {
    outPath.push_back(toPoint(current));
    if (current == startIndex) {
      std::reverse(outPath.begin(), outPath.end());
      return;
    }

    if (!hasParent(current, generation)) {
      outPath.clear();
      return;
    }

    const int parent = getParent(current, generation);
    if (parent == current || parent < 0) {
      outPath.clear();
      return;
    }
    current = parent;
  }

  outPath.clear();
}

bool Pathfinding::heapLess(const QueueNode &lhs, const QueueNode &rhs) const {
  if (lhs.fCost != rhs.fCost) {
    return lhs.fCost < rhs.fCost;
  }
  return lhs.gCost < rhs.gCost;
}

void Pathfinding::pushOpenNode(const QueueNode &node) {
  m_openHeap.push_back(node);
  std::size_t index = m_openHeap.size() - 1;
  while (index > 0) {
    std::size_t parent = (index - 1) / 2;
    if (heapLess(m_openHeap[parent], m_openHeap[index])) {
      break;
    }
    std::swap(m_openHeap[parent], m_openHeap[index]);
    index = parent;
  }
}

Pathfinding::QueueNode Pathfinding::popOpenNode() {
  QueueNode top = m_openHeap.front();
  QueueNode last = m_openHeap.back();
  m_openHeap.pop_back();
  if (!m_openHeap.empty()) {
    m_openHeap[0] = last;
    std::size_t index = 0;
    const std::size_t size = m_openHeap.size();
    while (true) {
      std::size_t left = index * 2 + 1;
      std::size_t right = left + 1;
      std::size_t smallest = index;

      if (left < size && !heapLess(m_openHeap[smallest], m_openHeap[left])) {
        smallest = left;
      }
      if (right < size && !heapLess(m_openHeap[smallest], m_openHeap[right])) {
        smallest = right;
      }
      if (smallest == index) {
        break;
      }
      std::swap(m_openHeap[index], m_openHeap[smallest]);
      index = smallest;
    }
  }
  return top;
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