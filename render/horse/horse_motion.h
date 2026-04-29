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

constexpr float k_saddle_thickness_offset = 0.05F;
constexpr float k_saddle_body_length_offset = -0.02F;
constexpr float k_saddle_seat_forward_scale = 0.15F;
constexpr float k_seat_position_height_scale = 0.95F;
constexpr float k_saddle_body_height_lift_scale = 0.18F;
constexpr float k_seat_body_height_lift_scale = 0.26F;

constexpr float k_stirrup_width_scale = 0.92F;
constexpr float k_stirrup_thickness_offset = 0.10F;
constexpr float k_stirrup_forward_scale = 0.28F;

constexpr float k_neck_top_body_height_scale = 1.16F;
constexpr float k_neck_top_body_length_scale = 0.73F;
constexpr float k_head_center_height_scale = 0.12F;
constexpr float k_head_center_length_scale = 0.40F;

constexpr float k_muzzle_height_offset = 0.12F;
constexpr float k_muzzle_length_offset = 0.34F;
constexpr float k_bridle_height_offset = 0.05F;
constexpr float k_bridle_length_offset = 0.08F;

constexpr float k_bit_width_offset = 0.55F;
constexpr float k_bit_height_offset = 0.08F;
constexpr float k_bit_length_offset = 0.04F;

} // namespace MountFrameConstants

namespace ReinConstants {

constexpr uint32_t k_slack_seed_salt = 0x707U;
constexpr float k_base_slack_scale = 0.08F;
constexpr float k_base_slack_offset = 0.02F;
constexpr float k_target_tension_bonus = 0.25F;
constexpr float k_attack_tension_bonus = 0.35F;
constexpr float k_min_slack = 0.01F;

constexpr float k_handle_right_offset = 0.08F;
constexpr float k_handle_forward_base = 0.18F;
constexpr float k_handle_forward_tension_scale = 0.18F;
constexpr float k_handle_up_base = -0.10F;
constexpr float k_handle_up_slack_scale = -0.30F;
constexpr float k_handle_up_tension_scale = 0.04F;
constexpr float k_dir_length_threshold = 1e-4F;
constexpr float k_rein_base_length = 0.85F;
constexpr float k_slack_length_scale = 0.12F;

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
