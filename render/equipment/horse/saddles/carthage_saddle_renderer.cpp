#include "carthage_saddle_renderer.h"

#include "../../../entity/registry.h"
#include "../../../gl/primitives.h"
#include <QMatrix4x4>

namespace Render::GL {

void CarthageSaddleRenderer::render(const DrawContext &ctx,
                                    const HorseBodyFrames &frames,
                                    const HorseVariant &variant,
                                    const HorseAnimationContext & /*anim*/,
                                    ISubmitter &out) const {

  const HorseAttachmentFrame &back = frames.back_center;

  QMatrix4x4 saddle_transform =
      back.makeLocalTransform(ctx.model, QVector3D(0.0F, 0.04F, 0.0F), 1.0F);

  QMatrix4x4 seat = saddle_transform;
  seat.scale(0.38F, 0.035F, 0.50F);
  out.mesh(getUnitSphere(), seat, variant.saddleColor, nullptr, 1.0F);

  QMatrix4x4 pommel =
      back.makeLocalTransform(ctx.model, QVector3D(0.0F, 0.07F, 0.28F), 0.75F);
  pommel.scale(0.30F, 0.10F, 0.07F);
  out.mesh(getUnitSphere(), pommel, variant.saddleColor, nullptr, 1.0F);

  QMatrix4x4 cantle =
      back.makeLocalTransform(ctx.model, QVector3D(0.0F, 0.09F, -0.28F), 0.85F);
  cantle.scale(0.34F, 0.14F, 0.09F);
  out.mesh(getUnitSphere(), cantle, variant.saddleColor, nullptr, 1.0F);
}

} // namespace Render::GL
