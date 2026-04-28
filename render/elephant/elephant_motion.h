#pragma once

#include "attachment_frames.h"

#include <QVector3D>

namespace Render::Creature {
struct ElephantAnimationStateComponent;
}

namespace Render::Elephant {
struct ElephantPoseMotion;
}

namespace Render::GL {

struct ElephantProfile;
struct AnimationInputs;
struct ElephantDimensions;

namespace HowdahFrameConstants {

constexpr float kHowdahBodyHeightOffset = 0.55F;
constexpr float kHowdahBodyLengthOffset = -0.10F;
constexpr float kSeatHeightOffset = 0.15F;
constexpr float kLegRevealLiftScale = 0.75F;

} // namespace HowdahFrameConstants

namespace GaitSystemConstants {

constexpr float kLegPhaseFL = 0.00F;
constexpr float kLegPhaseFR = 0.50F;
constexpr float kLegPhaseRL = 0.75F;
constexpr float kLegPhaseRR = 0.25F;

constexpr float kSwingDuration = 0.25F;

constexpr float kSwingLiftPeak = 0.22F;
constexpr float kSwingForwardReach = 0.60F;

constexpr float kWeightShiftLateral = 0.025F;
constexpr float kWeightShiftForeAft = 0.015F;

constexpr float kShoulderLagFactor = 0.08F;
constexpr float kHipLagFactor = 0.06F;

constexpr float kFootSettleDepth = 0.015F;
constexpr float kFootSettleDuration = 0.10F;

} // namespace GaitSystemConstants

auto compute_howdah_frame(const ElephantProfile &profile)
    -> HowdahAttachmentFrame;

auto evaluate_elephant_motion(const ElephantProfile &profile,
                              const AnimationInputs &anim,
                              Render::Creature::ElephantAnimationStateComponent
                                  *io_state = nullptr) -> ElephantMotionSample;

auto build_elephant_pose_motion(const ElephantMotionSample &motion,
                                const AnimationInputs &anim)
    -> Render::Elephant::ElephantPoseMotion;

void apply_howdah_vertical_offset(HowdahAttachmentFrame &frame, float bob);

void update_elephant_gait(ElephantGaitState &state,
                          const ElephantProfile &profile,
                          const AnimationInputs &anim,
                          const QVector3D &body_world_pos,
                          float body_forward_z);

auto solve_elephant_leg_ik(const QVector3D &hip, const QVector3D &foot_target,
                           float upper_len, float lower_len,
                           float lateral_sign) -> ElephantLegPose;

auto evaluate_swing_position(const ElephantLegState &leg,
                             float lift_height) -> QVector3D;

auto get_leg_phase_offset(int leg_index) -> float;
auto is_leg_in_swing(float cycle_phase, int leg_index) -> bool;
auto get_swing_progress(float cycle_phase, int leg_index) -> float;

} // namespace Render::GL
