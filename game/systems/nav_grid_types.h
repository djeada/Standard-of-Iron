#pragma once

namespace Game::Systems {

struct Point {
  int x = 0;
  int y = 0;

  constexpr Point() = default;
  constexpr Point(int x_, int y_)
      : x(x_)
      , y(y_) {}

  constexpr auto operator==(const Point& other) const -> bool {
    return x == other.x && y == other.y;
  }
};

struct DirtyRegion {
  int min_x;
  int max_x;
  int min_z;
  int max_z;

  DirtyRegion(int x1, int x2, int z1, int z2)
      : min_x(x1)
      , max_x(x2)
      , min_z(z1)
      , max_z(z2) {}
};

struct CellRange {
  int min_x{0};
  int max_x{0};
  int min_z{0};
  int max_z{0};
};

struct WorldRect {
  float min_x{0.0F};
  float max_x{0.0F};
  float min_z{0.0F};
  float max_z{0.0F};
};

} // namespace Game::Systems
