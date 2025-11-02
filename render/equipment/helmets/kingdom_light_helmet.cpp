#include "kingdom_light_helmet.h"
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

void KingdomLightHelmetRenderer::render(const DrawContext &ctx,
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

  // Kettle hat steel colors
  QVector3D const steel_color =
      saturate_color(palette.metal * QVector3D(0.88F, 0.90F, 0.95F));
  QVector3D const steel_dark = steel_color * 0.82F;

  float const bowl_scale = 1.06F;
  float const bowl_r = head_r * bowl_scale;

  // Main bowl
  QVector3D const bowl_top = headPoint(QVector3D(0.0F, 1.10F, 0.0F));
  QVector3D const bowl_bot = headPoint(QVector3D(0.0F, 0.15F, 0.0F));

  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, bowl_bot, bowl_top, bowl_r),
                 steel_color, nullptr, 1.0F);

  // Rounded top cap
  QMatrix4x4 cap_m = ctx.model;
  cap_m.translate(bowl_top);
  cap_m.scale(bowl_r * 0.92F, head_r * 0.28F, bowl_r * 0.92F);
  submitter.mesh(getUnitSphere(), cap_m, steel_color * 1.05F, nullptr, 1.0F);

  // Wide brim (characteristic kettle hat feature)
  QVector3D const brim_top = headPoint(QVector3D(0.0F, 0.18F, 0.0F));
  QVector3D const brim_bot = headPoint(QVector3D(0.0F, 0.08F, 0.0F));
  float const brim_r = head_r * 1.42F; // Wide brim for protection

  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, brim_bot, brim_top, brim_r),
                 steel_dark, nullptr, 1.0F);

  // Reinforcement rings on bowl
  auto ring = [&](float y_offset, float radius_scale, const QVector3D &col) {
    QVector3D const center = headPoint(QVector3D(0.0F, y_offset, 0.0F));
    float const height = head_r * 0.010F;
    QVector3D const a = center + head.up * (height * 0.5F);
    QVector3D const b = center - head.up * (height * 0.5F);
    submitter.mesh(getUnitCylinder(),
                   cylinderBetween(ctx.model, a, b, bowl_r * radius_scale),
                   col, nullptr, 1.0F);
  };

  ring(0.90F, 1.01F, steel_color * 1.05F);
  ring(0.50F, 1.01F, steel_color * 1.05F);
  ring(0.20F, 1.01F, steel_color * 1.05F);

  // Brim edge reinforcement
  QVector3D const brim_edge_top = headPoint(QVector3D(0.0F, 0.09F, 0.0F));
  QVector3D const brim_edge_bot = headPoint(QVector3D(0.0F, 0.07F, 0.0F));
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, brim_edge_bot, brim_edge_top,
                                 brim_r * 1.01F),
                 steel_color * 1.08F, nullptr, 1.0F);
}

} // namespace Render::GL
