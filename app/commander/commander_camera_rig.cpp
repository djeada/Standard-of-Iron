#include "app/commander/commander_camera_rig.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

#include "animation/locomotion_manifest.h"
#include "game/accessibility/motion_settings.h"
#include "game/core/component.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_line_of_sight.h"
#include "scene/camera.h"

namespace App::Core {

namespace {

constexpr float k_pi = std::numbers::pi_v<float>;
constexpr float k_deg2rad = 0.017453292519943295F;

constexpr float k_focus_height = 1.45F;

constexpr float k_walk_bob_freq =
    2.0F * k_pi / Animation::k_humanoid_walk_cycle_distance;
constexpr float k_run_bob_freq = 2.0F * k_pi / Animation::k_humanoid_run_cycle_distance;
constexpr float k_bob_vert_amp = 0.020F;
constexpr float k_bob_run_mult = 1.35F;
constexpr float k_bob_lat_amp = 0.013F;
constexpr float k_bob_decay = 5.5F;

constexpr float k_ground_follow_rate = 4.0F;
constexpr float k_ground_follow_max_lag = 0.55F;

constexpr float k_breath_freq = 0.2F;
constexpr float k_breath_vert_amp = 0.008F;

constexpr float k_anchor_follow_rate = 16.0F;
constexpr float k_anchor_max_lag = 0.30F;
constexpr float k_focus_point_follow = 12.0F;
constexpr float k_focus_weight_follow = 6.0F;
constexpr float k_occlusion_follow = 18.0F;

constexpr float k_lean_max_deg = 2.2F;
constexpr float k_lean_follow = 6.5F;

constexpr float k_fov_run_boost = 7.0F;
constexpr float k_fov_lerp = 5.0F;

constexpr float k_aim_blend_rate = 9.0F;
constexpr float k_framing_blend_rate = 9.0F;
constexpr float k_aim_sway_damping = 0.72F;

constexpr float k_hit_kick_decay = 9.0F;
constexpr float k_hit_kick_dolly = 0.14F;
constexpr float k_hit_kick_fov = 2.6F;

constexpr float k_threat_bias_follow = 3.0F;
constexpr float k_threat_bias_side = 0.30F;

constexpr float k_camera_body_radius = 0.28F;

constexpr float k_commander_near_plane = 0.05F;
constexpr float k_camera_terrain_clearance = 0.55F;

auto smooth_alpha(float rate, float dt) -> float {
  return 1.0F - std::exp(-rate * std::max(dt, 0.0F));
}

auto signed_degrees_between(float target, float current) -> float {
  float diff = target - current;
  while (diff > 180.0F) {
    diff -= 360.0F;
  }
  while (diff < -180.0F) {
    diff += 360.0F;
  }
  return diff;
}

} // namespace

auto CommanderCameraRig::select_framing(bool aiming_bow,
                                        bool lock_target_active,
                                        Engine::Core::FightContext fight_context)
    -> CommanderFramingState {
  if (aiming_bow) {
    return CommanderFramingState::BowAim;
  }
  if (lock_target_active && fight_context == Engine::Core::FightContext::Duel) {
    return CommanderFramingState::DuelLock;
  }
  if (fight_context != Engine::Core::FightContext::None) {
    return CommanderFramingState::Melee;
  }
  return CommanderFramingState::Explore;
}

auto CommanderCameraRig::framing_for(CommanderFramingState state,
                                     bool close_camera_mode) -> Framing {
  switch (state) {
  case CommanderFramingState::BowAim:
    return {2.10F, 0.58F, 0.70F, 14.0F, 48.0F, 0.0F};
  case CommanderFramingState::DuelLock:
    return close_camera_mode ? Framing{2.30F, 0.95F, 0.95F, 4.6F, 60.0F, 0.85F}
                             : Framing{2.70F, 1.05F, 1.10F, 5.0F, 62.0F, 0.85F};
  case CommanderFramingState::Melee:
    return close_camera_mode ? Framing{2.45F, 1.20F, 1.02F, 5.0F, 61.0F, 0.95F}
                             : Framing{3.20F, 1.34F, 1.30F, 5.6F, 63.0F, 1.05F};
  case CommanderFramingState::Explore:
    break;
  }
  return close_camera_mode ? Framing{2.25F, 1.05F, 0.72F, 5.2F, 64.0F, 0.0F}
                           : Framing{3.10F, 1.15F, 0.90F, 6.0F, 68.0F, 0.0F};
}

void CommanderCameraRig::reset() {
  m_framing_state = CommanderFramingState::Explore;
  m_framing_current = {};
  m_framing_valid = false;
  m_bob_phase = 0.0F;
  m_bob_amplitude = 0.0F;
  m_breath_phase = 0.0F;
  m_strafe_lean = 0.0F;
  m_fov_current = 75.0F;
  m_aim_blend = 0.0F;
  m_hit_impact_kick = 0.0F;
  m_threat_bias_smooth = 0.0F;
  m_smooth_valid = false;
  m_forward_valid = false;
  m_ground_valid = false;
  m_state = {};
  m_focus_point_valid = false;
  m_focus_weight_smooth = 0.0F;
  m_focus_side_nudge_smooth = 0.0F;
  m_occlusion_fraction = 1.0F;
  m_trace = {};
}

void CommanderCameraRig::add_impact_kick(float strength) {
  m_hit_impact_kick = std::max(m_hit_impact_kick, strength);
}

auto CommanderCameraRig::update(Render::GL::Camera& camera,
                                const CommanderCameraInputs& inputs) -> float {
  float const dt = inputs.dt;
  float const motion_scale = Game::Accessibility::MotionSettings::camera_motion_scale();

  float const yaw_step = signed_degrees_between(inputs.view_yaw_degrees, m_state.yaw);
  float const pitch_step = inputs.view_pitch_degrees - m_state.pitch;
  m_state.yaw_velocity = dt > 0.0F ? yaw_step / dt : 0.0F;
  m_state.pitch_velocity = dt > 0.0F ? pitch_step / dt : 0.0F;
  m_state.yaw = inputs.view_yaw_degrees;
  m_state.pitch = inputs.view_pitch_degrees;

  CommanderFramingState const previous_framing_state = m_framing_state;
  m_framing_state = select_framing(
      inputs.aiming_bow, inputs.lock_target_active, inputs.fight_context);
  Framing const framing_target = framing_for(m_framing_state, inputs.close_camera_mode);
  if (!m_framing_valid) {
    m_framing_current = framing_target;
    m_framing_valid = true;
  } else {
    float const framing_alpha = smooth_alpha(k_framing_blend_rate, dt);
    m_framing_current.back +=
        (framing_target.back - m_framing_current.back) * framing_alpha;
    m_framing_current.up += (framing_target.up - m_framing_current.up) * framing_alpha;
    m_framing_current.side +=
        (framing_target.side - m_framing_current.side) * framing_alpha;
    m_framing_current.distance +=
        (framing_target.distance - m_framing_current.distance) * framing_alpha;
    m_framing_current.fov +=
        (framing_target.fov - m_framing_current.fov) * framing_alpha;
    m_framing_current.look_drop +=
        (framing_target.look_drop - m_framing_current.look_drop) * framing_alpha;
  }

  m_aim_blend += ((inputs.aiming_bow ? 1.0F : 0.0F) - m_aim_blend) *
                 smooth_alpha(k_aim_blend_rate, dt);
  float const aim_blend = std::clamp(m_aim_blend, 0.0F, 1.0F);
  float const sway_scale = 1.0F - (k_aim_sway_damping * aim_blend);

  float const bob_amp_target = (inputs.move_speed > 0.05F) ? motion_scale : 0.0F;
  m_bob_amplitude += (bob_amp_target - m_bob_amplitude) * smooth_alpha(k_bob_decay, dt);
  float const previous_bob_phase = m_bob_phase;
  if (inputs.move_speed > 0.05F) {
    m_bob_phase += inputs.move_speed *
                   (inputs.move_running ? k_run_bob_freq : k_walk_bob_freq) * dt;
  } else if (m_bob_amplitude < 0.01F) {
    m_bob_phase = 0.0F;
  }
  float const bob_run_factor = inputs.move_running ? k_bob_run_mult : 1.0F;
  float const bob_v = std::sin(m_bob_phase) * k_bob_vert_amp * bob_run_factor *
                      m_bob_amplitude * sway_scale;
  float const bob_l = std::sin(m_bob_phase * 0.5F) * k_bob_lat_amp * bob_run_factor *
                      m_bob_amplitude * sway_scale;

  m_breath_phase += k_breath_freq * 2.0F * k_pi * std::max(dt, 0.0F);
  float const breath_idle = motion_scale - m_bob_amplitude;
  float const breath_v =
      std::sin(m_breath_phase) * k_breath_vert_amp * breath_idle * sway_scale;

  float const lean_target = -static_cast<float>(inputs.move_right_axis) *
                            k_lean_max_deg * motion_scale * sway_scale;
  m_strafe_lean += (lean_target - m_strafe_lean) * smooth_alpha(k_lean_follow, dt);

  m_threat_bias_smooth += (inputs.threat_side_bias - m_threat_bias_smooth) *
                          smooth_alpha(k_threat_bias_follow, dt);

  m_hit_impact_kick *= std::exp(-k_hit_kick_decay * std::max(dt, 0.0F));
  if (m_hit_impact_kick < 0.001F) {
    m_hit_impact_kick = 0.0F;
  }
  float const hit_kick = m_hit_impact_kick * motion_scale;

  float const fov_target =
      m_framing_current.fov +
      ((inputs.move_running && inputs.move_speed > 0.05F) ? k_fov_run_boost : 0.0F) +
      inputs.dodge_fov_kick + (k_hit_kick_fov * hit_kick);
  m_fov_current += (fov_target - m_fov_current) * smooth_alpha(k_fov_lerp, dt);
  camera.set_perspective(
      m_fov_current, camera.get_aspect(), k_commander_near_plane, camera.get_far());

  float const yaw_rad = inputs.view_yaw_degrees * k_deg2rad;
  float const pitch_rad = inputs.view_pitch_degrees * k_deg2rad;
  float const pitch_cos = std::cos(pitch_rad);
  QVector3D const forward_vec(std::sin(yaw_rad) * pitch_cos,
                              std::sin(pitch_rad),
                              std::cos(yaw_rad) * pitch_cos);
  QVector3D const flat_forward(std::sin(yaw_rad), 0.0F, std::cos(yaw_rad));
  QVector3D const flat_right(-flat_forward.z(), 0.0F, flat_forward.x());

  QVector3D const commander_position = inputs.commander_position;
  if (!m_state.anchor_valid) {
    m_state.visual_anchor = commander_position;
    m_state.anchor_valid = true;
  } else {
    float const planar_alpha = smooth_alpha(k_anchor_follow_rate, dt);
    float const vertical_alpha = smooth_alpha(k_ground_follow_rate, dt);
    QVector3D anchor = m_state.visual_anchor;
    anchor.setX(anchor.x() + (commander_position.x() - anchor.x()) * planar_alpha);
    anchor.setY(anchor.y() + (commander_position.y() - anchor.y()) * vertical_alpha);
    anchor.setZ(anchor.z() + (commander_position.z() - anchor.z()) * planar_alpha);
    anchor.setX(std::clamp(anchor.x(),
                           commander_position.x() - k_anchor_max_lag,
                           commander_position.x() + k_anchor_max_lag));
    anchor.setY(std::clamp(anchor.y(),
                           commander_position.y() - k_ground_follow_max_lag,
                           commander_position.y() + k_ground_follow_max_lag));
    anchor.setZ(std::clamp(anchor.z(),
                           commander_position.z() - k_anchor_max_lag,
                           commander_position.z() + k_anchor_max_lag));
    m_state.visual_anchor = anchor;
  }
  m_ground_y = m_state.visual_anchor.y();
  m_ground_valid = true;

  QVector3D const pivot(m_state.visual_anchor.x(),
                        m_ground_y + inputs.jump_height_offset + k_focus_height,
                        m_state.visual_anchor.z());

  float const side_offset =
      m_framing_current.side +
      (k_threat_bias_side * m_threat_bias_smooth * (1.0F - aim_blend));
  QVector3D eye_desired =
      pivot - flat_forward * m_framing_current.back +
      QVector3D(0.0F, m_framing_current.up + bob_v + breath_v, 0.0F) +
      flat_right * (side_offset + bob_l);
  eye_desired -= flat_forward * (k_hit_kick_dolly * hit_kick);

  std::optional<QVector3D> focus_world;
  float focus_weight_target = 0.0F;
  float focus_side_nudge_target = 0.0F;
  if (inputs.lock_target_position.has_value()) {
    focus_world = *inputs.lock_target_position + QVector3D(0.0F, 1.1F, 0.0F);
    focus_weight_target = 0.60F * (1.0F - aim_blend);
    focus_side_nudge_target =
        (inputs.close_camera_mode ? 0.24F : 0.38F) * (1.0F - aim_blend);
  } else if (inputs.soft_focus_position.has_value()) {
    focus_world = *inputs.soft_focus_position + QVector3D(0.0F, 1.1F, 0.0F);
    focus_weight_target = 0.32F * (1.0F - aim_blend);
  }

  if (focus_world.has_value()) {
    if (!m_focus_point_valid) {
      m_focus_point_smooth = *focus_world;
      m_focus_point_valid = true;
    } else {
      m_focus_point_smooth += (*focus_world - m_focus_point_smooth) *
                              smooth_alpha(k_focus_point_follow, dt);
    }
  }
  m_focus_weight_smooth += (focus_weight_target - m_focus_weight_smooth) *
                           smooth_alpha(k_focus_weight_follow, dt);
  m_focus_side_nudge_smooth += (focus_side_nudge_target - m_focus_side_nudge_smooth) *
                               smooth_alpha(k_focus_weight_follow, dt);
  float const focus_weight = std::clamp(m_focus_weight_smooth, 0.0F, 1.0F);
  if (focus_weight <= 0.001F && !focus_world.has_value()) {
    m_focus_point_valid = false;
  }

  eye_desired += flat_right * m_focus_side_nudge_smooth;

  QVector3D const free_look_target =
      eye_desired + forward_vec * m_framing_current.distance -
      QVector3D(0.0F, m_framing_current.look_drop * (1.0F - aim_blend), 0.0F);
  QVector3D target_desired = free_look_target;

  if (m_focus_point_valid && focus_weight > 0.001F) {
    QVector3D to_enemy = m_focus_point_smooth - pivot;
    if (to_enemy.lengthSquared() > 0.0001F) {
      float const enemy_distance = std::sqrt(to_enemy.lengthSquared());
      to_enemy /= enemy_distance;
      target_desired =
          free_look_target * (1.0F - focus_weight) +
          (m_focus_point_smooth + to_enemy * std::min(1.2F, enemy_distance * 0.15F)) *
              focus_weight;
    }
  }

  QVector3D const eye_unconstrained = eye_desired;
  QVector3D const target_unconstrained = target_desired;

  float const blocked_fraction =
      inputs.buildings != nullptr
          ? Game::Systems::first_building_body_intersection_fraction(
                *inputs.buildings, pivot, eye_desired, k_camera_body_radius)
          : 1.0F;
  float const occlusion_target =
      blocked_fraction < 1.0F ? std::clamp(blocked_fraction - 0.06F,
                                           inputs.close_camera_mode ? 0.12F : 0.22F,
                                           1.0F)
                              : 1.0F;
  if (occlusion_target < m_occlusion_fraction) {
    m_occlusion_fraction = occlusion_target;
  } else {
    m_occlusion_fraction += (occlusion_target - m_occlusion_fraction) *
                            smooth_alpha(k_occlusion_follow, dt);
  }
  if (m_occlusion_fraction < 0.999F) {
    eye_desired = pivot + (eye_desired - pivot) * m_occlusion_fraction;
  }

  if (inputs.buildings != nullptr) {

    QVector3D const cleared = Game::Systems::depenetrate_from_building_bodies(
        *inputs.buildings, eye_desired, k_camera_body_radius);
    eye_desired.setX(cleared.x());
    eye_desired.setZ(cleared.z());
  }

  float terrain_lift = 0.0F;
  if (inputs.terrain != nullptr && inputs.terrain->is_initialized()) {
    float const eye_ground_y = inputs.terrain->resolve_surface_world_y(
        eye_desired.x(), eye_desired.z(), 0.0F, eye_desired.y());
    float const lifted =
        std::max(eye_desired.y(), eye_ground_y + k_camera_terrain_clearance);
    terrain_lift = lifted - eye_desired.y();
    eye_desired.setY(lifted);
  }

  m_trace.valid = true;
  m_trace.commander_position = commander_position;
  m_trace.visual_anchor = m_state.visual_anchor;
  m_trace.anchor_lag = (commander_position - m_state.visual_anchor).length();
  m_trace.pivot = pivot;
  m_trace.eye_unconstrained = eye_unconstrained;
  m_trace.target_unconstrained = target_unconstrained;
  m_trace.eye_resolved = eye_desired;
  m_trace.target_resolved = target_desired;
  m_trace.boom_unconstrained = (eye_unconstrained - pivot).length();
  m_trace.boom_resolved = (eye_desired - pivot).length();
  m_trace.building_blocked_fraction = blocked_fraction;
  m_trace.occlusion_fraction = m_occlusion_fraction;
  m_trace.terrain_lift = terrain_lift;
  m_trace.eye_clearance =
      inputs.buildings != nullptr
          ? Game::Systems::nearest_building_body_clearance(*inputs.buildings, eye_desired)
          : std::numeric_limits<float>::max();
  m_trace.fov = m_fov_current;
  m_trace.yaw = m_state.yaw;
  m_trace.pitch = m_state.pitch;
  m_trace.yaw_velocity = m_state.yaw_velocity;
  m_trace.pitch_velocity = m_state.pitch_velocity;
  m_trace.ground_y = m_ground_y;
  m_trace.framing_state = m_framing_state;
  m_trace.framing_changed = m_framing_state != previous_framing_state;
  m_trace.dt = dt;

  m_eye_smooth = eye_desired;
  m_target_smooth = target_desired;
  m_smooth_valid = true;

  float const lean_rad = m_strafe_lean * k_deg2rad;
  QVector3D const world_up(0.0F, 1.0F, 0.0F);
  QVector3D const right_world =
      QVector3D::crossProduct(forward_vec.normalized(), world_up).normalized();
  QVector3D const up_leaned =
      (world_up + right_world * std::sin(lean_rad)).normalized();

  float dodge_tilt_rad = 0.0F;
  if (inputs.dodge_rolling) {
    float const tilt_curve =
        std::sin(std::clamp(inputs.dodge_tilt_progress, 0.0F, 1.0F) * k_pi) * 0.12F;
    float const dot_right = inputs.dodge_direction.x() * flat_right.x() +
                            inputs.dodge_direction.z() * flat_right.z();
    dodge_tilt_rad = tilt_curve * (dot_right > 0.0F ? 1.0F : -1.0F);
  }

  QVector3D const up_final = (up_leaned + right_world * dodge_tilt_rad).normalized();
  camera.look_at(m_eye_smooth, m_target_smooth, up_final);

  QVector3D const view_axis = m_target_smooth - m_eye_smooth;
  if (view_axis.lengthSquared() > 1.0e-6F) {
    m_forward = view_axis.normalized();
    m_forward_valid = true;
  }

  return previous_bob_phase;
}

} // namespace App::Core
