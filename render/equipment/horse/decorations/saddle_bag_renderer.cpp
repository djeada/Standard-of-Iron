#include "saddle_bag_renderer.h"

#include "../../../entity/registry.h"
#include "../../../gl/primitives.h"
#include <QMatrix4x4>

namespace Render::GL {

void SaddleBagRenderer::render(const DrawContext &ctx,
                               const HorseBodyFrames &frames,
                               const HorseVariant &variant,
                               const HorseAnimationContext &,
                               ISubmitter &out) const {

  const HorseAttachmentFrame &back = frames.back_center;

  QVector3D const bag_color = variant.saddle_color * 0.85F;

  for (int i = 0; i < 2; ++i) {
    float const side = (i == 0) ? 1.0F : -1.0F;

    QMatrix4x4 bag = back.make_local_transform(
        ctx.model, QVector3D(side * 0.28F, -0.12F, -0.15F), 1.0F);
    bag.scale(0.18F, 0.22F, 0.30F);
    out.mesh(getUnitSphere(), bag, bag_color, nullptr, 1.0F, 4);

    QMatrix4x4 strap_attachment = back.make_local_transform(
        ctx.model, QVector3D(side * 0.28F, 0.02F, -0.10F), 1.0F);

    QVector3D const strap_top = back.origin + back.right * side * 0.28F +
                                back.up * 0.02F - back.forward * 0.10F;
    QVector3D const strap_bottom = back.origin + back.right * side * 0.28F -
                                   back.up * 0.12F - back.forward * 0.15F;

    out.cylinder(strap_top, strap_bottom, 0.012F, variant.tack_color, 1.0F);
  }
}

} // namespace Render::GL
