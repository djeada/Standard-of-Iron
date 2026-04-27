#include "bridle_renderer.h"
#include "../../equipment_submit.h"
#include "../../oriented_archetype_utils.h"
#include "../../primitive_archetype_utils.h"

#include <array>

namespace Render::GL {

namespace {

void append_bridle_segment(EquipmentBatch &batch, const DrawContext &ctx,
                           const QVector3D &start, const QVector3D &end,
                           float radius, const QVector3D &color,
                           const QVector3D &right_hint) {
  std::array<QVector3D, 1> const palette{color};
  append_equipment_archetype(
      batch, single_cylinder_archetype(radius, 4, "horse_bridle_segment"),
      oriented_segment_transform(ctx.model, start, end - start, right_hint),
      palette);
}

} // namespace

void BridleRenderer::submit(const DrawContext &ctx,
                            const HorseBodyFrames &frames,
                            const HorseVariant &variant,
                            const HorseAnimationContext &,
                            EquipmentBatch &batch) {

  const HorseAttachmentFrame &head = frames.head;
  const HorseAttachmentFrame &muzzle = frames.muzzle;

  QVector3D const headband_start =
      head.origin + head.right * 0.22F + head.up * 0.15F;
  QVector3D const headband_end =
      head.origin - head.right * 0.22F + head.up * 0.15F;
  append_bridle_segment(batch, ctx, headband_start, headband_end, 0.012F,
                        variant.tack_color, head.forward);

  QVector3D const noseband_start =
      muzzle.origin + muzzle.right * 0.20F + muzzle.forward * 0.10F;
  QVector3D const noseband_end =
      muzzle.origin - muzzle.right * 0.20F + muzzle.forward * 0.10F;
  append_bridle_segment(batch, ctx, noseband_start, noseband_end, 0.010F,
                        variant.tack_color, muzzle.forward);

  for (int i = 0; i < 2; ++i) {
    float const side = (i == 0) ? 1.0F : -1.0F;
    QVector3D const cheek_top =
        head.origin + head.right * side * 0.22F + head.up * 0.15F;
    QVector3D const cheek_bottom =
        muzzle.origin + muzzle.right * side * 0.20F + muzzle.forward * 0.10F;
    append_bridle_segment(batch, ctx, cheek_top, cheek_bottom, 0.011F,
                          variant.tack_color, head.right * side);
  }

  QVector3D const throatlatch_start = head.origin - head.up * 0.10F;
  QVector3D const throatlatch_end = head.origin - head.up * 0.25F;
  append_bridle_segment(batch, ctx, throatlatch_start, throatlatch_end, 0.010F,
                        variant.tack_color, head.forward);
}

} // namespace Render::GL
