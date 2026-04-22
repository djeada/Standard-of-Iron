#include "horse_motion.h"

#include "../creature/creature_math_utils.h"
#include "../gl/humanoid/humanoid_types.h"
#include "dimensions.h"
#include "horse_animation_controller.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace Render::GL {

using Render::Creature::hash01;

namespace {

constexpr float k_pi = std::numbers::pi_v<float>;

} // namespace

auto compute_mount_frame(const HorseProfile &profile)
    -> MountedAttachmentFrame {
  using namespace MountFrameConstants;
  const HorseDimensions &d = profile.dims;
  MountedAttachmentFrame frame{};

  frame.seat_forward = QVector3D(0.0F, 0.0F, 1.0F);
  frame.seat_right = QVector3D(1.0F, 0.0F, 0.0F);
  frame.seat_up = QVector3D(0.0F, 1.0F, 0.0F);
  frame.ground_offset = QVector3D(0.0F, -d.barrel_center_y, 0.0F);

  frame.saddle_center = QVector3D(
      0.0F, d.saddle_height - d.saddle_thickness * kSaddleThicknessOffset,
      -d.body_length * kSaddleBodyLengthOffset +
          d.seat_forward_offset * kSaddleSeatForwardScale);

  frame.seat_position =
      frame.saddle_center +
      QVector3D(0.0F, d.saddle_thickness * kSeatPositionHeightScale, 0.0F);

  frame.stirrup_attach_left =
      frame.saddle_center +
      QVector3D(-d.body_width * kStirrupWidthScale,
                -d.saddle_thickness * kStirrupThicknessOffset,
                d.seat_forward_offset * kStirrupForwardScale);
  frame.stirrup_attach_right =
      frame.saddle_center +
      QVector3D(d.body_width * kStirrupWidthScale,
                -d.saddle_thickness * kStirrupThicknessOffset,
                d.seat_forward_offset * kStirrupForwardScale);

  frame.stirrup_bottom_left =
      frame.stirrup_attach_left + QVector3D(0.0F, -d.stirrup_drop, 0.0F);
  frame.stirrup_bottom_right =
      frame.stirrup_attach_right + QVector3D(0.0F, -d.stirrup_drop, 0.0F);

  QVector3D const neck_top(
      0.0F,
      d.barrel_center_y + d.body_height * kNeckTopBodyHeightScale + d.neck_rise,
      d.body_length * kNeckTopBodyLengthScale);
  QVector3D const head_center =
      neck_top + QVector3D(0.0F, d.head_height * kHeadCenterHeightScale,
                           d.head_length * kHeadCenterLengthScale);

  QVector3D const muzzle_center =
      head_center + QVector3D(0.0F, -d.head_height * kMuzzleHeightOffset,
                              d.head_length * kMuzzleLengthOffset);
  frame.bridle_base =
      muzzle_center + QVector3D(0.0F, -d.head_height * kBridleHeightOffset,
                                d.muzzle_length * kBridleLengthOffset);
  frame.rein_bit_left =
      muzzle_center + QVector3D(d.head_width * kBitWidthOffset,
                                -d.head_height * kBitHeightOffset,
                                d.muzzle_length * kBitLengthOffset);
  frame.rein_bit_right =
      muzzle_center + QVector3D(-d.head_width * kBitWidthOffset,
                                -d.head_height * kBitHeightOffset,
                                d.muzzle_length * kBitLengthOffset);

  return frame;
}

auto compute_rein_state(uint32_t horse_seed,
                        const HumanoidAnimationContext &rider_ctx)
    -> ReinState {
  using namespace ReinConstants;
  float const base_slack =
      hash01(horse_seed ^ kSlackSeedSalt) * kBaseSlackScale + kBaseSlackOffset;
  float rein_tension = rider_ctx.locomotion_normalized_speed();
  if (rider_ctx.gait.has_target) {
    rein_tension += kTargetTensionBonus;
  }
  if (rider_ctx.is_attacking()) {
    rein_tension += kAttackTensionBonus;
  }
  rein_tension = std::clamp(rein_tension, 0.0F, 1.0F);
  float const rein_slack =
      std::max(kMinSlack, base_slack * (1.0F - rein_tension));
  return ReinState{rein_slack, rein_tension};
}

