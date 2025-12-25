#include "roman_armor.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../humanoid/rig.h"
#include "../../humanoid/style_palette.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>
#include <numbers>

namespace Render::GL {

using Render::Geom::cone_from_to;
using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;
using Render::GL::Humanoid::saturate_color;

void RomanHeavyArmorRenderer::render(const DrawContext &ctx,
                                     const BodyFrames &frames,
                                     const HumanoidPalette &palette,
                                     const HumanoidAnimationContext &anim,
                                     ISubmitter &submitter) {
  (void)anim;

  const AttachmentFrame &torso = frames.torso;
  const AttachmentFrame &waist = frames.waist;
  const AttachmentFrame &head = frames.head;

  if (torso.radius <= 0.0F) {
    return;
  }

  using HP = HumanProportions;

  QVector3D const steel_color =
      saturate_color(palette.metal * QVector3D(0.88F, 0.92F, 1.08F));
  QVector3D const brass_color =
      saturate_color(palette.metal * QVector3D(1.25F, 1.05F, 0.62F));
  QVector3D const leather_color =
      saturate_color(palette.leather * QVector3D(0.58F, 0.38F, 0.26F));

  auto safeNormal = [](const QVector3D &v, const QVector3D &fallback) {
    return (v.lengthSquared() > 1e-6F) ? v.normalized() : fallback;
  };

  QVector3D up = safeNormal(torso.up, QVector3D(0.0F, 1.0F, 0.0F));
  QVector3D right = safeNormal(torso.right, QVector3D(1.0F, 0.0F, 0.0F));
  QVector3D forward = safeNormal(torso.forward, QVector3D(0.0F, 0.0F, 1.0F));
  QVector3D waist_up = safeNormal(waist.up, up);
  QVector3D head_up = safeNormal(head.up, up);

  float const torso_r = torso.radius;
  float const torso_depth =
      (torso.depth > 0.0F) ? torso.depth : torso_r * 0.75F;
  auto depth_scale_for = [&](float base) {
    float const ratio = torso_depth / std::max(0.001F, torso_r);
    return std::max(0.08F, base * ratio);
  };
  float const waist_r =
      waist.radius > 0.0F ? waist.radius : torso.radius * 0.88F;
  float const head_r = head.radius > 0.0F ? head.radius : torso.radius * 0.58F;

  QVector3D top = torso.origin + up * (torso_r * 0.60F);
  QVector3D head_guard = head.origin - head_up * (head_r * 1.30F);
  if (QVector3D::dotProduct(top - head_guard, up) > 0.0F) {
    top = head_guard - up * (torso_r * 0.05F);
  }

  QVector3D bottom = waist.origin - waist_up * (waist_r * 0.45F);

  top += forward * (torso_r * 0.010F);
  bottom += forward * (torso_r * 0.010F);

  QMatrix4x4 plates = cylinder_between(ctx.model, top, bottom, torso_r * 1.24F);
  plates.scale(1.18F, 1.0F, depth_scale_for(1.10F));
  Mesh *torso_mesh = torso_mesh_without_bottom_cap();
  submitter.mesh(torso_mesh != nullptr ? torso_mesh : get_unit_torso(), plates,
                 steel_color, nullptr, 1.0F, 1);

  auto renderShoulderGuard = [&](const QVector3D &shoulder_pos,
                                 const QVector3D &outward) {
    QVector3D upper_pos = shoulder_pos + outward * 0.03F + forward * 0.01F;
    QMatrix4x4 upper = ctx.model;
    upper.translate(upper_pos);
    upper.scale(HP::UPPER_ARM_R * 1.90F, HP::UPPER_ARM_R * 0.42F,
                HP::UPPER_ARM_R * 1.65F);
    submitter.mesh(get_unit_sphere(), upper, steel_color * 0.98F, nullptr, 1.0F,
                   1);

    QVector3D lower_pos = upper_pos - up * 0.06F + outward * 0.02F;
    QMatrix4x4 lower = ctx.model;
    lower.translate(lower_pos);
    lower.scale(HP::UPPER_ARM_R * 1.68F, HP::UPPER_ARM_R * 0.38F,
                HP::UPPER_ARM_R * 1.48F);
    submitter.mesh(get_unit_sphere(), lower, steel_color * 0.94F, nullptr, 1.0F,
                   1);

    QMatrix4x4 rivet = ctx.model;
    rivet.translate(upper_pos + forward * 0.04F);
    rivet.scale(0.012F);
    submitter.mesh(get_unit_sphere(), rivet, brass_color, nullptr, 1.0F);
  };

  renderShoulderGuard(frames.shoulder_l.origin, -right);
  renderShoulderGuard(frames.shoulder_r.origin, right);
}

void RomanLightArmorRenderer::render(const DrawContext &ctx,
                                     const BodyFrames &frames,
                                     const HumanoidPalette &palette,
                                     const HumanoidAnimationContext &anim,
                                     ISubmitter &submitter) {
  (void)anim;

  (void)palette;

  const AttachmentFrame &torso = frames.torso;
  const AttachmentFrame &waist = frames.waist;
  const AttachmentFrame &head = frames.head;

  if (torso.radius <= 0.0F) {
    return;
  }

  QVector3D leather_color = QVector3D(0.44F, 0.30F, 0.19F);
  QVector3D leather_shadow = leather_color * 0.90F;
  QVector3D leather_highlight = leather_color * 1.08F;

  QVector3D up = torso.up.normalized();
  QVector3D right = torso.right.normalized();
  QVector3D forward = torso.forward.normalized();

  float const torso_r = torso.radius;
  float const torso_depth =
      (torso.depth > 0.0F) ? torso.depth : torso_r * 0.75F;
  float const waist_r =
      waist.radius > 0.0F ? waist.radius : torso.radius * 0.85F;
  float const head_r = head.radius > 0.0F ? head.radius : torso.radius * 0.6F;

  QVector3D head_up =
      (head.up.lengthSquared() > 1e-6F) ? head.up.normalized() : up;
  QVector3D waist_up =
      (waist.up.lengthSquared() > 1e-6F) ? waist.up.normalized() : up;

  QVector3D top = torso.origin + up * (torso_r * 0.50F);
  QVector3D head_guard =
      head.origin -
      head_up * ((head_r > 0.0F ? head_r : torso_r * 0.6F) * 1.45F);
  if (QVector3D::dotProduct(top - head_guard, up) > 0.0F) {
    top = head_guard - up * (torso_r * 0.05F);
  }

  QVector3D bottom = waist.origin - waist_up * (waist_r * 0.24F);

  top += forward * (torso_r * 0.010F);
  bottom += forward * (torso_r * 0.010F);

  float main_radius = torso_r * 1.26F;
  float const main_depth = torso_depth * 1.24F;

  QMatrix4x4 cuirass = cylinder_between(ctx.model, top, bottom, main_radius);
  cuirass.scale(1.0F, 1.0F, std::max(0.15F, main_depth / main_radius));
  Mesh *torso_mesh = torso_mesh_without_bottom_cap();
  submitter.mesh(torso_mesh != nullptr ? torso_mesh : get_unit_torso(), cuirass,
                 leather_highlight, nullptr, 1.0F, 1);

  auto strap = [&](float side) {
    QVector3D shoulder_anchor =
        top + right * (torso_r * 0.54F * side) - up * (torso_r * 0.04F);
    QVector3D chest_anchor =
        shoulder_anchor - up * (torso_r * 0.82F) + forward * (torso_r * 0.22F);
    submitter.mesh(get_unit_cylinder(),
                   cylinder_between(ctx.model, shoulder_anchor, chest_anchor,
                                    torso_r * 0.10F),
                   leather_highlight * 0.95F, nullptr, 1.0F, 1);
  };
  strap(1.0F);
  strap(-1.0F);

  QVector3D front_panel_top =
      top + forward * (torso_depth * 0.35F) - up * (torso_r * 0.06F);
  QVector3D front_panel_bottom =
      bottom + forward * (torso_depth * 0.38F) + up * (torso_r * 0.03F);
  QMatrix4x4 front_panel = cylinder_between(
      ctx.model, front_panel_top, front_panel_bottom, torso_r * 0.48F);
  front_panel.scale(1.18F, 1.0F,
                    std::max(0.22F, (torso_depth * 0.76F) / (torso_r * 0.76F)));
  submitter.mesh(torso_mesh != nullptr ? torso_mesh : get_unit_torso(),
                 front_panel, leather_highlight, nullptr, 1.0F, 1);

  QVector3D back_panel_top =
      top - forward * (torso_depth * 0.32F) - up * (torso_r * 0.05F);
  QVector3D back_panel_bottom =
      bottom - forward * (torso_depth * 0.34F) + up * (torso_r * 0.02F);
  QMatrix4x4 back_panel = cylinder_between(ctx.model, back_panel_top,
                                           back_panel_bottom, torso_r * 0.50F);
  back_panel.scale(1.18F, 1.0F,
                   std::max(0.22F, (torso_depth * 0.74F) / (torso_r * 0.80F)));
  submitter.mesh(torso_mesh != nullptr ? torso_mesh : get_unit_torso(),
                 back_panel, leather_shadow, nullptr, 1.0F, 1);
}

} 
