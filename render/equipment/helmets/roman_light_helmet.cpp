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

using Render::Geom::cylinderBetween;
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

  // Simple iron cap helmet
  const QVector3D iron_color =
      saturate_color(palette.metal * QVector3D(0.88F, 0.90F, 0.95F));
  const QVector3D iron_dark = iron_color * 0.82F;
  const float helm_r = head_r * 1.12F;

  // Main bowl
  QVector3D const helm_bot = headPoint(QVector3D(0.0F, -0.15F, 0.0F));
  QVector3D const helm_top = headPoint(QVector3D(0.0F, 1.25F, 0.0F));

  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, helm_bot, helm_top, helm_r),
                 iron_color, nullptr, 1.0F);

  // Top cap
  QVector3D const cap_top = headPoint(QVector3D(0.0F, 1.32F, 0.0F));
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, helm_top, cap_top, helm_r * 0.96F),
                 iron_color * 1.04F, nullptr, 1.0F);

  // Reinforcement rings
  auto ring = [&](float y_offset, const QVector3D &col) {
    QVector3D const center = headPoint(QVector3D(0.0F, y_offset, 0.0F));
    float const height = head_r * 0.015F;
    QVector3D const a = center + head.up * (height * 0.5F);
    QVector3D const b = center - head.up * (height * 0.5F);
    submitter.mesh(getUnitCylinder(),
                   cylinderBetween(ctx.model, a, b, helm_r * 1.02F), col,
                   nullptr, 1.0F);
  };

  ring(0.95F, iron_color * 1.06F);
  ring(-0.02F, iron_color * 1.06F);

  // Simple cheek guards (minimal)
  float const cheek_forward = helm_r * 0.48F;
  for (int side = -1; side <= 1; side += 2) {
    QVector3D const cheek_top =
        headPoint(QVector3D(side * 0.90F, 0.20F, cheek_forward / head_r));
    QVector3D const cheek_bot =
        headPoint(QVector3D(side * 0.85F, -0.10F, cheek_forward / head_r));
    submitter.mesh(getUnitCylinder(),
                   cylinderBetween(ctx.model, cheek_bot, cheek_top,
                                   head_r * 0.08F),
                   iron_dark, nullptr, 1.0F);
  }
}

} // namespace Render::GL
