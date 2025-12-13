#include "carthage_saddle_renderer.h"

#include "../../../entity/registry.h"
#include "../../../gl/primitives.h"
#include <QMatrix4x4>

namespace Render::GL {

void CarthageSaddleRenderer::render(const DrawContext &ctx,
                                    const HorseBodyFrames &frames,
                                    const HorseVariant &variant,
                                    const HorseAnimationContext &,
                                    ISubmitter &out) const {

  const HorseAttachmentFrame &back = frames.back_center;

  QMatrix4x4 saddle_transform = back.make_local_transform(
      ctx.model, QVector3D(0.0F, 0.008F, 0.0F), 0.25F);

  QMatrix4x4 seat = saddle_transform;
  seat.scale(0.38F, 0.14F, 1.20F);
  out.mesh(get_unit_sphere(), seat, variant.saddle_color, nullptr, 1.0F, 4);

  QMatrix4x4 pommel = back.make_local_transform(
      ctx.model, QVector3D(0.0F, 0.020F, 0.18F), 0.19F);
  pommel.scale(0.12F, 0.42F, 0.38F);
  out.mesh(get_unit_sphere(), pommel, variant.saddle_color, nullptr, 1.0F, 4);

  QMatrix4x4 cantle = back.make_local_transform(
      ctx.model, QVector3D(0.0F, 0.028F, -0.16F), 0.21F);
  cantle.scale(0.16F, 0.58F, 0.48F);
  out.mesh(get_unit_sphere(), cantle, variant.saddle_color, nullptr, 1.0F, 4);
}

} // namespace Render::GL
