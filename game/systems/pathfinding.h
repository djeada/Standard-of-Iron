#pragma once

#include <vector>
#include <queue>

namespace Game::Systems {

struct Point {
    int x, y;
    Point(int x = 0, int y = 0) : x(x), y(y) {}
    bool operator==(const Point& other) const { return x == other.x && y == other.y; }
};

struct Node {
    Point position;
    int gCost, hCost;
    Node* parent;
    
    Node(const Point& pos, int g = 0, int h = 0, Node* p = nullptr)
        : position(pos), gCost(g), hCost(h), parent(p) {}
    
    int getFCost() const { return gCost + hCost; }
};

class Pathfinding {
public:
    Pathfinding(int width, int height);
    
    void setObstacle(int x, int y, bool isObstacle);
    bool isWalkable(int x, int y) const;
    
    std::vector<Point> findPath(const Point& start, const Point& end);
    
private:
    int m_width, m_height;
    std::vector<std::vector<bool>> m_obstacles;
    
    int calculateHeuristic(const Point& a, const Point& b) const;
    std::vector<Point> getNeighbors(const Point& point) const;
    std::vector<Point> reconstructPath(Node* endNode) const;
};

} // namespace Game::Systems