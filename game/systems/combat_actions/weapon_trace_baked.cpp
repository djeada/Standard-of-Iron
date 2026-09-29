#include "weapon_trace_baked.h"

#include <QMatrix4x4>
#include <QVector4D>

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include "../../../animation/clip_manifest.h"
#include "animation/bpat/bpat_format.h"
#include "animation/bpat/bpat_playback.h"
#include "animation/bpat/bpat_reader.h"
#include "animation/bpat/bpat_registry.h"

namespace Game::Systems::CombatActions {

namespace {

struct BakedTraceClip {
  bool valid{false};
  std::uint32_t species_id{0U};
  std::uint16_t clip_id{Animation::k_unmapped_clip};
};

struct SampledSocketFrame {
  bool valid{false};
  QMatrix4x4 transform;
};

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

} // namespace

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

} // namespace Game::Systems::CombatActions
