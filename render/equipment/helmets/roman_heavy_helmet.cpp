#include "roman_heavy_helmet.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/rig.h"
#include "../../humanoid/style_palette.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>

namespace Render::GL {

using Render::Geom::coneFromTo;
using Render::Geom::cylinderBetween;
using Render::Geom::sphereAt;
using Render::GL::Humanoid::saturate_color;

void RomanHeavyHelmetRenderer::render(const DrawContext &ctx,
                                      const BodyFrames &frames,
                                      const HumanoidPalette &palette,
                                      const HumanoidAnimationContext &anim,
                                      ISubmitter &submitter) {
  (void)anim;

  const AttachmentFrame &head = frames.head;
  float const head_r = head.radius;
  if (head_r <= 0.0F) {
    return;
  }

  auto headPoint = [&](const QVector3D &normalized) -> QVector3D {
    return HumanoidRendererBase::frameLocalPosition(head, normalized);
  };

  QVector3D const steel_color =
      saturate_color(palette.metal * QVector3D(0.95F, 0.96F, 1.0F));
  QVector3D const brass_color =
      saturate_color(palette.metal * QVector3D(1.3F, 1.1F, 0.7F));
  QVector3D const visor_color(0.1F, 0.1F, 0.1F);

  float const helm_r = head_r * 1.15F;
  float const helm_ratio = helm_r / head_r;

  QVector3D const helm_bot = headPoint(QVector3D(0.0F, -0.20F, 0.0F));
  QVector3D const helm_top = headPoint(QVector3D(0.0F, 1.40F, 0.0F));

  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, helm_bot, helm_top, helm_r),
                 steel_color, nullptr, 1.0F);

  QVector3D const cap_top = headPoint(QVector3D(0.0F, 1.48F, 0.0F));
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, helm_top, cap_top, helm_r * 0.98F),
                 steel_color * 1.05F, nullptr, 1.0F);

  auto ring = [&](float y_offset, const QVector3D &col) {
    QVector3D const center = headPoint(QVector3D(0.0F, y_offset, 0.0F));
    float const height = head_r * 0.015F;
    QVector3D const a = center + head.up * (height * 0.5F);
    QVector3D const b = center - head.up * (height * 0.5F);
    submitter.mesh(getUnitCylinder(),
                   cylinderBetween(ctx.model, a, b, helm_r * 1.02F), col,
                   nullptr, 1.0F);
  };

  ring(1.25F, steel_color * 1.08F);
  ring(0.50F, steel_color * 1.08F);
  ring(-0.05F, steel_color * 1.08F);

  QVector3D const brow_center = headPoint(QVector3D(0.0F, 0.15F, 0.0F));
  QVector3D const brow_top = brow_center + head.up * 0.03F;
  QVector3D const brow_bot = brow_center - head.up * 0.02F;
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, brow_bot, brow_top, helm_r * 1.08F),
                 brass_color * 0.95F, nullptr, 1.0F);

  float const visor_y = 0.15F;
  float const visor_forward = helm_r * 0.72F;
  float const visor_forward_norm = visor_forward / head_r;
  QVector3D const visor_center =
      headPoint(QVector3D(0.0F, visor_y, visor_forward_norm));

  QVector3D const visor_hl = visor_center - head.right * (helm_r * 0.35F);
  QVector3D const visor_hr = visor_center + head.right * (helm_r * 0.35F);
  submitter.mesh(
      getUnitCylinder(),
      cylinderBetween(ctx.model, visor_hl, visor_hr, head_r * 0.012F),
      visor_color, nullptr, 1.0F);

  QVector3D const visor_vt = visor_center + head.up * (helm_r * 0.25F);
  QVector3D const visor_vb = visor_center - head.up * (helm_r * 0.25F);
  submitter.mesh(
      getUnitCylinder(),
      cylinderBetween(ctx.model, visor_vb, visor_vt, head_r * 0.012F),
      visor_color, nullptr, 1.0F);

  auto draw_breathing_hole = [&](float x_sign, float y_offset) {
    QVector3D const pos = headPoint(QVector3D(
        x_sign * 0.50F * helm_ratio, y_offset, visor_forward_norm * 0.97F));
    QMatrix4x4 m = ctx.model;
    m.translate(pos);
    m.scale(0.010F);
    submitter.mesh(getUnitSphere(), m, visor_color, nullptr, 1.0F);
  };

  for (int i = 0; i < 4; ++i) {
    draw_breathing_hole(1.0F, 0.05F - i * 0.10F);
    draw_breathing_hole(-1.0F, 0.05F - i * 0.10F);
  }

  QVector3D const cross_center =
      headPoint(QVector3D(0.0F, 0.60F, (helm_r * 0.75F) / head_r));

  QVector3D const cross_h1 = cross_center - head.right * 0.04F;
  QVector3D const cross_h2 = cross_center + head.right * 0.04F;
  submitter.mesh(
      getUnitCylinder(),
      cylinderBetween(ctx.model, cross_h1, cross_h2, head_r * 0.008F),
      brass_color, nullptr, 1.0F);

  QVector3D const cross_v1 = cross_center - head.up * 0.04F;
  QVector3D const cross_v2 = cross_center + head.up * 0.04F;
  submitter.mesh(
      getUnitCylinder(),
      cylinderBetween(ctx.model, cross_v1, cross_v2, head_r * 0.008F),
      brass_color, nullptr, 1.0F);

  float const cheek_w = head_r * 0.52F;
  QVector3D const cheek_top = headPoint(QVector3D(0.0F, 0.18F, 0.0F));
  QVector3D const cheek_bot = headPoint(QVector3D(0.0F, -0.45F, 0.0F));

  QVector3D const cheek_ltop =
      cheek_top + head.right * (-cheek_w / head_r) + head.forward * 0.42F;
  QVector3D const cheek_lbot = cheek_bot +
                               head.right * (-cheek_w * 0.85F / head_r) +
                               head.forward * 0.32F;
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, cheek_lbot, cheek_ltop, 0.032F),
                 steel_color * 0.94F, nullptr, 1.0F);

  QVector3D const cheek_rtop =
      cheek_top + head.right * (cheek_w / head_r) + head.forward * 0.42F;
  QVector3D const cheek_rbot = cheek_bot +
                               head.right * (cheek_w * 0.85F / head_r) +
                               head.forward * 0.32F;
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, cheek_rbot, cheek_rtop, 0.032F),
                 steel_color * 0.94F, nullptr, 1.0F);

  QVector3D const neck_top = headPoint(QVector3D(0.0F, -0.10F, -1.02F));
  QVector3D const neck_bot = headPoint(QVector3D(0.0F, -0.28F, -0.95F));
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, neck_bot, neck_top, helm_r * 0.95F),
                 steel_color * 0.92F, nullptr, 1.0F);

  QVector3D const crest_base = cap_top;
  QVector3D const crest_mid = crest_base + head.up * 0.08F;
  QVector3D const crest_top = crest_mid + head.up * 0.15F;

  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, crest_base, crest_mid, 0.020F),
                 brass_color, nullptr, 1.0F);

  submitter.mesh(getUnitCone(),
                 coneFromTo(ctx.model, crest_mid, crest_top, 0.048F),
                 QVector3D(0.92F, 0.15F, 0.15F), nullptr, 1.0F);

  submitter.mesh(getUnitSphere(), sphereAt(ctx.model, crest_top, 0.022F),
                 brass_color, nullptr, 1.0F);
}

} // namespace Render::GL
