#include "roman_saddle_renderer.h"

#include "../../../entity/registry.h"
#include "../../../gl/primitives.h"
#include <QMatrix4x4>

namespace Render::GL {

void RomanSaddleRenderer::render(const DrawContext &ctx,
                                 const HorseBodyFrames &frames,
                                 const HorseVariant &variant,
                                 const HorseAnimationContext & /*anim*/,
                                 ISubmitter &out) const {

  const HorseAttachmentFrame &back = frames.back_center;

  QMatrix4x4 saddle_transform =
      back.makeLocalTransform(ctx.model, QVector3D(0.0F, 0.05F, 0.0F), 1.0F);

  QMatrix4x4 seat = saddle_transform;
  seat.scale(0.35F, 0.04F, 0.45F);
  out.mesh(getUnitSphere(), seat, variant.saddleColor, nullptr, 1.0F);

  QMatrix4x4 pommel =
      back.makeLocalTransform(ctx.model, QVector3D(0.0F, 0.08F, 0.25F), 0.8F);
  pommel.scale(0.32F, 0.12F, 0.08F);
  out.mesh(getUnitSphere(), pommel, variant.saddleColor, nullptr, 1.0F);

  QMatrix4x4 cantle =
      back.makeLocalTransform(ctx.model, QVector3D(0.0F, 0.08F, -0.25F), 0.8F);
  cantle.scale(0.32F, 0.12F, 0.08F);
  out.mesh(getUnitSphere(), cantle, variant.saddleColor, nullptr, 1.0F);

  for (int i = 0; i < 2; ++i) {
    float const side = (i == 0) ? 1.0F : -1.0F;
    QMatrix4x4 horn = back.makeLocalTransform(
        ctx.model, QVector3D(side * 0.15F, 0.14F, 0.28F), 0.6F);
    horn.scale(0.06F, 0.18F, 0.06F);
    out.mesh(getUnitSphere(), horn, variant.saddleColor, nullptr, 1.0F);
  }
}

} // namespace Render::GL
