#include "reins_renderer.h"

#include "../../../entity/registry.h"
#include "../../../gl/primitives.h"
#include <QMatrix4x4>

namespace Render::GL {

void ReinsRenderer::render(const DrawContext &ctx,
                           const HorseBodyFrames &frames,
                           const HorseVariant &variant,
                           const HorseAnimationContext & /*anim*/,
                           ISubmitter &out) const {

  const HorseAttachmentFrame &muzzle = frames.muzzle;
  const HorseAttachmentFrame &back = frames.back_center;

  for (int i = 0; i < 2; ++i) {
    float const side = (i == 0) ? 1.0F : -1.0F;

    QVector3D const bit_pos =
        muzzle.origin + muzzle.right * side * 0.18F + muzzle.forward * 0.12F;

    QVector3D const rein_handle =
        back.origin + back.right * side * 0.20F + back.up * 0.25F +
        back.forward * 0.15F;

    QVector3D const mid_point =
        (bit_pos + rein_handle) * 0.5F - back.up * 0.15F;

    out.cylinder(bit_pos, mid_point, 0.008F, variant.tack_color, 1.0F);
    out.cylinder(mid_point, rein_handle, 0.008F, variant.tack_color, 1.0F);
  }
}

} // namespace Render::GL
