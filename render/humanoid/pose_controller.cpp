#include "pose_controller.h"
#include "humanoid_math.h"
#include "spear_pose_utils.h"
#include <QVector3D>
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <optional>

namespace Render::GL {

namespace {

auto hash_to_unit(std::uint32_t x) -> float {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return float(x & 0x7FFFFFU) / float(0x7FFFFFU);
}

auto hash_u32(std::uint32_t x) -> std::uint32_t {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

constexpr float k_min_idle_duration = 5.0F;
constexpr float k_ambient_duration = 6.0F;
constexpr float k_initial_delay_base = 0.60F;
constexpr float k_initial_delay_range = 40.0F;
constexpr float k_base_cycle_period = 60.0F;
constexpr float k_cycle_period_range = 40.0F;
constexpr float k_tap_frequency_multiplier = 6.0F;

struct AmbientIdleSchedule {
  AmbientIdleType type{AmbientIdleType::None};
  float phase{0.0F};
};

auto compute_ambient_idle_schedule(std::uint32_t seed, float idle_duration)
    -> std::optional<AmbientIdleSchedule> {
  if (idle_duration < k_min_idle_duration) {
    return std::nullopt;
  }

  float const initial_delay =
      k_initial_delay_base +
      hash_to_unit(seed ^ 0x41D64E6DU) * k_initial_delay_range;
  float const ambient_elapsed =
      idle_duration - k_min_idle_duration - initial_delay;
  if (ambient_elapsed < 0.0F) {
    return std::nullopt;
  }

  float const cycle_period =
      k_base_cycle_period +
      hash_to_unit(seed ^ 0x9E3779B9U) * k_cycle_period_range;
  float const cycle_time = std::fmod(ambient_elapsed, cycle_period);
  if (cycle_time < 0.0F || cycle_time >= k_ambient_duration) {
    return std::nullopt;
  }

  auto const cycle_number =
      static_cast<std::uint32_t>(ambient_elapsed / cycle_period);
  std::uint32_t const anim_selector =
      hash_u32(seed ^ (cycle_number * 1664525U) ^ 0xB5297A4DU);
  constexpr std::array<AmbientIdleType, 4> k_baked_types{
      AmbientIdleType::SitDown, AmbientIdleType::Jump,
      AmbientIdleType::RaiseWeapon, AmbientIdleType::ShiftWeight};

  AmbientIdleSchedule schedule;
  schedule.type = k_baked_types[anim_selector % k_baked_types.size()];
  schedule.phase = std::clamp(cycle_time / k_ambient_duration, 0.0F, 1.0F);
  return schedule;
}

auto smooth01(float t) noexcept -> float {
  t = std::clamp(t, 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

auto plateau01(float phase, float enter_end,
               float exit_start) noexcept -> float {
  if (phase < enter_end) {
    return smooth01(phase / enter_end);
  }
  if (phase < exit_start) {
    return 1.0F;
  }
  return smooth01((1.0F - phase) / (1.0F - exit_start));
}

} // namespace

HumanoidPoseController::HumanoidPoseController(
    HumanoidPose &pose, const HumanoidAnimationContext &anim_ctx)
    : m_pose(pose), m_anim_ctx(anim_ctx) {}

void HumanoidPoseController::stand_idle() {}

void HumanoidPoseController::apply_micro_idle(float time, std::uint32_t seed) {
  using HP = HumanProportions;

  float const breath_phase_off =
      hash_to_unit(seed ^ 0x1A2B3C4DU) * 2.0F * std::numbers::pi_v<float>;
  float const sway_phase_off =
      hash_to_unit(seed ^ 0x5E6F7A8BU) * 2.0F * std::numbers::pi_v<float>;
  float const head_phase_off =
      hash_to_unit(seed ^ 0x9CDE0F12U) * 2.0F * std::numbers::pi_v<float>;

  float const breath_freq = 0.85F + hash_to_unit(seed ^ 0x33445566U) * 0.45F;
  float const sway_freq = 0.18F + hash_to_unit(seed ^ 0x778899AAU) * 0.10F;
  float const head_freq = 0.32F + hash_to_unit(seed ^ 0xBBCCDDEEU) * 0.18F;

  float const two_pi = 2.0F * std::numbers::pi_v<float>;

  float const breath = std::sin(time * breath_freq * two_pi + breath_phase_off);
  float const breath_amp = 0.012F;
  float const chest_lift = breath * breath_amp;

  m_pose.shoulder_l.setY(m_pose.shoulder_l.y() + chest_lift);
  m_pose.shoulder_r.setY(m_pose.shoulder_r.y() + chest_lift);
  m_pose.neck_base.setY(m_pose.neck_base.y() + chest_lift * 0.75F);
  m_pose.head_pos.setY(m_pose.head_pos.y() + chest_lift * 0.55F);

  float const breath_torso_z = breath * 0.005F;
  m_pose.shoulder_l.setZ(m_pose.shoulder_l.z() - breath_torso_z);
  m_pose.shoulder_r.setZ(m_pose.shoulder_r.z() - breath_torso_z);

  float const weight_shift =
      std::sin(time * sway_freq * two_pi + sway_phase_off);
  float const sway_amp = 0.016F;
  float const lateral = weight_shift * sway_amp;
  m_pose.pelvis_pos.setX(m_pose.pelvis_pos.x() + lateral);
  m_pose.shoulder_l.setX(m_pose.shoulder_l.x() + lateral * 0.4F);
  m_pose.shoulder_r.setX(m_pose.shoulder_r.x() + lateral * 0.4F);
  m_pose.head_pos.setX(m_pose.head_pos.x() + lateral * 0.25F);
  float const weight_dip = -std::abs(weight_shift) * 0.005F;
  m_pose.pelvis_pos.setY(m_pose.pelvis_pos.y() + weight_dip);

  float const head_yaw = std::sin(time * head_freq * two_pi + head_phase_off);
  float const head_yaw_amp = 0.014F;
  m_pose.head_pos.setX(m_pose.head_pos.x() + head_yaw * head_yaw_amp);
  float const head_nod =
      std::sin(time * head_freq * 0.7F * two_pi + head_phase_off * 0.6F) *
      0.006F;
  m_pose.head_pos.setZ(m_pose.head_pos.z() + head_nod);

  float const arm_drift =
      std::sin(time * (breath_freq * 0.55F) * two_pi + breath_phase_off);
  float const arm_drift_amp = 0.009F;
  m_pose.hand_l.setY(m_pose.hand_l.y() + arm_drift * arm_drift_amp);
  m_pose.hand_r.setY(m_pose.hand_r.y() + arm_drift * arm_drift_amp * 0.85F);

  (void)HP::SHOULDER_Y;
}

auto HumanoidPoseController::get_ambient_idle_type(
    float time, std::uint32_t seed, float idle_duration) -> AmbientIdleType {
  (void)time;
  auto const schedule = compute_ambient_idle_schedule(seed, idle_duration);
  return schedule.has_value() ? schedule->type : AmbientIdleType::None;
}

void HumanoidPoseController::apply_ambient_idle(float time, std::uint32_t seed,
                                                float idle_duration) {
  (void)time;
  auto const schedule = compute_ambient_idle_schedule(seed, idle_duration);
  if (!schedule.has_value()) {
    return;
  }
  apply_ambient_idle_explicit(schedule->type, schedule->phase);
}

auto HumanoidPoseController::compute_ambient_idle_phase(
    float idle_duration, std::uint32_t seed) -> float {
  auto const schedule = compute_ambient_idle_schedule(seed, idle_duration);
  return schedule.has_value() ? schedule->phase : 0.0F;
}

void HumanoidPoseController::apply_ambient_idle_explicit(
    AmbientIdleType idle_type, float phase) {
  using HP = HumanProportions;

  if (idle_type == AmbientIdleType::None) {
    return;
  }

  phase = std::clamp(phase, 0.0F, 1.0F);

  float const intensity = std::sin(phase * std::numbers::pi_v<float>);

  switch (idle_type) {
  case AmbientIdleType::SitDown: {

    float const sit_intensity = plateau01(phase, 0.30F, 0.74F);

    float const sit_drop = sit_intensity * 0.18F;

    m_pose.pelvis_pos.setY(m_pose.pelvis_pos.y() - sit_drop);
    m_pose.shoulder_l.setY(m_pose.shoulder_l.y() - sit_drop * 0.70F);
    m_pose.shoulder_r.setY(m_pose.shoulder_r.y() - sit_drop * 0.70F);
    m_pose.neck_base.setY(m_pose.neck_base.y() - sit_drop * 0.62F);
    m_pose.head_pos.setY(m_pose.head_pos.y() - sit_drop * 0.55F);
    m_pose.head_pos.setZ(m_pose.head_pos.z() + sit_intensity * 0.025F);

    m_pose.knee_l.setY(m_pose.knee_l.y() - sit_drop * 0.30F);
    m_pose.knee_r.setY(m_pose.knee_r.y() - sit_drop * 0.30F);
    m_pose.knee_l.setZ(m_pose.knee_l.z() + sit_intensity * 0.09F);
    m_pose.knee_r.setZ(m_pose.knee_r.z() + sit_intensity * 0.09F);

    m_pose.foot_l.setX(m_pose.foot_l.x() - sit_intensity * 0.025F);
    m_pose.foot_r.setX(m_pose.foot_r.x() + sit_intensity * 0.025F);
    break;
  }

  case AmbientIdleType::ShuffleFeet: {

    float const shuffle_phase = phase * 2.0F * std::numbers::pi_v<float>;
    float const shuffle_amount = std::sin(shuffle_phase) * intensity * 0.04F;

    m_pose.foot_l.setZ(m_pose.foot_l.z() + shuffle_amount);
    m_pose.foot_r.setZ(m_pose.foot_r.z() - shuffle_amount);
    m_pose.knee_l.setZ(m_pose.knee_l.z() + shuffle_amount * 0.5F);
    m_pose.knee_r.setZ(m_pose.knee_r.z() - shuffle_amount * 0.5F);
    break;
  }

  case AmbientIdleType::TapFoot: {

    float const tap_phase = std::fmod(phase * k_tap_frequency_multiplier, 1.0F);
    float const tap_lift =
        (tap_phase < 0.3F)
            ? std::sin(tap_phase / 0.3F * std::numbers::pi_v<float>)
            : 0.0F;
    float const tap_amount = tap_lift * intensity * 0.03F;

    m_pose.foot_r.setY(m_pose.foot_r.y() + tap_amount);
    m_pose.knee_r.setY(m_pose.knee_r.y() + tap_amount * 0.3F);
    break;
  }

  case AmbientIdleType::ShiftWeight: {

    float const shift_hold = plateau01(phase, 0.32F, 0.78F);
    float const settle =
        std::sin(phase * 2.0F * std::numbers::pi_v<float>) * 0.012F;
    float const shift_amount = shift_hold * 0.075F + settle * intensity;

    m_pose.pelvis_pos.setX(m_pose.pelvis_pos.x() + shift_amount);
    m_pose.shoulder_l.setX(m_pose.shoulder_l.x() + shift_amount * 0.45F);
    m_pose.shoulder_r.setX(m_pose.shoulder_r.x() + shift_amount * 0.45F);
    m_pose.neck_base.setX(m_pose.neck_base.x() + shift_amount * 0.30F);
    m_pose.head_pos.setX(m_pose.head_pos.x() + shift_amount * 0.22F);
    m_pose.head_pos.setZ(m_pose.head_pos.z() - shift_hold * 0.018F);

    m_pose.knee_l.setY(m_pose.knee_l.y() - shift_hold * 0.025F);
    m_pose.knee_r.setY(m_pose.knee_r.y() + shift_hold * 0.012F);
    break;
  }

  case AmbientIdleType::StepInPlace: {

    float step_phase = phase * 2.0F;
    bool const is_left_step = step_phase < 1.0F;
    if (!is_left_step) {
      step_phase -= 1.0F;
    }

    float const step_lift =
        std::sin(step_phase * std::numbers::pi_v<float>) * intensity * 0.05F;

    if (is_left_step) {
      m_pose.foot_l.setY(m_pose.foot_l.y() + step_lift);
      m_pose.knee_l.setY(m_pose.knee_l.y() + step_lift * 0.6F);
    } else {
      m_pose.foot_r.setY(m_pose.foot_r.y() + step_lift);
      m_pose.knee_r.setY(m_pose.knee_r.y() + step_lift * 0.6F);
    }
    break;
  }

  case AmbientIdleType::BendKnee: {

    float const bend_amount = intensity * 0.06F;

    m_pose.knee_l.setY(m_pose.knee_l.y() - bend_amount);
    m_pose.knee_l.setZ(m_pose.knee_l.z() + bend_amount * 0.4F);
    m_pose.foot_l.setY(m_pose.foot_l.y() + bend_amount * 0.2F);

    float const shift = bend_amount * 0.25F;
    m_pose.pelvis_pos.setX(m_pose.pelvis_pos.x() + shift);
    m_pose.shoulder_l.setX(m_pose.shoulder_l.x() + shift);
    m_pose.shoulder_r.setX(m_pose.shoulder_r.x() + shift);
    m_pose.neck_base.setX(m_pose.neck_base.x() + shift);
    m_pose.head_pos.setX(m_pose.head_pos.x() + shift);
    break;
  }

  case AmbientIdleType::RaiseWeapon: {

    float const raise_intensity = plateau01(phase, 0.34F, 0.78F);
    float const wave =
        std::sin(phase * 4.0F * std::numbers::pi_v<float>) * raise_intensity;

    float const right_raise = raise_intensity * 0.28F;
    float const left_raise = raise_intensity * 0.07F;
    m_pose.hand_r.setY(m_pose.hand_r.y() + right_raise);
    m_pose.elbow_r.setY(m_pose.elbow_r.y() + right_raise * 0.55F);
    m_pose.hand_r.setX(m_pose.hand_r.x() + wave * 0.035F);
    m_pose.elbow_r.setX(m_pose.elbow_r.x() + wave * 0.020F);
    m_pose.hand_r.setZ(m_pose.hand_r.z() - raise_intensity * 0.06F);

    m_pose.hand_l.setY(m_pose.hand_l.y() + left_raise);
    m_pose.elbow_l.setY(m_pose.elbow_l.y() + left_raise * 0.45F);

    m_pose.head_pos.setY(m_pose.head_pos.y() + raise_intensity * 0.018F);
    m_pose.head_pos.setZ(m_pose.head_pos.z() - raise_intensity * 0.035F);
    break;
  }

  case AmbientIdleType::Jump: {

    float const prep_crouch =
        (phase < 0.34F) ? std::sin((phase / 0.34F) * std::numbers::pi_v<float>)
                        : 0.0F;
    float const landing_crouch =
        (phase > 0.58F && phase < 0.90F)
            ? std::sin(((phase - 0.58F) / 0.32F) * std::numbers::pi_v<float>)
            : 0.0F;
    float const air =
        (phase > 0.26F && phase < 0.66F)
            ? std::sin(((phase - 0.26F) / 0.40F) * std::numbers::pi_v<float>)
            : 0.0F;

    float const airborne_scale = m_anim_ctx.gait.is_airborne ? 1.0F : 0.45F;
    float const crouch_amount = prep_crouch * 0.75F + landing_crouch;
    float const torso_lift = air * (0.020F + 0.140F * airborne_scale) -
                             crouch_amount * (0.020F + 0.075F * airborne_scale);
    float const knee_lift =
        air * (0.010F + 0.080F * airborne_scale) - crouch_amount * 0.012F;
    float const foot_lift = air * (0.010F + 0.110F * airborne_scale);
    float const knee_drive =
        crouch_amount * 0.080F + air * 0.090F * airborne_scale;
    float const foot_drive =
        crouch_amount * 0.020F + air * 0.035F * airborne_scale;
    float const hand_lift =
        air * 0.050F * airborne_scale - crouch_amount * 0.030F;

    m_pose.pelvis_pos.setY(m_pose.pelvis_pos.y() + torso_lift);
    m_pose.shoulder_l.setY(m_pose.shoulder_l.y() + torso_lift * 0.92F);
    m_pose.shoulder_r.setY(m_pose.shoulder_r.y() + torso_lift * 0.92F);
    m_pose.neck_base.setY(m_pose.neck_base.y() + torso_lift * 0.96F);
    m_pose.head_pos.setY(m_pose.head_pos.y() + torso_lift);
    m_pose.knee_l.setY(m_pose.knee_l.y() + torso_lift * 0.55F + knee_lift);
    m_pose.knee_r.setY(m_pose.knee_r.y() + torso_lift * 0.55F + knee_lift);
    m_pose.knee_l.setZ(m_pose.knee_l.z() + knee_drive);
    m_pose.knee_r.setZ(m_pose.knee_r.z() + knee_drive);
    m_pose.foot_l.setY(m_pose.foot_l.y() + foot_lift);
    m_pose.foot_r.setY(m_pose.foot_r.y() + foot_lift);
    m_pose.foot_l.setZ(m_pose.foot_l.z() + foot_drive);
    m_pose.foot_r.setZ(m_pose.foot_r.z() + foot_drive);
    m_pose.hand_l.setY(m_pose.hand_l.y() + hand_lift);
    m_pose.hand_r.setY(m_pose.hand_r.y() + hand_lift);
    break;
  }

  case AmbientIdleType::None:
  default:
    break;
  }
}

void HumanoidPoseController::kneel(float depth) {
  using HP = HumanProportions;

  depth = std::clamp(depth, 0.0F, 1.0F);

  if (depth < 1e-6F) {
    return;
  }

  float const eased_depth = depth * depth * (3.0F - 2.0F * depth);

  float const kneel_offset = eased_depth * 0.40F;
  float const pelvis_y = HP::WAIST_Y - kneel_offset;
  m_pose.pelvis_pos.setY(pelvis_y);

  float const stance_narrow = 0.11F;

  float const left_knee_y = HP::GROUND_Y + 0.07F * eased_depth;
  float const left_knee_z = -0.06F * eased_depth;
  m_pose.knee_l = QVector3D(-stance_narrow, left_knee_y, left_knee_z);
  m_pose.foot_l =
      QVector3D(-stance_narrow - 0.025F, HP::GROUND_Y,
                left_knee_z - HP::LOWER_LEG_LEN * 0.93F * eased_depth);

  float const right_knee_y = pelvis_y - 0.12F;
  float const right_foot_z = 0.28F * eased_depth;
  m_pose.knee_r = QVector3D(stance_narrow, right_knee_y, right_foot_z - 0.05F);
  m_pose.foot_r = QVector3D(stance_narrow, HP::GROUND_Y + m_pose.foot_y_offset,
                            right_foot_z);

  float const upper_body_drop = kneel_offset;
  float const forward_lean = 0.03F * eased_depth;

  m_pose.shoulder_l.setY(m_pose.shoulder_l.y() - upper_body_drop);
  m_pose.shoulder_r.setY(m_pose.shoulder_r.y() - upper_body_drop);
  m_pose.neck_base.setY(m_pose.neck_base.y() - upper_body_drop);
  m_pose.head_pos.setY(m_pose.head_pos.y() - upper_body_drop);

  m_pose.shoulder_l.setZ(m_pose.shoulder_l.z() + forward_lean);
  m_pose.shoulder_r.setZ(m_pose.shoulder_r.z() + forward_lean);
  m_pose.neck_base.setZ(m_pose.neck_base.z() + forward_lean * 0.8F);
  m_pose.head_pos.setZ(m_pose.head_pos.z() + forward_lean * 0.6F);
}

void HumanoidPoseController::kneel_transition(float progress,
                                              bool standing_up) {
  using HP = HumanProportions;

  progress = std::clamp(progress, 0.0F, 1.0F);

  auto ease_in_out = [](float t) { return t * t * (3.0F - 2.0F * t); };

  float kneel_amount = standing_up ? (1.0F - progress) : progress;
  float eased_progress = ease_in_out(progress);

  kneel(kneel_amount);

  if (standing_up) {

    if (progress < 0.35F) {
      float t = progress / 0.35F;
      float push_t = ease_in_out(t);

      m_pose.foot_r.setZ(m_pose.foot_r.z() - 0.08F * push_t);
      m_pose.knee_r.setZ(m_pose.knee_r.z() - 0.05F * push_t);

      float momentum_lean = 0.06F * push_t;
      m_pose.shoulder_l.setZ(m_pose.shoulder_l.z() + momentum_lean);
      m_pose.shoulder_r.setZ(m_pose.shoulder_r.z() + momentum_lean);
      m_pose.neck_base.setZ(m_pose.neck_base.z() + momentum_lean * 0.9F);
      m_pose.head_pos.setZ(m_pose.head_pos.z() + momentum_lean * 0.7F);

      m_pose.hand_l.setZ(m_pose.hand_l.z() + 0.04F * push_t);
      m_pose.hand_r.setZ(m_pose.hand_r.z() + 0.04F * push_t);
    }

    else if (progress < 0.70F) {
      float t = (progress - 0.35F) / 0.35F;
      float rise_t = ease_in_out(t);

      float lift_boost = 0.02F * std::sin(rise_t * std::numbers::pi_v<float>);
      m_pose.pelvis_pos.setY(m_pose.pelvis_pos.y() + lift_boost);
      m_pose.shoulder_l.setY(m_pose.shoulder_l.y() + lift_boost);
      m_pose.shoulder_r.setY(m_pose.shoulder_r.y() + lift_boost);

      m_pose.foot_l.setZ(m_pose.foot_l.z() + 0.15F * rise_t);
      m_pose.knee_l.setZ(m_pose.knee_l.z() + 0.10F * rise_t);
      m_pose.knee_l.setY(m_pose.knee_l.y() + 0.20F * rise_t);
    }

    else {
      float t = (progress - 0.70F) / 0.30F;
      float settle_t = ease_in_out(t);

      float correct_lean = -0.04F * settle_t * (1.0F - kneel_amount);
      m_pose.shoulder_l.setZ(m_pose.shoulder_l.z() + correct_lean);
      m_pose.shoulder_r.setZ(m_pose.shoulder_r.z() + correct_lean);
    }
  } else {

    if (progress < 0.30F) {
      float t = progress / 0.30F;
      float prep_t = ease_in_out(t);

      m_pose.pelvis_pos.setZ(m_pose.pelvis_pos.z() - 0.03F * prep_t);

      m_pose.hand_l.setY(m_pose.hand_l.y() - 0.02F * prep_t);
      m_pose.hand_r.setY(m_pose.hand_r.y() - 0.02F * prep_t);
    }

    else if (progress < 0.75F) {
      float t = (progress - 0.30F) / 0.45F;

      float controlled_lean = 0.04F * std::sin(t * std::numbers::pi_v<float>);
      m_pose.shoulder_l.setZ(m_pose.shoulder_l.z() + controlled_lean);
      m_pose.shoulder_r.setZ(m_pose.shoulder_r.z() + controlled_lean);
    }

    else {
      float t = (progress - 0.75F) / 0.25F;
      float settle_t = ease_in_out(t);

      m_pose.knee_l.setY(m_pose.knee_l.y() - 0.01F * settle_t);
    }
  }
}

void HumanoidPoseController::lean(const QVector3D &direction, float amount) {

  amount = std::clamp(amount, 0.0F, 1.0F);

  QVector3D dir = direction;
  if (dir.lengthSquared() > 1e-6F) {
    dir.normalize();
  } else {
    dir = QVector3D(0.0F, 0.0F, 1.0F);
  }

  float const lean_magnitude = 0.12F * amount;
  QVector3D const lean_offset = dir * lean_magnitude;

  m_pose.shoulder_l += lean_offset;
  m_pose.shoulder_r += lean_offset;
  m_pose.neck_base += lean_offset * 0.85F;
  m_pose.head_pos += lean_offset * 0.75F;
}

void HumanoidPoseController::place_hand_at(bool is_left,
                                           const QVector3D &target_position) {

  get_hand(is_left) = target_position;

  const QVector3D &shoulder = get_shoulder(is_left);
  const QVector3D outward_dir = compute_outward_dir(is_left);

  float const along_frac = is_left ? 0.45F : 0.48F;
  float const lateral_offset = is_left ? 0.15F : 0.12F;
  float const y_bias = is_left ? -0.08F : 0.02F;
  float const outward_sign = 1.0F;

  get_elbow(is_left) =
      solve_elbow_ik(is_left, shoulder, target_position, outward_dir,
                     along_frac, lateral_offset, y_bias, outward_sign);
}

auto HumanoidPoseController::solve_elbow_ik(
    bool, const QVector3D &shoulder, const QVector3D &hand,
    const QVector3D &outward_dir, float along_frac, float lateral_offset,
    float y_bias, float outward_sign) const -> QVector3D {

  return elbow_bend_torso(shoulder, hand, outward_dir, along_frac,
                          lateral_offset, y_bias, outward_sign);
}

auto HumanoidPoseController::solve_knee_ik(
    bool is_left, const QVector3D &hip, const QVector3D &foot,
    float height_scale) const -> QVector3D {
  using HP = HumanProportions;

  QVector3D hip_to_foot = foot - hip;
  float const distance = hip_to_foot.length();
  if (distance < 1e-5F) {
    return hip;
  }

  float const upper_len = HP::UPPER_LEG_LEN * height_scale;
  float const lower_len = HP::LOWER_LEG_LEN * height_scale;
  float const reach = upper_len + lower_len;
  float const min_reach =
      std::max(std::abs(upper_len - lower_len) + 1e-4F, 1e-3F);
  float const max_reach = std::max(reach - 1e-4F, min_reach + 1e-4F);
  float const clamped_dist = std::clamp(distance, min_reach, max_reach);

  QVector3D const dir = hip_to_foot / distance;

  float cos_theta = (upper_len * upper_len + clamped_dist * clamped_dist -
                     lower_len * lower_len) /
                    (2.0F * upper_len * clamped_dist);
  cos_theta = std::clamp(cos_theta, -1.0F, 1.0F);
  float const sin_theta =
      std::sqrt(std::max(0.0F, 1.0F - cos_theta * cos_theta));

  QVector3D bend_pref =
      is_left ? QVector3D(-0.24F, 0.0F, 0.95F) : QVector3D(0.24F, 0.0F, 0.95F);
  bend_pref.normalize();

  QVector3D bend_axis = bend_pref - dir * QVector3D::dotProduct(dir, bend_pref);
  if (bend_axis.lengthSquared() < 1e-6F) {
    bend_axis = QVector3D::crossProduct(dir, QVector3D(0.0F, 1.0F, 0.0F));
    if (bend_axis.lengthSquared() < 1e-6F) {
      bend_axis = QVector3D::crossProduct(dir, QVector3D(1.0F, 0.0F, 0.0F));
    }
  }
  bend_axis.normalize();

  QVector3D knee =
      hip + dir * (cos_theta * upper_len) + bend_axis * (sin_theta * upper_len);

  float const knee_floor = HP::GROUND_Y + m_pose.foot_y_offset * 0.5F;
  if (knee.y() < knee_floor) {
    knee.setY(knee_floor);
  }

  if (knee.y() > hip.y()) {
    knee.setY(hip.y());
  }

  return knee;
}

auto HumanoidPoseController::get_shoulder(bool is_left) const
    -> const QVector3D & {
  return is_left ? m_pose.shoulder_l : m_pose.shoulder_r;
}

auto HumanoidPoseController::get_hand(bool is_left) -> QVector3D & {
  return is_left ? m_pose.hand_l : m_pose.hand_r;
}

auto HumanoidPoseController::get_hand(bool is_left) const -> const QVector3D & {
  return is_left ? m_pose.hand_l : m_pose.hand_r;
}

auto HumanoidPoseController::get_elbow(bool is_left) -> QVector3D & {
  return is_left ? m_pose.elbow_l : m_pose.elbow_r;
}

auto HumanoidPoseController::compute_right_axis() const -> QVector3D {
  QVector3D right_axis = m_pose.shoulder_r - m_pose.shoulder_l;
  right_axis.setY(0.0F);
  if (right_axis.lengthSquared() < 1e-8F) {
    right_axis = QVector3D(1.0F, 0.0F, 0.0F);
  }
  right_axis.normalize();
  return right_axis;
}

auto HumanoidPoseController::compute_outward_dir(bool is_left) const
    -> QVector3D {
  QVector3D const right_axis = compute_right_axis();
  return is_left ? -right_axis : right_axis;
}

auto HumanoidPoseController::get_shoulder_y(bool is_left) const -> float {
  return is_left ? m_pose.shoulder_l.y() : m_pose.shoulder_r.y();
}

auto HumanoidPoseController::get_pelvis_y() const -> float {
  return m_pose.pelvis_pos.y();
}

void HumanoidPoseController::aim_bow(float draw_phase) {
  using HP = HumanProportions;

  draw_phase = std::clamp(draw_phase, 0.0F, 1.0F);

  QVector3D const aim_pos(-0.02F, HP::SHOULDER_Y + 0.18F, 0.42F);
  QVector3D const draw_pos(-0.05F, HP::SHOULDER_Y + 0.12F, 0.22F);
  QVector3D const release_pos(-0.02F, HP::SHOULDER_Y + 0.20F, 0.34F);

  QVector3D hand_l_target;
  float shoulder_twist = 0.0F;
  float head_recoil = 0.0F;
  float forward_lean = 0.0F;
  float exhale_dip = 0.0F;
  float draw_sway_x = 0.0F;
  float draw_sway_y = 0.0F;

  float const two_pi = 2.0F * std::numbers::pi_v<float>;
  float const jitter_phase = m_anim_ctx.jitter_seed * two_pi;

  if (draw_phase < 0.20F) {

    float t = draw_phase / 0.20F;
    t = t * t;
    hand_l_target = aim_pos * (1.0F - t) + draw_pos * t;
    shoulder_twist = t * 0.08F;
    forward_lean = t * 0.025F;
  } else if (draw_phase < 0.50F) {

    hand_l_target = draw_pos;
    shoulder_twist = 0.08F;
    forward_lean = 0.025F;

    float const tension_t = (draw_phase - 0.20F) / 0.30F;
    float const sway_freq_a = 5.0F;
    float const sway_freq_b = 7.3F;
    float const sway_amp = 0.004F + tension_t * 0.004F;
    draw_sway_x =
        std::sin(draw_phase * sway_freq_a * two_pi + jitter_phase) * sway_amp;
    draw_sway_y =
        std::cos(draw_phase * sway_freq_b * two_pi + jitter_phase * 1.7F) *
        sway_amp * 0.7F;
  } else if (draw_phase < 0.58F) {

    float t = (draw_phase - 0.50F) / 0.08F;
    t = t * t * t;
    hand_l_target = draw_pos * (1.0F - t) + release_pos * t;
    shoulder_twist = 0.08F * (1.0F - t * 0.6F);
    head_recoil = t * 0.04F;
    forward_lean = 0.025F * (1.0F - t);
    exhale_dip = t * 0.012F;
  } else {

    float t = (draw_phase - 0.58F) / 0.42F;
    t = 1.0F - (1.0F - t) * (1.0F - t);
    hand_l_target = release_pos * (1.0F - t) + aim_pos * t;
    shoulder_twist = 0.08F * 0.4F * (1.0F - t);
    head_recoil = 0.04F * (1.0F - t);
    exhale_dip = 0.012F * (1.0F - t);
  }

  hand_l_target.setX(hand_l_target.x() + draw_sway_x);
  hand_l_target.setY(hand_l_target.y() + draw_sway_y);

  QVector3D hand_r_target(0.03F, HP::SHOULDER_Y + 0.08F, 0.55F);
  hand_r_target.setX(hand_r_target.x() + draw_sway_x * 0.3F);
  hand_r_target.setY(hand_r_target.y() + draw_sway_y * 0.3F);
  place_hand_at(false, hand_r_target);
  place_hand_at(true, hand_l_target);

  if (shoulder_twist > 0.01F) {
    m_pose.shoulder_l.setY(m_pose.shoulder_l.y() + shoulder_twist);
    m_pose.shoulder_r.setY(m_pose.shoulder_r.y() - shoulder_twist * 0.5F);
  }

  if (forward_lean > 0.001F) {
    m_pose.shoulder_l.setZ(m_pose.shoulder_l.z() + forward_lean);
    m_pose.shoulder_r.setZ(m_pose.shoulder_r.z() + forward_lean);
    m_pose.neck_base.setZ(m_pose.neck_base.z() + forward_lean * 0.7F);
    m_pose.head_pos.setZ(m_pose.head_pos.z() + forward_lean * 0.5F);
  }

  if (exhale_dip > 0.001F) {
    m_pose.shoulder_l.setY(m_pose.shoulder_l.y() - exhale_dip);
    m_pose.shoulder_r.setY(m_pose.shoulder_r.y() - exhale_dip);
    m_pose.neck_base.setY(m_pose.neck_base.y() - exhale_dip * 0.7F);
  }

  if (head_recoil > 0.01F) {
    m_pose.head_pos.setZ(m_pose.head_pos.z() - head_recoil);
  }
}

void HumanoidPoseController::melee_strike(float strike_phase) {
  using HP = HumanProportions;

  strike_phase = std::clamp(strike_phase, 0.0F, 1.0F);

  QVector3D const rest_pos(0.22F, HP::SHOULDER_Y + 0.02F, 0.18F);
  QVector3D const chamber_pos(0.30F, HP::SHOULDER_Y + 0.08F, 0.05F);
  QVector3D const strike_pos(0.28F, HP::SHOULDER_Y - 0.05F, 0.65F);
  QVector3D const followthrough_pos(0.10F, HP::SHOULDER_Y - 0.12F, 0.55F);

  QVector3D hand_r_target;
  QVector3D hand_l_target;

  float torso_twist = 0.0F;
  float forward_lean = 0.0F;
  float shoulder_dip = 0.0F;
  float step_forward = 0.0F;

  if (strike_phase < 0.20F) {

    float t = strike_phase / 0.20F;
    float ease_t = t * t;
    hand_r_target = rest_pos * (1.0F - ease_t) + chamber_pos * ease_t;
    hand_l_target =
        QVector3D(-0.18F, HP::SHOULDER_Y + 0.02F, 0.22F - 0.08F * t);

    torso_twist = -0.04F * ease_t;
    shoulder_dip = -0.02F * ease_t;
  } else if (strike_phase < 0.28F) {

    hand_r_target = chamber_pos;
    hand_l_target = QVector3D(-0.18F, HP::SHOULDER_Y + 0.02F, 0.14F);
    torso_twist = -0.04F;
    shoulder_dip = -0.02F;
  } else if (strike_phase < 0.48F) {

    float t = (strike_phase - 0.28F) / 0.20F;
    float power_t = t * t * (3.0F - 2.0F * t);
    hand_r_target = chamber_pos * (1.0F - power_t) + strike_pos * power_t;
    hand_l_target = QVector3D(-0.18F + 0.06F * power_t,
                              HP::SHOULDER_Y + 0.02F - 0.08F * power_t,
                              0.14F + 0.20F * power_t);

    torso_twist = -0.04F + 0.10F * power_t;
    forward_lean = 0.08F * power_t;
    shoulder_dip = -0.02F + 0.05F * power_t;
    step_forward = 0.06F * power_t;
  } else if (strike_phase < 0.65F) {

    float t = (strike_phase - 0.48F) / 0.17F;
    float ease_t = t * t;
    hand_r_target = strike_pos * (1.0F - ease_t) + followthrough_pos * ease_t;
    hand_l_target = QVector3D(-0.12F, HP::SHOULDER_Y - 0.06F, 0.34F);

    torso_twist = 0.06F - 0.02F * t;
    forward_lean = 0.08F - 0.03F * t;
    shoulder_dip = 0.03F;
    step_forward = 0.06F;
  } else {

    float t = (strike_phase - 0.65F) / 0.35F;
    float ease_t = 1.0F - (1.0F - t) * (1.0F - t);
    hand_r_target = followthrough_pos * (1.0F - ease_t) + rest_pos * ease_t;
    hand_l_target =
        QVector3D(-0.12F + (-0.18F + 0.12F) * ease_t,
                  HP::SHOULDER_Y - 0.06F * (1.0F - ease_t) + 0.02F * ease_t,
                  0.34F * (1.0F - ease_t) + 0.22F * ease_t);

    torso_twist = 0.04F * (1.0F - ease_t);
    forward_lean = 0.05F * (1.0F - ease_t);
    shoulder_dip = 0.03F * (1.0F - ease_t);
    step_forward = 0.06F * (1.0F - ease_t);
  }

  if (std::abs(torso_twist) > 0.001F) {
    m_pose.shoulder_r.setZ(m_pose.shoulder_r.z() + torso_twist);
    m_pose.shoulder_l.setZ(m_pose.shoulder_l.z() - torso_twist * 0.5F);
  }

  if (forward_lean > 0.001F) {
    m_pose.shoulder_l.setZ(m_pose.shoulder_l.z() + forward_lean);
    m_pose.shoulder_r.setZ(m_pose.shoulder_r.z() + forward_lean);
    m_pose.neck_base.setZ(m_pose.neck_base.z() + forward_lean * 0.8F);
    m_pose.head_pos.setZ(m_pose.head_pos.z() + forward_lean * 0.6F);
  }

  if (std::abs(shoulder_dip) > 0.001F) {
    m_pose.shoulder_r.setY(m_pose.shoulder_r.y() + shoulder_dip);
  }

  if (step_forward > 0.001F) {
    m_pose.foot_r.setZ(m_pose.foot_r.z() + step_forward);
    m_pose.knee_r.setZ(m_pose.knee_r.z() + step_forward * 0.5F);
  }

  place_hand_at(false, hand_r_target);
  place_hand_at(true, hand_l_target);
}

void HumanoidPoseController::grasp_two_handed(const QVector3D &grip_center,
                                              float hand_separation) {
  hand_separation = std::clamp(hand_separation, 0.1F, 0.8F);

  QVector3D const right_axis = compute_right_axis();

  QVector3D const right_hand_pos =
      grip_center + right_axis * (hand_separation * 0.5F);
  QVector3D const left_hand_pos =
      grip_center - right_axis * (hand_separation * 0.5F);

  place_hand_at(false, right_hand_pos);
  place_hand_at(true, left_hand_pos);
}

void HumanoidPoseController::spear_thrust(float attack_phase) {
  using HP = HumanProportions;

  attack_phase = std::clamp(attack_phase, 0.0F, 1.0F);

  QVector3D const guard_pos(0.26F, HP::SHOULDER_Y + 0.08F, 0.28F);
  QVector3D const chamber_pos(0.32F, HP::SHOULDER_Y + 0.12F, 0.02F);
  QVector3D const thrust_pos(0.28F, HP::SHOULDER_Y + 0.05F, 0.95F);
  QVector3D const extended_pos(0.25F, HP::SHOULDER_Y + 0.02F, 1.05F);
  QVector3D const recover_pos(0.28F, HP::SHOULDER_Y + 0.06F, 0.45F);

  QVector3D hand_r_target;
  QVector3D hand_l_target;

  float forward_lean = 0.0F;
  float torso_twist = 0.0F;
  float shoulder_drop = 0.0F;
  float step_forward = 0.0F;
  float hip_rotation = 0.0F;

  auto ease_in_out_cubic = [](float t) {
    return t < 0.5F ? 4.0F * t * t * t
                    : 1.0F - std::pow(-2.0F * t + 2.0F, 3.0F) / 2.0F;
  };

  auto smoothstep = [](float t) { return t * t * (3.0F - 2.0F * t); };
  auto ease_out = [](float t) { return 1.0F - (1.0F - t) * (1.0F - t); };

  if (attack_phase < 0.18F) {

    float const t = ease_in_out_cubic(attack_phase / 0.18F);
    hand_r_target = guard_pos * (1.0F - t) + chamber_pos * t;
    hand_l_target = QVector3D(-0.08F, HP::SHOULDER_Y - 0.04F,
                              0.22F * (1.0F - t) + 0.06F * t);

    torso_twist = -0.06F * t;
    hip_rotation = -0.04F * t;
    forward_lean = -0.03F * t;
  } else if (attack_phase < 0.28F) {

    float const t = (attack_phase - 0.18F) / 0.10F;
    hand_r_target = chamber_pos;
    hand_l_target = QVector3D(-0.08F, HP::SHOULDER_Y - 0.04F, 0.06F);

    torso_twist = -0.06F;
    hip_rotation = -0.04F;
    forward_lean = -0.03F - 0.02F * t;
  } else if (attack_phase < 0.48F) {

    float t = (attack_phase - 0.28F) / 0.20F;
    float power_t = t * t * t;
    hand_r_target = chamber_pos * (1.0F - power_t) + thrust_pos * power_t;
    hand_l_target = QVector3D(-0.08F + 0.06F * power_t,
                              HP::SHOULDER_Y - 0.04F + 0.02F * power_t,
                              0.06F + 0.50F * power_t);

    torso_twist = -0.06F + 0.14F * power_t;
    hip_rotation = -0.04F + 0.10F * power_t;
    forward_lean = -0.05F + 0.18F * power_t;
    shoulder_drop = 0.05F * power_t;
    step_forward = 0.10F * power_t;
  } else if (attack_phase < 0.60F) {

    float const t = smoothstep((attack_phase - 0.48F) / 0.12F);
    hand_r_target = thrust_pos * (1.0F - t) + extended_pos * t;
    hand_l_target =
        QVector3D(-0.02F, HP::SHOULDER_Y - 0.02F, 0.56F + 0.10F * t);

    torso_twist = 0.08F;
    hip_rotation = 0.06F;
    forward_lean = 0.13F + 0.05F * t;
    shoulder_drop = 0.05F + 0.02F * t;
    step_forward = 0.10F + 0.04F * t;
  } else if (attack_phase < 0.78F) {

    float const t = ease_in_out_cubic((attack_phase - 0.60F) / 0.18F);
    hand_r_target = extended_pos * (1.0F - t) + recover_pos * t;
    hand_l_target = QVector3D(-0.02F * (1.0F - t) - 0.08F * t,
                              HP::SHOULDER_Y - 0.02F * (1.0F - t) - 0.05F * t,
                              0.66F * (1.0F - t) + 0.38F * t);

    torso_twist = 0.08F * (1.0F - t);
    hip_rotation = 0.06F * (1.0F - t);
    forward_lean = 0.18F * (1.0F - t) + 0.04F * t;
    shoulder_drop = 0.07F * (1.0F - t);
    step_forward = 0.14F * (1.0F - t * 0.5F);
  } else {

    float const t = ease_out((attack_phase - 0.78F) / 0.22F);
    hand_r_target = recover_pos * (1.0F - t) + guard_pos * t;
    hand_l_target =
        QVector3D(-0.08F, HP::SHOULDER_Y - 0.05F * (1.0F - t) - 0.02F * t,
                  0.38F * (1.0F - t) + 0.22F * t);

    forward_lean = 0.04F * (1.0F - t);
    step_forward = 0.07F * (1.0F - t);
  }

  if (std::abs(torso_twist) > 0.001F) {
    m_pose.shoulder_r.setZ(m_pose.shoulder_r.z() + torso_twist);
    m_pose.shoulder_l.setZ(m_pose.shoulder_l.z() - torso_twist * 0.4F);
  }

  if (std::abs(hip_rotation) > 0.001F) {
    m_pose.pelvis_pos.setZ(m_pose.pelvis_pos.z() + hip_rotation * 0.5F);
  }

  if (std::abs(forward_lean) > 0.001F) {
    m_pose.shoulder_l.setZ(m_pose.shoulder_l.z() + forward_lean);
    m_pose.shoulder_r.setZ(m_pose.shoulder_r.z() + forward_lean);
    m_pose.neck_base.setZ(m_pose.neck_base.z() + forward_lean * 0.85F);
    m_pose.head_pos.setZ(m_pose.head_pos.z() + forward_lean * 0.7F);
  }

  if (shoulder_drop > 0.001F) {
    m_pose.shoulder_r.setY(m_pose.shoulder_r.y() - shoulder_drop);
    m_pose.shoulder_l.setY(m_pose.shoulder_l.y() - shoulder_drop * 0.3F);
  }

  if (step_forward > 0.001F) {
    m_pose.foot_r.setZ(m_pose.foot_r.z() + step_forward);
    m_pose.knee_r.setZ(m_pose.knee_r.z() + step_forward * 0.6F);
    m_pose.foot_l.setZ(m_pose.foot_l.z() - step_forward * 0.15F);
  }

  float const thrust_extent =
      std::clamp((attack_phase - 0.18F) / 0.60F, 0.0F, 1.0F);
  float const along_offset = -0.08F + 0.04F * thrust_extent;
  float const y_drop = 0.08F + 0.03F * thrust_extent;

  hand_l_target = compute_offhand_spear_grip(
      m_pose, m_anim_ctx, hand_r_target, false, along_offset, y_drop, -0.06F);

  place_hand_at(false, hand_r_target);
  place_hand_at(true, hand_l_target);
}

void HumanoidPoseController::spear_thrust_from_hold(float attack_phase,
                                                    float hold_depth) {
  using HP = HumanProportions;

  attack_phase = std::clamp(attack_phase, 0.0F, 1.0F);
  hold_depth = std::clamp(hold_depth, 0.0F, 1.0F);

  float const height_offset = -hold_depth * 0.35F;

  QVector3D const guard_pos(0.22F, HP::SHOULDER_Y + height_offset + 0.05F,
                            0.32F);
  QVector3D const chamber_pos(0.28F, HP::SHOULDER_Y + height_offset + 0.10F,
                              0.08F);
  QVector3D const thrust_pos(0.24F, HP::SHOULDER_Y + height_offset - 0.08F,
                             0.90F);
  QVector3D const extended_pos(0.22F, HP::SHOULDER_Y + height_offset - 0.12F,
                               1.00F);
  QVector3D const recover_pos(0.24F, HP::SHOULDER_Y + height_offset + 0.02F,
                              0.48F);

  QVector3D hand_r_target;
  QVector3D hand_l_target;

  float forward_lean = 0.0F;
  float torso_twist = 0.0F;
  float shoulder_extension = 0.0F;

  auto smoothstep = [](float t) { return t * t * (3.0F - 2.0F * t); };
  auto ease_out = [](float t) { return 1.0F - (1.0F - t) * (1.0F - t); };
  auto ease_in = [](float t) { return t * t; };

  if (attack_phase < 0.15F) {

    float const t = ease_in(attack_phase / 0.15F);
    hand_r_target = guard_pos * (1.0F - t) + chamber_pos * t;
    hand_l_target = QVector3D(-0.06F, HP::SHOULDER_Y + height_offset - 0.03F,
                              0.28F * (1.0F - t) + 0.10F * t);

    torso_twist = -0.04F * t;
  } else if (attack_phase < 0.22F) {

    hand_r_target = chamber_pos;
    hand_l_target =
        QVector3D(-0.06F, HP::SHOULDER_Y + height_offset - 0.03F, 0.10F);

    torso_twist = -0.04F;
  } else if (attack_phase < 0.42F) {

    float t = (attack_phase - 0.22F) / 0.20F;
    float power_t = t * t * t;
    hand_r_target = chamber_pos * (1.0F - power_t) + thrust_pos * power_t;
    hand_l_target =
        QVector3D(-0.06F + 0.05F * power_t,
                  HP::SHOULDER_Y + height_offset - 0.03F + 0.01F * power_t,
                  0.10F + 0.48F * power_t);

    torso_twist = -0.04F + 0.10F * power_t;
    forward_lean = 0.12F * power_t;
    shoulder_extension = 0.06F * power_t;
  } else if (attack_phase < 0.55F) {

    float const t = smoothstep((attack_phase - 0.42F) / 0.13F);
    hand_r_target = thrust_pos * (1.0F - t) + extended_pos * t;
    hand_l_target = QVector3D(-0.01F, HP::SHOULDER_Y + height_offset - 0.02F,
                              0.58F + 0.08F * t);

    torso_twist = 0.06F;
    forward_lean = 0.12F + 0.04F * t;
    shoulder_extension = 0.06F + 0.03F * t;
  } else if (attack_phase < 0.75F) {

    float const t = smoothstep((attack_phase - 0.55F) / 0.20F);
    hand_r_target = extended_pos * (1.0F - t) + recover_pos * t;
    hand_l_target = QVector3D(-0.01F * (1.0F - t) - 0.05F * t,
                              HP::SHOULDER_Y + height_offset -
                                  0.02F * (1.0F - t) - 0.04F * t,
                              0.66F * (1.0F - t) + 0.40F * t);

    torso_twist = 0.06F * (1.0F - t);
    forward_lean = 0.16F * (1.0F - t) + 0.03F * t;
    shoulder_extension = 0.09F * (1.0F - t);
  } else {

    float const t = ease_out((attack_phase - 0.75F) / 0.25F);
    hand_r_target = recover_pos * (1.0F - t) + guard_pos * t;
    hand_l_target = QVector3D(-0.05F - 0.01F * t,
                              HP::SHOULDER_Y + height_offset -
                                  0.04F * (1.0F - t) - 0.03F * t,
                              0.40F * (1.0F - t) + 0.28F * t);

    forward_lean = 0.03F * (1.0F - t);
  }

  if (std::abs(torso_twist) > 0.001F) {
    m_pose.shoulder_r.setZ(m_pose.shoulder_r.z() + torso_twist);
    m_pose.shoulder_l.setZ(m_pose.shoulder_l.z() - torso_twist * 0.3F);
  }

  if (forward_lean > 0.001F) {

    m_pose.shoulder_l.setZ(m_pose.shoulder_l.z() + forward_lean);
    m_pose.shoulder_r.setZ(m_pose.shoulder_r.z() + forward_lean);
    m_pose.neck_base.setZ(m_pose.neck_base.z() + forward_lean * 0.9F);
    m_pose.head_pos.setZ(m_pose.head_pos.z() + forward_lean * 0.75F);
  }

  if (shoulder_extension > 0.001F) {

    m_pose.shoulder_r.setZ(m_pose.shoulder_r.z() + shoulder_extension);
    m_pose.shoulder_r.setY(m_pose.shoulder_r.y() - shoulder_extension * 0.3F);
  }

  float const thrust_extent =
      std::clamp((attack_phase - 0.15F) / 0.55F, 0.0F, 1.0F);
  float const along_offset = -0.06F + 0.03F * thrust_extent;
  float const y_drop = 0.06F + 0.02F * thrust_extent;

  hand_l_target = compute_offhand_spear_grip(
      m_pose, m_anim_ctx, hand_r_target, false, along_offset, y_drop, -0.05F);

  place_hand_at(false, hand_r_target);
  place_hand_at(true, hand_l_target);
}

void HumanoidPoseController::sword_slash(float attack_phase) {
  using HP = HumanProportions;

  attack_phase = std::clamp(attack_phase, 0.0F, 1.0F);

  QVector3D const rest_pos(0.20F, HP::SHOULDER_Y + 0.05F, 0.15F);
  QVector3D const chamber_pos(0.28F, HP::SHOULDER_Y + 0.20F, 0.02F);
  QVector3D const apex_pos(0.30F, HP::SHOULDER_Y + 0.25F, 0.08F);
  QVector3D const strike_pos(0.18F, HP::SHOULDER_Y - 0.15F, 0.62F);
  QVector3D const followthrough_pos(0.05F, HP::WAIST_Y + 0.10F, 0.50F);
  QVector3D const recover_pos(0.22F, HP::SHOULDER_Y + 0.02F, 0.22F);

  QVector3D hand_r_target;
  QVector3D hand_l_target;

  float torso_twist = 0.0F;
  float forward_lean = 0.0F;
  float shoulder_rotation = 0.0F;
  float weight_shift = 0.0F;

  auto smoothstep = [](float t) { return t * t * (3.0F - 2.0F * t); };
  auto ease_out = [](float t) { return 1.0F - (1.0F - t) * (1.0F - t); };
  auto ease_in = [](float t) { return t * t; };

  if (attack_phase < 0.15F) {

    float t = attack_phase / 0.15F;
    float ease_t = ease_in(t);
    hand_r_target = rest_pos * (1.0F - ease_t) + chamber_pos * ease_t;
    hand_l_target =
        QVector3D(-0.20F, HP::SHOULDER_Y - 0.02F, 0.15F + 0.02F * t);

    torso_twist = -0.05F * ease_t;
    shoulder_rotation = 0.03F * ease_t;
  } else if (attack_phase < 0.28F) {

    float t = (attack_phase - 0.15F) / 0.13F;
    float ease_t = smoothstep(t);
    hand_r_target = chamber_pos * (1.0F - ease_t) + apex_pos * ease_t;
    hand_l_target = QVector3D(-0.20F, HP::SHOULDER_Y - 0.04F, 0.17F);

    torso_twist = -0.05F;
    shoulder_rotation = 0.03F + 0.02F * ease_t;
    weight_shift = -0.02F * ease_t;
  } else if (attack_phase < 0.48F) {

    float t = (attack_phase - 0.28F) / 0.20F;
    float power_t = t * t * t;
    hand_r_target = apex_pos * (1.0F - power_t) + strike_pos * power_t;
    hand_l_target = QVector3D(-0.20F + 0.08F * power_t,
                              HP::SHOULDER_Y - 0.04F - 0.06F * power_t,
                              0.17F + 0.22F * power_t);

    torso_twist = -0.05F + 0.14F * power_t;
    forward_lean = 0.10F * power_t;
    shoulder_rotation = 0.05F - 0.08F * power_t;
    weight_shift = -0.02F + 0.08F * power_t;
  } else if (attack_phase < 0.62F) {

    float t = (attack_phase - 0.48F) / 0.14F;
    float ease_t = smoothstep(t);
    hand_r_target = strike_pos * (1.0F - ease_t) + followthrough_pos * ease_t;
    hand_l_target = QVector3D(-0.12F, HP::SHOULDER_Y - 0.10F, 0.39F);

    torso_twist = 0.09F - 0.03F * t;
    forward_lean = 0.10F - 0.02F * t;
    weight_shift = 0.06F;
  } else {

    float t = (attack_phase - 0.62F) / 0.38F;
    float ease_t = ease_out(t);
    hand_r_target = followthrough_pos * (1.0F - ease_t) +
                    recover_pos * 0.5F * ease_t + rest_pos * 0.5F * ease_t;
    hand_l_target = QVector3D(-0.12F - 0.08F * ease_t,
                              HP::SHOULDER_Y - 0.10F * (1.0F - ease_t),
                              0.39F * (1.0F - ease_t) + 0.15F * ease_t);

    torso_twist = 0.06F * (1.0F - ease_t);
    forward_lean = 0.08F * (1.0F - ease_t);
    weight_shift = 0.06F * (1.0F - ease_t);
  }

  if (std::abs(torso_twist) > 0.001F) {
    m_pose.shoulder_r.setZ(m_pose.shoulder_r.z() + torso_twist);
    m_pose.shoulder_l.setZ(m_pose.shoulder_l.z() - torso_twist * 0.6F);
  }

  if (std::abs(shoulder_rotation) > 0.001F) {
    m_pose.shoulder_r.setY(m_pose.shoulder_r.y() - shoulder_rotation);
    m_pose.shoulder_l.setY(m_pose.shoulder_l.y() + shoulder_rotation * 0.4F);
  }

  if (forward_lean > 0.001F) {
    m_pose.shoulder_l.setZ(m_pose.shoulder_l.z() + forward_lean);
    m_pose.shoulder_r.setZ(m_pose.shoulder_r.z() + forward_lean);
    m_pose.neck_base.setZ(m_pose.neck_base.z() + forward_lean * 0.7F);
    m_pose.head_pos.setZ(m_pose.head_pos.z() + forward_lean * 0.5F);
    m_pose.pelvis_pos.setZ(m_pose.pelvis_pos.z() + forward_lean * 0.3F);
  }

  if (std::abs(weight_shift) > 0.001F) {
    m_pose.foot_r.setZ(m_pose.foot_r.z() + weight_shift);
    m_pose.knee_r.setZ(m_pose.knee_r.z() + weight_shift * 0.6F);
  }

  place_hand_at(false, hand_r_target);
  place_hand_at(true, hand_l_target);
}

void HumanoidPoseController::mount_on_horse(float saddle_height) {

  float const offset_y = saddle_height - m_pose.pelvis_pos.y();
  m_pose.pelvis_pos.setY(saddle_height);
}

void HumanoidPoseController::brace_spear_for_hold() {
  using HP = HumanProportions;

  QVector3D const hand_r_target(0.24F, HP::SHOULDER_Y - 0.12F, 0.56F);
  QVector3D hand_l_target = compute_offhand_spear_grip(
      m_pose, m_anim_ctx, hand_r_target, false, -0.30F, 0.07F, -0.04F);
  hand_l_target.setZ(hand_l_target.z() + 0.03F);

  place_hand_at(false, hand_r_target);
  place_hand_at(true, hand_l_target);

  m_pose.shoulder_r.setY(m_pose.shoulder_r.y() - 0.05F);
  m_pose.shoulder_l.setY(m_pose.shoulder_l.y() - 0.03F);
  m_pose.shoulder_r.setZ(m_pose.shoulder_r.z() + 0.08F);
  m_pose.shoulder_l.setZ(m_pose.shoulder_l.z() + 0.06F);
  m_pose.neck_base.setZ(m_pose.neck_base.z() + 0.07F);
  m_pose.head_pos.setZ(m_pose.head_pos.z() + 0.06F);
  m_pose.head_pos.setY(m_pose.head_pos.y() - 0.01F);
}

void HumanoidPoseController::hold_bow_ready() {
  using HP = HumanProportions;

  QVector3D const bow_hand_pos(0.08F, HP::SHOULDER_Y - 0.08F, 0.34F);
  QVector3D const support_hand_pos(0.02F, HP::SHOULDER_Y + 0.02F, 0.60F);

  place_hand_at(false, bow_hand_pos);
  place_hand_at(true, support_hand_pos);

  m_pose.shoulder_l.setY(m_pose.shoulder_l.y() - 0.01F);
  m_pose.shoulder_r.setY(m_pose.shoulder_r.y() - 0.05F);
  m_pose.shoulder_l.setZ(m_pose.shoulder_l.z() + 0.05F);
  m_pose.shoulder_r.setZ(m_pose.shoulder_r.z() + 0.09F);
  m_pose.neck_base.setZ(m_pose.neck_base.z() + 0.05F);
  m_pose.head_pos.setZ(m_pose.head_pos.z() + 0.04F);
  m_pose.head_pos.setY(m_pose.head_pos.y() - 0.01F);
}

void HumanoidPoseController::brace_sword_and_shield_for_hold() {
  using HP = HumanProportions;

  QVector3D const sword_hand_pos(0.26F, HP::SHOULDER_Y - 0.16F, 0.32F);
  QVector3D const shield_hand_pos(-0.32F, HP::SHOULDER_Y + 0.05F, 0.40F);

  place_hand_at(false, sword_hand_pos);
  place_hand_at(true, shield_hand_pos);

  m_pose.shoulder_l.setY(m_pose.shoulder_l.y() + 0.03F);
  m_pose.shoulder_r.setY(m_pose.shoulder_r.y() - 0.03F);
  m_pose.shoulder_l.setZ(m_pose.shoulder_l.z() + 0.09F);
  m_pose.shoulder_r.setZ(m_pose.shoulder_r.z() + 0.04F);
  m_pose.neck_base.setZ(m_pose.neck_base.z() + 0.05F);
  m_pose.head_pos.setZ(m_pose.head_pos.z() + 0.04F);
}

void HumanoidPoseController::hold_sword_and_shield() {
  using HP = HumanProportions;

  float const moving_mix =
      (m_anim_ctx.inputs.is_moving || m_anim_ctx.gait.speed > 0.1F) ? 1.0F
                                                                    : 0.0F;
  QVector3D const sword_hand_pos(0.34F + moving_mix * 0.03F,
                                 HP::SHOULDER_Y - 0.06F - moving_mix * 0.05F,
                                 0.44F + moving_mix * 0.06F);
  QVector3D const shield_hand_pos(-0.32F - moving_mix * 0.03F,
                                  HP::SHOULDER_Y - 0.02F - moving_mix * 0.03F,
                                  0.28F + moving_mix * 0.05F);

  place_hand_at(false, sword_hand_pos);
  place_hand_at(true, shield_hand_pos);
}

void HumanoidPoseController::look_at(const QVector3D &target) {
  QVector3D const head_to_target = target - m_pose.head_pos;

  if (head_to_target.lengthSquared() < 1e-6F) {
    return;
  }

  QVector3D const direction = head_to_target.normalized();

  float const max_head_turn = 0.03F;
  QVector3D const head_offset = direction * max_head_turn;

  m_pose.head_pos += QVector3D(head_offset.x(), 0.0F, head_offset.z());

  float const neck_follow = 0.5F;
  m_pose.neck_base += QVector3D(head_offset.x() * neck_follow, 0.0F,
                                head_offset.z() * neck_follow);
}

void HumanoidPoseController::hit_flinch(float intensity) {
  intensity = std::clamp(intensity, 0.0F, 1.0F);

  if (intensity < 0.01F) {
    return;
  }

  float const flinch_back = intensity * 0.06F;
  float const flinch_down = intensity * 0.04F;
  float const shoulder_drop = intensity * 0.03F;

  m_pose.head_pos.setZ(m_pose.head_pos.z() - flinch_back);
  m_pose.head_pos.setY(m_pose.head_pos.y() - flinch_down * 0.5F);

  m_pose.neck_base.setZ(m_pose.neck_base.z() - flinch_back * 0.8F);

  m_pose.shoulder_l.setY(m_pose.shoulder_l.y() - shoulder_drop);
  m_pose.shoulder_r.setY(m_pose.shoulder_r.y() - shoulder_drop);
  m_pose.shoulder_l.setZ(m_pose.shoulder_l.z() - flinch_back * 0.6F);
  m_pose.shoulder_r.setZ(m_pose.shoulder_r.z() - flinch_back * 0.6F);

  m_pose.pelvis_pos.setY(m_pose.pelvis_pos.y() - flinch_down * 0.3F);
}

void HumanoidPoseController::sword_slash_variant(float attack_phase,
                                                 std::uint8_t variant) {
  using HP = HumanProportions;

  attack_phase = std::clamp(attack_phase, 0.0F, 1.0F);

  constexpr float k_strike_right_to_left = 1.0F;
  constexpr float k_strike_left_to_right = -1.0F;

  QVector3D rest_pos(0.20F, HP::SHOULDER_Y + 0.02F, 0.15F);
  QVector3D chamber_pos(0.30F, HP::SHOULDER_Y + 0.16F, -0.02F);
  QVector3D apex_pos(0.32F, HP::SHOULDER_Y + 0.20F, 0.04F);
  QVector3D strike_pos(0.16F, HP::SHOULDER_Y - 0.24F, 0.72F);
  QVector3D followthrough_pos(0.02F, HP::WAIST_Y - 0.02F, 0.58F);

  float strike_direction = k_strike_right_to_left;
  switch (variant % 3) {
  case 1:

    chamber_pos = QVector3D(-0.12F, HP::SHOULDER_Y + 0.18F, 0.00F);
    apex_pos = QVector3D(-0.10F, HP::SHOULDER_Y + 0.22F, 0.06F);
    strike_pos = QVector3D(0.36F, HP::SHOULDER_Y - 0.22F, 0.68F);
    followthrough_pos = QVector3D(0.44F, HP::WAIST_Y - 0.02F, 0.56F);
    strike_direction = k_strike_left_to_right;
    break;
  case 2:

    chamber_pos = QVector3D(0.36F, HP::SHOULDER_Y + 0.06F, -0.02F);
    apex_pos = QVector3D(0.40F, HP::SHOULDER_Y + 0.04F, 0.04F);
    strike_pos = QVector3D(0.04F, HP::SHOULDER_Y - 0.18F, 0.74F);
    followthrough_pos = QVector3D(-0.14F, HP::SHOULDER_Y - 0.24F, 0.64F);
    break;
  default:
    break;
  }

  QVector3D hand_r_target;
  QVector3D hand_l_target;

  float torso_twist = 0.0F;
  float forward_lean = 0.0F;
  float shoulder_rotation = 0.0F;
  float weight_shift = 0.0F;

  auto smoothstep = [](float t) { return t * t * (3.0F - 2.0F * t); };
  auto ease_out = [](float t) { return 1.0F - (1.0F - t) * (1.0F - t); };

  if (attack_phase < 0.15F) {
    float t = attack_phase / 0.15F;
    float ease_t = t * t;
    hand_r_target = rest_pos * (1.0F - ease_t) + chamber_pos * ease_t;
    hand_l_target = QVector3D(-0.20F, HP::SHOULDER_Y - 0.02F, 0.15F);

    torso_twist = strike_direction * (-0.05F * ease_t);
    shoulder_rotation = 0.03F * ease_t;
  } else if (attack_phase < 0.28F) {
    float t = (attack_phase - 0.15F) / 0.13F;
    float ease_t = smoothstep(t);
    hand_r_target = chamber_pos * (1.0F - ease_t) + apex_pos * ease_t;
    hand_l_target = QVector3D(-0.20F, HP::SHOULDER_Y - 0.04F, 0.17F);

    torso_twist = strike_direction * (-0.05F);
    shoulder_rotation = 0.03F + 0.02F * ease_t;
    weight_shift = -0.02F * ease_t;
  } else if (attack_phase < 0.48F) {
    float t = (attack_phase - 0.28F) / 0.20F;
    float power_t = t * t * t;
    hand_r_target = apex_pos * (1.0F - power_t) + strike_pos * power_t;
    hand_l_target = QVector3D(-0.20F + 0.08F * power_t,
                              HP::SHOULDER_Y - 0.04F - 0.06F * power_t,
                              0.17F + 0.22F * power_t);

    torso_twist = strike_direction * (-0.05F + 0.14F * power_t);
    forward_lean = 0.15F * power_t;
    shoulder_rotation = 0.05F - 0.12F * power_t;
    weight_shift = -0.02F + 0.12F * power_t;
  } else if (attack_phase < 0.62F) {
    float t = (attack_phase - 0.48F) / 0.14F;
    float ease_t = smoothstep(t);
    hand_r_target = strike_pos * (1.0F - ease_t) + followthrough_pos * ease_t;
    hand_l_target = QVector3D(-0.12F, HP::SHOULDER_Y - 0.10F, 0.39F);

    torso_twist = strike_direction * (0.09F - 0.03F * t);
    forward_lean = 0.14F - 0.03F * t;
    weight_shift = 0.10F;
  } else {
    float t = (attack_phase - 0.62F) / 0.38F;
    float ease_t = ease_out(t);
    hand_r_target = followthrough_pos * (1.0F - ease_t) + rest_pos * ease_t;
    hand_l_target = QVector3D(-0.12F - 0.08F * ease_t,
                              HP::SHOULDER_Y - 0.10F * (1.0F - ease_t),
                              0.39F * (1.0F - ease_t) + 0.15F * ease_t);

    torso_twist = 0.06F * strike_direction * (1.0F - ease_t);
    forward_lean = 0.10F * (1.0F - ease_t);
    weight_shift = 0.08F * (1.0F - ease_t);
  }

  if (std::abs(torso_twist) > 0.001F) {
    m_pose.shoulder_r.setZ(m_pose.shoulder_r.z() + torso_twist);
    m_pose.shoulder_l.setZ(m_pose.shoulder_l.z() - torso_twist * 0.6F);
  }

  if (std::abs(shoulder_rotation) > 0.001F) {
    m_pose.shoulder_r.setY(m_pose.shoulder_r.y() - shoulder_rotation);
    m_pose.shoulder_l.setY(m_pose.shoulder_l.y() + shoulder_rotation * 0.4F);
  }

  if (forward_lean > 0.001F) {
    m_pose.shoulder_l.setZ(m_pose.shoulder_l.z() + forward_lean);
    m_pose.shoulder_r.setZ(m_pose.shoulder_r.z() + forward_lean);
    m_pose.neck_base.setZ(m_pose.neck_base.z() + forward_lean * 0.7F);
    m_pose.head_pos.setZ(m_pose.head_pos.z() + forward_lean * 0.5F);
  }

  if (std::abs(weight_shift) > 0.001F) {
    m_pose.foot_r.setZ(m_pose.foot_r.z() + weight_shift);
    m_pose.knee_r.setZ(m_pose.knee_r.z() + weight_shift * 0.6F);
  }

  place_hand_at(false, hand_r_target);
  place_hand_at(true, hand_l_target);
}

void HumanoidPoseController::spear_thrust_variant(float attack_phase,
                                                  std::uint8_t variant) {
  using HP = HumanProportions;

  attack_phase = std::clamp(attack_phase, 0.0F, 1.0F);

  constexpr float k_thrust_high = 1.0F;
  constexpr float k_thrust_middle = 0.0F;
  constexpr float k_thrust_low = -1.0F;

  QVector3D guard_pos(0.26F, HP::SHOULDER_Y + 0.02F, 0.28F);
  QVector3D chamber_pos(0.34F, HP::SHOULDER_Y + 0.08F, 0.00F);
  QVector3D thrust_pos(0.28F, HP::SHOULDER_Y - 0.10F, 1.04F);
  QVector3D extended_pos(0.24F, HP::SHOULDER_Y - 0.16F, 1.18F);
  QVector3D recover_pos(0.28F, HP::SHOULDER_Y - 0.02F, 0.45F);

  float thrust_height = k_thrust_middle;
  float crouch_amount = 0.0F;

  switch (variant % 3) {
  case 1:

    chamber_pos = QVector3D(0.30F, HP::SHOULDER_Y + 0.10F, 0.0F);
    thrust_pos = QVector3D(0.28F, HP::WAIST_Y + 0.03F, 1.06F);
    extended_pos = QVector3D(0.24F, HP::WAIST_Y - 0.03F, 1.20F);
    recover_pos = QVector3D(0.28F, HP::SHOULDER_Y - 0.08F, 0.42F);
    thrust_height = k_thrust_low;
    crouch_amount = 0.08F;
    break;
  case 2:

    chamber_pos = QVector3D(0.35F, HP::SHOULDER_Y + 0.02F, 0.08F);
    thrust_pos = QVector3D(0.30F, HP::SHOULDER_Y - 0.02F, 1.00F);
    extended_pos = QVector3D(0.28F, HP::SHOULDER_Y - 0.06F, 1.12F);
    thrust_height = k_thrust_high;
    break;
  default:

    break;
  }

  QVector3D hand_r_target;
  QVector3D hand_l_target;

  float forward_lean = 0.0F;
  float torso_twist = 0.0F;
  float shoulder_drop = 0.0F;
  float step_forward = 0.0F;
  float hip_rotation = 0.0F;
  float crouch_factor = 0.0F;

  auto ease_in_out_cubic = [](float t) {
    return t < 0.5F ? 4.0F * t * t * t
                    : 1.0F - std::pow(-2.0F * t + 2.0F, 3.0F) / 2.0F;
  };

  auto smoothstep = [](float t) { return t * t * (3.0F - 2.0F * t); };
  auto ease_out = [](float t) { return 1.0F - (1.0F - t) * (1.0F - t); };

  if (attack_phase < 0.18F) {

    float const t = ease_in_out_cubic(attack_phase / 0.18F);
    hand_r_target = guard_pos * (1.0F - t) + chamber_pos * t;
    hand_l_target = QVector3D(-0.08F, HP::SHOULDER_Y - 0.04F,
                              0.22F * (1.0F - t) + 0.06F * t);

    torso_twist = -0.06F * t;
    hip_rotation = -0.04F * t;
    forward_lean = -0.03F * t;
    crouch_factor = crouch_amount * t;
  } else if (attack_phase < 0.28F) {

    hand_r_target = chamber_pos;
    hand_l_target = QVector3D(-0.08F, HP::SHOULDER_Y - 0.04F, 0.06F);

    torso_twist = -0.06F;
    hip_rotation = -0.04F;
    forward_lean = -0.03F;
    crouch_factor = crouch_amount;
  } else if (attack_phase < 0.48F) {

    float t = (attack_phase - 0.28F) / 0.20F;
    float power_t = t * t * t;
    hand_r_target = chamber_pos * (1.0F - power_t) + thrust_pos * power_t;
    hand_l_target = QVector3D(-0.08F + 0.06F * power_t,
                              HP::SHOULDER_Y - 0.04F + 0.02F * power_t,
                              0.06F + 0.50F * power_t);

    torso_twist = -0.06F + 0.14F * power_t;
    hip_rotation = -0.04F + 0.10F * power_t;
    forward_lean = -0.05F + 0.26F * power_t;
    shoulder_drop = 0.08F * power_t;
    step_forward = 0.18F * power_t;
    crouch_factor = crouch_amount * (1.0F - power_t * 0.3F);

    if (thrust_height < 0) {

      crouch_factor += 0.06F * power_t;
    } else if (thrust_height > 0) {

      crouch_factor += 0.02F * power_t;
    }
  } else if (attack_phase < 0.60F) {

    float const t = smoothstep((attack_phase - 0.48F) / 0.12F);
    hand_r_target = thrust_pos * (1.0F - t) + extended_pos * t;
    hand_l_target =
        QVector3D(-0.02F, HP::SHOULDER_Y - 0.02F, 0.56F + 0.10F * t);

    torso_twist = 0.08F;
    hip_rotation = 0.06F;
    forward_lean = 0.21F + 0.06F * t;
    shoulder_drop = 0.08F + 0.03F * t;
    step_forward = 0.18F + 0.05F * t;
    crouch_factor = crouch_amount * 0.7F;
  } else if (attack_phase < 0.78F) {

    float const t = ease_in_out_cubic((attack_phase - 0.60F) / 0.18F);
    hand_r_target = extended_pos * (1.0F - t) + recover_pos * t;
    hand_l_target = QVector3D(-0.02F * (1.0F - t) - 0.08F * t,
                              HP::SHOULDER_Y - 0.02F * (1.0F - t) - 0.05F * t,
                              0.66F * (1.0F - t) + 0.38F * t);

    torso_twist = 0.08F * (1.0F - t);
    hip_rotation = 0.06F * (1.0F - t);
    forward_lean = 0.25F * (1.0F - t) + 0.04F * t;
    shoulder_drop = 0.10F * (1.0F - t);
    step_forward = 0.22F * (1.0F - t * 0.5F);
    crouch_factor = crouch_amount * 0.7F * (1.0F - t);
  } else {

    float const t = ease_out((attack_phase - 0.78F) / 0.22F);
    hand_r_target = recover_pos * (1.0F - t) + guard_pos * t;
    hand_l_target =
        QVector3D(-0.08F, HP::SHOULDER_Y - 0.05F * (1.0F - t) - 0.02F * t,
                  0.38F * (1.0F - t) + 0.22F * t);

    forward_lean = 0.04F * (1.0F - t);
    step_forward = 0.08F * (1.0F - t);
  }

  if (std::abs(torso_twist) > 0.001F) {
    m_pose.shoulder_r.setZ(m_pose.shoulder_r.z() + torso_twist);
    m_pose.shoulder_l.setZ(m_pose.shoulder_l.z() - torso_twist * 0.4F);
  }

  if (std::abs(hip_rotation) > 0.001F) {
    m_pose.pelvis_pos.setZ(m_pose.pelvis_pos.z() + hip_rotation * 0.5F);
  }

  if (std::abs(forward_lean) > 0.001F) {
    m_pose.shoulder_l.setZ(m_pose.shoulder_l.z() + forward_lean);
    m_pose.shoulder_r.setZ(m_pose.shoulder_r.z() + forward_lean);
    m_pose.neck_base.setZ(m_pose.neck_base.z() + forward_lean * 0.85F);
    m_pose.head_pos.setZ(m_pose.head_pos.z() + forward_lean * 0.7F);
  }

  if (shoulder_drop > 0.001F) {
    m_pose.shoulder_r.setY(m_pose.shoulder_r.y() - shoulder_drop);
    m_pose.shoulder_l.setY(m_pose.shoulder_l.y() - shoulder_drop * 0.3F);
  }

  if (step_forward > 0.001F) {
    m_pose.foot_r.setZ(m_pose.foot_r.z() + step_forward);
    m_pose.knee_r.setZ(m_pose.knee_r.z() + step_forward * 0.6F);
    m_pose.foot_l.setZ(m_pose.foot_l.z() - step_forward * 0.15F);
  }

  if (crouch_factor > 0.001F) {
    m_pose.pelvis_pos.setY(m_pose.pelvis_pos.y() - crouch_factor);
    m_pose.shoulder_l.setY(m_pose.shoulder_l.y() - crouch_factor * 0.6F);
    m_pose.shoulder_r.setY(m_pose.shoulder_r.y() - crouch_factor * 0.6F);
    m_pose.neck_base.setY(m_pose.neck_base.y() - crouch_factor * 0.5F);
    m_pose.head_pos.setY(m_pose.head_pos.y() - crouch_factor * 0.4F);
  }

  place_hand_at(false, hand_r_target);
  place_hand_at(true, hand_l_target);
}

void HumanoidPoseController::tilt_torso(float side_tilt, float forward_tilt) {

  QVector3D const right = m_anim_ctx.heading_right();
  QVector3D const forward = m_anim_ctx.heading_forward();

  QVector3D const offset = right * side_tilt + forward * forward_tilt;

  m_pose.shoulder_l += offset;
  m_pose.shoulder_r += offset;
  m_pose.neck_base += offset * 1.2F;
  m_pose.head_pos += offset * 1.5F;

  m_pose.body_frames.torso.origin += offset;
  m_pose.body_frames.head.origin += offset * 1.5F;
}

} // namespace Render::GL
