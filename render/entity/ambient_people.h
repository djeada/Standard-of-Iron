#pragma once

#include <QVector3D>

#include <cstdint>
#include <span>

namespace Render::GL {

struct DrawContext;
class ISubmitter;

enum class AmbientRole : std::uint8_t {
  Stand,
  Weave,
  Squat,
  Kneel,
  Haggle,
  Stroll,
  Porter,
};

struct AmbientPerson {
  AmbientRole role{AmbientRole::Stand};
  std::span<const QVector3D> route;
  QVector3D facing{0.0F, 0.0F, 0.0F};

  AmbientRole linger{AmbientRole::Stand};

  bool priest{false};
};

struct WalkSurface {
  float min_x{0.0F};
  float max_x{0.0F};
  float min_z{0.0F};
  float max_z{0.0F};
  float top{0.0F};
};

void submit_ambient_people(const DrawContext& ctx,
                           ISubmitter& out,
                           bool carthage,
                           std::span<const AmbientPerson> people,
                           std::span<const WalkSurface> surfaces = {});

} // namespace Render::GL
