#include "roman_light_helmet.h"
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

void RomanLightHelmetRenderer::render(const DrawContext &ctx,
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

  // Roman galea (auxiliary helmet) - bronze/brass colored
  QVector3D const helmet_color =
      saturate_color(palette.metal * QVector3D(1.08F, 0.98F, 0.78F));
  QVector3D const helmet_accent = helmet_color * 1.12F;

  // Main helmet bowl
  QVector3D const helmet_top = headPoint(QVector3D(0.0F, 1.28F, 0.0F));
  QVector3D const helmet_bot = headPoint(QVector3D(0.0F, 0.08F, 0.0F));
  float const helmet_r = head_r * 1.10F;

  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, helmet_bot, helmet_top, helmet_r),
                 helmet_color, nullptr, 1.0F);

  // Conical top (characteristic Roman galea feature)
  QVector3D const apex_pos = headPoint(QVector3D(0.0F, 1.48F, 0.0F));
  submitter.mesh(getUnitCone(),
                 coneFromTo(ctx.model, helmet_top, apex_pos, helmet_r * 0.97F),
                 helmet_accent, nullptr, 1.0F);

  // Decorative reinforcement rings
  auto ring = [&](float y_offset, float r_scale, float h,
                  const QVector3D &col) {
    QVector3D const center = headPoint(QVector3D(0.0F, y_offset, 0.0F));
    QVector3D const a = center + head.up * (h * 0.5F);
    QVector3D const b = center - head.up * (h * 0.5F);
    submitter.mesh(getUnitCylinder(),
                   cylinderBetween(ctx.model, a, b, helmet_r * r_scale), col,
                   nullptr, 1.0F);
  };

  ring(0.35F, 1.07F, 0.020F, helmet_accent);
  ring(0.65F, 1.03F, 0.015F, helmet_color * 1.05F);
  ring(0.95F, 1.01F, 0.012F, helmet_color * 1.03F);

  // Cheek guards (left and right)
  float const cheek_w = head_r * 0.48F;
  QVector3D const cheek_top = headPoint(QVector3D(0.0F, 0.22F, 0.0F));
  QVector3D const cheek_bot = headPoint(QVector3D(0.0F, -0.42F, 0.0F));

  // Left cheek guard
  QVector3D const cheek_ltop =
      cheek_top + head.right * (-cheek_w / head_r) + head.forward * 0.38F;
  QVector3D const cheek_lbot = cheek_bot +
                                head.right * (-cheek_w * 0.82F / head_r) +
                                head.forward * 0.28F;
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, cheek_lbot, cheek_ltop, 0.028F),
                 helmet_color * 0.96F, nullptr, 1.0F);

  // Right cheek guard
  QVector3D const cheek_rtop =
      cheek_top + head.right * (cheek_w / head_r) + head.forward * 0.38F;
  QVector3D const cheek_rbot =
      cheek_bot + head.right * (cheek_w * 0.82F / head_r) +
      head.forward * 0.28F;
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, cheek_rbot, cheek_rtop, 0.028F),
                 helmet_color * 0.96F, nullptr, 1.0F);

  // Neck guard (back protection)
  QVector3D const neck_guard_top = headPoint(QVector3D(0.0F, 0.03F, -0.82F));
  QVector3D const neck_guard_bot = headPoint(QVector3D(0.0F, -0.32F, -0.88F));
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, neck_guard_bot, neck_guard_top,
                                 helmet_r * 0.88F),
                 helmet_color * 0.93F, nullptr, 1.0F);

  // Crest/plume for identification
  QVector3D const crest_base = apex_pos;
  QVector3D const crest_mid = crest_base + head.up * 0.09F;
  QVector3D const crest_top = crest_mid + head.up * 0.12F;

  // Crest holder (metal)
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, crest_base, crest_mid, 0.018F),
                 helmet_accent, nullptr, 1.0F);

  // Plume (red/crimson - traditional Roman color)
  submitter.mesh(getUnitCone(),
                 coneFromTo(ctx.model, crest_mid, crest_top, 0.042F),
                 QVector3D(0.88F, 0.18F, 0.18F), nullptr, 1.0F);

  // Plume ornament (top ball)
  submitter.mesh(getUnitSphere(), sphereAt(ctx.model, crest_top, 0.020F),
                 helmet_accent, nullptr, 1.0F);
}

} // namespace Render::GL
