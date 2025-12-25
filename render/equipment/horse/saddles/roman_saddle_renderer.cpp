#include "roman_saddle_renderer.h"

#include "../../../entity/registry.h"
#include "../../../gl/primitives.h"
#include <QMatrix4x4>

namespace Render::GL {

void RomanSaddleRenderer::render(const DrawContext &ctx,
                                 const HorseBodyFrames &frames,
                                 const HorseVariant &variant,
                                 const HorseAnimationContext &,
                                 ISubmitter &out) const {

  const HorseAttachmentFrame &back = frames.back_center;

  QMatrix4x4 saddle_transform =
      back.make_local_transform(ctx.model, QVector3D(0.0F, 0.01F, 0.0F), 0.25F);

  QMatrix4x4 seat = saddle_transform;
  seat.scale(0.34F, 0.15F, 1.10F);
  out.mesh(get_unit_sphere(), seat, variant.saddle_color, nullptr, 1.0F, 4);

  QMatrix4x4 pommel = back.make_local_transform(
      ctx.model, QVector3D(0.0F, 0.025F, 0.16F), 0.20F);
  pommel.scale(0.14F, 0.48F, 0.40F);
  out.mesh(get_unit_sphere(), pommel, variant.saddle_color, nullptr, 1.0F, 4);

  QMatrix4x4 cantle = back.make_local_transform(
      ctx.model, QVector3D(0.0F, 0.03F, -0.12F), 0.22F);
  cantle.scale(0.18F, 0.60F, 0.52F);
  out.mesh(get_unit_sphere(), cantle, variant.saddle_color, nullptr, 1.0F, 4);

  for (int i = 0; i < 2; ++i) {
    float const side = (i == 0) ? 1.0F : -1.0F;
    QMatrix4x4 horn = back.make_local_transform(
        ctx.model, QVector3D(0.0F, 0.04F, side * 0.08F), 0.15F);
    horn.scale(0.15F, 0.45F, 0.15F);
    out.mesh(get_unit_sphere(), horn, variant.saddle_color, nullptr, 1.0F, 4);
  }
}

} 
