#include "reins_renderer.h"
#include "../../equipment_submit.h"
#include "../../oriented_archetype_utils.h"
#include "../../primitive_archetype_utils.h"

#include <array>

namespace Render::GL {

void ReinsRenderer::submit(const DrawContext &ctx,
                           const HorseBodyFrames &frames,
                           const HorseVariant &variant,
                           const HorseAnimationContext &,
                           EquipmentBatch &batch) {

  const HorseAttachmentFrame &muzzle = frames.muzzle;
  const HorseAttachmentFrame &back = frames.back_center;

  constexpr float k_rein_radius = 0.004F;
  constexpr float k_bit_side_offset = 0.10F;
  constexpr float k_bit_forward_offset = 0.10F;
  constexpr float k_handle_side_offset = 0.12F;
  constexpr float k_handle_up_offset = 0.22F;
  constexpr float k_handle_forward_offset = 0.05F;
  constexpr float k_mid_drop = 0.12F;

  auto append_segment = [&](const QVector3D &start, const QVector3D &end,
                            const QVector3D &right_hint) {
    std::array<QVector3D, 1> const palette{variant.tack_color};
    append_equipment_archetype(
        batch,
        single_cylinder_archetype(k_rein_radius, 4, "horse_rein_segment"),
        oriented_segment_transform(ctx.model, start, end - start, right_hint),
        palette);
  };

  struct ReinEndpoints {
    QVector3D bit;
    QVector3D handle;
  };
  std::array<ReinEndpoints, 2> endpoints{};

  for (int i = 0; i < 2; ++i) {
    float const side = (i == 0) ? 1.0F : -1.0F;

    QVector3D const bit_pos_local = muzzle.origin +
                                    muzzle.right * side * k_bit_side_offset +
                                    muzzle.forward * k_bit_forward_offset;

    QVector3D const rein_handle_local =
        back.origin + back.right * side * k_handle_side_offset +
        back.up * k_handle_up_offset + back.forward * k_handle_forward_offset;

    QVector3D const mid_point_local =
        (bit_pos_local + rein_handle_local) * 0.5F - back.up * k_mid_drop;

    endpoints[i].bit = bit_pos_local;
    endpoints[i].handle = rein_handle_local;

    append_segment(bit_pos_local, mid_point_local, muzzle.right * side);
    append_segment(mid_point_local, rein_handle_local, back.right * side);
  }

  append_segment(endpoints[0].bit, endpoints[1].bit, muzzle.forward);
  append_segment(endpoints[0].handle, endpoints[1].handle, back.forward);
}

} // namespace Render::GL
