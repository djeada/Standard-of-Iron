#pragma once

#include <QVector3D>
#include <cstdint>

namespace Render::GL {

struct HumanoidPalette {
  QVector3D cloth;
  QVector3D skin;
  QVector3D leather;
  QVector3D leatherDark;
  QVector3D wood;
  QVector3D metal;
};

auto makeHumanoidPalette(const QVector3D &team_tint,
                         uint32_t seed) -> HumanoidPalette;

} // namespace Render::GL
