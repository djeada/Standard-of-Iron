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
constexpr float k_two_pi = 2.0F * k_pi;

inline auto swing_ease(float t) -> float { return t * t * (3.0F - 2.0F * t); }

inline auto swing_arc(float t) -> float { return 4.0F * t * (1.0F - t); }

inline auto normalized_locomotion_intensity(bool is_moving, bool is_running,
                                            const ElephantGait &g) -> float {
  if (!is_moving) {
    return 0.0F;
  }
  float const stride_swing = std::clamp(g.stride_swing, 0.0F, 1.0F);
  float const stride_lift = std::clamp(g.stride_lift * 2.2F, 0.0F, 1.0F);
  float const locomotion_boost = is_running ? 0.22F : 0.0F;
  return std::clamp(0.62F + stride_swing * 0.24F + stride_lift * 0.14F +
                        locomotion_boost,
                    0.0F, 1.0F);
}

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

inline auto body_sway_for_motion(bool is_moving, float phase, float time,
                                 float locomotion_intensity,
                                 float stride_swing) -> float {
  if (!is_moving) {
    return std::sin(time * 0.3F) * 0.008F;
  }
  float const primary = std::sin(phase * k_two_pi);
  float const secondary = std::sin((phase + 0.17F) * 2.0F * k_two_pi) * 0.35F;
  float const sway_scale =
      0.008F + std::clamp(stride_swing, 0.0F, 1.0F) * 0.0035F;
  return (primary + secondary) * sway_scale * (0.8F + locomotion_intensity);
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
  sample.is_fighting = anim.is_attacking ||
                       (anim.combat_phase != Render::GL::CombatAnimPhase::Idle);
  float const locomotion_intensity =
      normalized_locomotion_intensity(sample.is_moving, anim.is_running, g);
  float const cycle_time = std::max(g.cycle_time, 0.001F);

  if (sample.is_moving) {
    float const cycle_progress = std::fmod(anim.time / cycle_time, 1.0F);
    sample.phase = cycle_progress;
    float const primary = std::sin(cycle_progress * k_two_pi);
    float const secondary = std::sin((cycle_progress + 0.19F) * 2.0F * k_two_pi);
    sample.bob = (primary * 0.70F + secondary * 0.30F) * d.move_bob_amplitude *
                 (0.88F + locomotion_intensity * 0.18F);
  } else {
    sample.phase = std::fmod(anim.time * 0.3F, 1.0F);
    sample.bob = std::sin(anim.time * 0.45F) * d.idle_bob_amplitude;
  }

  float const trunk_primary =
      sample.is_moving
          ? std::sin((sample.phase + 0.11F) * k_two_pi) *
                (0.10F + locomotion_intensity * 0.05F)
          : std::sin(anim.time * 0.8F) * 0.09F;
  float const trunk_secondary = sample.is_moving
                                    ? std::sin((sample.phase + 0.39F) *
                                               2.0F * k_two_pi) *
                                          0.03F
                                    : std::sin(anim.time * 1.3F + 0.5F) * 0.05F;
  sample.trunk_swing = trunk_primary + trunk_secondary;

  float const ear_base = sample.is_moving
                             ? std::sin((sample.phase + 0.33F) * 2.0F * k_two_pi)
                             : std::sin(anim.time * 0.6F);
  sample.ear_flap = sample.is_moving
                        ? ear_base * (0.16F + locomotion_intensity * 0.08F)
                        : ear_base * 0.10F;

  sample.body_sway = body_sway_for_motion(sample.is_moving, sample.phase,
                                          anim.time, locomotion_intensity,
                                          g.stride_swing);

  float const weight_transfer = std::sin((sample.phase + 0.20F) * k_two_pi);
  float const shoulder_settle =
      sample.is_moving
          ? std::max(0.0F, weight_transfer) * d.move_bob_amplitude *
                (0.50F + locomotion_intensity * 0.10F)
          : 0.0F;
  float const hip_settle =
      sample.is_moving
          ? std::max(0.0F, -weight_transfer) * d.move_bob_amplitude *
                (0.42F + locomotion_intensity * 0.10F)
          : 0.0F;
  float const spine_wave = sample.is_moving
                               ? std::sin((sample.phase + 0.47F) * k_two_pi) *
                                     d.move_bob_amplitude * 0.18F
                               : 0.0F;
  float const head_nod = sample.trunk_swing * 0.16F;

  sample.barrel_center =
      QVector3D(sample.body_sway, d.barrel_center_y + sample.bob, 0.0F);
  sample.chest_center =
      sample.barrel_center +
      QVector3D(0.0F, d.body_height * 0.12F - shoulder_settle + spine_wave,
                d.body_length * 0.30F);
  sample.rump_center =
      sample.barrel_center +
      QVector3D(0.0F, d.body_height * 0.04F - hip_settle - spine_wave * 0.35F,
                -d.body_length * 0.32F);
  sample.neck_base =
      sample.chest_center +
      QVector3D(0.0F, d.body_height * 0.23F + head_nod * 0.35F,
                d.body_length * 0.14F);
  sample.neck_top = sample.neck_base + QVector3D(0.0F, d.neck_length * 0.60F,
                                                 d.neck_length * 0.46F);
  sample.head_center =
      sample.neck_top +
      QVector3D(0.0F, d.head_height * 0.18F + head_nod,
                d.head_length * (0.35F + sample.trunk_swing * 0.04F));

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
  float const cycle_time = std::max(g.cycle_time, 0.001F);
  float const locomotion_scale = anim.is_running ? 1.18F : 1.0F;
  (void)body_world_pos;
  (void)body_forward_z;

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
    state.cycle_phase = std::fmod(anim.time / cycle_time, 1.0F);
  } else {

    state.cycle_phase = 0.0F;
    for (int i = 0; i < 4; ++i) {
      state.legs[i].in_swing = false;
    }
  }

  QVector3D const barrel_center(0.0F, d.barrel_center_y, 0.0F);

  float const stride_length =
      d.body_length * (0.18F + std::clamp(g.stride_swing, 0.0F, 1.0F) * 0.28F) *
      locomotion_scale;

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

    float const target_shift_x = -center_x * kWeightShiftLateral;
    float const target_shift_z = -center_z * kWeightShiftForeAft * 0.5F;
    state.weight_shift_x += (target_shift_x - state.weight_shift_x) * 0.22F;
    state.weight_shift_z += (target_shift_z - state.weight_shift_z) * 0.20F;
  }

  if (anim.is_moving) {
    float const cycle_sin = std::sin(state.cycle_phase * 2.0F * k_pi);
    float const shoulder_target =
        cycle_sin * kShoulderLagFactor * (0.90F + 0.15F * locomotion_scale);
    float const hip_target =
        -cycle_sin * kHipLagFactor * (0.90F + 0.10F * locomotion_scale);
    state.shoulder_lag += (shoulder_target - state.shoulder_lag) * 0.28F;
    state.hip_lag += (hip_target - state.hip_lag) * 0.28F;
  } else {
    state.shoulder_lag *= 0.86F;
    state.hip_lag *= 0.86F;
  }
}

} // namespace Render::GL
