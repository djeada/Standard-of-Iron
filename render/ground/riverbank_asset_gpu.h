#pragma once

#include <QVector3D>
#include <array>

namespace Render::GL {

struct RiverbankAssetInstanceGpu {
  std::array<float, 3> position;
  std::array<float, 3> scale;
  std::array<float, 4> rotation;
  std::array<float, 3> color;
  float asset_type;
};

struct RiverbankAssetBatchParams {
  QVector3D light_direction{0.35F, 0.8F, 0.45F};
  float time{0.0F};
};

} // namespace Render::GL
