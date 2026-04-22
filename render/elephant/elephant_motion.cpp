#include "elephant_motion.h"

#include "dimensions.h"
#include "elephant_spec.h"

#include "../gl/humanoid/animation/animation_inputs.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Render::GL {

namespace {

constexpr float k_pi = std::numbers::pi_v<float>;

inline auto swing_ease(float t) -> float { return t * t * (3.0F - 2.0F * t); }

inline auto swing_arc(float t) -> float { return 4.0F * t * (1.0F - t); }

inline auto
get_default_foot_position(const ElephantDimensions &d, int leg_index,
                          const QVector3D &barrel_center) -> QVector3D {
  bool const is_front = (leg_index < 2);
  bool const is_left = (leg_index == 0 || leg_index == 2);
  float const lateral_sign = is_left ? 1.0F : -1.0F;

  float const forward_offset =
      is_front ? d.body_length * 0.48F : -d.body_length * 0.48F;

  QVector3D const hip =
      barrel_center + QVector3D(lateral_sign * d.body_width * 0.52F,
                                -d.body_height * 0.42F, forward_offset);

  return QVector3D(hip.x(), 0.0F, hip.z());
}

inline auto calculate_swing_target(const ElephantDimensions &d, int leg_index,
                                   const QVector3D &barrel_center,
                                   float stride_length) -> QVector3D {
  QVector3D const default_pos =
      get_default_foot_position(d, leg_index, barrel_center);

  return default_pos + QVector3D(0.0F, 0.0F, stride_length);
}

inline auto body_sway_for_motion(bool is_moving, float phase,
                                 float time) -> float {
  return is_moving ? std::sin(phase * 2.0F * k_pi) * 0.015F
                   : std::sin(time * 0.3F) * 0.008F;
}

} // namespace

auto compute_howdah_frame(const ElephantProfile &profile)
    -> HowdahAttachmentFrame {
  using namespace HowdahFrameConstants;
  const ElephantDimensions &d = profile.dims;
  HowdahAttachmentFrame frame{};

  frame.seat_forward = QVector3D(0.0F, 0.0F, 1.0F);
  frame.seat_right = QVector3D(1.0F, 0.0F, 0.0F);
  frame.seat_up = QVector3D(0.0F, 1.0F, 0.0F);

  frame.ground_offset = QVector3D(
      0.0F, -d.barrel_center_y + d.leg_length * kLegRevealLiftScale, 0.0F);

  frame.howdah_center = QVector3D(
      0.0F, d.barrel_center_y + d.body_height * kHowdahBodyHeightOffset,
      d.body_length * kHowdahBodyLengthOffset);

  frame.seat_position =
      frame.howdah_center +
      QVector3D(0.0F, d.howdah_height * kSeatHeightOffset, 0.0F);

  return frame;
}

auto evaluate_elephant_motion(const ElephantProfile &profile,
                              const AnimationInputs &anim)
    -> ElephantMotionSample {
  ElephantMotionSample sample{};
  const ElephantGait &g = profile.gait;
  const ElephantDimensions &d = profile.dims;
  sample.gait = g;

  sample.is_moving = anim.is_moving;
  sample.is_fighting =
      anim.is_attacking ||
      (anim.combat_phase != Render::GL::CombatAnimPhase::Idle);

  if (sample.is_moving) {
    float const cycle_progress = std::fmod(anim.time / g.cycle_time, 1.0F);
    sample.phase = cycle_progress;
    sample.bob = std::sin(cycle_progress * 2.0F * k_pi) * d.move_bob_amplitude;
  } else {
    sample.phase = std::fmod(anim.time * 0.3F, 1.0F);
    sample.bob = std::sin(anim.time * 0.5F) * d.idle_bob_amplitude;
  }

  float const trunk_primary = std::sin(anim.time * 0.8F) * 0.15F;
  float const trunk_secondary = std::sin(anim.time * 1.3F + 0.5F) * 0.08F;
  sample.trunk_swing = trunk_primary + trunk_secondary;

  float const ear_base = std::sin(anim.time * 0.6F);
  sample.ear_flap = sample.is_moving ? ear_base * 0.25F : ear_base * 0.12F;

  sample.body_sway =
      body_sway_for_motion(sample.is_moving, sample.phase, anim.time);

  sample.barrel_center =
      QVector3D(sample.body_sway, d.barrel_center_y + sample.bob, 0.0F);
  sample.chest_center =
      sample.barrel_center +
      QVector3D(0.0F, d.body_height * 0.10F, d.body_length * 0.30F);
  sample.rump_center =
      sample.barrel_center +
      QVector3D(0.0F, d.body_height * 0.02F, -d.body_length * 0.32F);
  sample.neck_base =
      sample.chest_center +
      QVector3D(0.0F, d.body_height * 0.25F, d.body_length * 0.15F);
  sample.neck_top =
      sample.neck_base +
      QVector3D(0.0F, d.neck_length * 0.60F, d.neck_length * 0.50F);
  sample.head_center =
      sample.neck_top +
      QVector3D(0.0F, d.head_height * 0.20F, d.head_length * 0.35F);

  sample.howdah = compute_howdah_frame(profile);
  apply_howdah_vertical_offset(sample.howdah, sample.bob);

  return sample;
}

