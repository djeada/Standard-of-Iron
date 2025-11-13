#include "stirrup_renderer.h"

#include "../../../entity/registry.h"
#include "../../../gl/primitives.h"
#include <QMatrix4x4>

namespace Render::GL {

void StirrupRenderer::render(const DrawContext &ctx,
                             const HorseBodyFrames &frames,
                             const HorseVariant &variant,
                             const HorseAnimationContext &,
                             ISubmitter &out) const {

  const HorseAttachmentFrame &back = frames.back_center;

  for (int i = 0; i < 2; ++i) {
    float const side = (i == 0) ? 1.0F : -1.0F;

    QVector3D const stirrup_attach =
        back.origin + back.right * side * 0.45F - back.up * 0.02F;
    QVector3D const stirrup_bottom = stirrup_attach - back.up * 0.30F;

    out.cylinder(stirrup_attach, stirrup_bottom, 0.008F, variant.tack_color,
                 1.0F);

    QMatrix4x4 foot_plate = ctx.model;
    foot_plate.translate(stirrup_bottom);
    foot_plate.scale(0.10F, 0.015F, 0.12F);
    out.mesh(getUnitSphere(), foot_plate, variant.tack_color, nullptr, 1.0F);
  }
}

} // namespace Render::GL
