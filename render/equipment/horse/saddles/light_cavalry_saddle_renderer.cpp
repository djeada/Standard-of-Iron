#include "light_cavalry_saddle_renderer.h"

#include "../../../entity/registry.h"
#include "../../../gl/primitives.h"
#include <QMatrix4x4>

namespace Render::GL {

void LightCavalrySaddleRenderer::render(const DrawContext &ctx,
                                        const HorseBodyFrames &frames,
                                        const HorseVariant &variant,
                                        const HorseAnimationContext &,
                                        ISubmitter &out) const {

  const HorseAttachmentFrame &back = frames.back_center;

  QMatrix4x4 saddle_transform = back.make_local_transform(
      ctx.model, QVector3D(0.0F, 0.006F, 0.0F), 0.24F);

  QMatrix4x4 seat = saddle_transform;
  seat.scale(0.32F, 0.12F, 1.05F);
  out.mesh(get_unit_sphere(), seat, variant.saddle_color, nullptr, 1.0F, 4);

  QMatrix4x4 pommel = back.make_local_transform(
      ctx.model, QVector3D(0.0F, 0.015F, 0.15F), 0.17F);
  pommel.scale(0.10F, 0.35F, 0.35F);
  out.mesh(get_unit_sphere(), pommel, variant.saddle_color, nullptr, 1.0F, 4);

  QMatrix4x4 cantle = back.make_local_transform(
      ctx.model, QVector3D(0.0F, 0.020F, -0.15F), 0.17F);
  cantle.scale(0.10F, 0.38F, 0.35F);
  out.mesh(get_unit_sphere(), cantle, variant.saddle_color, nullptr, 1.0F, 4);
}

} // namespace Render::GL
