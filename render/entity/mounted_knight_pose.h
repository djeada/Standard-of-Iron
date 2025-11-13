#pragma once

#include "../horse/rig.h"
#include "../humanoid/humanoid_math.h"
#include "../humanoid/rig.h"
#include <algorithm>

namespace Render::GL {

struct MountedKnightPoseTuning {
  float stirrupInsetFactor = 0.60F;
  float stirrupDropScale = 0.74F;
  float stirrupForwardBias = 0.02F;
  float stirrupBackOffset = -0.05F;
  float stirrupHeightBias = 0.18F;
  float stirrupOutwardBias = 0.08F;
  float thighWrapFactor = 0.64F;
  float kneeAlong = 0.46F;
  float kneePlaneLerp = 0.65F;
  float kneeBlend = 0.60F;
  float calfSurfaceBlend = 0.65F;
  float calfOutOffset = 0.24F;
  float calfBackOffset = -0.16F;
  float calfDownExtra = 0.22F;
  float calfBehindGirth = -0.05F;
  float calfRelax = 0.32F;
  float calfBlend = 0.48F;
  float footBackOffset = -0.12F;
  float footDownOffset = 0.14F;
  float footBlend = 0.55F;
  float shieldRaiseSpeed = 8.0F;
  float shieldOutsetFactor = 0.68F;
  float swordOutsetFactor = 0.72F;
};

inline void tuneMountedKnightFrame(const HorseDimensions &dims,
                                   MountedAttachmentFrame &mount,
                                   const MountedKnightPoseTuning &cfg = {}) {
  auto reposition_stirrup = [&](bool is_left) {
    float const side = is_left ? -1.0F : 1.0F;
    QVector3D attach =
        mount.seat_position +
        mount.seat_right * (side * dims.bodyWidth * cfg.stirrupInsetFactor) +
        mount.seat_forward * (dims.bodyLength * cfg.stirrupForwardBias +
                              dims.seatForwardOffset * 0.20F) -
        mount.seat_up * (dims.stirrupDrop * cfg.stirrupHeightBias);
    QVector3D bottom =
        attach - mount.seat_up * (dims.stirrupDrop * cfg.stirrupDropScale) +
        mount.seat_forward * (dims.bodyLength * cfg.stirrupBackOffset) +
        mount.seat_right * (side * dims.bodyWidth * cfg.stirrupOutwardBias);

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

inline void applyMountedKnightLowerBody(
    const HorseDimensions &dims, const MountedAttachmentFrame &mount,
    const HumanoidAnimationContext &anim_ctx, HumanoidPose &pose,
    const MountedKnightPoseTuning &cfg = {}) {
  (void)anim_ctx;

  auto shape_leg = [&](QVector3D &knee, QVector3D &foot, bool is_left) {
    float const side = is_left ? -1.0F : 1.0F;
    QVector3D pelvis = pose.pelvis_pos + mount.seat_up * -0.01F;
    QVector3D stirrup =
        (is_left ? mount.stirrup_bottom_left : mount.stirrup_bottom_right) +
        mount.ground_offset;

    QVector3D pelvis_to_stirrup = stirrup - pelvis;
    QVector3D thigh_target =
        pelvis + pelvis_to_stirrup * cfg.kneeAlong +
        mount.seat_right * (side * dims.bodyWidth * cfg.thighWrapFactor);
    float const knee_contact_plane =
        mount.seat_position.x() + side * dims.bodyWidth * 0.56F;
    thigh_target.setX(thigh_target.x() * (1.0F - cfg.kneePlaneLerp) +
                      knee_contact_plane * cfg.kneePlaneLerp);
    knee = knee * (1.0F - cfg.kneeBlend) + thigh_target * cfg.kneeBlend;

    QVector3D base_foot =
        is_left ? mount.stirrup_bottom_left : mount.stirrup_bottom_right;
    base_foot += mount.ground_offset;

    QVector3D calf_surface =
        knee + mount.seat_right * (side * dims.bodyWidth * cfg.calfOutOffset) +
        mount.seat_forward * (dims.bodyLength * cfg.calfBackOffset) -
        mount.seat_up * (dims.stirrupDrop * cfg.calfDownExtra);
    QVector3D calf_from_foot =
        base_foot +
        mount.seat_forward * (dims.bodyLength * cfg.calfBehindGirth) -
        mount.seat_up * (dims.stirrupDrop * cfg.calfRelax);

    QVector3D calf_target = calf_from_foot * (1.0F - cfg.calfSurfaceBlend) +
                            calf_surface * cfg.calfSurfaceBlend;

    calf_target.setY(
        std::clamp(calf_target.y(), base_foot.y() + 0.02F, knee.y() - 0.03F));
    foot = foot * (1.0F - cfg.calfBlend) + calf_target * cfg.calfBlend;

    QVector3D foot_target =
        calf_target +
        mount.seat_forward * (dims.bodyLength * cfg.footBackOffset) -
        mount.seat_up * (dims.stirrupDrop * cfg.footDownOffset);
    foot = foot * (1.0F - cfg.footBlend) + foot_target * cfg.footBlend;
  };

  shape_leg(pose.knee_l, pose.foot_l, true);
  shape_leg(pose.knee_r, pose.foot_r, false);
}

} // namespace Render::GL
