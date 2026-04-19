#pragma once

#include "frame_budget.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cstddef>
#include <cstdint>

namespace Render::GL {

class Mesh;
class Texture;
struct Material;

struct BonePaletteRef {
  const QMatrix4x4 *data = nullptr;
  std::size_t count = 0;

  [[nodiscard]] auto empty() const noexcept -> bool { return count == 0; }
};

struct DrawPartCmd {
  Mesh *mesh = nullptr;
  const Material *material = nullptr;
  QMatrix4x4 world;
  BonePaletteRef palette{};

  QVector3D color{1.0F, 1.0F, 1.0F};
  float alpha = 1.0F;
  Texture *texture = nullptr;
  std::int32_t material_id = 0;

  CommandPriority priority{CommandPriority::Normal};
};

} // namespace Render::GL
