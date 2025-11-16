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

  // Steel with cooler blue-grey tint for heavy helmet (distinguished from light bronze)
  QVector3D const steel_color =
      saturate_color(palette.metal * QVector3D(0.88F, 0.92F, 1.08F));
  QVector3D const brass_accent =
      saturate_color(palette.metal * QVector3D(1.4F, 1.15F, 0.65F));
  
  float const helm_r = head_r * 1.18F;  // Slightly larger than light helmet

  // Main helmet bowl - shader will add detail
  QVector3D const helm_bot = headPoint(QVector3D(0.0F, 0.65F, 0.0F));
  QVector3D const helm_top = headPoint(QVector3D(0.0F, 1.42F, 0.0F));
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, helm_bot, helm_top, helm_r),
                 steel_color, nullptr, 1.0F, 2);  // materialId=2 (helmet)

  // Dome cap
  QVector3D const cap_top = headPoint(QVector3D(0.0F, 1.52F, 0.0F));
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, helm_top, cap_top, helm_r * 0.96F),
                 steel_color * 1.06F, nullptr, 1.0F, 2);  // materialId=2 (helmet)

  // Single brass brow band - shader will add rivets and detail
  QVector3D const brow_center = headPoint(QVector3D(0.0F, 0.12F, 0.0F));
  QVector3D const brow_top = brow_center + head.up * 0.035F;
  QVector3D const brow_bot = brow_center - head.up * 0.025F;
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, brow_bot, brow_top, helm_r * 1.10F),
                 brass_accent * 0.92F, nullptr, 1.0F, 2);  // materialId=2 (helmet)

  // Neck guard - shader adds segmented lamellae appearance
  QVector3D const neck_top = headPoint(QVector3D(0.0F, -0.12F, -1.08F));
  QVector3D const neck_bot = headPoint(QVector3D(0.0F, -0.35F, -1.02F));
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, neck_bot, neck_top, helm_r * 0.98F),
                 steel_color * 0.88F, nullptr, 1.0F, 2);  // materialId=2 (helmet)

  // Crest mount - minimal geometry, shader adds detail
  QVector3D const crest_base = cap_top;
  QVector3D const crest_mid = crest_base + head.up * 0.10F;
  QVector3D const crest_top = crest_mid + head.up * 0.18F;

  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, crest_base, crest_mid, 0.022F),
                 brass_accent, nullptr, 1.0F, 2);  // materialId=2 (helmet)

  // Red horsehair crest
  submitter.mesh(getUnitCone(),
                 coneFromTo(ctx.model, crest_mid, crest_top, 0.052F),
                 QVector3D(0.96F, 0.12F, 0.12F), nullptr, 1.0F, 0);  // materialId=0 (decoration)

  submitter.mesh(getUnitSphere(), sphereAt(ctx.model, crest_top, 0.024F),
                 brass_accent, nullptr, 1.0F, 2);  // materialId=2 (helmet)
}

} // namespace Render::GL
