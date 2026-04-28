#include "horse_motion.h"

#include "../creature/animation_state_components.h"
#include "../creature/creature_math_utils.h"
#include "../gl/humanoid/humanoid_types.h"
#include "dimensions.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace Render::GL {

using Render::Creature::hash01;

namespace {

constexpr float k_pi = std::numbers::pi_v<float>;
constexpr float kEpsilon = 1.0e-4F;

auto wrap_phase(float phase) -> float {
  phase = std::fmod(phase, 1.0F);
  if (phase < 0.0F) {
    phase += 1.0F;
  }
  return phase;
}

auto normalized_or(const QVector3D &v, const QVector3D &fallback) -> QVector3D {
  if (v.lengthSquared() <= kEpsilon) {
    return fallback;
  }
  return v.normalized();
}

auto resolve_turn_amount(const HumanoidAnimationContext &rider_ctx,
                         float rider_intensity) -> float {
  QVector3D heading = rider_ctx.heading_forward();
  heading.setY(0.0F);
  heading = normalized_or(heading, QVector3D(0.0F, 0.0F, 1.0F));

  QVector3D velocity = rider_ctx.locomotion_velocity_flat();
  velocity.setY(0.0F);
  QVector3D const velocity_dir = normalized_or(velocity, heading);

  float const signed_vel_turn = std::clamp(
      QVector3D::crossProduct(heading, velocity_dir).y(), -1.0F, 1.0F);
  float const yaw_turn =
      std::clamp(rider_ctx.yaw_radians / (k_pi * (1.0F / 3.0F)), -1.0F, 1.0F);
  return std::clamp((yaw_turn * 0.55F + signed_vel_turn * 0.45F) *
                        rider_intensity,
                    -1.0F, 1.0F);
}

auto resolve_stop_intent(float speed, bool is_moving,
                         const HumanoidAnimationContext &rider_ctx) -> float {
  if (!is_moving) {
    return 1.0F;
  }
  float const normalized_low_speed =
      std::clamp((3.0F - speed) / 3.0F, 0.0F, 1.0F);
  float const target_bias = rider_ctx.gait.has_target ? 1.0F : 0.70F;
  return normalized_low_speed * target_bias;
}

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
      0.0F,
      d.saddle_height + d.body_height * kSaddleBodyHeightLiftScale -
          d.saddle_thickness * kSaddleThicknessOffset,
      -d.body_length * kSaddleBodyLengthOffset +
          d.seat_forward_offset * kSaddleSeatForwardScale);

  frame.seat_position =
      frame.saddle_center +
      QVector3D(0.0F,
                d.saddle_thickness * kSeatPositionHeightScale +
                    d.body_height * kSeatBodyHeightLiftScale,
                0.0F);

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

