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
#include "../../../render/creature/bpat/bpat_format.h"
#include "../../../render/creature/bpat/bpat_reader.h"
#include "../../../render/creature/bpat/bpat_registry.h"
#include "../../../render/creature/pipeline/bpat_playback.h"
#include "../../../render/entity/mounted_knight_pose.h"
#include "../../../render/equipment/weapons/spear_renderer.h"
#include "../../../render/horse/horse_motion.h"
#include "../../../render/humanoid/humanoid_specs.h"
#include "../../core/component.h"
#include "../../core/world.h"
#include "../combat_system/combat_utils.h"

namespace Game::Systems::CombatActions {

namespace {

struct LocalTargetSample {
  Engine::Core::Entity* entity{nullptr};
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
  frame.right = QVector3D(-frame.forward.z(), 0.0F, frame.forward.x());
  frame.origin =
      QVector3D(transform->position.x, transform->position.y, transform->position.z);
  frame.valid = true;
  return frame;
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
    case CombatActionId::RpgSpearSweep:
    case CombatActionId::RpgBowShot:
    case CombatActionId::MountedSpearThrust:
    case CombatActionId::MountedChargeImpact:
      break;
    }
  } else if (definition.weapon_family == WeaponFamily::Spear) {
    switch (definition.id) {
    case CombatActionId::RpgSpearThrust:
      return {
          .valid = true,
          .species_id = Render::Creature::Bpat::k_species_humanoid_spear,
          .clip_id = Animation::k_humanoid_attack_spear_a_clip,
      };
    case CombatActionId::RpgSpearSweep:
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
    case CombatActionId::RpgBowShot:
    case CombatActionId::MountedSwordSlash:
    case CombatActionId::MountedChargeImpact:
      break;
    }
  }
  return {};
}

[[nodiscard]] auto default_mounted_trace_frame() -> Render::GL::MountedAttachmentFrame {
  auto horse_profile = Render::GL::make_horse_profile(0U, {}, {});
  auto mount = Render::GL::compute_mount_frame(horse_profile);
  Render::GL::tune_mounted_knight_frame(horse_profile.dims, mount);
  return mount;
}

