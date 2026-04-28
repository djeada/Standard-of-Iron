#pragma once

#include "../horse/horse_renderer_base.h"
#include "../humanoid/humanoid_math.h"
#include "../humanoid/humanoid_renderer_base.h"
#include <algorithm>

namespace Render::GL {

struct MountedKnightPoseTuning {
  float stirrup_inset_factor = 0.60F;
  float stirrup_drop_scale = 0.74F;
  float stirrup_forward_bias = 0.02F;
  float stirrupBackOffset = -0.05F;
  float stirrup_height_bias = 0.18F;
  float stirrup_outward_bias = 0.08F;
  float thigh_wrap_factor = 0.64F;
  float knee_along = 0.46F;
  float knee_plane_lerp = 0.65F;
  float knee_blend = 0.60F;
  float calf_surface_blend = 0.65F;
  float calf_out_offset = 0.24F;
  float calfBackOffset = -0.16F;
  float calf_down_extra = 0.22F;
  float calf_behind_girth = -0.05F;
  float calf_relax = 0.32F;
  float calf_blend = 0.48F;
  float footBackOffset = -0.12F;
  float foot_down_offset = 0.14F;
  float foot_blend = 0.55F;
  float shield_raise_speed = 8.0F;
  float shield_outset_factor = 0.68F;
  float sword_outset_factor = 0.72F;
};

inline void tune_mounted_knight_frame(const HorseDimensions &dims,
                                      MountedAttachmentFrame &mount,
                                      const MountedKnightPoseTuning &cfg = {}) {
  auto reposition_stirrup = [&](bool is_left) {
    float const side = is_left ? -1.0F : 1.0F;
    QVector3D attach =
        mount.seat_position +
        mount.seat_right * (side * dims.body_width * cfg.stirrup_inset_factor) +
        mount.seat_forward * (dims.body_length * cfg.stirrup_forward_bias +
                              dims.seat_forward_offset * 0.20F) -
        mount.seat_up * (dims.stirrup_drop * cfg.stirrup_height_bias);
    QVector3D bottom =
        attach - mount.seat_up * (dims.stirrup_drop * cfg.stirrup_drop_scale) +
        mount.seat_forward * (dims.body_length * cfg.stirrupBackOffset) +
        mount.seat_right * (side * dims.body_width * cfg.stirrup_outward_bias);

    if (is_left) {
      mount.stirrup_attach_left = attach;
      mount.stirrup_bottom_left = bottom;
    } else {
      mount.stirrup_attach_right = attach;
      mount.stirrup_bottom_right = bottom;
    }
  };

  reposition_stirrup(true);
  reposition_stirrup(false);
}

inline void apply_mounted_knight_lower_body(
    const HorseDimensions &dims, const MountedAttachmentFrame &mount,
    const HumanoidAnimationContext &anim_ctx, HumanoidPose &pose,
    const MountedKnightPoseTuning &cfg = {}) {
  (void)anim_ctx;

  auto shape_leg = [&](QVector3D &knee, QVector3D &foot, bool is_left) {
    float const side = is_left ? -1.0F : 1.0F;
    QVector3D pelvis = pose.pelvis_pos + mount.seat_up * -0.01F;
    QVector3D stirrup =
        (is_left ? mount.stirrup_bottom_left : mount.stirrup_bottom_right);

    QVector3D pelvis_to_stirrup = stirrup - pelvis;
    QVector3D thigh_target =
        pelvis + pelvis_to_stirrup * cfg.knee_along +
        mount.seat_right * (side * dims.body_width * cfg.thigh_wrap_factor);
    float const knee_contact_plane =
        mount.seat_position.x() + side * dims.body_width * 0.56F;
    thigh_target.setX(thigh_target.x() * (1.0F - cfg.knee_plane_lerp) +
                      knee_contact_plane * cfg.knee_plane_lerp);
    knee = knee * (1.0F - cfg.knee_blend) + thigh_target * cfg.knee_blend;

    QVector3D base_foot =
        is_left ? mount.stirrup_bottom_left : mount.stirrup_bottom_right;

    QVector3D calf_surface =
        knee +
        mount.seat_right * (side * dims.body_width * cfg.calf_out_offset) +
        mount.seat_forward * (dims.body_length * cfg.calfBackOffset) -
        mount.seat_up * (dims.stirrup_drop * cfg.calf_down_extra);
    QVector3D calf_from_foot =
        base_foot +
        mount.seat_forward * (dims.body_length * cfg.calf_behind_girth) -
        mount.seat_up * (dims.stirrup_drop * cfg.calf_relax);

    QVector3D calf_target = calf_from_foot * (1.0F - cfg.calf_surface_blend) +
                            calf_surface * cfg.calf_surface_blend;

    calf_target.setY(
        std::clamp(calf_target.y(), base_foot.y() + 0.02F, knee.y() - 0.03F));
    foot = foot * (1.0F - cfg.calf_blend) + calf_target * cfg.calf_blend;

    QVector3D foot_target =
        calf_target +
        mount.seat_forward * (dims.body_length * cfg.footBackOffset) -
        mount.seat_up * (dims.stirrup_drop * cfg.foot_down_offset);
    foot = foot * (1.0F - cfg.foot_blend) + foot_target * cfg.foot_blend;
  };

  shape_leg(pose.knee_l, pose.foot_l, true);
  shape_leg(pose.knee_r, pose.foot_r, false);
}

} // namespace Render::GL
