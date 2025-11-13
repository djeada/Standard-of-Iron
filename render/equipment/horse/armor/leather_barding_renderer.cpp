#include "leather_barding_renderer.h"

#include "../../../entity/registry.h"
#include "../../../gl/primitives.h"
#include <QMatrix4x4>

namespace Render::GL {

void LeatherBardingRenderer::render(const DrawContext &ctx,
                                    const HorseBodyFrames &frames,
                                    const HorseVariant &variant,
                                    const HorseAnimationContext &,
                                    ISubmitter &out) const {

  QVector3D const armor_color = variant.saddleColor * 0.90F;

  const HorseAttachmentFrame &chest = frames.chest;
  QMatrix4x4 chest_armor = chest.make_local_transform(
      ctx.model, QVector3D(0.0F, -0.03F, 0.0F), 1.0F);
  chest_armor.scale(0.38F, 0.28F, 0.32F);
  out.mesh(getUnitSphere(), chest_armor, armor_color, nullptr, 1.0F);

  const HorseAttachmentFrame &barrel = frames.barrel;
  for (int i = 0; i < 2; ++i) {
    float const side = (i == 0) ? 1.0F : -1.0F;
    QMatrix4x4 side_armor = barrel.make_local_transform(
        ctx.model, QVector3D(side * 0.32F, -0.08F, 0.0F), 1.0F);
    side_armor.scale(0.10F, 0.25F, 0.45F);
    out.mesh(getUnitSphere(), side_armor, armor_color, nullptr, 1.0F);
  }
}

} // namespace Render::GL