auto build_elephant_reduced_motion(const ElephantMotionSample &motion,
                                   const AnimationInputs &anim)
    -> Render::Elephant::ElephantReducedMotion {
  return Render::Elephant::ElephantReducedMotion{
      .phase = motion.phase,
      .bob = motion.bob,
      .is_moving = motion.is_moving,
      .is_fighting = motion.is_fighting,
      .anim_time = anim.time,
      .combat_phase = anim.combat_phase,
  };
}

void apply_howdah_vertical_offset(HowdahAttachmentFrame &frame, float bob) {
  QVector3D const offset(0.0F, bob, 0.0F);
  frame.howdah_center += offset;
  frame.seat_position += offset;
}

auto evaluate_swing_position(const ElephantLegState &leg,
                             float lift_height) -> QVector3D {
  float const t = leg.swing_progress;
  float const eased_t = swing_ease(t);

  QVector3D const horizontal =
      leg.swing_start * (1.0F - eased_t) + leg.swing_target * eased_t;

  float const lift = swing_arc(t) * lift_height;

  return QVector3D(horizontal.x(), horizontal.y() + lift, horizontal.z());
}

auto solve_elephant_leg_ik(const QVector3D &hip, const QVector3D &foot_target,
                           float upper_len, float lower_len,
                           float lateral_sign) -> ElephantLegPose {
  ElephantLegPose pose{};
  pose.hip = hip;

  QVector3D const to_foot = foot_target - hip;
  float const reach = to_foot.length();

  float const max_reach = upper_len + lower_len - 0.01F;
  float const min_reach = std::abs(upper_len - lower_len) + 0.01F;
  float const clamped_reach = std::clamp(reach, min_reach, max_reach);

  QVector3D const reach_dir =
      reach > 0.001F ? to_foot / reach : QVector3D(0.0F, -1.0F, 0.0F);

  QVector3D const actual_foot = hip + reach_dir * clamped_reach;
  pose.foot = actual_foot;
  pose.ankle = actual_foot + QVector3D(0.0F, lower_len * 0.08F, 0.0F);

  float const a2 = upper_len * upper_len;
  float const b2 = lower_len * lower_len;
  float const c2 = clamped_reach * clamped_reach;

  float const cos_hip_angle =
      (a2 + c2 - b2) / (2.0F * upper_len * clamped_reach);
  float const hip_angle = std::acos(std::clamp(cos_hip_angle, -1.0F, 1.0F));

  QVector3D const up(0.0F, 1.0F, 0.0F);
  QVector3D bend_axis = QVector3D::crossProduct(reach_dir, up);
  if (bend_axis.lengthSquared() < 0.001F) {

    bend_axis = QVector3D(lateral_sign, 0.0F, 0.0F);
  }
  bend_axis.normalize();

  QMatrix4x4 rot;
  rot.setToIdentity();
  rot.rotate(hip_angle * 180.0F / 3.14159265F, bend_axis);

  QVector3D const knee_dir = rot.map(reach_dir);
  pose.knee = hip + knee_dir * upper_len;

  return pose;
}

