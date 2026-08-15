#pragma once

#include <QVector3D>

namespace Render {

struct MistVolume {
  enum class Kind : int {
    WaterMist = 0,
    Miasma = 1
  };

  QVector3D start;
  QVector3D end;
  float radius = 4.0F;
  float strength = 0.5F;
  Kind kind = Kind::WaterMist;
};

inline constexpr int k_max_mist_volumes = 24;

} // namespace Render
