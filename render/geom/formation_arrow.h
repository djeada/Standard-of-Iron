#pragma once

#include <QVector3D>

#include <optional>
#include <vector>

namespace Render::GL {
class Renderer;
class ResourceManager;
} // namespace Render::GL

namespace Render::GL {

struct FormationSlotMarker {
  QVector3D position;
  float radius = 1.0F;
  float facing_degrees = 0.0F;
  bool blocked = false;
  bool adjusted = false;
};

struct FormationPlacementInfo {
  QVector3D position;
  float angle_degrees = 0.0F;
  bool active = false;

  float aim_distance = 0.0F;

  float fade_alpha = 1.0F;

  std::optional<QVector3D> accent_color;
  std::vector<FormationSlotMarker> slot_markers;
};

void render_formation_arrow(Renderer* renderer,
                            ResourceManager* resources,
                            const FormationPlacementInfo& placement);

void render_formation_slot_preview(Renderer* renderer,
                                   ResourceManager* resources,
                                   const FormationPlacementInfo& placement);

} // namespace Render::GL
