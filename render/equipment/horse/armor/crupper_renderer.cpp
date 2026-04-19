#include "crupper_renderer.h"
#include "../../equipment_submit.h"

#include "../../../entity/registry.h"
#include "../../../gl/primitives.h"
#include <QMatrix4x4>

namespace Render::GL {

void CrupperRenderer::submit(const DrawContext &ctx,
                             const HorseBodyFrames &frames,
                             const HorseVariant &variant,
                             const HorseAnimationContext &,
                             EquipmentBatch &batch) {

  QVector3D const armor_color = variant.tack_color * 0.88F;

  const HorseAttachmentFrame &rump = frames.rump;

  QMatrix4x4 rear_plate = rump.make_local_transform(
      ctx.model, QVector3D(0.0F, 0.02F, -0.15F), 1.0F);
  rear_plate.scale(0.48F, 0.32F, 0.28F);
  batch.meshes.push_back(
      {get_unit_sphere(), nullptr, rear_plate, armor_color, nullptr, 1.0F, 1});

  for (int i = 0; i < 2; ++i) {
    float const side = (i == 0) ? 1.0F : -1.0F;
    QMatrix4x4 side_plate = rump.make_local_transform(
        ctx.model, QVector3D(side * 0.28F, -0.05F, -0.20F), 0.8F);
    side_plate.scale(0.20F, 0.25F, 0.22F);
    batch.meshes.push_back({get_unit_sphere(), nullptr, side_plate,
                            armor_color * 0.95F, nullptr, 1.0F, 1});
  }
}

} // namespace Render::GL
