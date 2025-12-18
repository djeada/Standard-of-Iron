#include "reins_renderer.h"

#include "../../../entity/registry.h"
#include "../../../gl/primitives.h"
#include <QMatrix4x4>
#include <array>

namespace Render::GL {

void ReinsRenderer::render(const DrawContext &ctx,
                           const HorseBodyFrames &frames,
                           const HorseVariant &variant,
                           const HorseAnimationContext &,
                           ISubmitter &out) const {

  const HorseAttachmentFrame &muzzle = frames.muzzle;
  const HorseAttachmentFrame &back = frames.back_center;

  constexpr float k_rein_radius = 0.004F;
  constexpr float k_bit_side_offset = 0.10F;
  constexpr float k_bit_forward_offset = 0.10F;
  constexpr float k_handle_side_offset = 0.12F;
  constexpr float k_handle_up_offset = 0.22F;
  constexpr float k_handle_forward_offset = 0.05F;
  constexpr float k_mid_drop = 0.12F;

  auto toWorld = [&ctx](const QVector3D &local) -> QVector3D {
    return ctx.model.map(local);
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

    QVector3D const bit_pos = toWorld(bit_pos_local);
    QVector3D const rein_handle = toWorld(rein_handle_local);
    QVector3D const mid_point = toWorld(mid_point_local);

    endpoints[i].bit = bit_pos;
    endpoints[i].handle = rein_handle;

    out.cylinder(bit_pos, mid_point, k_rein_radius, variant.tack_color, 1.0F);
    out.cylinder(mid_point, rein_handle, k_rein_radius, variant.tack_color,
                 1.0F);
  }

  out.cylinder(endpoints[0].bit, endpoints[1].bit, k_rein_radius,
               variant.tack_color, 1.0F);
  out.cylinder(endpoints[0].handle, endpoints[1].handle, k_rein_radius,
               variant.tack_color, 1.0F);
}

} 
