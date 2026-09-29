#include "weapon_trace_sampling.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "../../../animation/attack_pose_manifest.h"
#include "../../../animation/melee_swing_manifest.h"
#include "animation/rig/humanoid_proportions.h"
#include "animation/rig/mounted_seat.h"
#include "weapon_trace_baked.h"

namespace Game::Systems::CombatActions {

namespace {

[[nodiscard]] auto pose_vec_to_qvec(Animation::PoseVec3 value) -> QVector3D {
  return {value.x, value.y, value.z};
}

[[nodiscard]] auto attacker_frame(
    const Game::Systems::RpgCombat::SoldierTarget& soldier) -> AttackerFrame {
  AttackerFrame frame;
  if (soldier.entity == nullptr) {
    return frame;
  }
  float const yaw = soldier.yaw_degrees * (std::numbers::pi_v<float> / 180.0F);
  frame.forward = QVector3D(std::sin(yaw), 0.0F, std::cos(yaw));
  frame.right = QVector3D(frame.forward.z(), 0.0F, -frame.forward.x());
  frame.origin = soldier.position;
  frame.valid = true;
  return frame;
}

[[nodiscard]] auto
trace_window_start(const CombatActionDefinition& definition) -> float {
  for (auto const& event : definition.events) {
    if (event.type == CombatActionEventType::WeaponTraceStart) {
      return event.normalized_time;
    }
  }
  return 0.0F;
}

[[nodiscard]] auto trace_window_end(const CombatActionDefinition& definition) -> float {
  for (auto const& event : definition.events) {
    if (event.type == CombatActionEventType::WeaponTraceEnd) {
      return event.normalized_time;
    }
  }
  return 1.0F;
}

[[nodiscard]] auto is_mounted_weapon_action(CombatActionId id) -> bool {
  return id == CombatActionId::MountedSwordSlash ||
         id == CombatActionId::MountedSpearThrust;
}

[[nodiscard]] auto
mounted_seat_relative(Animation::MountedSeatOffset offset) -> QVector3D {
  using namespace Animation::Rig::MountedSeat;
  return position + forward * offset.forward + right * offset.right + up * offset.up;
}

[[nodiscard]] auto attack_pose_kind_for_definition(
    const CombatActionDefinition& definition) -> Animation::HumanoidWeaponAttackKind {
  if (definition.weapon_family == WeaponFamily::Spear) {
    return Animation::HumanoidWeaponAttackKind::SpearThrust;
  }
  return Animation::HumanoidWeaponAttackKind::CombatSwordSlash;
}

[[nodiscard]] auto attack_pose_variant_for_definition(
    const CombatActionDefinition& definition) -> std::uint8_t {
  switch (definition.attack_direction) {
  case Engine::Core::AttackDirection::RightSlash:
    return 1U;
  case Engine::Core::AttackDirection::Overhead:
  case Engine::Core::AttackDirection::HeavyOverhead:
    return 2U;
  case Engine::Core::AttackDirection::Thrust:
  case Engine::Core::AttackDirection::LeftSlash:
  default:
    return 0U;
  }
}

[[nodiscard]] auto normalized_or_forward(QVector3D value) -> QVector3D {
  return normalized_or(value, QVector3D(0.0F, 0.0F, 1.0F));
}

[[nodiscard]] auto sample_weapon_attack_pose(const CombatActionDefinition& definition,
                                             float normalized_time)
    -> Animation::HumanoidWeaponAttackPoseSample {
  using HP = Render::GL::HumanProportions;
  return Animation::resolve_humanoid_weapon_attack_pose({
      .kind = attack_pose_kind_for_definition(definition),
      .attack_phase = std::clamp(normalized_time, 0.0F, 1.0F),
      .variant = attack_pose_variant_for_definition(definition),
      .reach_scale = 1.0F,
      .hold_depth = 0.0F,
      .attack_emphasis = definition.damage.base_multiplier,
      .finisher_attack =
          definition.id == CombatActionId::RpgSwordFinisher ||
          definition.attack_direction == Engine::Core::AttackDirection::HeavyOverhead,
      .shoulder_y = HP::SHOULDER_Y,
      .waist_y = HP::WAIST_Y,
  });
}

[[nodiscard]] auto
sample_mounted_spear_trace_segment(const AttackerFrame& frame,
                                   const CombatActionDefinition& definition,
                                   float previous,
                                   float current) -> WeaponTraceSegment {
  WeaponTraceSegment segment;
  if (definition.id != CombatActionId::MountedSpearThrust) {
    return segment;
  }

  auto const previous_pose = Animation::resolve_mounted_spear_thrust_pose({previous});
  auto const current_pose = Animation::resolve_mounted_spear_thrust_pose({current});

  QVector3D const previous_grip = mounted_seat_relative(previous_pose.right_hand);
  QVector3D const current_grip = mounted_seat_relative(current_pose.right_hand);

  QVector3D const spear_dir = normalized_or(Animation::Rig::MountedSeat::forward +
                                                Animation::Rig::MountedSeat::up * 0.05F,
                                            QVector3D(0.0F, 0.0F, 1.0F));
  float constexpr k_shaft_base_offset = -0.28F;
  float const spear_tip_offset = Animation::Rig::WeaponReach::spear_total;

  segment.previous_base =
      to_world(frame, previous_grip + spear_dir * k_shaft_base_offset);
  segment.previous_tip = to_world(frame, previous_grip + spear_dir * spear_tip_offset);
  segment.current_base =
      to_world(frame, current_grip + spear_dir * k_shaft_base_offset);
  segment.current_tip = to_world(frame, current_grip + spear_dir * spear_tip_offset);
  segment.radius = std::max(0.04F, definition.hit_shape.radius);
  segment.source = WeaponTraceSegmentSource::AuthoredPose;
  segment.valid = true;
  return segment;
}

inline constexpr float k_steered_trace_threshold = 0.15F;

[[nodiscard]] auto steer_amount(const CombatActionDefinition& definition,
                                const Engine::Core::MeleeIntent& intent) -> float {
  return Engine::Core::melee_intent_strike_delta(anchor_intent_for(definition), intent);
}

[[nodiscard]] auto sample_solved_swing_segment(const AttackerFrame& frame,
                                               const CombatActionDefinition& definition,
                                               const Engine::Core::MeleeIntent& intent,
                                               float window_start,
                                               float window_end,
                                               float previous,
                                               float current) -> WeaponTraceSegment {
  using HP = Render::GL::HumanProportions;
  WeaponTraceSegment segment;

  float const window = std::max(window_end - window_start, 1.0e-4F);
  auto swing_phase = [&](float action_time) {
    float const t = std::clamp((action_time - window_start) / window, 0.0F, 1.0F);
    return std::lerp(Animation::k_melee_apex_time, Animation::k_melee_follow_time, t);
  };

  Animation::MeleeSwingInputs swing{};
  swing.intent = intent;
  swing.shoulder_y = HP::SHOULDER_Y;
  swing.arm_reach = HP::UPPER_ARM_LEN + HP::FORE_ARM_LEN;

  swing.phase = swing_phase(previous);
  auto const previous_sample = Animation::resolve_melee_swing(swing);
  swing.phase = swing_phase(current);
  auto const current_sample = Animation::resolve_melee_swing(swing);

  auto grip_of = [](const Animation::MeleeSwingSample& sample) {
    return QVector3D(sample.grip.x, sample.grip.y, sample.grip.z);
  };
  auto blade_of = [](const Animation::MeleeSwingSample& sample) {
    return QVector3D(
        sample.blade_direction.x, sample.blade_direction.y, sample.blade_direction.z);
  };

  float const blade_length = definition.weapon_family == WeaponFamily::Spear
                                 ? definition.hit_shape.reach
                                 : std::max(0.45F, definition.hit_shape.reach * 0.46F);
  float const base_offset =
      definition.weapon_family == WeaponFamily::Spear ? 0.15F : 0.08F;

  QVector3D const previous_grip = grip_of(previous_sample);
  QVector3D const current_grip = grip_of(current_sample);
  QVector3D const previous_dir = blade_of(previous_sample);
  QVector3D const current_dir = blade_of(current_sample);

  segment.previous_base = to_world(frame, previous_grip + previous_dir * base_offset);
  segment.current_base = to_world(frame, current_grip + current_dir * base_offset);
  segment.previous_tip = to_world(frame, previous_grip + previous_dir * blade_length);
  segment.current_tip = to_world(frame, current_grip + current_dir * blade_length);
  segment.radius = std::max(0.04F, definition.hit_shape.radius);
  segment.source = WeaponTraceSegmentSource::AuthoredPose;
  segment.valid = true;
  return segment;
}

[[nodiscard]] auto
sample_authored_pose_segment(const AttackerFrame& frame,
                             const CombatActionDefinition& definition,
                             const Engine::Core::MeleeIntent& intent,
                             float previous,
                             float current) -> WeaponTraceSegment {
  WeaponTraceSegment segment;
  auto const previous_pose = sample_weapon_attack_pose(definition, previous);
  auto const current_pose = sample_weapon_attack_pose(definition, current);
  QVector3D const previous_grip = pose_vec_to_qvec(previous_pose.right_hand);
  QVector3D const current_grip = pose_vec_to_qvec(current_pose.right_hand);

  QVector3D previous_tip;
  QVector3D current_tip;
  if (definition.weapon_family == WeaponFamily::Spear) {
    QVector3D const previous_dir =
        normalized_or_forward(pose_vec_to_qvec(previous_pose.offhand_spear_direction));
    QVector3D const current_dir =
        normalized_or_forward(pose_vec_to_qvec(current_pose.offhand_spear_direction));
    previous_tip = previous_grip + previous_dir * definition.hit_shape.reach;
    current_tip = current_grip + current_dir * definition.hit_shape.reach;
    segment.previous_base = to_world(frame, previous_grip + previous_dir * 0.15F);
    segment.current_base = to_world(frame, current_grip + current_dir * 0.15F);
  } else {
    float const blade_length = std::max(0.45F, definition.hit_shape.reach * 0.46F);
    QVector3D const previous_dir = normalized_or_forward(
        QVector3D(intent.blade_dir_x, intent.blade_dir_y, intent.blade_dir_z));
    QVector3D const current_dir = previous_dir;
    previous_tip = previous_grip + previous_dir * blade_length;
    current_tip = current_grip + current_dir * blade_length;
    segment.previous_base = to_world(frame, previous_grip + previous_dir * 0.08F);
    segment.current_base = to_world(frame, current_grip + current_dir * 0.08F);
  }

  segment.previous_tip = to_world(frame, previous_tip);
  segment.current_tip = to_world(frame, current_tip);
  segment.radius = std::max(0.04F, definition.hit_shape.radius);
  segment.source = WeaponTraceSegmentSource::AuthoredPose;
  segment.valid = true;
  return segment;
}

struct PreferredSegment {
  WeaponTraceSegment segment;
  bool handled{false};
};

[[nodiscard]] auto
sample_baked_or_mounted_segment(const AttackerFrame& frame,
                                const CombatActionDefinition& definition,
                                float previous,
                                float current) -> PreferredSegment {
  if (definition.weapon_family == WeaponFamily::Sword) {
    auto baked_segment =
        sample_baked_sword_trace_segment(frame, definition, previous, current);
    if (baked_segment.valid) {
      return {.segment = baked_segment, .handled = true};
    }
  } else if (definition.weapon_family == WeaponFamily::Spear) {
    auto baked_segment =
        sample_baked_spear_trace_segment(frame, definition, previous, current);
    if (baked_segment.valid) {
      return {.segment = baked_segment, .handled = true};
    }
  }

  if (definition.id == CombatActionId::MountedSpearThrust) {
    auto mounted_segment =
        sample_mounted_spear_trace_segment(frame, definition, previous, current);
    if (mounted_segment.valid) {
      return {.segment = mounted_segment, .handled = true};
    }
  }
  if (is_mounted_weapon_action(definition.id)) {
    return {.handled = true};
  }

  return {};
}

} // namespace

auto attacker_frame(Engine::Core::Entity& attacker) -> AttackerFrame {
  AttackerFrame frame;
  auto* transform = attacker.get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return frame;
  }

