#pragma once

// Shared by the pose_controller*.cpp translation units; not a public header.

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "animation/ambient_pose_manifest.h"
#include "animation/attack_pose_manifest.h"
#include "animation/hold_pose_manifest.h"
#include "animation/melee_swing_manifest.h"
#include "animation/posture_pose_manifest.h"
#include "animation/showcase_pose_manifest.h"
#include "render/creature/humanoid/runtime/grip_axis.h"
#include "render/creature/humanoid/runtime/humanoid_math.h"
#include "render/creature/humanoid/runtime/pose_controller.h"
#include "render/creature/humanoid/runtime/pose_primitives.h"
#include "render/creature/humanoid/runtime/spear_pose_utils.h"
#include "render/creature/humanoid/runtime/sword_pose_utils.h"
#include "render/creature/movement_state.h"

namespace Render::GL {

namespace pose_controller_detail {

auto to_qvec(const Animation::PoseVec3& value) -> QVector3D;

const QVector3D k_baked_bow_axis(0.0F, 1.0F, 0.0F);

auto baked_spear_direction() -> QVector3D;

void aim_held_weapon(HumanoidPose& pose,
                     const QVector3D& wanted_direction,
                     const QVector3D& baked_direction);

auto to_pose_vec(const QVector3D& value) -> Animation::PoseVec3;

[[nodiscard]] auto
is_running_locomotion(const HumanoidAnimationContext& anim_ctx) noexcept -> bool;

void apply_weapon_attack_body_deltas(
    HumanoidPose& pose, const Animation::HumanoidWeaponAttackPoseSample& sample);

void apply_bow_draw_body_deltas(HumanoidPose& pose,
                                const Animation::HumanoidBowDrawPoseSample& sample);

void apply_construction_body_deltas(
    HumanoidPose& pose, const Animation::HumanoidConstructionPoseSample& sample);

void apply_held_pose_body_deltas(HumanoidPose& pose,
                                 const Animation::HumanoidHeldPoseSample& sample);

void apply_ambient_pose_sample(HumanoidPose& pose,
                               const Animation::HumanoidAmbientPoseSample& sample);

void apply_posture_delta_sample(HumanoidPose& pose,
                                const Animation::HumanoidPostureDeltaSample& sample);

auto weapon_attack_pose_inputs(Animation::HumanoidWeaponAttackKind kind,
                               const HumanoidAnimationContext& anim_ctx,
                               float attack_phase,
                               std::uint8_t variant,
                               float reach_scale,
                               float hold_depth = 0.0F)
    -> Animation::HumanoidWeaponAttackPoseInputs;

void apply_weapon_attack_sample(
    HumanoidPoseController& controller,
    HumanoidPose& pose,
    const Animation::HumanoidWeaponAttackPoseSample& sample);

void apply_spear_attack_sample(HumanoidPoseController& controller,
                               HumanoidPose& pose,
                               const Animation::HumanoidWeaponAttackPoseSample& sample);

void apply_held_pose_sample(HumanoidPoseController& controller,
                            HumanoidPose& pose,
                            const Animation::HumanoidHeldPoseSample& sample);

} // namespace pose_controller_detail

} // namespace Render::GL