namespace {

constexpr float k_idle_breathing_primary_freq = 0.4F;
constexpr float k_idle_breathing_secondary_freq = 0.15F;
constexpr float k_idle_breathing_primary_weight = 0.6F;
constexpr float k_idle_breathing_secondary_weight = 0.4F;
constexpr float k_idle_phase_speed = 0.15F;
constexpr float k_gait_transition_duration = 0.3F;

auto bob_scale_for_gait(GaitType gait) noexcept -> float {
  switch (gait) {
  case GaitType::IDLE:
    return 0.30F;
  case GaitType::WALK:
    return 0.50F;
  case GaitType::TROT:
    return 0.85F;
  case GaitType::CANTER:
    return 1.00F;
  case GaitType::GALLOP:
    return 1.12F;
  }
  return 0.30F;
}

void update_target_gait(Render::Creature::HorseAnimationStateComponent &state,
                        GaitType desired, float anim_time,
                        float idle_bob_intensity) {
  if (desired == GaitType::IDLE) {
    state.idle_bob_intensity = std::clamp(idle_bob_intensity, 0.0F, 1.5F);
  } else {
    state.idle_bob_intensity = 1.0F;
  }
  if (state.target_gait != desired) {
    state.target_gait = desired;
    state.gait_transition_progress = 0.0F;
    state.transition_start_time = anim_time;
  }
}

auto resolve_persistent_gait(
    Render::Creature::HorseAnimationStateComponent &state,
    const HorseProfile &profile, float anim_time) -> HorseGait {
  if (state.gait_transition_progress < 1.0F) {
    float const elapsed = anim_time - state.transition_start_time;
    state.gait_transition_progress =
        std::clamp(elapsed / k_gait_transition_duration, 0.0F, 1.0F);
    if (state.gait_transition_progress >= 1.0F) {
      state.current_gait = state.target_gait;
    }
  } else {
    state.current_gait = state.target_gait;
  }

  HorseGait const current = gait_for_type(state.current_gait, profile.gait);
  if (state.gait_transition_progress < 1.0F &&
      state.current_gait != state.target_gait) {
    HorseGait const target = gait_for_type(state.target_gait, profile.gait);
    float const t = state.gait_transition_progress;
    float const eased = t * t * (3.0F - 2.0F * t);
    return blend_gaits(current, target, eased);
  }
  return current;
}

void evaluate_phase_and_bob(
    Render::Creature::HorseAnimationStateComponent &state,
    const HorseProfile &profile, const AnimationInputs &anim,
    const HumanoidAnimationContext &rider_ctx, const HorseGait &resolved,
    float rider_intensity, float &out_phase, float &out_bob) {
  bool const is_moving = state.current_gait != GaitType::IDLE ||
                         state.target_gait != GaitType::IDLE;
  float const phase_offset = resolved.phase_offset;

  if (is_moving) {
    float const cycle_time = std::max(resolved.cycle_time, 0.0001F);
    if (rider_ctx.gait.cycle_time > 0.0001F) {
      out_phase = wrap_phase(rider_ctx.gait.cycle_phase);
    } else if (state.locomotion_phase_valid) {
      float const elapsed =
          std::max(anim.time - state.locomotion_phase_time, 0.0F);
      out_phase = wrap_phase(state.locomotion_phase + elapsed / cycle_time);
    } else {
      out_phase = wrap_phase(anim.time / cycle_time + phase_offset);
    }
    state.locomotion_phase = out_phase;
    state.locomotion_phase_time = anim.time;
    state.locomotion_phase_valid = true;
    float const base_bob_amp =
        profile.dims.idle_bob_amplitude +
        rider_intensity *
            (profile.dims.move_bob_amplitude - profile.dims.idle_bob_amplitude);
    float const bob_amp = base_bob_amp * bob_scale_for_gait(state.target_gait);

    float const primary_bob = std::sin(out_phase * 2.0F * k_pi);
    float const secondary_bob = std::sin(out_phase * 4.0F * k_pi) * 0.25F;
    float const tertiary_variation =
        std::sin(anim.time * 0.7F + phase_offset * k_pi) * 0.08F + 1.0F;

    float bob_pattern = primary_bob;
    if (state.current_gait == GaitType::TROT) {
      bob_pattern = primary_bob + secondary_bob * 0.5F;
    } else if (state.current_gait == GaitType::CANTER) {
      bob_pattern = primary_bob + std::sin(out_phase * 3.0F * k_pi) * 0.15F;
    } else if (state.current_gait == GaitType::GALLOP) {
      bob_pattern = primary_bob * 1.2F + secondary_bob * 0.3F;
    }
    out_bob = bob_pattern * bob_amp * tertiary_variation;
  } else {
    float const breathing =
        std::sin((anim.time + phase_offset * 2.0F) *
                 k_idle_breathing_primary_freq * 2.0F * k_pi) *
            k_idle_breathing_primary_weight +
        std::sin((anim.time + phase_offset) * k_idle_breathing_secondary_freq *
                 2.0F * k_pi) *
            k_idle_breathing_secondary_weight;
    float const weight_shift =
        std::sin(anim.time * 0.18F + phase_offset * 3.0F) * 0.20F;
    out_phase = wrap_phase(anim.time * k_idle_phase_speed + phase_offset);
    out_bob = (breathing + weight_shift) * profile.dims.idle_bob_amplitude *
              0.8F * state.idle_bob_intensity;
    state.locomotion_phase_valid = false;
  }
}

} // namespace

