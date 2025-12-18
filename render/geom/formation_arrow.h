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
};

void renderFormationArrow(Renderer *renderer, ResourceManager *resources,
                          const FormationPlacementInfo &placement);

} // namespace Render::GL
