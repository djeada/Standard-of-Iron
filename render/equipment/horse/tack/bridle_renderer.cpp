#include "bridle_renderer.h"
#include "../../equipment_submit.h"

#include "../../../entity/registry.h"
#include "../../../gl/primitives.h"
#include <QMatrix4x4>

namespace Render::GL {

void BridleRenderer::render(const DrawContext &ctx,
                            const HorseBodyFrames &frames,
                            const HorseVariant &variant,
                            const HorseAnimationContext &,
                            EquipmentBatch &batch) const {

  const HorseAttachmentFrame &head = frames.head;
  const HorseAttachmentFrame &muzzle = frames.muzzle;

  QVector3D const headband_start =
      head.origin + head.right * 0.22F + head.up * 0.15F;
  QVector3D const headband_end =
      head.origin - head.right * 0.22F + head.up * 0.15F;
  batch.cylinders.push_back({headband_start, headband_end, 0.012F, variant.tack_color, 1.0F});

  QVector3D const noseband_start =
      muzzle.origin + muzzle.right * 0.20F + muzzle.forward * 0.10F;
  QVector3D const noseband_end =
      muzzle.origin - muzzle.right * 0.20F + muzzle.forward * 0.10F;
  batch.cylinders.push_back({noseband_start, noseband_end, 0.010F, variant.tack_color, 1.0F});

  for (int i = 0; i < 2; ++i) {
    float const side = (i == 0) ? 1.0F : -1.0F;
    QVector3D const cheek_top =
        head.origin + head.right * side * 0.22F + head.up * 0.15F;
    QVector3D const cheek_bottom =
        muzzle.origin + muzzle.right * side * 0.20F + muzzle.forward * 0.10F;
    batch.cylinders.push_back({cheek_top, cheek_bottom, 0.011F, variant.tack_color, 1.0F});
  }

  QVector3D const throatlatch_start = head.origin - head.up * 0.10F;
  QVector3D const throatlatch_end = head.origin - head.up * 0.25F;
  batch.cylinders.push_back({throatlatch_start, throatlatch_end, 0.010F, variant.tack_color,
               1.0F});
}

} // namespace Render::GL
