#include "weapon_trace.h"

#include <QMatrix4x4>
#include <QVector4D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <string_view>

#include "../../../animation/attack_pose_manifest.h"
#include "../../../animation/clip_manifest.h"
#include "../../../animation/melee_swing_manifest.h"
#include "../../core/component_combat.h"
#include "../../core/simulation_timing.h"
#include "../../core/world.h"
#include "../combat_rules.h"
#include "../combat_system/combat_utils.h"
#include "../rpg_combat_system/rpg_targeting.h"
#include "animation/bpat/bpat_format.h"
#include "animation/bpat/bpat_playback.h"
#include "animation/bpat/bpat_reader.h"
#include "animation/bpat/bpat_registry.h"
#include "animation/rig/humanoid_proportions.h"
#include "animation/rig/mounted_seat.h"

namespace Game::Systems::CombatActions {

namespace {

struct LocalTargetSample {
  Engine::Core::Entity* entity{nullptr};
  std::uint16_t soldier_slot{
      Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot};
  float forward{0.0F};
  float right{0.0F};
  float distance{0.0F};
  float radius{0.0F};
  QVector3D world_position{0.0F, 0.0F, 0.0F};
};

struct AttackerFrame {
  bool valid{false};
  QVector3D origin{0.0F, 0.0F, 0.0F};
  QVector3D forward{0.0F, 0.0F, 1.0F};
  QVector3D right{-1.0F, 0.0F, 0.0F};
};

struct SegmentDistance {
  float distance{std::numeric_limits<float>::infinity()};
  QVector3D point{0.0F, 0.0F, 0.0F};
};

struct BakedTraceClip {
  bool valid{false};
  std::uint32_t species_id{0U};
  std::uint16_t clip_id{Animation::k_unmapped_clip};
};

struct SampledSocketFrame {
  bool valid{false};
  QMatrix4x4 transform;
};

[[nodiscard]] auto pose_vec_to_qvec(Animation::PoseVec3 value) -> QVector3D {
  return {value.x, value.y, value.z};
}

[[nodiscard]] auto attacker_frame(Engine::Core::Entity& attacker) -> AttackerFrame {
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

struct PresentedAttackerFrame {
  AttackerFrame frame{};
  std::uint16_t soldier_slot{
      Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot};
};

[[nodiscard]] auto
presented_attacker_frame(Engine::Core::Entity& attacker,
                         Engine::Core::EntityID target_id) -> PresentedAttackerFrame {
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

[[nodiscard]] auto normalized_or(QVector3D value,
                                 const QVector3D& fallback) -> QVector3D {
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

[[nodiscard]] auto baked_trace_clip_for_definition(
    const CombatActionDefinition& definition) -> BakedTraceClip {
  if (definition.weapon_family == WeaponFamily::Sword) {
    switch (definition.id) {
    case CombatActionId::RpgSwordSlashLeft:
    case CombatActionId::RpgSwordSlashRight:
    case CombatActionId::RpgSwordOverhead:
    case CombatActionId::RpgSwordThrust:
    case CombatActionId::RpgSwordFinisher:
    case CombatActionId::CommanderSwordSpin:
    case CombatActionId::CommanderSwordLauncher:
    case CombatActionId::CommanderSwordGapCloser:
    case CombatActionId::CommanderSwordAirLight:
    case CombatActionId::CommanderSwordAirReverse:
    case CombatActionId::CommanderSwordDive:
    case CombatActionId::RtsSwordStrike:
    case CombatActionId::RtsHeavyOverhead:
      return {
          .valid = true,
          .species_id = Render::Creature::Bpat::k_species_humanoid_sword,
          .clip_id = Animation::humanoid_sword_attack_clip(definition.sword_clip),
      };
    case CombatActionId::MountedSwordSlash:
      return {
          .valid = true,
          .species_id = Render::Creature::Bpat::k_species_humanoid_sword,
          .clip_id = Animation::k_humanoid_riding_sword_strike_clip,
      };
    case CombatActionId::None:
    case CombatActionId::RpgSpearThrust:
    case CombatActionId::CommanderSpearStepThrust:
    case CombatActionId::RpgSpearSweep:
    case CombatActionId::RpgSpearFinisher:
    case CombatActionId::CommanderSpearLauncher:
    case CombatActionId::CommanderSpearGapCloser:
    case CombatActionId::CommanderSpearAirThrust:
    case CombatActionId::CommanderSpearDive:
    case CombatActionId::RpgBowShot:
    case CombatActionId::CommanderBowPowerShot:
    case CombatActionId::CommanderBowEvasiveShot:
    case CombatActionId::MountedSpearThrust:
    case CombatActionId::MountedChargeImpact:
    case CombatActionId::RtsSpearThrust:
    case CombatActionId::RtsBowShot:
    case CombatActionId::RtsElephantStomp:
    case CombatActionId::RtsCommanderThrust:
    case CombatActionId::RtsCommanderCut:
    case CombatActionId::RtsCommanderShot:
      break;
    }
  } else if (definition.weapon_family == WeaponFamily::Spear) {
    switch (definition.id) {
    case CombatActionId::RpgSpearThrust:
    case CombatActionId::CommanderSpearStepThrust:
    case CombatActionId::RpgSpearFinisher:
    case CombatActionId::CommanderSpearGapCloser:
    case CombatActionId::CommanderSpearAirThrust:
    case CombatActionId::RtsSpearThrust:
      return {
          .valid = true,
          .species_id = Render::Creature::Bpat::k_species_humanoid_spear,
          .clip_id = Animation::k_humanoid_attack_spear_a_clip,
      };
    case CombatActionId::RpgSpearSweep:
    case CombatActionId::CommanderSpearLauncher:
    case CombatActionId::CommanderSpearDive:
      return {
          .valid = true,
          .species_id = Render::Creature::Bpat::k_species_humanoid_spear,
          .clip_id = Animation::k_humanoid_attack_spear_b_clip,
      };
    case CombatActionId::MountedSpearThrust:
      return {
          .valid = true,
          .species_id = Render::Creature::Bpat::k_species_humanoid_spear,
          .clip_id = Animation::k_humanoid_riding_spear_thrust_clip,
      };
    case CombatActionId::None:
    case CombatActionId::RpgSwordSlashLeft:
    case CombatActionId::RpgSwordSlashRight:
    case CombatActionId::RpgSwordOverhead:
    case CombatActionId::RpgSwordThrust:
    case CombatActionId::RpgSwordFinisher:
    case CombatActionId::CommanderSwordSpin:
    case CombatActionId::CommanderSwordLauncher:
    case CombatActionId::CommanderSwordGapCloser:
    case CombatActionId::CommanderSwordAirLight:
    case CombatActionId::CommanderSwordAirReverse:
    case CombatActionId::CommanderSwordDive:
    case CombatActionId::RtsHeavyOverhead:
    case CombatActionId::RpgBowShot:
    case CombatActionId::CommanderBowPowerShot:
    case CombatActionId::CommanderBowEvasiveShot:
    case CombatActionId::MountedSwordSlash:
    case CombatActionId::MountedChargeImpact:
    case CombatActionId::RtsSwordStrike:
    case CombatActionId::RtsBowShot:
    case CombatActionId::RtsElephantStomp:
    case CombatActionId::RtsCommanderThrust:
    case CombatActionId::RtsCommanderCut:
    case CombatActionId::RtsCommanderShot:
      break;
    }
  }
  return {};
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
matrix_from_socket_row(std::span<const float> row) -> SampledSocketFrame {
  SampledSocketFrame frame;
  if (row.size() < Render::Creature::Bpat::k_socket_matrix_floats) {
    return frame;
  }

  std::array<float, 16> full{};
  for (int row_idx = 0; row_idx < 3; ++row_idx) {
    for (int col = 0; col < 4; ++col) {
      full[(row_idx * 4) + col] = row[(row_idx * 4) + col];
    }
  }
  full[15] = 1.0F;
  frame.transform = QMatrix4x4(full.data());
  frame.valid = true;
  return frame;
}

[[nodiscard]] auto
blend_vector(const QVector3D& from, const QVector3D& to, float t) -> QVector3D {
  return from + (to - from) * t;
}

[[nodiscard]] auto interpolate_socket_transform(const QMatrix4x4& from,
                                                const QMatrix4x4& to,
                                                float t) -> QMatrix4x4 {
  t = std::clamp(t, 0.0F, 1.0F);
  QVector3D const right = normalized_or(
      blend_vector(from.column(0).toVector3D(), to.column(0).toVector3D(), t),
      from.column(0).toVector3D());
  QVector3D const up = normalized_or(
      blend_vector(from.column(1).toVector3D(), to.column(1).toVector3D(), t),
      from.column(1).toVector3D());
  QVector3D const forward = normalized_or(
      blend_vector(from.column(2).toVector3D(), to.column(2).toVector3D(), t),
      from.column(2).toVector3D());
  QVector3D const origin =
      blend_vector(from.column(3).toVector3D(), to.column(3).toVector3D(), t);

  QMatrix4x4 blended;
  blended.setColumn(0, QVector4D(right, 0.0F));
  blended.setColumn(1, QVector4D(up, 0.0F));
  blended.setColumn(2, QVector4D(forward, 0.0F));
  blended.setColumn(3, QVector4D(origin, 1.0F));
  return blended;
}

[[nodiscard]] auto find_socket_index(const Render::Creature::Bpat::BpatBlob& blob,
                                     std::string_view preferred_name,
                                     std::string_view fallback_name) -> std::uint32_t {
  auto find_by_name = [&](std::string_view name) -> std::uint32_t {
    for (std::uint32_t i = 0U; i < blob.socket_count(); ++i) {
      if (blob.socket(i).name == name) {
        return i;
      }
    }
    return blob.socket_count();
  };

  std::uint32_t const index = find_by_name(preferred_name);
  if (index < blob.socket_count()) {
    return index;
  }
  return find_by_name(fallback_name);
}

[[nodiscard]] auto find_socket_index(const Render::Creature::Bpat::BpatBlob& blob,
                                     std::string_view name) -> std::uint32_t {
  for (std::uint32_t i = 0U; i < blob.socket_count(); ++i) {
    if (blob.socket(i).name == name) {
      return i;
    }
  }
  return blob.socket_count();
}

[[nodiscard]] auto
sample_interpolated_socket_frame(const Render::Creature::Bpat::BpatBlob& blob,
                                 std::uint16_t clip_id,
                                 std::uint32_t socket_index,
                                 float normalized_time) -> SampledSocketFrame {
  auto const playback = Render::Creature::Pipeline::resolve_bpat_playback(
      &blob, clip_id, normalized_time);
  if (!playback.valid() || socket_index >= blob.socket_count()) {
    return {};
  }

  auto const current =
      matrix_from_socket_row(blob.socket_matrix(playback.global_frame, socket_index));
  if (!current.valid) {
    return {};
  }

  if (playback.next_global_frame == playback.global_frame ||
      playback.frame_lerp <= 1.0e-5F) {
    return current;
  }

  auto const next = matrix_from_socket_row(
      blob.socket_matrix(playback.next_global_frame, socket_index));
  if (!next.valid) {
    return current;
  }

  return {
      .valid = true,
      .transform = interpolate_socket_transform(
          current.transform, next.transform, playback.frame_lerp),
  };
}

[[nodiscard]] auto socket_direction(const QMatrix4x4& socket,
                                    const QVector3D& local_direction) -> QVector3D {
  return normalized_or(socket.column(0).toVector3D() * local_direction.x() +
                           socket.column(1).toVector3D() * local_direction.y() +
                           socket.column(2).toVector3D() * local_direction.z(),
                       QVector3D(0.0F, 1.0F, 0.0F));
}

[[nodiscard]] auto socket_point_along_direction(const QMatrix4x4& socket,
                                                const QVector3D& local_direction,
                                                float distance) -> QVector3D {
  return socket.column(3).toVector3D() +
         socket_direction(socket, local_direction) * distance;
}

[[nodiscard]] auto
sample_baked_sword_endpoint_trace_segment(const AttackerFrame& frame,
                                          const CombatActionDefinition& definition,
                                          const Render::Creature::Bpat::BpatBlob& blob,
                                          std::uint16_t clip_id,
                                          float previous,
                                          float current) -> WeaponTraceSegment {
  WeaponTraceSegment segment;
  std::uint32_t const base_index = find_socket_index(blob, "sword_blade_base_r");
  std::uint32_t const tip_index = find_socket_index(blob, "sword_blade_tip_r");
  if (base_index >= blob.socket_count() || tip_index >= blob.socket_count()) {
    return segment;
  }

  auto const previous_base =
      sample_interpolated_socket_frame(blob, clip_id, base_index, previous);
  auto const previous_tip =
      sample_interpolated_socket_frame(blob, clip_id, tip_index, previous);
  auto const current_base =
      sample_interpolated_socket_frame(blob, clip_id, base_index, current);
  auto const current_tip =
      sample_interpolated_socket_frame(blob, clip_id, tip_index, current);
  if (!previous_base.valid || !previous_tip.valid || !current_base.valid ||
      !current_tip.valid) {
    return segment;
  }

  segment.previous_base =
      to_world(frame, previous_base.transform.column(3).toVector3D());
  segment.previous_tip = to_world(frame, previous_tip.transform.column(3).toVector3D());
  segment.current_base = to_world(frame, current_base.transform.column(3).toVector3D());
  segment.current_tip = to_world(frame, current_tip.transform.column(3).toVector3D());
  segment.radius = std::max(0.04F, definition.hit_shape.radius);
  segment.source = WeaponTraceSegmentSource::BakedSocket;
  segment.valid = true;
  return segment;
}

[[nodiscard]] auto
sample_baked_spear_endpoint_trace_segment(const AttackerFrame& frame,
                                          const CombatActionDefinition& definition,
                                          const Render::Creature::Bpat::BpatBlob& blob,
                                          std::uint16_t clip_id,
                                          float previous,
                                          float current) -> WeaponTraceSegment {
  WeaponTraceSegment segment;
  std::uint32_t const base_index = find_socket_index(blob, "spear_shaft_base_r");
  std::uint32_t const tip_index = find_socket_index(blob, "spear_head_tip_r");
  if (base_index >= blob.socket_count() || tip_index >= blob.socket_count()) {
    return segment;
  }

  auto const previous_base =
      sample_interpolated_socket_frame(blob, clip_id, base_index, previous);
  auto const previous_tip =
      sample_interpolated_socket_frame(blob, clip_id, tip_index, previous);
  auto const current_base =
      sample_interpolated_socket_frame(blob, clip_id, base_index, current);
  auto const current_tip =
      sample_interpolated_socket_frame(blob, clip_id, tip_index, current);
  if (!previous_base.valid || !previous_tip.valid || !current_base.valid ||
      !current_tip.valid) {
    return segment;
  }

  segment.previous_base =
      to_world(frame, previous_base.transform.column(3).toVector3D());
  segment.previous_tip = to_world(frame, previous_tip.transform.column(3).toVector3D());
  segment.current_base = to_world(frame, current_base.transform.column(3).toVector3D());
  segment.current_tip = to_world(frame, current_tip.transform.column(3).toVector3D());
  segment.radius = std::max(0.04F, definition.hit_shape.radius);
  segment.source = WeaponTraceSegmentSource::BakedSocket;
  segment.valid = true;
  return segment;
}

[[nodiscard]] auto
sample_baked_sword_trace_segment(const AttackerFrame& frame,
                                 const CombatActionDefinition& definition,
                                 float previous,
                                 float current) -> WeaponTraceSegment {
  WeaponTraceSegment segment;
  auto const clip = baked_trace_clip_for_definition(definition);
  if (!clip.valid) {
    return segment;
  }

  auto const* blob =
      Render::Creature::Bpat::BpatRegistry::instance().blob(clip.species_id);
  if (blob == nullptr) {
    return segment;
  }

  auto endpoint_segment = sample_baked_sword_endpoint_trace_segment(
      frame, definition, *blob, clip.clip_id, previous, current);
  if (endpoint_segment.valid) {
    return endpoint_segment;
  }

  std::uint32_t const socket_index = find_socket_index(*blob, "grip_r", "hand_r");
  if (socket_index >= blob->socket_count()) {
    return segment;
  }

  auto const previous_socket =
      sample_interpolated_socket_frame(*blob, clip.clip_id, socket_index, previous);
  auto const current_socket =
      sample_interpolated_socket_frame(*blob, clip.clip_id, socket_index, current);
  if (!previous_socket.valid || !current_socket.valid) {
    return segment;
  }

  QVector3D blade_axis_local(0.02F, 0.97F, 0.0F);
  blade_axis_local.normalize();
  float const blade_length = std::max(0.45F, definition.hit_shape.reach * 0.46F);
  float constexpr k_blade_base_offset = 0.05F;

  QVector3D const previous_base = socket_point_along_direction(
      previous_socket.transform, blade_axis_local, k_blade_base_offset);
  QVector3D const previous_tip = socket_point_along_direction(
      previous_socket.transform, blade_axis_local, k_blade_base_offset + blade_length);
  QVector3D const current_base = socket_point_along_direction(
      current_socket.transform, blade_axis_local, k_blade_base_offset);
  QVector3D const current_tip = socket_point_along_direction(
      current_socket.transform, blade_axis_local, k_blade_base_offset + blade_length);

  segment.previous_base = to_world(frame, previous_base);
  segment.previous_tip = to_world(frame, previous_tip);
  segment.current_base = to_world(frame, current_base);
  segment.current_tip = to_world(frame, current_tip);
  segment.radius = std::max(0.04F, definition.hit_shape.radius);
  segment.source = WeaponTraceSegmentSource::BakedSocket;
  segment.valid = true;
  return segment;
}

[[nodiscard]] auto
sample_baked_spear_trace_segment(const AttackerFrame& frame,
                                 const CombatActionDefinition& definition,
                                 float previous,
                                 float current) -> WeaponTraceSegment {
  WeaponTraceSegment segment;
  auto const clip = baked_trace_clip_for_definition(definition);
  if (!clip.valid) {
    return segment;
  }

  auto const* blob =
      Render::Creature::Bpat::BpatRegistry::instance().blob(clip.species_id);
  if (blob == nullptr) {
    return segment;
  }

  return sample_baked_spear_endpoint_trace_segment(
      frame, definition, *blob, clip.clip_id, previous, current);
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

[[nodiscard]] auto distance_to_segment_xz(const QVector3D& point,
                                          const QVector3D& start,
                                          const QVector3D& end) -> SegmentDistance {
  float const ax = start.x();
  float const az = start.z();
  float const bx = end.x();
  float const bz = end.z();
  float const px = point.x();
  float const pz = point.z();
  float const abx = bx - ax;
  float const abz = bz - az;
  float const ab_len_sq = abx * abx + abz * abz;

  float t = 0.0F;
  if (ab_len_sq > 1.0e-6F) {
    t = ((px - ax) * abx + (pz - az) * abz) / ab_len_sq;
    t = std::clamp(t, 0.0F, 1.0F);
  }

  QVector3D const closest = start + (end - start) * t;
  float const dx = px - closest.x();
  float const dz = pz - closest.z();
  return {.distance = std::sqrt(dx * dx + dz * dz), .point = closest};
}

[[nodiscard]] auto best_segment_distance_xz(const WeaponTraceSegment& segment,
                                            const QVector3D& point) -> SegmentDistance {
  SegmentDistance best;
  auto consider = [&](const QVector3D& start, const QVector3D& end) {
    auto const distance = distance_to_segment_xz(point, start, end);
    if (distance.distance < best.distance) {
      best = distance;
    }
  };

  consider(segment.current_base, segment.current_tip);
  consider(segment.previous_tip, segment.current_tip);
  consider(segment.previous_base, segment.current_base);
  consider(segment.previous_base, segment.previous_tip);
  return best;
}

[[nodiscard]] auto make_local_sample(
    const AttackerFrame& frame,
    const Game::Systems::RpgCombat::SoldierTarget& target) -> LocalTargetSample {
  LocalTargetSample sample;
  sample.entity = target.entity;
  sample.soldier_slot = target.soldier_slot;

  if (!frame.valid || target.entity == nullptr) {
    sample.entity = nullptr;
    return sample;
  }

  sample.world_position = target.position;
  QVector3D const to_target = sample.world_position - frame.origin;
  sample.forward = QVector3D::dotProduct(to_target, frame.forward);
  sample.right = QVector3D::dotProduct(to_target, frame.right);
  sample.distance =
      std::sqrt(sample.forward * sample.forward + sample.right * sample.right);
  sample.radius = target.body_radius;
  return sample;
}

struct WeaponContactShape {
  float lateral_limit{0.0F};
  float min_forward{0.0F};
};

[[nodiscard]] auto contact_shape(const CombatActionDefinition& definition,
                                 const Engine::Core::MeleeIntent& intent,
                                 float target_radius) -> WeaponContactShape {
  float const vertical = std::clamp(std::abs(intent.strike_dir_y), 0.0F, 1.0F);
  float const thrust = intent.thrust_amount;

  float const cut_lateral = definition.hit_shape.reach * 0.70F;
  float const overhead_lateral = std::max(0.55F, definition.hit_shape.radius);
  float const sweep_lateral = std::lerp(cut_lateral, overhead_lateral, vertical);

  return {.lateral_limit =
              std::lerp(sweep_lateral, definition.hit_shape.radius, thrust) +
              target_radius,
          .min_forward = std::lerp(std::lerp(0.05F, 0.10F, vertical), 0.25F, thrust)};
}

[[nodiscard]] auto
weapon_contact_score(const LocalTargetSample& sample,
                     const CombatActionDefinition& definition,
                     const Engine::Core::MeleeIntent& intent) -> float {
  if (sample.entity == nullptr || !std::isfinite(sample.forward) ||
      !std::isfinite(sample.right) || !std::isfinite(sample.distance) ||
      sample.forward <= 0.0F) {
    return std::numeric_limits<float>::infinity();
  }

  float const reach = definition.hit_shape.reach + sample.radius;
  if (sample.distance > reach) {
    return std::numeric_limits<float>::infinity();
  }

  auto const shape = contact_shape(definition, intent, sample.radius);
  if (sample.forward < shape.min_forward ||
      std::abs(sample.right) > shape.lateral_limit) {
    return std::numeric_limits<float>::infinity();
  }

  return sample.distance + std::abs(sample.right) * 0.15F;
}

} // namespace

namespace {

[[nodiscard]] auto anchor_intent_for(const CombatActionDefinition& definition)
    -> Engine::Core::MeleeIntent {
  return Engine::Core::melee_intent_from_attack_direction(definition.attack_direction,
                                                          definition.hit_shape.reach);
}

inline constexpr float k_steered_trace_threshold = 0.15F;

inline constexpr float k_directed_hint_score_bias = 0.25F;
inline constexpr float k_aimed_hint_score_bias = 0.04F;

[[nodiscard]] auto hint_score_bias(const Engine::Core::Entity& attacker) -> float {
  return Game::Systems::CombatRules::is_player_driven(&attacker)
             ? k_aimed_hint_score_bias
             : k_directed_hint_score_bias;
}

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

auto sample_authored_weapon_trace_segment(const AttackerFrame& frame,
                                          const CombatActionDefinition& definition,
                                          const Engine::Core::MeleeIntent& intent,
                                          WeaponTraceTimeSpan time_span)
    -> WeaponTraceSegment {
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

  if (definition.weapon_family == WeaponFamily::Sword) {
    auto baked_segment =
        sample_baked_sword_trace_segment(frame, definition, previous, current);
    if (baked_segment.valid) {
      return baked_segment;
    }
  } else if (definition.weapon_family == WeaponFamily::Spear) {
    auto baked_segment =
        sample_baked_spear_trace_segment(frame, definition, previous, current);
    if (baked_segment.valid) {
      return baked_segment;
    }
  }

  if (definition.id == CombatActionId::MountedSpearThrust) {
    auto mounted_segment =
        sample_mounted_spear_trace_segment(frame, definition, previous, current);
    if (mounted_segment.valid) {
      return mounted_segment;
    }
  }
  if (is_mounted_weapon_action(definition.id)) {
    return segment;
  }

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

[[nodiscard]] auto
live_intent_of(const Engine::Core::Entity& attacker,
               const CombatActionDefinition& definition) -> Engine::Core::MeleeIntent {
  if (auto const* combat =
          attacker.get_component<Engine::Core::CombatStateComponent>()) {
    return combat->intent;
  }
  return anchor_intent_for(definition);
}

} // namespace

auto sample_authored_weapon_trace_segment(Engine::Core::Entity& attacker,
                                          const CombatActionDefinition& definition,
                                          WeaponTraceTimeSpan time_span)
    -> WeaponTraceSegment {
  return sample_authored_weapon_trace_segment(attacker_frame(attacker),
                                              definition,
                                              live_intent_of(attacker, definition),
                                              time_span);
}

auto find_weapon_trace_contact(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    const CombatActionDefinition& definition,
    WeaponTraceTimeSpan time_span,
    Engine::Core::EntityID target_hint_id,
    std::span<const Engine::Core::EntityID> ignored_target_ids,
    std::span<const WeaponTraceIgnoredTarget> ignored_target_slots)
    -> WeaponTraceContact {
  Engine::Core::Timing::ScopedAccumulator const scope(
      Engine::Core::Timing::commander_weapon_trace());
  constexpr float k_max_trace_sample_span = 0.025F;
  float const trace_span =
      time_span.current_normalized_time - time_span.previous_normalized_time;
  if (trace_span > k_max_trace_sample_span) {
    int const sample_count =
        std::max(1, static_cast<int>(std::ceil(trace_span / k_max_trace_sample_span)));
    for (int sample = 0; sample < sample_count; ++sample) {
      float const sample_start =
          time_span.previous_normalized_time +
          trace_span * (static_cast<float>(sample) / static_cast<float>(sample_count));
      float const sample_end = time_span.previous_normalized_time +
                               trace_span * (static_cast<float>(sample + 1) /
                                             static_cast<float>(sample_count));
      auto contact =
          find_weapon_trace_contact(world,
                                    attacker,
                                    definition,
                                    {.previous_normalized_time = sample_start,
                                     .current_normalized_time = sample_end},
                                    target_hint_id,
                                    ignored_target_ids,
                                    ignored_target_slots);
      if (contact.target_id != 0) {
        return contact;
      }
    }
    return {};
  }

  auto const presented_attacker = presented_attacker_frame(attacker, target_hint_id);
  auto const segment =
      sample_authored_weapon_trace_segment(presented_attacker.frame,
                                           definition,
                                           live_intent_of(attacker, definition),
                                           time_span);
  if (!segment.valid) {
    return find_weapon_trace_contact(world,
                                     attacker,
                                     definition,
                                     target_hint_id,
                                     ignored_target_ids,
                                     ignored_target_slots);
  }

  WeaponTraceContact contact;
  contact.attacker_id = attacker.get_id();
  contact.attacker_soldier_slot = presented_attacker.soldier_slot;
  auto const* attacker_unit = attacker.get_component<Engine::Core::UnitComponent>();
  if (attacker_unit == nullptr) {
    return contact;
  }

  auto const* running_action =
      attacker.get_component<Engine::Core::RpgCommanderActionComponent>();
  float const action_seconds = running_action != nullptr
                                   ? std::max(0.001F, running_action->action_duration)
                                   : 1.0F;
  float const slice_seconds = std::max(
      0.001F,
      (time_span.current_normalized_time - time_span.previous_normalized_time) *
          action_seconds);
  QVector3D const tip_travel = segment.current_tip - segment.previous_tip;
  float const tip_speed = tip_travel.length() / slice_seconds;

  auto consider = [&](Engine::Core::Entity* candidate,
                      float& best_score,
                      WeaponTraceContact& best_contact) {
    if (candidate == nullptr || candidate == &attacker ||
        !Game::Systems::Combat::is_valid_enemy_unit(attacker_unit, candidate, false)) {
      return;
    }
    if (std::find(ignored_target_ids.begin(),
                  ignored_target_ids.end(),
                  candidate->get_id()) != ignored_target_ids.end()) {
      return;
    }

    float const hint_bias =
        candidate->get_id() == target_hint_id ? hint_score_bias(attacker) : 0.0F;

    for (auto const& soldier :
         Game::Systems::RpgCombat::live_soldier_targets(*candidate)) {
      bool const ignored_slot =
          std::any_of(ignored_target_slots.begin(),
                      ignored_target_slots.end(),
                      [&](auto const& ignored) {
                        return ignored.entity_id == candidate->get_id() &&
                               ignored.soldier_slot == soldier.soldier_slot;
                      });
      if (ignored_slot) {
        continue;
      }
      auto sample = make_local_sample(presented_attacker.frame, soldier);
      if (sample.entity == nullptr || !std::isfinite(sample.forward) ||
          !std::isfinite(sample.right) || !std::isfinite(sample.distance) ||
          sample.forward <= 0.0F) {
        continue;
      }

      auto const distance = best_segment_distance_xz(segment, sample.world_position);
      float const hit_radius = segment.radius + sample.radius;
      if (!std::isfinite(distance.distance) || distance.distance > hit_radius) {
        continue;
      }

      float const score = distance.distance + sample.distance * 0.03F - hint_bias;
      if (score >= best_score) {
        continue;
      }

      best_score = score;
      best_contact.target_id = candidate->get_id();
      best_contact.target_soldier_slot = sample.soldier_slot;
      best_contact.distance = sample.distance;
      best_contact.local_forward = sample.forward;
      best_contact.local_right = sample.right;
      best_contact.contact_point =
          Game::Systems::RpgCombat::hurt_body_contact_point(soldier, distance.point);
      best_contact.contact_speed = tip_speed;
    }
  };

  float best_score = std::numeric_limits<float>::infinity();
  for (auto [candidate, candidate_unit] :
       world.entity_view<Engine::Core::UnitComponent>()) {
    (void)candidate_unit;
    consider(&candidate, best_score, contact);
  }

  return contact;
}

auto find_weapon_trace_contact(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    const CombatActionDefinition& definition,
    Engine::Core::EntityID target_hint_id,
    std::span<const Engine::Core::EntityID> ignored_target_ids,
    std::span<const WeaponTraceIgnoredTarget> ignored_target_slots)
    -> WeaponTraceContact {
  Engine::Core::Timing::ScopedAccumulator const scope(
      Engine::Core::Timing::commander_weapon_trace());
  WeaponTraceContact contact;
  contact.attacker_id = attacker.get_id();

  auto const* attacker_unit = attacker.get_component<Engine::Core::UnitComponent>();
  if (attacker_unit == nullptr || (definition.weapon_family != WeaponFamily::Sword &&
                                   definition.weapon_family != WeaponFamily::Spear)) {
    return contact;
  }
  auto const presented_attacker = presented_attacker_frame(attacker, target_hint_id);
  contact.attacker_soldier_slot = presented_attacker.soldier_slot;
  auto const intent = live_intent_of(attacker, definition);

  auto consider = [&](Engine::Core::Entity* candidate,
                      float& best_score,
                      WeaponTraceContact& best_contact) {
    if (candidate == nullptr || candidate == &attacker ||
        !Game::Systems::Combat::is_valid_enemy_unit(attacker_unit, candidate, false)) {
      return;
    }
    if (std::find(ignored_target_ids.begin(),
                  ignored_target_ids.end(),
                  candidate->get_id()) != ignored_target_ids.end()) {
      return;
    }

    float const hint_bias =
        candidate->get_id() == target_hint_id ? hint_score_bias(attacker) : 0.0F;

    for (auto const& soldier :
         Game::Systems::RpgCombat::live_soldier_targets(*candidate)) {
      bool const ignored_slot =
          std::any_of(ignored_target_slots.begin(),
                      ignored_target_slots.end(),
                      [&](auto const& ignored) {
                        return ignored.entity_id == candidate->get_id() &&
                               ignored.soldier_slot == soldier.soldier_slot;
                      });
      if (ignored_slot) {
        continue;
      }
      auto sample = make_local_sample(presented_attacker.frame, soldier);
      if (sample.entity == nullptr || !std::isfinite(sample.forward) ||
          !std::isfinite(sample.right) || !std::isfinite(sample.distance) ||
          sample.forward <= 0.0F) {
        continue;
      }
      float const score = weapon_contact_score(sample, definition, intent) - hint_bias;
      if (!std::isfinite(score) || score >= best_score) {
        continue;
      }

      best_score = score;
      best_contact.target_id = candidate->get_id();
      best_contact.target_soldier_slot = sample.soldier_slot;
      best_contact.distance = sample.distance;
      best_contact.local_forward = sample.forward;
      best_contact.local_right = sample.right;
      best_contact.contact_point = Game::Systems::RpgCombat::hurt_body_contact_point(
          soldier, presented_attacker.frame.origin);
    }
  };

  float best_score = std::numeric_limits<float>::infinity();
  for (auto [candidate, candidate_unit] :
       world.entity_view<Engine::Core::UnitComponent>()) {
    (void)candidate_unit;
    consider(&candidate, best_score, contact);
  }

  return contact;
}

} // namespace Game::Systems::CombatActions
