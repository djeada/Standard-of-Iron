#include "formation_arrow.h"
#include "../gl/primitives.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include "selection_disc.h"
#include <QMatrix4x4>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <qvectornd.h>

namespace Render::GL {

void render_formation_arrow(Renderer *renderer, ResourceManager *resources,
                            const FormationPlacementInfo &placement) {
  if ((renderer == nullptr) || (resources == nullptr) || !placement.active) {
    return;
  }
  if (placement.fade_alpha <= 0.0F) {
    return;
  }

  Mesh *const arrow_mesh = get_orientation_arrow();
  if (arrow_mesh == nullptr) {
    return;
  }

  float const visual_angle_degrees = placement.angle_degrees + 180.0F;

  float const base_y = placement.position.y() + 0.12F;

  QVector3D const body_color(0.10F, 0.90F, 1.00F);
  QVector3D const glow_color =
      placement.accent_color.value_or(QVector3D(0.72F, 1.00F, 1.00F));

  float const fa = std::clamp(placement.fade_alpha, 0.0F, 1.0F);

  QMatrix4x4 base_xform;
  base_xform.translate(placement.position.x(), base_y, placement.position.z());
  base_xform.rotate(visual_angle_degrees, 0.0F, 1.0F, 0.0F);

  if (Mesh *const glow_mesh = Render::Geom::SelectionDisc::get()) {
    QMatrix4x4 glow_xform;
    glow_xform.translate(placement.position.x(),
                         placement.position.y() + 0.035F,
                         placement.position.z());
    glow_xform.rotate(visual_angle_degrees, 0.0F, 1.0F, 0.0F);
    glow_xform.translate(0.0F, 0.0F, -0.78F);
    glow_xform.scale(0.58F, 1.0F, 1.10F);
    renderer->mesh(glow_mesh, glow_xform, glow_color, nullptr, 0.26F * fa);
  }

  QMatrix4x4 arrow_xform = base_xform;
  arrow_xform.scale(1.0F, 0.34F, 1.0F);
  renderer->mesh(arrow_mesh, arrow_xform, body_color, nullptr, 0.88F * fa);
}

} // namespace Render::GL