auto get_leg_phase_offset(int leg_index) -> float {
  using namespace GaitSystemConstants;
  switch (leg_index) {
  case 0:
    return kLegPhaseFL;
  case 1:
    return kLegPhaseFR;
  case 2:
    return kLegPhaseRL;
  case 3:
    return kLegPhaseRR;
  default:
    return 0.0F;
  }
}

auto is_leg_in_swing(float cycle_phase, int leg_index) -> bool {
  using namespace GaitSystemConstants;
  float const leg_phase =
      std::fmod(cycle_phase - get_leg_phase_offset(leg_index) + 1.0F, 1.0F);
  return leg_phase < kSwingDuration;
}

auto get_swing_progress(float cycle_phase, int leg_index) -> float {
  using namespace GaitSystemConstants;
  float const leg_phase =
      std::fmod(cycle_phase - get_leg_phase_offset(leg_index) + 1.0F, 1.0F);
  if (leg_phase < kSwingDuration) {
    return leg_phase / kSwingDuration;
  }
  return -1.0F;
}

void update_elephant_gait(ElephantGaitState &state,
                          const ElephantProfile &profile,
                          const AnimationInputs &anim,
                          const QVector3D &body_world_pos,
                          float body_forward_z) {
  using namespace GaitSystemConstants;
  const ElephantDimensions &d = profile.dims;
  const ElephantGait &g = profile.gait;

  if (!state.initialized) {
    QVector3D const barrel_center(0.0F, d.barrel_center_y, 0.0F);
    for (int i = 0; i < 4; ++i) {
      state.legs[i].planted_foot =
          get_default_foot_position(d, i, barrel_center);
      state.legs[i].swing_start = state.legs[i].planted_foot;
      state.legs[i].swing_target = state.legs[i].planted_foot;
      state.legs[i].in_swing = false;
      state.legs[i].swing_progress = 0.0F;
    }
    state.initialized = true;
  }

  if (anim.is_moving) {
    state.cycle_phase = std::fmod(anim.time / g.cycle_time, 1.0F);
  } else {

    state.cycle_phase = 0.0F;
    for (int i = 0; i < 4; ++i) {
      state.legs[i].in_swing = false;
    }
  }

  QVector3D const barrel_center(0.0F, d.barrel_center_y, 0.0F);

  float const stride_length = g.stride_swing * 1.8F;

  for (int i = 0; i < 4; ++i) {
    ElephantLegState &leg = state.legs[i];
    float const swing_progress = get_swing_progress(state.cycle_phase, i);

    if (swing_progress >= 0.0F && anim.is_moving) {

      if (!leg.in_swing) {

        leg.swing_start = leg.planted_foot;
        leg.swing_target =
            calculate_swing_target(d, i, barrel_center, stride_length);
        leg.in_swing = true;
      }
      leg.swing_progress = swing_progress;
    } else {

      if (leg.in_swing) {

        leg.planted_foot = leg.swing_target;
        leg.in_swing = false;
      }
      leg.swing_progress = 0.0F;
    }
  }

  float total_x = 0.0F;
  float total_z = 0.0F;
  int planted_count = 0;

  for (int i = 0; i < 4; ++i) {
    if (!state.legs[i].in_swing) {
      total_x += state.legs[i].planted_foot.x();
      total_z += state.legs[i].planted_foot.z();
      ++planted_count;
    }
  }

  if (planted_count > 0) {
    float const center_x = total_x / static_cast<float>(planted_count);
    float const center_z = total_z / static_cast<float>(planted_count);

    state.weight_shift_x = -center_x * kWeightShiftLateral;
    state.weight_shift_z = -center_z * kWeightShiftForeAft * 0.5F;
  }

  if (anim.is_moving) {
    float const cycle_sin = std::sin(state.cycle_phase * 2.0F * k_pi);
    state.shoulder_lag = cycle_sin * kShoulderLagFactor;
    state.hip_lag = -cycle_sin * kHipLagFactor;
  } else {
    state.shoulder_lag *= 0.9F;
    state.hip_lag *= 0.9F;
  }
}

} // namespace Render::GL
