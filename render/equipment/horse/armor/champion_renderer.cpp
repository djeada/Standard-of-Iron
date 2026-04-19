#include "champion_renderer.h"
#include "../../equipment_submit.h"

#include "../../../entity/registry.h"
#include "../../../gl/primitives.h"
#include <QMatrix4x4>

namespace Render::GL {

void ChampionRenderer::submit(const DrawContext &ctx,
                              const HorseBodyFrames &frames,
                              const HorseVariant &variant,
                              const HorseAnimationContext &,
                              EquipmentBatch &batch) {

  QVector3D const armor_color = variant.tack_color * 0.82F;

  const HorseAttachmentFrame &chest = frames.chest;

  QMatrix4x4 plate_center =
      chest.make_local_transform(ctx.model, QVector3D(0.0F, 0.0F, 0.0F), 1.0F);
  plate_center.scale(0.42F, 0.35F, 0.38F);
  batch.meshes.push_back({get_unit_sphere(), nullptr, plate_center, armor_color,
                          nullptr, 1.0F, 1});

  QMatrix4x4 upper_plate = chest.make_local_transform(
      ctx.model, QVector3D(0.0F, 0.12F, 0.05F), 0.85F);
  upper_plate.scale(0.38F, 0.18F, 0.32F);
  batch.meshes.push_back({get_unit_sphere(), nullptr, upper_plate,
                          armor_color * 1.05F, nullptr, 1.0F, 1});

  QMatrix4x4 lower_plate = chest.make_local_transform(
      ctx.model, QVector3D(0.0F, -0.12F, 0.05F), 0.85F);
  lower_plate.scale(0.38F, 0.18F, 0.32F);
  batch.meshes.push_back({get_unit_sphere(), nullptr, lower_plate,
                          armor_color * 0.95F, nullptr, 1.0F, 1});
}

} // namespace Render::GL
