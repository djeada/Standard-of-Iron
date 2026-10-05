#pragma once

#include <QVector3D>

#include <array>
#include <cstdint>
#include <span>

namespace Render::GL {
class Renderer;
} // namespace Render::GL

namespace Render::GL {

struct RaftDrawable {
  QVector3D position;
  QVector3D across;
  std::array<QVector3D, 2> docks;
  float half_length{1.9F};
  float half_width{2.7F};
  bool moving{false};
  std::uint32_t seed{0};
};

void draw_rafts(Renderer* renderer, std::span<const RaftDrawable> rafts);

} // namespace Render::GL
