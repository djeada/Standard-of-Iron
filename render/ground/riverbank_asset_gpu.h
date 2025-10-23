#pragma once

#include <QVector3D>
#include <array>

namespace Render::GL {

struct RiverbankAssetInstanceGpu {
  std::array<float, 3> position;
  std::array<float, 3> scale;
  std::array<float, 4> rotation;
  std::array<float, 3> color;
  float assetType;
};

struct RiverbankAssetBatchParams {
  QVector3D lightDirection{0.35f, 0.8f, 0.45f};
  float time{0.0f};
};

} 
