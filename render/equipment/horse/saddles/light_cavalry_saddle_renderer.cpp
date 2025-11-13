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
      back.make_local_transform(ctx.model, QVector3D(0.0F, 0.03F, 0.0F), 1.0F);

  QMatrix4x4 seat = saddle_transform;
  seat.scale(0.32F, 0.03F, 0.42F);
  out.mesh(getUnitSphere(), seat, variant.saddleColor, nullptr, 1.0F);

  QMatrix4x4 pommel =
      back.make_local_transform(ctx.model, QVector3D(0.0F, 0.05F, 0.24F), 0.7F);
  pommel.scale(0.28F, 0.08F, 0.06F);
  out.mesh(getUnitSphere(), pommel, variant.saddleColor, nullptr, 1.0F);

  QMatrix4x4 cantle =
      back.make_local_transform(ctx.model, QVector3D(0.0F, 0.06F, -0.24F), 0.7F);
  cantle.scale(0.28F, 0.09F, 0.06F);
  out.mesh(getUnitSphere(), cantle, variant.saddleColor, nullptr, 1.0F);
}

} // namespace Render::GL
