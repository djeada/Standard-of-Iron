#include "headwrap.h"
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

void HeadwrapRenderer::render(const DrawContext &ctx, const BodyFrames &frames,
                              const HumanoidPalette &palette,
                              const HumanoidAnimationContext &anim,
                              ISubmitter &submitter) {
  (void)anim;

  QVector3D const cloth_color =
      saturate_color(palette.cloth * QVector3D(0.9F, 1.05F, 1.05F));
  const AttachmentFrame &head = frames.head;
  float const head_r = head.radius;
  if (head_r <= 0.0F) {
    return;
  }

  auto headPoint = [&](const QVector3D &normalized) -> QVector3D {
    return HumanoidRendererBase::frameLocalPosition(head, normalized);
  };

  QVector3D const band_top = headPoint(QVector3D(0.0F, 0.70F, 0.0F));
  QVector3D const band_bot = headPoint(QVector3D(0.0F, 0.30F, 0.0F));
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, band_bot, band_top, head_r * 1.08F),
                 cloth_color, nullptr, 1.0F);

  QVector3D const knot_center = headPoint(QVector3D(0.10F, 0.60F, 0.72F));
  QMatrix4x4 knot_m = ctx.model;
  knot_m.translate(knot_center);
  knot_m.scale(head_r * 0.32F);
  submitter.mesh(getUnitSphere(), knot_m, cloth_color * 1.05F, nullptr, 1.0F);

  QVector3D const tail_top = knot_center + head.right * (-0.08F) +
                             head.up * (-0.05F) + head.forward * (-0.06F);
  QVector3D const tail_bot = tail_top + head.right * 0.02F +
                             head.up * (-0.28F) + head.forward * (-0.08F);
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, tail_top, tail_bot, head_r * 0.28F),
                 cloth_color * QVector3D(0.92F, 0.98F, 1.05F), nullptr, 1.0F);
}

} // namespace Render::GL
