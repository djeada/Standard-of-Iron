#pragma once

#include <QString>
#include <QVector3D>

#include <algorithm>
#include <cmath>

#include "animation/attack_pose_manifest.h"
#include "animation/mounted_pose_manifest.h"
#include "animation/posture_pose_manifest.h"
#include "animation/rig/humanoid_proportions.h"
#include "render/creature/horse/horse_motion.h"
#include "render/creature/horse/horse_renderer_base.h"
#include "render/creature/humanoid/runtime/grip_axis.h"
#include "render/creature/humanoid/runtime/humanoid_math.h"
#include "render/creature/humanoid/runtime/mounted_pose_controller.h"
#include "render/creature/humanoid/runtime/pose_controller.h"
#include "render/creature/humanoid/runtime/pose_primitives.h"
#include "render/creature/humanoid/runtime/spear_pose_utils.h"

namespace Render::GL {

namespace mounted_pose_controller_detail {

auto seat_relative(const MountedAttachmentFrame& mount,
                   float forward,
                   float right,
                   float up) -> QVector3D;

auto seat_relative(const MountedAttachmentFrame& mount,
                   Animation::MountedSeatOffset offset) -> QVector3D;

void aim_mounted_spear(HumanoidPose& pose,
                       const MountedAttachmentFrame& mount,
                       Animation::MountedSeatOffset direction);

auto rein_anchor(const MountedAttachmentFrame& mount,
                 Side side,
                 float slack,
                 float tension) -> QVector3D;

auto mounted_axis_delta(const MountedAttachmentFrame& mount,
                        Animation::MountedAxisDelta delta) -> QVector3D;

auto mounted_offset(const MountedAttachmentFrame& mount,
                    Animation::MountedSeatOffset offset) -> QVector3D;

auto mounted_hand_anchor(const HumanoidPose& pose,
                         const MountedAttachmentFrame& mount,
                         Animation::MountedHandAnchor anchor) -> QVector3D;

void apply_mounted_posture_sample(HumanoidPose& pose,
                                  const MountedAttachmentFrame& mount,
                                  const Animation::MountedRiderPostureSample& sample);

auto to_animation_spear_grip(SpearGrip grip) -> Animation::MountedSpearGuardGrip;

auto to_animation_seat_pose(MountedPoseController::MountedSeatPose pose)
    -> Animation::MountedSeatPoseKind;

auto to_animation_weapon_pose(MountedPoseController::MountedWeaponPose pose)
    -> Animation::MountedWeaponPoseKind;

auto to_animation_shield_pose(MountedPoseController::MountedShieldPose pose)
    -> Animation::MountedShieldPoseKind;

} // namespace mounted_pose_controller_detail

} // namespace Render::GL
