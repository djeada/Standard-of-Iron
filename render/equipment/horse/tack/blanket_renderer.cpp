#include "blanket_renderer.h"

#include "../../../entity/registry.h"
#include "../../../gl/primitives.h"
#include <QMatrix4x4>

namespace Render::GL {

void BlanketRenderer::render(const DrawContext &ctx,
                             const HorseBodyFrames &frames,
                             const HorseVariant &variant,
                             const HorseAnimationContext & /*anim*/,
                             ISubmitter &out) const {

  const HorseAttachmentFrame &back = frames.back_center;

  QMatrix4x4 blanket_transform =
      back.makeLocalTransform(ctx.model, QVector3D(0.0F, 0.0F, 0.0F), 1.0F);

  blanket_transform.scale(0.48F, 0.02F, 0.60F);
  out.mesh(getUnitSphere(), blanket_transform, variant.blanketColor, nullptr,
           1.0F);

  for (int i = 0; i < 2; ++i) {
    float const side = (i == 0) ? 1.0F : -1.0F;
    QMatrix4x4 drape =
        back.makeLocalTransform(ctx.model, QVector3D(side * 0.30F, -0.08F, 0.0F),
                                0.8F);
    drape.scale(0.22F, 0.15F, 0.55F);
    out.mesh(getUnitSphere(), drape, variant.blanketColor, nullptr, 1.0F);
  }
}

} // namespace Render::GL
