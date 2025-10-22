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

HumanoidPalette makeHumanoidPalette(const QVector3D &teamTint, uint32_t seed);

} 
