#include "plume_renderer.h"
#include "../../equipment_submit.h"
#include "../../oriented_archetype_utils.h"
#include "../../primitive_archetype_utils.h"

#include <array>
#include <cmath>

namespace Render::GL {

void PlumeRenderer::submit(const DrawContext &ctx,
                           const HorseBodyFrames &frames,
                           const HorseVariant &variant,
                           const HorseAnimationContext &anim,
                           EquipmentBatch &batch) {

  const HorseAttachmentFrame &head = frames.head;

  QVector3D const plume_color = variant.blanket_color;
  float const sway = std::sin(anim.time * 2.5F) * 0.08F;

  QVector3D const base_pos =
      head.origin + head.up * 0.28F + head.forward * 0.05F;
  std::array<QVector3D, 1> const palette{plume_color};

  for (int i = 0; i < 3; ++i) {
    float const offset = (i - 1) * 0.04F;
    QVector3D const feather_base = base_pos + head.right * offset;
    QVector3D const feather_tip =
        feather_base + head.up * 0.25F + head.forward * sway;
    append_equipment_archetype(
        batch, single_cylinder_archetype(0.018F - i * 0.002F, 4, "horse_plume"),
        oriented_segment_transform(ctx.model, feather_base,
                                   feather_tip - feather_base, head.right),
        palette, nullptr, 0.85F);
  }
}

} // namespace Render::GL
