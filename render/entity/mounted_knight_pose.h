#pragma once

#include <algorithm>

#include "render/horse/horse_renderer_base.h"
#include "render/humanoid/humanoid_math.h"
#include "render/humanoid/humanoid_renderer_base.h"

namespace Render::GL {

struct MountedKnightPoseTuning {
  float stirrup_inset_factor = 0.82F;
  float stirrup_drop_scale = 0.74F;
  float stirrup_forward_bias = 0.12F;
  float stirrup_back_offset = -0.02F;
  float stirrup_height_bias = 0.18F;
  float stirrup_outward_bias = 0.18F;
  float thigh_wrap_factor = 0.78F;
  float knee_along = 0.42F;
  float knee_plane_lerp = 0.65F;
  float knee_blend = 0.60F;
  float calf_surface_blend = 0.72F;
  float calf_out_offset = 0.32F;
  float calf_back_offset = -0.08F;
  float calf_down_extra = 0.22F;
  float calf_behind_girth = 0.02F;
  float calf_relax = 0.32F;
  float calf_blend = 0.48F;
  float foot_back_offset = -0.04F;
  float foot_down_offset = 0.14F;
  float foot_blend = 0.55F;
  float shield_raise_speed = 8.0F;
  float shield_outset_factor = 0.68F;
  float sword_outset_factor = 0.72F;
};

inline void tune_mounted_knight_frame(const HorseDimensions& dims,
                                      MountedAttachmentFrame& mount,
                                      const MountedKnightPoseTuning& cfg = {}) {
  auto reposition_stirrup = [&](Side side) {
    float const sign = (side == Side::Left) ? -1.0F : 1.0F;
    QVector3D attach =
        mount.seat_position +
        mount.seat_right * (sign * dims.body_width * cfg.stirrup_inset_factor) +
        mount.seat_forward * (dims.body_length * cfg.stirrup_forward_bias +
                              dims.seat_forward_offset * 0.20F) -
        mount.seat_up * (dims.stirrup_drop * cfg.stirrup_height_bias);
    QVector3D bottom =
        attach - mount.seat_up * (dims.stirrup_drop * cfg.stirrup_drop_scale) +
        mount.seat_forward * (dims.body_length * cfg.stirrup_back_offset) +
        mount.seat_right * (sign * dims.body_width * cfg.stirrup_outward_bias);

    if (side == Side::Left) {
      mount.stirrup_attach_left = attach;
      mount.stirrup_bottom_left = bottom;
    } else {
      mount.stirrup_attach_right = attach;
      mount.stirrup_bottom_right = bottom;
    }
  };

  reposition_stirrup(Side::Left);
  reposition_stirrup(Side::Right);
}

} // namespace Render::GL
