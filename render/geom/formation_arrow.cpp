#include "formation_arrow.h"
#include "../gl/primitives.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include <QMatrix4x4>
#include <cmath>
#include <numbers>
#include <qvectornd.h>

namespace Render::GL {

void render_formation_arrow(Renderer *renderer, ResourceManager *resources,
                            const FormationPlacementInfo &placement) {
  if ((renderer == nullptr) || (resources == nullptr) || !placement.active) {
    return;
  }

  Mesh *const arrow_mesh = get_orientation_arrow();
  if (arrow_mesh == nullptr) {
    return;
  }

  // The orientation arrow points toward –Z in mesh space.  Rotating around Y
  // by visual_angle_degrees then aligns it with the intended facing direction.
  float const visual_angle_degrees = placement.angle_degrees + 180.0F;

  // Slightly above the ground so the arrow sits on top of the terrain.
  float const base_y = placement.position.y() + 0.10F;

  // Primary body – bright cyan with a slight blue tint.
  QVector3D const body_color(0.10F, 0.80F, 0.95F);
  // Bright highlight rim drawn on top of the body.
  QVector3D const rim_color(0.30F, 1.00F, 1.00F);
  // Subtle dark underlay slightly larger than the body to give a soft shadow.
  QVector3D const shadow_color(0.02F, 0.20F, 0.30F);

  QMatrix4x4 base_xform;
  base_xform.translate(placement.position.x(), base_y, placement.position.z());
  base_xform.rotate(visual_angle_degrees, 0.0F, 1.0F, 0.0F);

  // ── Shadow underlay (drawn first, slightly below and scaled wider) ─────────
  {
    QMatrix4x4 shadow_xform = base_xform;
    shadow_xform.translate(0.0F, -0.06F, 0.0F);
    shadow_xform.scale(1.18F, 0.6F, 1.08F);
    renderer->mesh(arrow_mesh, shadow_xform, shadow_color, nullptr, 0.55F);
  }

  // ── Main body ─────────────────────────────────────────────────────────────
  renderer->mesh(arrow_mesh, base_xform, body_color, nullptr, 0.92F);

  // ── Highlight rim (very thin layer, full opacity, bright) ─────────────────
  {
    QMatrix4x4 rim_xform = base_xform;
    rim_xform.translate(0.0F, 0.04F, 0.0F);
    rim_xform.scale(0.78F, 0.35F, 0.78F);
    renderer->mesh(arrow_mesh, rim_xform, rim_color, nullptr, 0.80F);
  }
}

} // namespace Render::GL