auto evaluate_horse_motion(const HorseProfile &profile,
                           const AnimationInputs &anim,
                           const HumanoidAnimationContext &rider_ctx,
                           Render::Creature::HorseAnimationStateComponent
                               *io_state) -> HorseMotionSample {
  Render::Creature::HorseAnimationStateComponent fallback_state{};
  Render::Creature::HorseAnimationStateComponent &state =
      io_state != nullptr ? *io_state : fallback_state;

  HorseMotionSample sample{};
  bool const rider_has_motion =
      rider_ctx.is_walking() || rider_ctx.is_running();
  bool const has_locomotion_input = rider_has_motion || anim.is_moving;

  constexpr float kIdleSpeedMax = 0.5F;
  constexpr float kWalkSpeedMax = 3.0F;
  constexpr float kTrotSpeedMax = 5.5F;
  constexpr float kCanterSpeedMax = 8.0F;
  constexpr float kGaitHysteresis = 0.35F;

  float speed = rider_ctx.locomotion_speed();
  if (speed < 0.01F) {
    speed = anim.is_running ? 6.2F : (anim.is_moving ? 1.4F : 0.0F);
  } else if (anim.is_running) {
    speed = std::max(speed, 6.2F);
  }
  sample.rider_intensity =
      std::max(rider_ctx.locomotion_normalized_speed(),
               std::clamp(speed / kCanterSpeedMax, 0.0F, 1.0F));

  auto const select_gait = [&]() -> GaitType {
    if (!has_locomotion_input) {
      return GaitType::IDLE;
    }
    GaitType const anchor = state.target_gait;
    float const idle_up = kIdleSpeedMax + kGaitHysteresis;
    float const idle_dn = kIdleSpeedMax - kGaitHysteresis;
    float const walk_up = kWalkSpeedMax + kGaitHysteresis;
    float const walk_dn = kWalkSpeedMax - kGaitHysteresis;
    float const trot_up = kTrotSpeedMax + kGaitHysteresis;
    float const trot_dn = kTrotSpeedMax - kGaitHysteresis;
    float const canter_up = kCanterSpeedMax + kGaitHysteresis;
    float const canter_dn = kCanterSpeedMax - kGaitHysteresis;
    auto const upshift = [&](float threshold) { return speed >= threshold; };
    auto const downshift = [&](float threshold) { return speed < threshold; };

    switch (anchor) {
    case GaitType::IDLE:
      if (!anim.is_moving && speed < kIdleSpeedMax) {
        return GaitType::IDLE;
      }
      if (upshift(idle_up)) {
        if (upshift(walk_up)) {
          if (upshift(trot_up)) {
            return upshift(canter_up) ? GaitType::GALLOP : GaitType::CANTER;
          }
          return GaitType::TROT;
        }
        return GaitType::WALK;
      }
      return anim.is_moving ? GaitType::WALK : GaitType::IDLE;
    case GaitType::WALK:
      if (downshift(idle_dn) && !anim.is_moving) {
        return GaitType::IDLE;
      }
      if (upshift(walk_up)) {
        return upshift(trot_up) ? GaitType::TROT : GaitType::TROT;
      }
      return GaitType::WALK;
    case GaitType::TROT:
      if (downshift(walk_dn)) {
        return GaitType::WALK;
      }
      if (upshift(trot_up)) {
        return GaitType::CANTER;
      }
      return GaitType::TROT;
    case GaitType::CANTER:
      if (downshift(trot_dn)) {
        return GaitType::TROT;
      }
      if (upshift(canter_up)) {
        return GaitType::GALLOP;
      }
      return GaitType::CANTER;
    case GaitType::GALLOP:
      if (downshift(canter_dn)) {
        return GaitType::CANTER;
      }
      return GaitType::GALLOP;
    }
    return GaitType::IDLE;
  };

  GaitType const desired_gait = select_gait();
  update_target_gait(state, desired_gait, anim.time, 1.0F);

  HorseGait resolved = resolve_persistent_gait(state, profile, anim.time);
  sample.is_moving = state.current_gait != GaitType::IDLE ||
                     state.target_gait != GaitType::IDLE;
  evaluate_phase_and_bob(state, profile, anim, rider_ctx, resolved,
                         sample.rider_intensity, sample.phase, sample.bob);
  sample.is_fighting = anim.is_attacking ||
                       (anim.combat_phase != CombatAnimPhase::Idle);
  sample.gait = resolved;
  sample.gait_type = state.current_gait;
  sample.turn_amount = resolve_turn_amount(rider_ctx, sample.rider_intensity);
  sample.stop_intent =
      resolve_stop_intent(speed, has_locomotion_input, rider_ctx);

  sample.gait.lateral_lead_front =
      std::clamp(0.50F - sample.turn_amount * 0.18F, 0.15F, 0.85F);
  sample.gait.lateral_lead_rear =
      std::clamp(0.50F - sample.turn_amount * 0.12F, 0.18F, 0.82F);

  float const turn_speed_penalty = std::abs(sample.turn_amount);
  sample.gait.cycle_time *=
      1.0F + sample.stop_intent * 0.10F + turn_speed_penalty * 0.04F;
  sample.gait.stride_swing *=
      1.0F - sample.stop_intent * 0.20F - turn_speed_penalty * 0.06F;
  sample.gait.stride_lift *=
      1.0F - sample.stop_intent * 0.26F - turn_speed_penalty * 0.05F;
  sample.gait.stride_swing = std::max(sample.gait.stride_swing, 0.02F);
  sample.gait.stride_lift = std::max(sample.gait.stride_lift, 0.01F);

  float const sway_intensity =
      sample.is_moving ? (1.0F - sample.rider_intensity * 0.5F) : 0.3F;
  float const gait_swing = std::clamp(sample.gait.stride_swing, 0.0F, 1.0F);
  float const gait_lift =
      std::clamp(sample.gait.stride_lift / 0.5F, 0.0F, 1.0F);
  sample.body_sway = sample.is_moving
                         ? std::sin(sample.phase * 2.0F * k_pi) *
                               (0.007F + gait_swing * 0.004F) * sway_intensity
                         : std::sin(anim.time * 0.4F) * 0.005F;
  sample.body_sway +=
      sample.turn_amount * (0.004F + sample.rider_intensity * 0.006F);

  float const pitch_intensity = sample.rider_intensity * 0.7F + 0.1F;
  sample.body_pitch = sample.is_moving
                          ? std::sin((sample.phase + 0.22F) * 2.0F * k_pi) *
                                (0.004F + gait_lift * 0.004F) * pitch_intensity
                          : std::sin(anim.time * 0.25F) * 0.003F;
  sample.body_pitch -= sample.stop_intent * sample.rider_intensity * 0.003F;

  float const nod_base =
      sample.is_moving
          ? std::sin((sample.phase + 0.18F) * 2.0F * k_pi) *
                (0.014F + gait_lift * 0.015F + sample.rider_intensity * 0.010F)
          : std::sin(anim.time * 1.5F) * 0.008F;
  float const nod_secondary = std::sin(anim.time * 0.8F) * 0.003F;
  sample.head_nod =
      (nod_base + nod_secondary) * (1.0F - sample.stop_intent * 0.15F);
  sample.head_lateral = sample.body_sway * (0.40F + gait_swing * 0.10F) +
                        sample.turn_amount * 0.006F;
  sample.spine_flex = sample.is_moving
                          ? std::sin((sample.phase + 0.08F) * 2.0F * k_pi) *
                                (0.002F + gait_lift * 0.0025F) *
                                sample.rider_intensity
                          : 0.0F;
  sample.spine_flex += sample.turn_amount * (0.0015F + gait_lift * 0.001F);
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
