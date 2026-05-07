#include "formation_arrow.h"
#include "../gl/primitives.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
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

  // The orientation arrow points toward –Z in mesh space.  Rotating around Y
  // by visual_angle_degrees then aligns it with the intended facing direction.
  float const visual_angle_degrees = placement.angle_degrees + 180.0F;

  // Slightly above the ground so the arrow sits on top of the terrain.
  float const base_y = placement.position.y() + 0.12F;

  // ── Colour palette ────────────────────────────────────────────────────────
  // Primary body – solid warm white with a cyan tint (clean, readable).
  QVector3D const body_color(0.12F, 0.85F, 0.98F);
  // Bright highlight drawn as a thin inner layer on top of the body.
  QVector3D const rim_color(0.55F, 1.00F, 1.00F);
  // Dark underlay slightly larger than the body to give contrast over light
  // terrain.
  QVector3D const shadow_color(0.02F, 0.18F, 0.28F);

  // Clamp fade so all opacity values stay in [0,1].
  float const fa = std::clamp(placement.fade_alpha, 0.0F, 1.0F);

  QMatrix4x4 base_xform;
  base_xform.translate(placement.position.x(), base_y, placement.position.z());
  base_xform.rotate(visual_angle_degrees, 0.0F, 1.0F, 0.0F);

  // ── Optional faction accent border ────────────────────────────────────────
  // Drawn first (lowest layer) as a slightly wider/taller version of the
  // arrow in the faction colour, providing a coloured halo effect.
  if (placement.accent_color.has_value()) {
    QMatrix4x4 accent_xform = base_xform;
    accent_xform.translate(0.0F, -0.04F, 0.0F);
    accent_xform.scale(1.22F, 0.80F, 1.12F);
    renderer->mesh(arrow_mesh, accent_xform, *placement.accent_color, nullptr,
                   0.70F * fa);
  }

  // ── Shadow underlay (drawn below body, scaled wider) ─────────────────────
  {
    QMatrix4x4 shadow_xform = base_xform;
    shadow_xform.translate(0.0F, -0.07F, 0.0F);
    shadow_xform.scale(1.14F, 0.55F, 1.06F);
    renderer->mesh(arrow_mesh, shadow_xform, shadow_color, nullptr, 0.50F * fa);
  }

  // ── Main body ─────────────────────────────────────────────────────────────
  renderer->mesh(arrow_mesh, base_xform, body_color, nullptr, 0.93F * fa);

  // ── Highlight rim (thin inner layer, bright) ──────────────────────────────
  {
    QMatrix4x4 rim_xform = base_xform;
    rim_xform.translate(0.0F, 0.05F, 0.0F);
    rim_xform.scale(0.72F, 0.30F, 0.72F);
    renderer->mesh(arrow_mesh, rim_xform, rim_color, nullptr, 0.75F * fa);
  }
}

} // namespace Render::GL
