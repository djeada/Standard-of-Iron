#include "pathfinding.h"
#include <algorithm>
#include <cmath>

namespace Game::Systems {

Pathfinding::Pathfinding(int width, int height)
    : m_width(width), m_height(height) {
  m_obstacles.resize(height, std::vector<bool>(width, false));
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

std::vector<Point> Pathfinding::findPath(const Point &start, const Point &end) {
  if (!isWalkable(start.x, start.y) || !isWalkable(end.x, end.y)) {
    return {};
  }

  if (start == end) {
    return {start};
  }

  std::vector<Node *> openList;
  std::vector<std::vector<bool>> closedList(m_height,
                                            std::vector<bool>(m_width, false));
  std::vector<std::vector<Node *>> allNodes(
      m_height, std::vector<Node *>(m_width, nullptr));

  Node *startNode = new Node(start, 0, calculateHeuristic(start, end));
  openList.push_back(startNode);
  allNodes[start.y][start.x] = startNode;

  std::vector<Point> path;

  while (!openList.empty()) {

    auto currentIt = std::min_element(openList.begin(), openList.end(),
                                      [](const Node *a, const Node *b) {
                                        return a->getFCost() < b->getFCost();
                                      });

    Node *current = *currentIt;
    openList.erase(currentIt);

    closedList[current->position.y][current->position.x] = true;

    if (current->position == end) {
      path = reconstructPath(current);
      break;
    }

    for (const auto &neighborPos : getNeighbors(current->position)) {
      if (!isWalkable(neighborPos.x, neighborPos.y) ||
          closedList[neighborPos.y][neighborPos.x]) {
        continue;
      }

      int tentativeGCost = current->gCost + 1;
      Node *neighbor = allNodes[neighborPos.y][neighborPos.x];

      if (!neighbor) {

        neighbor = new Node(neighborPos, tentativeGCost,
                            calculateHeuristic(neighborPos, end), current);
        allNodes[neighborPos.y][neighborPos.x] = neighbor;
        openList.push_back(neighbor);
      } else if (tentativeGCost < neighbor->gCost) {

        neighbor->gCost = tentativeGCost;
        neighbor->parent = current;
      }
    }
  }

  for (int y = 0; y < m_height; ++y) {
    for (int x = 0; x < m_width; ++x) {
      delete allNodes[y][x];
    }
  }

  return path;
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
        neighbors.emplace_back(x, y);
      }
    }
  }

  return neighbors;
}

std::vector<Point> Pathfinding::reconstructPath(Node *endNode) const {
  std::vector<Point> path;
  Node *current = endNode;

  while (current) {
    path.push_back(current->position);
    current = current->parent;
  }

  std::reverse(path.begin(), path.end());
  return path;
}

} // namespace Game::Systems