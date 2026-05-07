#pragma once

#include <QVector3D>
#include <optional>

namespace Render::GL {
class Renderer;
class ResourceManager;
} // namespace Render::GL

namespace Render::GL {

struct FormationPlacementInfo {
  QVector3D position;
  float angle_degrees = 0.0F;
  bool active = false;
  /// Overall opacity multiplier for smooth fade-in / fade-out (0 = invisible,
  /// 1 = fully opaque).  Defaults to fully opaque.
  float fade_alpha = 1.0F;
  /// Optional faction accent colour rendered as a thin coloured border around
  /// the arrow body (e.g. Roman crimson or Carthaginian purple).  When absent
  /// the arrow is drawn in its default neutral tactical palette.
  std::optional<QVector3D> accent_color;
};

void render_formation_arrow(Renderer *renderer, ResourceManager *resources,
                            const FormationPlacementInfo &placement);

} // namespace Render::GL
