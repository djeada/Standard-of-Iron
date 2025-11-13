#include "light_cavalry_saddle_renderer.h"

#include "../../../entity/registry.h"
#include "../../../gl/primitives.h"
#include <QMatrix4x4>

namespace Render::GL {

void LightCavalrySaddleRenderer::render(const DrawContext &ctx,
                                        const HorseBodyFrames &frames,
                                        const HorseVariant &variant,
                                        const HorseAnimationContext & /*anim*/,
                                        ISubmitter &out) const {

  const HorseAttachmentFrame &back = frames.back_center;

  QMatrix4x4 saddle_transform =
      back.make_local_transform(ctx.model, QVector3D(0.0F, 0.006F, 0.0F), 0.24F);

  // Saddle seat - width along horse (forward), length across horse (right)
  QMatrix4x4 seat = saddle_transform;
  seat.scale(0.32F, 0.12F, 1.05F);
  out.mesh(getUnitSphere(), seat, variant.saddleColor, nullptr, 1.0F);

  // Pommel at front
  QMatrix4x4 pommel =
      back.make_local_transform(ctx.model, QVector3D(0.0F, 0.015F, 0.15F), 0.17F);
  pommel.scale(0.10F, 0.35F, 0.35F);
  out.mesh(getUnitSphere(), pommel, variant.saddleColor, nullptr, 1.0F);

  // Cantle at back
  QMatrix4x4 cantle =
      back.make_local_transform(ctx.model, QVector3D(0.0F, 0.020F, -0.15F), 0.17F);
  cantle.scale(0.10F, 0.38F, 0.35F);
  out.mesh(getUnitSphere(), cantle, variant.saddleColor, nullptr, 1.0F);
}

} // namespace Render::GL
