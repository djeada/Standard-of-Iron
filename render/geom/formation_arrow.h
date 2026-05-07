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

  float fade_alpha = 1.0F;

  std::optional<QVector3D> accent_color;
};

void render_formation_arrow(Renderer *renderer, ResourceManager *resources,
                            const FormationPlacementInfo &placement);

} // namespace Render::GL
