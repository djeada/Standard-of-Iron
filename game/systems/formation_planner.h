#pragma once

#include <QVector3D>
#include <cmath>
#include <vector>

namespace Game {
namespace Systems {

class FormationPlanner {
public:
  static std::vector<QVector3D> spreadFormation(int n, const QVector3D &center,
                                                float spacing = 1.0f) {
    std::vector<QVector3D> out;
    out.reserve(n);
    if (n <= 0)
      return out;
    int side = std::ceil(std::sqrt(float(n)));
    for (int i = 0; i < n; ++i) {
      int gx = i % side;
      int gy = i / side;
      float ox = (gx - (side - 1) * 0.5f) * spacing;
      float oz = (gy - (side - 1) * 0.5f) * spacing;
      out.emplace_back(center.x() + ox, center.y(), center.z() + oz);
    }
    return out;
  }
};

} // namespace Systems
} // namespace Game
