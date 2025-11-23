#include "scale_barding_renderer.h"

#include "../../../entity/registry.h"
#include "../../../gl/primitives.h"
#include <QMatrix4x4>

namespace Render::GL {

void ScaleBardingRenderer::render(const DrawContext &ctx,
                                  const HorseBodyFrames &frames,
                                  const HorseVariant &variant,
                                  const HorseAnimationContext &,
                                  ISubmitter &out) const {

  QVector3D const armor_color = variant.tack_color * 0.85F;

  const HorseAttachmentFrame &chest = frames.chest;
  QMatrix4x4 chest_armor = chest.make_local_transform(
      ctx.model, QVector3D(0.0F, -0.05F, 0.0F), 1.0F);
  chest_armor.scale(0.40F, 0.32F, 0.35F);
  out.mesh(getUnitSphere(), chest_armor, armor_color, nullptr, 1.0F, 1);

  const HorseAttachmentFrame &barrel = frames.barrel;
  for (int i = 0; i < 2; ++i) {
    float const side = (i == 0) ? 1.0F : -1.0F;
    QMatrix4x4 side_armor = barrel.make_local_transform(
        ctx.model, QVector3D(side * 0.35F, -0.10F, 0.0F), 1.0F);
    side_armor.scale(0.12F, 0.28F, 0.48F);
    out.mesh(getUnitSphere(), side_armor, armor_color, nullptr, 1.0F, 1);
  }

  const HorseAttachmentFrame &neck = frames.neck_base;
  QMatrix4x4 neck_armor =
      neck.make_local_transform(ctx.model, QVector3D(0.0F, 0.0F, 0.15F), 1.0F);
  neck_armor.scale(0.36F, 0.30F, 0.38F);
  out.mesh(getUnitSphere(), neck_armor, armor_color, nullptr, 1.0F, 1);
}

} // namespace Render::GL
