#include "kingdom_heavy_helmet.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/rig.h"
#include "../../humanoid/style_palette.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>

namespace Render::GL {

using Render::Geom::cylinderBetween;
using Render::Geom::sphereAt;
using Render::GL::Humanoid::saturate_color;

void KingdomHeavyHelmetRenderer::render(const DrawContext &ctx,
                                        const BodyFrames &frames,
                                        const HumanoidPalette &palette,
                                        const HumanoidAnimationContext &anim,
                                        ISubmitter &submitter) {
  (void)anim; // Unused

  const AttachmentFrame &head = frames.head;
  float const head_r = head.radius;
  if (head_r <= 0.0F) {
    return;
  }

  auto headPoint = [&](const QVector3D &normalized) -> QVector3D {
    return HumanoidRendererBase::frameLocalPosition(head, normalized);
  };

  // Great helm steel colors
  QVector3D const steel_color =
      saturate_color(palette.metal * QVector3D(0.95F, 0.96F, 1.0F));
  QVector3D const brass_color =
      saturate_color(palette.metal * QVector3D(1.3F, 1.1F, 0.7F));
  QVector3D const visor_color(0.1F, 0.1F, 0.1F);

  float const helm_r = head_r * 1.15F;
  float const helm_ratio = helm_r / head_r;

  // Main enclosed helm body
  QVector3D const helm_bot = headPoint(QVector3D(0.0F, -0.20F, 0.0F));
  QVector3D const helm_top = headPoint(QVector3D(0.0F, 1.40F, 0.0F));

  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, helm_bot, helm_top, helm_r),
                 steel_color, nullptr, 1.0F);

  // Flat top (characteristic of great helm)
  QVector3D const cap_top = headPoint(QVector3D(0.0F, 1.48F, 0.0F));
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, helm_top, cap_top, helm_r * 0.98F),
                 steel_color * 1.05F, nullptr, 1.0F);

  // Decorative reinforcement bands
  auto ring = [&](float y_offset, float radius_scale, const QVector3D &col) {
    QVector3D const center = headPoint(QVector3D(0.0F, y_offset, 0.0F));
    float const height = head_r * 0.015F;
    QVector3D const a = center + head.up * (height * 0.5F);
    QVector3D const b = center - head.up * (height * 0.5F);
    submitter.mesh(getUnitCylinder(),
                   cylinderBetween(ctx.model, a, b, helm_r * radius_scale),
                   col, nullptr, 1.0F);
  };

  ring(1.25F, 1.02F, steel_color * 1.08F);
  ring(0.50F, 1.02F, steel_color * 1.08F);
  ring(-0.05F, 1.02F, steel_color * 1.08F);

  // Eye slit (horizontal bar)
  float const visor_y = 0.15F;
  float const visor_forward = helm_r * 0.72F;
  float const visor_forward_norm = visor_forward / head_r;
  QVector3D const visor_center =
      headPoint(QVector3D(0.0F, visor_y, visor_forward_norm));

  QVector3D const visor_hl = visor_center - head.right * (helm_r * 0.35F);
  QVector3D const visor_hr = visor_center + head.right * (helm_r * 0.35F);
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, visor_hl, visor_hr,
                                 head_r * 0.012F),
                 visor_color, nullptr, 1.0F);

  // Vertical nose guard
  QVector3D const visor_vt = visor_center + head.up * (helm_r * 0.25F);
  QVector3D const visor_vb = visor_center - head.up * (helm_r * 0.25F);
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, visor_vb, visor_vt,
                                 head_r * 0.012F),
                 visor_color, nullptr, 1.0F);

  // Breathing holes (arranged in cross pattern)
  auto draw_breathing_hole = [&](float x_norm, float y_norm) {
    QVector3D const pos = headPoint(
        QVector3D(x_norm * helm_ratio, y_norm, visor_forward_norm * 0.97F));
    QMatrix4x4 m = ctx.model;
    m.translate(pos);
    m.scale(0.010F);
    submitter.mesh(getUnitSphere(), m, visor_color, nullptr, 1.0F);
  };

  for (int i = 0; i < 4; ++i) {
    draw_breathing_hole(+0.50F, 0.05F - i * 0.10F);
    draw_breathing_hole(-0.50F, 0.05F - i * 0.10F);
  }

  // Heraldic cross on top (brass)
  QVector3D const top_center = headPoint(QVector3D(0.0F, 1.45F, 0.0F));

  QVector3D const cross_h1 = top_center - head.right * 0.05F;
  QVector3D const cross_h2 = top_center + head.right * 0.05F;
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, cross_h1, cross_h2,
                                 head_r * 0.010F),
                 brass_color, nullptr, 1.0F);

  QVector3D const cross_v1 = top_center - head.forward * 0.05F;
  QVector3D const cross_v2 = top_center + head.forward * 0.05F;
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, cross_v1, cross_v2,
                                 head_r * 0.010F),
                 brass_color, nullptr, 1.0F);

  // Face plate edge (slightly forward from main helm)
  float const face_forward = helm_r * 0.68F;
  QVector3D const face_top =
      headPoint(QVector3D(0.0F, 0.40F, face_forward / head_r));
  QVector3D const face_bot =
      headPoint(QVector3D(0.0F, -0.15F, face_forward / head_r));
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, face_bot, face_top,
                                 head_r * 0.015F),
                 steel_color * 1.08F, nullptr, 1.0F);
}

} // namespace Render::GL