  float const yaw = transform->rotation.y * (std::numbers::pi_v<float> / 180.0F);
  frame.forward = QVector3D(std::sin(yaw), 0.0F, std::cos(yaw));
  frame.right = QVector3D(frame.forward.z(), 0.0F, -frame.forward.x());
  frame.origin =
      QVector3D(transform->position.x, transform->position.y, transform->position.z);
  frame.valid = true;
  return frame;
}

auto presented_attacker_frame(Engine::Core::Entity& attacker,
                              Engine::Core::EntityID target_id)
    -> PresentedAttackerFrame {
  auto const carrier =
      Game::Systems::RpgCombat::resolve_damage_carrier(attacker, target_id);
  if (carrier.has_value()) {
    return {
        .frame = attacker_frame(*carrier),
        .soldier_slot = carrier->soldier_slot,
    };
  }
  return {.frame = attacker_frame(attacker)};
}

[[nodiscard]] auto to_world(const AttackerFrame& frame,
                            const QVector3D& local) -> QVector3D {
  return frame.origin + frame.right * local.x() +
         QVector3D(0.0F, 1.0F, 0.0F) * local.y() + frame.forward * local.z();
}

auto normalized_or(QVector3D value, const QVector3D& fallback) -> QVector3D {
  if (value.lengthSquared() <= 1.0e-6F) {
    QVector3D normalized_fallback = fallback;
    if (normalized_fallback.lengthSquared() <= 1.0e-6F) {
      return {0.0F, 0.0F, 1.0F};
    }
    normalized_fallback.normalize();
    return normalized_fallback;
  }
  value.normalize();
  return value;
}