auto compute_rein_handle(const MountedAttachmentFrame &mount, bool is_left,
                         float slack, float tension) -> QVector3D {
  using namespace ReinConstants;
  float const clamped_slack = std::clamp(slack, 0.0F, 1.0F);
  float const clamped_tension = std::clamp(tension, 0.0F, 1.0F);

  QVector3D const &bit = is_left ? mount.rein_bit_left : mount.rein_bit_right;

  QVector3D desired = mount.seat_position;
  desired +=
      (is_left ? -mount.seat_right : mount.seat_right) * kHandleRightOffset;
  desired +=
      -mount.seat_forward *
      (kHandleForwardBase + clamped_tension * kHandleForwardTensionScale);
  desired +=
      mount.seat_up * (kHandleUpBase + clamped_slack * kHandleUpSlackScale +
                       clamped_tension * kHandleUpTensionScale);

  QVector3D dir = desired - bit;
  if (dir.lengthSquared() < kDirLengthThreshold) {
    dir = -mount.seat_forward;
  }
  dir.normalize();

  float const rein_length = kReinBaseLength + clamped_slack * kSlackLengthScale;
  return bit + dir * rein_length;
}

auto evaluate_horse_motion(const HorseProfile &profile, const AnimationInputs &anim,
                           const HumanoidAnimationContext &rider_ctx)
    -> HorseMotionSample {
  HorseMotionSample sample{};
  HorseAnimationController controller(profile, anim, rider_ctx);
  bool const rider_has_motion =
      rider_ctx.is_walking() || rider_ctx.is_running();
  sample.is_moving = rider_has_motion || anim.is_moving;

  constexpr float kIdleSpeedMax = 0.5F;
  constexpr float kWalkSpeedMax = 3.0F;
  constexpr float kTrotSpeedMax = 5.5F;
  constexpr float kCanterSpeedMax = 8.0F;

  float speed = rider_ctx.locomotion_speed();
  if (speed < 0.01F) {
    speed = anim.is_running ? 6.2F : (anim.is_moving ? 1.4F : 0.0F);
  } else if (anim.is_running) {
    speed = std::max(speed, 6.2F);
  }
  sample.rider_intensity =
      std::max(rider_ctx.locomotion_normalized_speed(),
               std::clamp(speed / kCanterSpeedMax, 0.0F, 1.0F));

  if (sample.is_moving) {
    if (speed < kIdleSpeedMax && !anim.is_moving) {
      controller.idle(1.0F);
    } else if (speed < kWalkSpeedMax) {
      controller.set_gait(GaitType::WALK);
    } else if (speed < kTrotSpeedMax) {
      controller.set_gait(GaitType::TROT);
    } else if (speed < kCanterSpeedMax) {
      controller.set_gait(GaitType::CANTER);
    } else {
      controller.set_gait(GaitType::GALLOP);
    }
  } else {
    controller.idle(1.0F);
  }

  controller.update_gait_parameters();
  sample.gait = controller.get_resolved_gait();
  sample.phase = controller.get_current_phase();
  sample.bob = controller.get_current_bob();

  float const sway_intensity =
      sample.is_moving ? (1.0F - sample.rider_intensity * 0.5F) : 0.3F;
  float const gait_swing = std::clamp(sample.gait.stride_swing, 0.0F, 1.0F);
  float const gait_lift =
      std::clamp(sample.gait.stride_lift / 0.5F, 0.0F, 1.0F);
  sample.body_sway = sample.is_moving
                         ? std::sin(sample.phase * 2.0F * k_pi) *
                               (0.007F + gait_swing * 0.004F) * sway_intensity
                         : std::sin(anim.time * 0.4F) * 0.005F;

  float const pitch_intensity = sample.rider_intensity * 0.7F + 0.1F;
  sample.body_pitch =
      sample.is_moving
          ? std::sin((sample.phase + 0.22F) * 2.0F * k_pi) *
                (0.004F + gait_lift * 0.004F) * pitch_intensity
          : std::sin(anim.time * 0.25F) * 0.003F;

  float const nod_base =
      sample.is_moving
          ? std::sin((sample.phase + 0.18F) * 2.0F * k_pi) *
                (0.014F + gait_lift * 0.015F +
                 sample.rider_intensity * 0.010F)
          : std::sin(anim.time * 1.5F) * 0.008F;
  float const nod_secondary = std::sin(anim.time * 0.8F) * 0.003F;
  sample.head_nod = nod_base + nod_secondary;
  sample.head_lateral = sample.body_sway * (0.40F + gait_swing * 0.10F);
  sample.spine_flex =
      sample.is_moving
          ? std::sin((sample.phase + 0.08F) * 2.0F * k_pi) *
                (0.002F + gait_lift * 0.0025F) * sample.rider_intensity
          : 0.0F;
  return sample;
}

void apply_mount_vertical_offset(MountedAttachmentFrame &frame, float bob) {
  QVector3D const offset(0.0F, bob, 0.0F);
  frame.saddle_center += offset;
  frame.seat_position += offset;
  frame.stirrup_attach_left += offset;
  frame.stirrup_attach_right += offset;
  frame.stirrup_bottom_left += offset;
  frame.stirrup_bottom_right += offset;
  frame.rein_bit_left += offset;
  frame.rein_bit_right += offset;
  frame.bridle_base += offset;
}

} // namespace Render::GL