[[nodiscard]] auto
mounted_seat_relative(const Render::GL::MountedAttachmentFrame& mount,
                      Animation::MountedSeatOffset offset) -> QVector3D {
  return mount.seat_position + mount.seat_forward * offset.forward +
         mount.seat_right * offset.right + mount.seat_up * offset.up;
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

[[nodiscard]] auto
local_sword_blade_direction(const CombatActionDefinition& definition,
                            const QVector3D& previous_grip,
                            const QVector3D& current_grip) -> QVector3D {
  if (definition.attack_direction == Engine::Core::AttackDirection::Thrust) {
    return QVector3D(0.0F, -0.08F, 1.0F).normalized();
  }

  QVector3D const sweep = current_grip - previous_grip;
  if (definition.attack_direction == Engine::Core::AttackDirection::LeftSlash ||
      definition.attack_direction == Engine::Core::AttackDirection::RightSlash) {
    QVector3D const direction = QVector3D(sweep.x() * 0.35F, -0.08F, 1.0F);
    return normalized_or_forward(direction);
  }

  return QVector3D(0.0F, -0.20F, 1.0F).normalized();
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

  auto const mount = default_mounted_trace_frame();
  auto const previous_pose = Animation::resolve_mounted_spear_thrust_pose({previous});
  auto const current_pose = Animation::resolve_mounted_spear_thrust_pose({current});

  QVector3D const previous_grip =
      mounted_seat_relative(mount, previous_pose.right_hand);
  QVector3D const current_grip = mounted_seat_relative(mount, current_pose.right_hand);

  Render::GL::SpearRenderConfig const spear_config{};
  QVector3D const spear_dir = normalized_or(mount.seat_forward + mount.seat_up * 0.05F,
                                            QVector3D(0.0F, 0.0F, 1.0F));
  float constexpr k_shaft_base_offset = -0.28F;
  float const spear_tip_offset =
      spear_config.spear_length + spear_config.spearhead_length;

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

[[nodiscard]] auto
make_local_sample(Engine::Core::Entity& attacker,
                  Engine::Core::Entity& target) -> LocalTargetSample {
  LocalTargetSample sample;
  sample.entity = &target;

  auto* attacker_transform = attacker.get_component<Engine::Core::TransformComponent>();
  auto* target_transform = target.get_component<Engine::Core::TransformComponent>();
  if (attacker_transform == nullptr || target_transform == nullptr) {
    sample.entity = nullptr;
    return sample;
  }

  float const yaw =
      attacker_transform->rotation.y * (std::numbers::pi_v<float> / 180.0F);
  QVector3D const forward(std::sin(yaw), 0.0F, std::cos(yaw));
  QVector3D const right(-forward.z(), 0.0F, forward.x());
  QVector3D const origin(attacker_transform->position.x,
                         attacker_transform->position.y,
                         attacker_transform->position.z);
  sample.world_position = QVector3D(target_transform->position.x,
                                    target_transform->position.y,
                                    target_transform->position.z);
  QVector3D const to_target = sample.world_position - origin;
  sample.forward = QVector3D::dotProduct(to_target, forward);
  sample.right = QVector3D::dotProduct(to_target, right);
  sample.distance =
      std::sqrt(sample.forward * sample.forward + sample.right * sample.right);
  sample.radius = Game::Systems::Combat::combat_radius(&target);
  return sample;
}

[[nodiscard]] auto
weapon_contact_score(const LocalTargetSample& sample,
                     const CombatActionDefinition& definition) -> float {
  if (sample.entity == nullptr || !std::isfinite(sample.forward) ||
      !std::isfinite(sample.right) || !std::isfinite(sample.distance) ||
      sample.forward <= 0.0F) {
    return std::numeric_limits<float>::infinity();
  }

  float const reach = definition.hit_shape.reach + sample.radius;
  if (sample.distance > reach) {
    return std::numeric_limits<float>::infinity();
  }

  float lateral_limit = definition.hit_shape.reach * 0.70F + sample.radius;
  float min_forward = 0.0F;
  switch (definition.attack_direction) {
  case Engine::Core::AttackDirection::Thrust:
    lateral_limit = definition.hit_shape.radius + sample.radius;
    min_forward = 0.20F;
    break;
  case Engine::Core::AttackDirection::Overhead:
  case Engine::Core::AttackDirection::HeavyOverhead:
    lateral_limit = std::max(0.55F, definition.hit_shape.radius) + sample.radius;
    min_forward = 0.10F;
    break;
  case Engine::Core::AttackDirection::LeftSlash:
  case Engine::Core::AttackDirection::RightSlash:
  default:
    min_forward = 0.05F;
    break;
  }
  if (definition.weapon_family == WeaponFamily::Spear &&
      definition.attack_direction == Engine::Core::AttackDirection::Thrust) {
    lateral_limit = definition.hit_shape.radius + sample.radius;
    min_forward = std::max(min_forward, 0.25F);
  }

  if (sample.forward < min_forward || std::abs(sample.right) > lateral_limit) {
    return std::numeric_limits<float>::infinity();
  }

  return sample.distance + std::abs(sample.right) * 0.15F;
}

} // namespace

auto sample_authored_weapon_trace_segment(Engine::Core::Entity& attacker,
                                          const CombatActionDefinition& definition,
                                          WeaponTraceTimeSpan time_span)
    -> WeaponTraceSegment {
  WeaponTraceSegment segment;
  if (definition.weapon_family != WeaponFamily::Sword &&
      definition.weapon_family != WeaponFamily::Spear) {
    return segment;
  }

  auto const frame = attacker_frame(attacker);
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
    QVector3D const previous_dir =
        local_sword_blade_direction(definition, previous_grip, current_grip);
    QVector3D const current_dir =
        local_sword_blade_direction(definition, previous_grip, current_grip);
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

auto find_weapon_trace_contact(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    const CombatActionDefinition& definition,
    WeaponTraceTimeSpan time_span,
    Engine::Core::EntityID target_hint_id,
    std::span<const Engine::Core::EntityID> ignored_target_ids) -> WeaponTraceContact {
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
                                    ignored_target_ids);
      if (contact.target_id != 0) {
        return contact;
      }
    }
    return {};
  }

  auto const segment =
      sample_authored_weapon_trace_segment(attacker, definition, time_span);
  if (!segment.valid) {
    return find_weapon_trace_contact(
        world, attacker, definition, target_hint_id, ignored_target_ids);
  }

  WeaponTraceContact contact;
  contact.attacker_id = attacker.get_id();
  auto const* attacker_unit = attacker.get_component<Engine::Core::UnitComponent>();
  if (attacker_unit == nullptr) {
    return contact;
  }

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

    auto sample = make_local_sample(attacker, *candidate);
    if (sample.entity == nullptr || !std::isfinite(sample.forward) ||
        !std::isfinite(sample.right) || !std::isfinite(sample.distance) ||
        sample.forward <= 0.0F) {
      return;
    }

    auto const distance = best_segment_distance_xz(segment, sample.world_position);
    float const hit_radius = segment.radius + sample.radius;
    if (!std::isfinite(distance.distance) || distance.distance > hit_radius) {
      return;
    }

    float const score = distance.distance + sample.distance * 0.03F;
    if (score >= best_score) {
      return;
    }

    best_score = score;
    best_contact.target_id = candidate->get_id();
    best_contact.distance = sample.distance;
    best_contact.local_forward = sample.forward;
    best_contact.local_right = sample.right;
    best_contact.contact_point = distance.point;
  };

  float best_score = std::numeric_limits<float>::infinity();
  if (target_hint_id != 0) {
    consider(world.get_entity(target_hint_id), best_score, contact);
    if (contact.target_id != 0) {
      return contact;
    }
  }

  for (auto* candidate : world.get_entities_with<Engine::Core::UnitComponent>()) {
    consider(candidate, best_score, contact);
  }

  return contact;
}