auto anchor_intent_for(const CombatActionDefinition& definition)
    -> Engine::Core::MeleeIntent {
  return Engine::Core::melee_intent_from_attack_direction(definition.attack_direction,
                                                          definition.hit_shape.reach);
}

auto live_intent_of(const Engine::Core::Entity& attacker,
                    const CombatActionDefinition& definition)
    -> Engine::Core::MeleeIntent {
  if (auto const* combat =
          attacker.get_component<Engine::Core::CombatStateComponent>()) {
    return combat->intent;
  }
  return anchor_intent_for(definition);
}

auto sample_segment_in_frame(const AttackerFrame& frame,
                             const CombatActionDefinition& definition,
                             const Engine::Core::MeleeIntent& intent,
                             WeaponTraceTimeSpan time_span) -> WeaponTraceSegment {
  WeaponTraceSegment segment;
  if (definition.weapon_family != WeaponFamily::Sword &&
      definition.weapon_family != WeaponFamily::Spear) {
    return segment;
  }

  if (!frame.valid) {
    return segment;
  }

  float const window_start = trace_window_start(definition);
  float const window_end = trace_window_end(definition);
  float const raw_previous = time_span.previous_normalized_time;
  float const raw_current = time_span.current_normalized_time;
  if (raw_current < window_start || raw_previous > window_end) {
    return segment;
  }
  float previous = std::clamp(raw_previous, window_start, window_end);
  float const current = std::clamp(raw_current, window_start, window_end);
  if (previous > current) {
    previous = current;
  }

  if (steer_amount(definition, intent) > k_steered_trace_threshold &&
      !is_mounted_weapon_action(definition.id)) {
    return sample_solved_swing_segment(
        frame, definition, intent, window_start, window_end, previous, current);
  }

  auto const preferred =
      sample_baked_or_mounted_segment(frame, definition, previous, current);
  if (preferred.handled) {
    return preferred.segment;
  }

  return sample_authored_pose_segment(frame, definition, intent, previous, current);
}

} // namespace Game::Systems::CombatActions
