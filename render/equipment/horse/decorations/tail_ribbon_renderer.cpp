#include "tail_ribbon_renderer.h"
#include "../../equipment_submit.h"
#include "../../oriented_archetype_utils.h"
#include "../../primitive_archetype_utils.h"

#include <QMatrix4x4>
#include <array>
#include <cmath>

namespace Render::GL {

void TailRibbonRenderer::submit(const DrawContext &ctx,
                                const HorseBodyFrames &frames,
                                const HorseVariant &variant,
                                const HorseAnimationContext &anim,
                                EquipmentBatch &batch) {

  const HorseAttachmentFrame &tail = frames.tail_base;

  QVector3D const ribbon_color = variant.blanket_color;

  QVector3D const ribbon_start = tail.origin + tail.up * 0.05F;
  QVector3D const ribbon_end =
      ribbon_start - tail.forward * 0.15F + tail.up * 0.08F;

  float const wave = std::sin(anim.time * 3.0F + anim.phase * 6.28F) * 0.05F;
  QVector3D const ribbon_mid =
      (ribbon_start + ribbon_end) * 0.5F + tail.right * wave;
  std::array<QVector3D, 1> const palette{ribbon_color};

  append_equipment_archetype(
      batch, single_cylinder_archetype(0.015F, 4, "horse_tail_ribbon"),
      oriented_segment_transform(ctx.model, ribbon_start,
                                 ribbon_mid - ribbon_start, tail.right),
      palette, nullptr, 0.90F);
  append_equipment_archetype(
      batch, single_cylinder_archetype(0.015F, 4, "horse_tail_ribbon"),
      oriented_segment_transform(ctx.model, ribbon_mid, ribbon_end - ribbon_mid,
                                 tail.right),
      palette, nullptr, 0.90F);

  QMatrix4x4 bow = ctx.model;
  bow.translate(ribbon_start);
  bow.scale(0.08F, 0.08F, 0.06F);
  append_equipment_archetype(
      batch, single_sphere_archetype(4, "horse_tail_bow"), bow, palette);
}

} // namespace Render::GL