auto find_weapon_trace_contact(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    const CombatActionDefinition& definition,
    Engine::Core::EntityID target_hint_id,
    std::span<const Engine::Core::EntityID> ignored_target_ids) -> WeaponTraceContact {
  WeaponTraceContact contact;
  contact.attacker_id = attacker.get_id();

  auto const* attacker_unit = attacker.get_component<Engine::Core::UnitComponent>();
  if (attacker_unit == nullptr || (definition.weapon_family != WeaponFamily::Sword &&
                                   definition.weapon_family != WeaponFamily::Spear)) {
    return contact;
  }

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

    auto sample = make_local_sample(attacker, *candidate);
    if (sample.entity == nullptr || !std::isfinite(sample.forward) ||
        !std::isfinite(sample.right) || !std::isfinite(sample.distance) ||
        sample.forward <= 0.0F) {
      return;
    }
    if (definition.weapon_family == WeaponFamily::Spear &&
        definition.attack_direction == Engine::Core::AttackDirection::Thrust &&
        std::abs(sample.right) > definition.hit_shape.radius + sample.radius) {
      return;
    }

    float const score = weapon_contact_score(sample, definition);
    if (!std::isfinite(score) || score >= best_score) {
      return;
    }

    best_score = score;
    best_contact.target_id = candidate->get_id();
    best_contact.distance = sample.distance;
    best_contact.local_forward = sample.forward;
    best_contact.local_right = sample.right;
    best_contact.contact_point = sample.world_position;
  };

  float best_score = std::numeric_limits<float>::infinity();
  if (target_hint_id != 0) {
    consider(world.get_entity(target_hint_id), best_score, contact);
    if (contact.target_id != 0) {
      return contact;
    }
  }

  for (auto* candidate : world.get_entities_with<Engine::Core::UnitComponent>()) {
    consider(candidate, best_score, contact);
  }

  return contact;
}

auto find_sword_trace_contact(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    const CombatActionDefinition& definition,
    Engine::Core::EntityID target_hint_id,
    std::span<const Engine::Core::EntityID> ignored_target_ids) -> WeaponTraceContact {
  return find_weapon_trace_contact(
      world, attacker, definition, target_hint_id, ignored_target_ids);
}

} // namespace Game::Systems::CombatActions
