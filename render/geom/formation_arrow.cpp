#include "formation_arrow.h"

#include <QMatrix4x4>
#include <qvectornd.h>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "render/gl/primitives.h"
#include "render/gl/resources.h"
#include "render/scene_renderer.h"
#include "selection_disc.h"

namespace Render::GL {

void render_formation_arrow(Renderer* renderer,
                            ResourceManager* resources,
                            const FormationPlacementInfo& placement) {
  if ((renderer == nullptr) || (resources == nullptr) || !placement.active) {
    return;
  }
  if (placement.fade_alpha <= 0.0F) {
    return;
  }

  Mesh* const arrow_mesh = get_orientation_arrow();
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

  if (Mesh* const glow_mesh = Render::Geom::SelectionDisc::get()) {
    QMatrix4x4 glow_xform;
    glow_xform.translate(placement.position.x(),
                         placement.position.y() + 0.035F,
                         placement.position.z());
    glow_xform.rotate(visual_angle_degrees, 0.0F, 1.0F, 0.0F);
    glow_xform.translate(0.0F, 0.0F, -0.78F);
    glow_xform.scale(0.58F, 1.0F, 1.10F);
    renderer->mesh(glow_mesh, glow_xform, glow_color, nullptr, 0.26F * fa);
  }

  constexpr float k_arrow_length = 1.56F;
  constexpr float k_aim_arrow_scale = 1.35F;
  float const aim = std::max(0.0F, placement.aim_distance);
  bool const aiming = aim > k_arrow_length;
  float const arrow_scale = aiming ? k_aim_arrow_scale : 1.0F;
  float const reach =
      aiming ? std::max(aim - k_arrow_length * arrow_scale, 0.0F) : 0.0F;

  if (aiming && reach > 0.0F) {
    if (Mesh* const beam_mesh = get_unit_cube()) {
      QVector3D const beam_color(0.55F, 0.96F, 1.00F);
      QMatrix4x4 beam_xform = base_xform;
      beam_xform.translate(0.0F, 0.0F, -reach * 0.5F);
      beam_xform.scale(0.08F, 0.03F, reach * 0.5F);
      renderer->mesh(beam_mesh, beam_xform, beam_color, nullptr, 0.9F * fa);
    }
    if (Mesh* const pivot_mesh = Render::Geom::SelectionDisc::get()) {
      QMatrix4x4 pivot_xform;
      pivot_xform.translate(placement.position.x(),
                            placement.position.y() + 0.045F,
                            placement.position.z());
      pivot_xform.scale(0.42F, 1.0F, 0.42F);
      renderer->mesh(pivot_mesh, pivot_xform, body_color, nullptr, 0.55F * fa);
    }
  }

  QMatrix4x4 arrow_xform = base_xform;
  arrow_xform.translate(0.0F, 0.0F, -reach);
  arrow_xform.scale(arrow_scale, 0.34F, arrow_scale);
  renderer->mesh(arrow_mesh, arrow_xform, body_color, nullptr, 0.88F * fa);
}

void render_formation_slot_preview(Renderer* renderer,
                                   ResourceManager* resources,
                                   const FormationPlacementInfo& placement) {
  if ((renderer == nullptr) || (resources == nullptr) || !placement.active) {
    return;
  }
  if (placement.fade_alpha <= 0.0F || placement.slot_markers.empty()) {
    return;
  }

  Mesh* const disc = Render::Geom::SelectionDisc::get();
  if (disc == nullptr) {
    return;
  }

  float const fa = std::clamp(placement.fade_alpha, 0.0F, 1.0F);
  QVector3D const valid_color =
      placement.accent_color.value_or(QVector3D(0.72F, 1.00F, 1.00F));
  QVector3D const adjusted_color(1.00F, 0.78F, 0.24F);
  QVector3D const blocked_color(0.95F, 0.22F, 0.20F);

  Mesh* const arrow_mesh = get_orientation_arrow();

  for (const auto& marker : placement.slot_markers) {
    QVector3D const color = marker.blocked    ? blocked_color
                            : marker.adjusted ? adjusted_color
                                              : valid_color;
    float const alpha = (marker.blocked ? 0.55F : 0.34F) * fa;

    QMatrix4x4 xform;
    xform.translate(
        marker.position.x(), marker.position.y() + 0.03F, marker.position.z());
    xform.rotate(marker.facing_degrees + 180.0F, 0.0F, 1.0F, 0.0F);
    xform.scale(marker.radius, 1.0F, marker.radius);
    renderer->mesh(disc, xform, color, nullptr, alpha);

    if (arrow_mesh == nullptr || marker.blocked) {
      continue;
    }
    QMatrix4x4 facing_xform;
    facing_xform.translate(
        marker.position.x(), marker.position.y() + 0.06F, marker.position.z());
    facing_xform.rotate(marker.facing_degrees + 180.0F, 0.0F, 1.0F, 0.0F);
    facing_xform.scale(marker.radius * 0.45F, 0.18F, marker.radius * 0.45F);
    renderer->mesh(arrow_mesh, facing_xform, color, nullptr, 0.5F * fa);
  }
}

} // namespace Render::GL
