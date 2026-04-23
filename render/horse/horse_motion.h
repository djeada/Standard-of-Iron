#pragma once

#include "attachment_frames.h"
#include <QVector3D>
#include <cstdint>

namespace Render::Creature {
struct HorseAnimationStateComponent;
}

namespace Render::GL {

struct AnimationInputs;
struct HumanoidAnimationContext;
struct HorseProfile;

namespace MountFrameConstants {

constexpr float kSaddleThicknessOffset = 0.35F;
constexpr float kSaddleBodyLengthOffset = 0.05F;
constexpr float kSaddleSeatForwardScale = 0.25F;
constexpr float kSeatPositionHeightScale = 0.32F;

constexpr float kStirrupWidthScale = 0.92F;
constexpr float kStirrupThicknessOffset = 0.10F;
constexpr float kStirrupForwardScale = 0.28F;

constexpr float kNeckTopBodyHeightScale = 0.65F;
constexpr float kNeckTopBodyLengthScale = 0.25F;
constexpr float kHeadCenterHeightScale = 0.10F;
constexpr float kHeadCenterLengthScale = 0.40F;

constexpr float kMuzzleHeightOffset = 0.18F;
constexpr float kMuzzleLengthOffset = 0.58F;
constexpr float kBridleHeightOffset = 0.05F;
constexpr float kBridleLengthOffset = 0.20F;

constexpr float kBitWidthOffset = 0.55F;
constexpr float kBitHeightOffset = 0.08F;
constexpr float kBitLengthOffset = 0.10F;

} // namespace MountFrameConstants

namespace ReinConstants {

constexpr uint32_t kSlackSeedSalt = 0x707U;
constexpr float kBaseSlackScale = 0.08F;
constexpr float kBaseSlackOffset = 0.02F;
constexpr float kTargetTensionBonus = 0.25F;
constexpr float kAttackTensionBonus = 0.35F;
constexpr float kMinSlack = 0.01F;

constexpr float kHandleRightOffset = 0.08F;
constexpr float kHandleForwardBase = 0.18F;
constexpr float kHandleForwardTensionScale = 0.18F;
constexpr float kHandleUpBase = -0.10F;
constexpr float kHandleUpSlackScale = -0.30F;
constexpr float kHandleUpTensionScale = 0.04F;
constexpr float kDirLengthThreshold = 1e-4F;
constexpr float kReinBaseLength = 0.85F;
constexpr float kSlackLengthScale = 0.12F;

} // namespace ReinConstants

auto compute_mount_frame(const HorseProfile &profile) -> MountedAttachmentFrame;
auto compute_rein_state(uint32_t horse_seed,
                        const HumanoidAnimationContext &rider_ctx) -> ReinState;
auto compute_rein_handle(const MountedAttachmentFrame &mount, bool is_left,
                         float slack, float tension) -> QVector3D;

auto evaluate_horse_motion(const HorseProfile &profile,
                           const AnimationInputs &anim,
                           const HumanoidAnimationContext &rider_ctx,
                           Render::Creature::HorseAnimationStateComponent
                               *io_state = nullptr) -> HorseMotionSample;
void apply_mount_vertical_offset(MountedAttachmentFrame &frame, float bob);

} // namespace Render::GL
