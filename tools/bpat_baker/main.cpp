

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

#include "animation/clip_manifest.h"
#include "render/creature/bpat/bpat_format.h"
#include "render/creature/bpat/bpat_writer.h"
#include "render/creature/humanoid_clip_ids.h"
#include "render/creature/movement_state.h"
#include "render/creature/part_graph.h"
#include "render/creature/render_request.h"
#include "render/creature/snapshot_mesh_asset.h"
#include "render/creature/species_manifest.h"
#include "render/elephant/dimensions.h"
#include "render/elephant/elephant_gait.h"
#include "render/elephant/elephant_manifest.h"
#include "render/elephant/elephant_spec.h"
#include "render/entity/mounted_knight_pose.h"
#include "render/equipment/weapons/spear_renderer.h"
#include "render/equipment/weapons/sword_renderer.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/horse/dimensions.h"
#include "render/horse/horse_gait.h"
#include "render/horse/horse_manifest.h"
#include "render/horse/horse_motion.h"
#include "render/horse/horse_spec.h"
#include "render/humanoid/humanoid_full_builder.h"
#include "render/humanoid/humanoid_renderer_base.h"
#include "render/humanoid/mounted_pose_controller.h"
#include "render/humanoid/pose_controller.h"
#include "render/humanoid/skeleton.h"
#include "render/humanoid/spear_pose_utils.h"
#include "render/rigged_mesh_bake.h"
#include "render/snapshot_mesh_bake.h"

namespace {

namespace bpat = Render::Creature::Bpat;
namespace snapshot = Render::Creature::Snapshot;

void apply_markers(const Animation::ClipMarkers& markers, bpat::ClipDescriptor& desc) {
  desc.marker_anticipation_start = markers.anticipation_start;
  desc.marker_weapon_release = markers.weapon_release;
  desc.marker_contact = markers.contact;
  desc.marker_recover_unlocked = markers.recover_unlocked;
  desc.marker_exit_safe = markers.exit_safe;
}

void apply_generic_markers(bpat::ClipDescriptor& desc) {
  apply_markers(Animation::authored_generic_clip_markers(desc.name), desc);
}

enum class BakerAttackType : std::uint8_t {
  None,
  Sword,
  Spear,
  Bow,
  BowMelee,
  SpearFromHold,
  BowFromHold,
};
enum class BakerHoldType : std::uint8_t {
  None,
  Spear,
  Bow
};
enum class BakerDeathType : std::uint8_t {
  None,
  Infantry,
  Mounted
};
enum class BakerRidingType : std::uint8_t {
  None,
  Idle,
  Charge,
  Reining,
  BowShot,
  SwordStrike,
  SpearThrust
};
enum class BakerAmbientIdleType : std::uint8_t {
  None,
  SitDown,
  Jump,
  RaiseWeapon,
  ShiftWeight
};
enum class HumanoidBakeProfile : std::uint8_t {
  Default,
  SwordReady,
  SpearReady,
  Skeleton
};

auto animation_profile_for_bake(HumanoidBakeProfile profile) noexcept
    -> Animation::HumanoidClipProfile {
  switch (profile) {
  case HumanoidBakeProfile::Default:
    return Animation::HumanoidClipProfile::Default;
  case HumanoidBakeProfile::SwordReady:
    return Animation::HumanoidClipProfile::SwordReady;
  case HumanoidBakeProfile::SpearReady:
    return Animation::HumanoidClipProfile::SpearReady;
  case HumanoidBakeProfile::Skeleton:
    return Animation::HumanoidClipProfile::Skeleton;
  }
  return Animation::HumanoidClipProfile::Default;
}

void apply_humanoid_markers(std::uint16_t clip_id,
                            HumanoidBakeProfile profile,
                            bpat::ClipDescriptor& desc) {
  apply_markers(Animation::authored_humanoid_clip_markers(
                    clip_id, animation_profile_for_bake(profile)),
                desc);
}

struct HumanoidClipSpec {
  const char* name{};
  Render::GL::HumanoidMotionState state;
  BakerAttackType attack_type{BakerAttackType::None};
  std::uint8_t attack_variant{0};
  BakerDeathType death_type{BakerDeathType::None};
  BakerRidingType riding_type{BakerRidingType::None};
  BakerHoldType hold_type{BakerHoldType::None};
  BakerAmbientIdleType ambient_idle_type{BakerAmbientIdleType::None};
  std::uint32_t frames{};
  float fps{};
  float cycle_time{};
  bool loops{};
};

[[nodiscard]] auto is_rpg_sword_clip(const HumanoidClipSpec& clip) noexcept -> bool {
  return std::string_view{clip.name}.starts_with("rpg_sword_");
}

[[nodiscard]] auto is_ranged_attack_type(BakerAttackType type) noexcept -> bool {
  return type == BakerAttackType::Bow || type == BakerAttackType::BowFromHold;
}

constexpr auto k_humanoid_baker_clip_count = Animation::k_humanoid_clip_count;
constexpr std::array<HumanoidClipSpec, k_humanoid_baker_clip_count> k_humanoid_clips{{
    {"idle",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     BakerDeathType::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     24U,
     24.0F,
     1.6F,
     true},
    {"idle_squat",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     BakerDeathType::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::SitDown,
     72U,
     24.0F,
     3.0F,
     false},
    {"idle_jump",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     BakerDeathType::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::Jump,
     72U,
     24.0F,
     3.0F,
     false},
    {"idle_weapon",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     BakerDeathType::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::RaiseWeapon,
     72U,
     24.0F,
     3.0F,
     false},
    {"idle_weave",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     BakerDeathType::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::ShiftWeight,
     72U,
     24.0F,
     3.0F,
     false},
    {"walk",
     Render::GL::HumanoidMotionState::Walk,
     BakerAttackType::None,
     0,
     BakerDeathType::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     32U,
     24.0F,
     0.92F,
     true},
    {"run",
     Render::GL::HumanoidMotionState::Run,
     BakerAttackType::None,
     0,
     BakerDeathType::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     32U,
     24.0F,
     0.56F,
     true},
    {"hold",
     Render::GL::HumanoidMotionState::Hold,
     BakerAttackType::None,
     0,
     BakerDeathType::None,
     BakerRidingType::None,
     BakerHoldType::Spear,
     BakerAmbientIdleType::None,
     16U,
     24.0F,
     1.8F,
     true},
    {"hold_bow",
     Render::GL::HumanoidMotionState::Hold,
     BakerAttackType::None,
     0,
     BakerDeathType::None,
     BakerRidingType::None,
     BakerHoldType::Bow,
     BakerAmbientIdleType::None,
     16U,
     24.0F,
     1.8F,
     true},
    {"attack_sword_a",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Sword,
     0,
     BakerDeathType::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     32U,
     24.0F,
     1.0F,
     false},
    {"attack_sword_b",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Sword,
     1,
     BakerDeathType::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     32U,
     24.0F,
     1.0F,
     false},
    {"attack_sword_c",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Sword,
     2,
     BakerDeathType::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     32U,
     24.0F,
     1.0F,
     false},
    {"attack_spear_a",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Spear,
     0,
     BakerDeathType::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     32U,
     24.0F,
     1.0F,
     false},
    {"attack_spear_b",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Spear,
     1,
     BakerDeathType::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     32U,
     24.0F,
     1.0F,
     false},
    {"attack_spear_c",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Spear,
     2,
     BakerDeathType::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     32U,
     24.0F,
     1.0F,
     false},
    {"attack_bow",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Bow,
     0,
     BakerDeathType::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     32U,
     24.0F,
     1.0F,
     false},
    {"riding_idle",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     BakerDeathType::None,
     BakerRidingType::Idle,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     24U,
     24.0F,
     1.6F,
     true},
    {"riding_charge",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::None,
     0,
     BakerDeathType::None,
     BakerRidingType::Charge,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     24U,
     24.0F,
     1.0F,
     false},
    {"riding_reining",
     Render::GL::HumanoidMotionState::Hold,
     BakerAttackType::None,
     0,
     BakerDeathType::None,
     BakerRidingType::Reining,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     24U,
     24.0F,
     1.0F,
     false},
    {"riding_bow_shot",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::None,
     0,
     BakerDeathType::None,
     BakerRidingType::BowShot,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     24U,
     24.0F,
     1.0F,
     false},
    {"riding_sword_strike",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::None,
     0,
     BakerDeathType::None,
     BakerRidingType::SwordStrike,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     32U,
     24.0F,
     1.2F,
     false},
    {"riding_spear_thrust",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::None,
     0,
     BakerDeathType::None,
     BakerRidingType::SpearThrust,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     32U,
     24.0F,
     1.2F,
     false},
    {"die_infantry",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     BakerDeathType::Infantry,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     20U,
     24.0F,
     1.0F,
     false},
    {"dead_infantry",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     BakerDeathType::Infantry,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     1U,
     1.0F,
     1.0F,
     true},
    {"die_mounted",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     BakerDeathType::Mounted,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     20U,
     24.0F,
     1.0F,
     false},
    {"dead_mounted",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     BakerDeathType::Mounted,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     1U,
     1.0F,
     1.0F,
     true},
    {"rpg_sword_slash_left",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Sword,
     0,
     BakerDeathType::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     36U,
     24.0F,
     1.0F,
     false},
    {"rpg_sword_slash_right",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Sword,
     1,
     BakerDeathType::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     36U,
     24.0F,
     1.0F,
     false},
    {"rpg_sword_overhead",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Sword,
     3,
     BakerDeathType::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     40U,
     24.0F,
     1.1F,
     false},
    {"rpg_sword_thrust",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Sword,
     4,
     BakerDeathType::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     34U,
     24.0F,
     0.9F,
     false},
    {"rpg_sword_finisher",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Sword,
     5,
     BakerDeathType::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     44U,
     24.0F,
     1.25F,
     false},
    {"archer_melee",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::BowMelee,
     0,
     BakerDeathType::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     32U,
     24.0F,
     1.0F,
     false},
    {"hold_spear_attack",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::SpearFromHold,
     0,
     BakerDeathType::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     32U,
     24.0F,
     1.0F,
     false},
    {"hold_bow_attack",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::BowFromHold,
     0,
     BakerDeathType::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     32U,
     24.0F,
     1.0F,
     false},
}};

struct HumanoidSocketSpec {
  const char* name{};
  Render::Humanoid::HumanoidSocket socket;
  enum class Kind : std::uint8_t {
    TopologySocket,
    GripFrame,
    SwordBladeBase,
    SwordBladeTip,
    SpearShaftBase,
    SpearShaftTip,
    SpearHeadTip,
  };
  Kind kind{Kind::TopologySocket};
};

constexpr std::array<HumanoidSocketSpec, 17> k_humanoid_sockets{{
    {"head", Render::Humanoid::HumanoidSocket::Head},
    {"hand_r", Render::Humanoid::HumanoidSocket::HandR},
    {"hand_l", Render::Humanoid::HumanoidSocket::HandL},
    {"grip_r",
     Render::Humanoid::HumanoidSocket::GripR,
     HumanoidSocketSpec::Kind::GripFrame},
    {"grip_l",
     Render::Humanoid::HumanoidSocket::GripL,
     HumanoidSocketSpec::Kind::GripFrame},
    {"sword_blade_base_r",
     Render::Humanoid::HumanoidSocket::GripR,
     HumanoidSocketSpec::Kind::SwordBladeBase},
    {"sword_blade_tip_r",
     Render::Humanoid::HumanoidSocket::GripR,
     HumanoidSocketSpec::Kind::SwordBladeTip},
    {"spear_shaft_base_r",
     Render::Humanoid::HumanoidSocket::GripR,
     HumanoidSocketSpec::Kind::SpearShaftBase},
    {"spear_shaft_tip_r",
     Render::Humanoid::HumanoidSocket::GripR,
     HumanoidSocketSpec::Kind::SpearShaftTip},
    {"spear_head_tip_r",
     Render::Humanoid::HumanoidSocket::GripR,
     HumanoidSocketSpec::Kind::SpearHeadTip},
    {"back", Render::Humanoid::HumanoidSocket::Back},
    {"hip_l", Render::Humanoid::HumanoidSocket::HipL},
    {"hip_r", Render::Humanoid::HumanoidSocket::HipR},
    {"chest_front", Render::Humanoid::HumanoidSocket::ChestFront},
    {"chest_back", Render::Humanoid::HumanoidSocket::ChestBack},
    {"foot_l", Render::Humanoid::HumanoidSocket::FootL},
    {"foot_r", Render::Humanoid::HumanoidSocket::FootR},
}};

auto blend_vec(const QVector3D& from, const QVector3D& to, float t) -> QVector3D {
  return from * (1.0F - t) + to * t;
}

auto blend_attachment_frame(const Render::GL::AttachmentFrame& from,
                            const Render::GL::AttachmentFrame& to,
                            float t) -> Render::GL::AttachmentFrame {
  Render::GL::AttachmentFrame blended = from;
  blended.origin = blend_vec(from.origin, to.origin, t);
  blended.right = blend_vec(from.right, to.right, t);
  blended.up = blend_vec(from.up, to.up, t);
  blended.forward = blend_vec(from.forward, to.forward, t);
  blended.radius = from.radius * (1.0F - t) + to.radius * t;
  blended.depth = from.depth * (1.0F - t) + to.depth * t;
  return blended;
}

auto blend_body_frames(const Render::GL::BodyFrames& from,
                       const Render::GL::BodyFrames& to,
                       float t) -> Render::GL::BodyFrames {
  Render::GL::BodyFrames blended = from;
  blended.head = blend_attachment_frame(from.head, to.head, t);
  blended.torso = blend_attachment_frame(from.torso, to.torso, t);
  blended.back = blend_attachment_frame(from.back, to.back, t);
  blended.waist = blend_attachment_frame(from.waist, to.waist, t);
  blended.shoulder_l = blend_attachment_frame(from.shoulder_l, to.shoulder_l, t);
  blended.shoulder_r = blend_attachment_frame(from.shoulder_r, to.shoulder_r, t);
  blended.hand_l = blend_attachment_frame(from.hand_l, to.hand_l, t);
  blended.hand_r = blend_attachment_frame(from.hand_r, to.hand_r, t);
  blended.grip_l = blend_attachment_frame(from.grip_l, to.grip_l, t);
  blended.grip_r = blend_attachment_frame(from.grip_r, to.grip_r, t);
  blended.foot_l = blend_attachment_frame(from.foot_l, to.foot_l, t);
  blended.foot_r = blend_attachment_frame(from.foot_r, to.foot_r, t);
  blended.shin_l = blend_attachment_frame(from.shin_l, to.shin_l, t);
  blended.shin_r = blend_attachment_frame(from.shin_r, to.shin_r, t);
  return blended;
}

auto normalized_or(const QVector3D& value, const QVector3D& fallback) -> QVector3D {
  QVector3D out = value;
  if (out.lengthSquared() <= 1.0e-6F) {
    out = fallback;
  }
  if (out.lengthSquared() <= 1.0e-6F) {
    return {0.0F, 1.0F, 0.0F};
  }
  out.normalize();
  return out;
}

auto attachment_frame_matrix(const Render::GL::AttachmentFrame& frame) -> QMatrix4x4 {
  QMatrix4x4 out;
  out.setColumn(
      0, QVector4D(normalized_or(frame.right, QVector3D(1.0F, 0.0F, 0.0F)), 0.0F));
  out.setColumn(1,
                QVector4D(normalized_or(frame.up, QVector3D(0.0F, 1.0F, 0.0F)), 0.0F));
  out.setColumn(
      2, QVector4D(normalized_or(frame.forward, QVector3D(0.0F, 0.0F, 1.0F)), 0.0F));
  out.setColumn(3, QVector4D(frame.origin, 1.0F));
  return out;
}

auto sword_local_pose(const QVector3D& blade_axis_local) -> QMatrix4x4 {
  QVector3D const blade_dir =
      normalized_or(blade_axis_local, QVector3D(0.0F, 1.0F, 0.0F));

  QVector3D guard_right(0.0F, 0.0F, 1.0F);
  guard_right -= blade_dir * QVector3D::dotProduct(guard_right, blade_dir);
  guard_right = normalized_or(guard_right, QVector3D(1.0F, 0.0F, 0.0F));

  QVector3D const z_axis = normalized_or(
      QVector3D::crossProduct(guard_right, blade_dir), QVector3D(0.0F, 0.0F, 1.0F));

  QMatrix4x4 pose;
  pose.setColumn(0, QVector4D(guard_right, 0.0F));
  pose.setColumn(1, QVector4D(blade_dir, 0.0F));
  pose.setColumn(2, QVector4D(z_axis, 0.0F));
  pose.setColumn(3, QVector4D(blade_dir * 0.05F, 1.0F));
  return pose;
}

auto baked_sword_blade_socket_matrix(const Render::GL::AttachmentFrame& grip,
                                     bool tip) -> QMatrix4x4 {
  Render::GL::SwordRenderConfig const config{};
  QVector3D const blade_axis_local(0.02F, 0.97F, 0.0F);
  QMatrix4x4 socket =
      attachment_frame_matrix(grip) * sword_local_pose(blade_axis_local);
  if (tip) {
    QVector3D const tip_origin =
        (socket * QVector4D(0.0F, config.sword_length, 0.0F, 1.0F)).toVector3D();
    socket.setColumn(3, QVector4D(tip_origin, 1.0F));
  }
  return socket;
}

auto spear_endpoint_distance(HumanoidSocketSpec::Kind kind,
                             const Render::GL::SpearRenderConfig& config) -> float {
  switch (kind) {
  case HumanoidSocketSpec::Kind::SpearShaftBase:
    return -0.28F;
  case HumanoidSocketSpec::Kind::SpearShaftTip:
    return config.spear_length;
  case HumanoidSocketSpec::Kind::SpearHeadTip:
    return config.spear_length + config.spearhead_length;
  case HumanoidSocketSpec::Kind::TopologySocket:
  case HumanoidSocketSpec::Kind::GripFrame:
  case HumanoidSocketSpec::Kind::SwordBladeBase:
  case HumanoidSocketSpec::Kind::SwordBladeTip:
    break;
  }
  return 0.0F;
}

auto baked_spear_socket_matrix(const Render::GL::AttachmentFrame& grip,
                               const Render::GL::AnimationInputs& inputs,
                               float attack_phase,
                               HumanoidSocketSpec::Kind kind) -> QMatrix4x4 {
  Render::GL::SpearRenderConfig const config{};
  QVector3D const spear_dir = Render::GL::resolve_spear_direction(inputs, attack_phase);

  QVector3D right =
      grip.right - spear_dir * QVector3D::dotProduct(grip.right, spear_dir);
  right = normalized_or(right, QVector3D(1.0F, 0.0F, 0.0F));
  QVector3D const forward = normalized_or(QVector3D::crossProduct(right, spear_dir),
                                          QVector3D(0.0F, 0.0F, 1.0F));

  QMatrix4x4 socket;
  socket.setColumn(0, QVector4D(right, 0.0F));
  socket.setColumn(1, QVector4D(spear_dir, 0.0F));
  socket.setColumn(2, QVector4D(forward, 0.0F));
  socket.setColumn(
      3,
      QVector4D(grip.origin + spear_dir * spear_endpoint_distance(kind, config), 1.0F));
  return socket;
}

auto transition_phase(std::uint32_t frame_index, std::uint32_t frame_count) -> float {
  if (frame_count <= 1U) {
    return 1.0F;
  }
  return std::clamp(static_cast<float>(frame_index) /
                        static_cast<float>(frame_count - 1U),
                    0.0F,
                    1.0F);
}

auto hold_gait_descriptor() -> Render::GL::HumanoidGaitDescriptor {
  Render::GL::HumanoidGaitDescriptor gait{};
  gait.state = Render::GL::HumanoidMotionState::Hold;
  gait.cycle_time = 1.8F;
  gait.cycle_phase = 0.0F;
  gait.speed = 0.0F;
  gait.normalized_speed = 0.0F;
  return gait;
}

struct AuthoredSwordPoseKey {
  float phase{0.0F};
  QVector3D right_hand;
  QVector3D left_hand;
  QVector3D pelvis_delta;
  QVector3D shoulder_r_delta;
  QVector3D shoulder_l_delta;
  QVector3D neck_delta;
  QVector3D head_delta;
  QVector3D foot_r_delta;
  QVector3D knee_r_delta;
  QVector3D foot_l_delta;
  QVector3D knee_l_delta;
};

using AuthoredSwordPoseKeys = std::array<AuthoredSwordPoseKey, 6>;

auto rpg_sword_pose_keys(std::uint8_t variant) -> const AuthoredSwordPoseKeys& {
  static const AuthoredSwordPoseKeys slash_left{{
      {0.00F, {0.24F, 1.22F, 0.18F}, {-0.22F, 1.16F, 0.18F}},
      {0.16F, {0.46F, 1.44F, -0.26F}, {-0.26F, 1.10F, 0.20F}},
      {0.38F,
       {0.42F, 1.48F, 0.02F},
       {-0.22F, 1.08F, 0.26F},
       {0.02F, -0.03F, 0.04F},
       {0.12F, 0.02F, -0.08F},
       {-0.06F, -0.01F, 0.07F}},
      {0.54F,
       {-0.22F, 0.86F, 1.18F},
       {0.04F, 0.98F, 0.66F},
       {0.02F, -0.06F, 0.16F},
       {0.20F, -0.12F, 0.16F},
       {-0.12F, 0.04F, 0.02F},
       {0.00F, -0.04F, 0.10F},
       {0.00F, -0.03F, 0.06F},
       {0.00F, 0.00F, 0.18F},
       {0.00F, 0.00F, 0.10F}},
      {0.76F, {-0.34F, 0.72F, 0.76F}, {-0.10F, 0.94F, 0.54F}, {0.00F, -0.04F, 0.10F}},
      {1.00F, {0.22F, 1.16F, 0.24F}, {-0.22F, 1.12F, 0.18F}},
  }};
  static const AuthoredSwordPoseKeys slash_right{{
      {0.00F, {0.22F, 1.18F, 0.20F}, {-0.22F, 1.14F, 0.18F}},
      {0.16F, {-0.28F, 1.42F, -0.22F}, {-0.26F, 1.10F, 0.20F}},
      {0.38F,
       {-0.22F, 1.46F, 0.02F},
       {-0.24F, 1.08F, 0.24F},
       {-0.02F, -0.03F, 0.03F},
       {-0.08F, 0.02F, 0.06F},
       {0.10F, -0.02F, -0.08F}},
      {0.54F,
       {0.58F, 0.88F, 1.14F},
       {0.10F, 0.98F, 0.66F},
       {-0.02F, -0.06F, 0.15F},
       {-0.18F, -0.10F, 0.14F},
       {0.12F, 0.04F, 0.00F},
       {0.00F, -0.04F, 0.08F},
       {0.00F, -0.03F, 0.05F},
       {0.00F, 0.00F, -0.02F},
       {0.00F, 0.00F, -0.01F},
       {0.00F, 0.00F, 0.12F},
       {0.00F, 0.00F, 0.08F}},
      {0.76F, {0.64F, 0.78F, 0.74F}, {0.04F, 0.94F, 0.54F}, {0.00F, -0.04F, 0.08F}},
      {1.00F, {0.22F, 1.16F, 0.24F}, {-0.22F, 1.12F, 0.18F}},
  }};
  static const AuthoredSwordPoseKeys overhead{{
      {0.00F, {0.22F, 1.18F, 0.20F}, {-0.22F, 1.12F, 0.18F}},
      {0.18F, {0.18F, 1.58F, -0.32F}, {-0.18F, 1.18F, 0.20F}},
      {0.42F,
       {0.10F, 1.72F, -0.10F},
       {-0.10F, 1.22F, 0.30F},
       {0.00F, -0.03F, 0.02F},
       {0.06F, 0.10F, -0.04F},
       {-0.04F, 0.08F, -0.02F}},
      {0.60F,
       {0.02F, 0.78F, 1.22F},
       {-0.02F, 0.96F, 0.74F},
       {0.00F, -0.10F, 0.20F},
       {0.04F, -0.18F, 0.20F},
       {-0.04F, -0.06F, 0.12F},
       {0.00F, -0.08F, 0.14F},
       {0.00F, -0.06F, 0.10F},
       {0.00F, 0.00F, 0.20F},
       {0.00F, 0.00F, 0.12F}},
      {0.82F, {-0.02F, 0.64F, 0.70F}, {-0.06F, 0.90F, 0.54F}},
      {1.00F, {0.22F, 1.14F, 0.24F}, {-0.22F, 1.10F, 0.18F}},
  }};
  static const AuthoredSwordPoseKeys thrust{{
      {0.00F, {0.20F, 1.16F, 0.18F}, {-0.22F, 1.12F, 0.18F}},
      {0.14F, {0.28F, 1.18F, -0.30F}, {-0.18F, 1.08F, 0.18F}},
      {0.34F, {0.18F, 1.10F, 0.28F}, {-0.06F, 1.06F, 0.34F}},
      {0.48F,
       {0.02F, 1.02F, 1.42F},
       {0.00F, 1.02F, 0.74F},
       {0.00F, -0.08F, 0.24F},
       {0.00F, -0.08F, 0.24F},
       {0.00F, -0.02F, 0.10F},
       {0.00F, -0.04F, 0.12F},
       {0.00F, -0.02F, 0.08F},
       {0.00F, 0.00F, 0.26F},
       {0.00F, 0.00F, 0.16F},
       {0.00F, 0.00F, -0.08F}},
      {0.72F, {0.08F, 1.02F, 1.02F}, {-0.04F, 1.00F, 0.58F}},
      {1.00F, {0.22F, 1.14F, 0.24F}, {-0.22F, 1.10F, 0.18F}},
  }};
  static const AuthoredSwordPoseKeys finisher{{
      {0.00F, {0.22F, 1.18F, 0.20F}, {-0.22F, 1.12F, 0.18F}},
      {0.18F, {0.26F, 1.68F, -0.42F}, {-0.20F, 1.22F, 0.18F}},
      {0.48F,
       {0.12F, 1.84F, -0.18F},
       {-0.08F, 1.28F, 0.30F},
       {0.00F, -0.04F, 0.04F},
       {0.08F, 0.12F, -0.08F}},
      {0.66F,
       {-0.10F, 0.58F, 1.42F},
       {-0.02F, 0.86F, 0.82F},
       {0.00F, -0.16F, 0.28F},
       {0.02F, -0.26F, 0.30F},
       {-0.06F, -0.10F, 0.16F},
       {0.00F, -0.12F, 0.20F},
       {0.00F, -0.10F, 0.14F},
       {0.00F, 0.00F, 0.30F},
       {0.00F, 0.00F, 0.18F}},
      {0.86F, {-0.24F, 0.52F, 0.74F}, {-0.08F, 0.82F, 0.54F}},
      {1.00F, {0.22F, 1.12F, 0.24F}, {-0.22F, 1.08F, 0.18F}},
  }};

  switch (variant) {
  case 1U:
    return slash_right;
  case 3U:
    return overhead;
  case 4U:
    return thrust;
  case 5U:
    return finisher;
  case 0U:
  default:
    return slash_left;
  }
}

auto sample_authored_sword_pose_key(const AuthoredSwordPoseKeys& keys,
                                    float phase) -> AuthoredSwordPoseKey {
  float const clamped = std::clamp(phase, 0.0F, 1.0F);
  for (std::size_t i = 1; i < keys.size(); ++i) {
    if (clamped <= keys[i].phase) {
      auto const& from = keys[i - 1U];
      auto const& to = keys[i];
      float const span = std::max(0.001F, to.phase - from.phase);
      float const raw_t = std::clamp((clamped - from.phase) / span, 0.0F, 1.0F);
      float const t = raw_t * raw_t * (3.0F - 2.0F * raw_t);
      return {
          .phase = clamped,
          .right_hand = blend_vec(from.right_hand, to.right_hand, t),
          .left_hand = blend_vec(from.left_hand, to.left_hand, t),
          .pelvis_delta = blend_vec(from.pelvis_delta, to.pelvis_delta, t),
          .shoulder_r_delta = blend_vec(from.shoulder_r_delta, to.shoulder_r_delta, t),
          .shoulder_l_delta = blend_vec(from.shoulder_l_delta, to.shoulder_l_delta, t),
          .neck_delta = blend_vec(from.neck_delta, to.neck_delta, t),
          .head_delta = blend_vec(from.head_delta, to.head_delta, t),
          .foot_r_delta = blend_vec(from.foot_r_delta, to.foot_r_delta, t),
          .knee_r_delta = blend_vec(from.knee_r_delta, to.knee_r_delta, t),
          .foot_l_delta = blend_vec(from.foot_l_delta, to.foot_l_delta, t),
          .knee_l_delta = blend_vec(from.knee_l_delta, to.knee_l_delta, t),
      };
    }
  }
  return keys.back();
}

void apply_authored_rpg_sword_pose(std::uint8_t variant,
                                   float phase,
                                   Render::GL::HumanoidPose& pose) {
  auto const sample =
      sample_authored_sword_pose_key(rpg_sword_pose_keys(variant), phase);
  pose.hand_r = sample.right_hand;
  pose.hand_l = sample.left_hand;
  pose.elbow_r =
      blend_vec(pose.shoulder_r, pose.hand_r, 0.52F) + QVector3D(0.05F, -0.06F, -0.02F);
  pose.elbow_l =
      blend_vec(pose.shoulder_l, pose.hand_l, 0.55F) + QVector3D(-0.04F, -0.05F, 0.02F);
  pose.pelvis_pos += sample.pelvis_delta;
  pose.shoulder_r += sample.shoulder_r_delta;
  pose.shoulder_l += sample.shoulder_l_delta;
  pose.neck_base += sample.neck_delta;
  pose.head_pos += sample.head_delta;
  pose.foot_r += sample.foot_r_delta;
  pose.knee_r += sample.knee_r_delta;
  pose.foot_l += sample.foot_l_delta;
  pose.knee_l += sample.knee_l_delta;
}

void bake_hold_pose(HumanoidBakeProfile profile,
                    BakerHoldType hold_type,
                    float sample_phase,
                    Render::GL::HumanoidPose& pose) {
  Render::GL::HumanoidAnimationContext anim_ctx{};
  anim_ctx.gait = hold_gait_descriptor();
  anim_ctx.gait.state = Render::GL::HumanoidMotionState::Hold;
  anim_ctx.inputs.is_in_hold_mode = true;
  anim_ctx.inputs.hold_entry_progress = 1.0F;
  anim_ctx.inputs.time = std::clamp(sample_phase, 0.0F, 1.0F) * 1.8F;

  Render::GL::HumanoidPoseController ctrl(pose, anim_ctx);

  float kneel_depth = 0.875F;
  if (profile == HumanoidBakeProfile::SwordReady) {
    kneel_depth = 0.825F;
  } else if (hold_type == BakerHoldType::Bow) {
    kneel_depth = 1.125F;
  }

  ctrl.kneel(kneel_depth);
  if (profile == HumanoidBakeProfile::SwordReady) {
    ctrl.guard_sword_and_shield_for_defense();
  } else if (hold_type == BakerHoldType::Bow) {
    ctrl.hold_bow_ready();
  } else {
    ctrl.brace_spear_for_hold();
  }

}

void bake_death_pose(BakerDeathType death_type,
                     float blend,
                     Render::GL::HumanoidPose& pose) {
  float const fall = std::clamp(blend, 0.0F, 1.0F);
  float const eased = fall * fall * (3.0F - 2.0F * fall);
  float const side_sign = (death_type == BakerDeathType::Mounted) ? -1.0F : 1.0F;

  pose.pelvis_pos.setY(pose.pelvis_pos.y() - 0.42F * eased);
  pose.pelvis_pos.setZ(pose.pelvis_pos.z() - 0.36F * eased);

  pose.neck_base.setY(pose.neck_base.y() - 0.58F * eased);
  pose.neck_base.setZ(pose.neck_base.z() - 0.52F * eased);
  pose.head_pos.setY(pose.head_pos.y() - 0.72F * eased);
  pose.head_pos.setZ(pose.head_pos.z() - 0.64F * eased);
  pose.head_pos.setX(pose.head_pos.x() + side_sign * 0.12F * eased);

  pose.shoulder_l.setY(pose.shoulder_l.y() - 0.45F * eased);
  pose.shoulder_r.setY(pose.shoulder_r.y() - 0.42F * eased);
  pose.shoulder_l.setZ(pose.shoulder_l.z() - 0.28F * eased);
  pose.shoulder_r.setZ(pose.shoulder_r.z() - 0.22F * eased);
  pose.shoulder_l.setX(pose.shoulder_l.x() - side_sign * 0.08F * eased);
  pose.shoulder_r.setX(pose.shoulder_r.x() - side_sign * 0.03F * eased);

  pose.hand_l.setY(pose.hand_l.y() - 0.58F * eased);
  pose.hand_r.setY(pose.hand_r.y() - 0.52F * eased);
  pose.hand_l.setZ(pose.hand_l.z() - 0.40F * eased);
  pose.hand_r.setZ(pose.hand_r.z() - 0.28F * eased);
  pose.hand_l.setX(pose.hand_l.x() - side_sign * 0.14F * eased);
  pose.hand_r.setX(pose.hand_r.x() - side_sign * 0.06F * eased);

  pose.knee_l.setY(pose.knee_l.y() - 0.26F * eased);
  pose.knee_r.setY(pose.knee_r.y() - 0.24F * eased);
  pose.knee_l.setZ(pose.knee_l.z() + 0.18F * eased);
  pose.knee_r.setZ(pose.knee_r.z() + 0.08F * eased);
  pose.foot_l.setZ(pose.foot_l.z() + 0.28F * eased);
  pose.foot_r.setZ(pose.foot_r.z() + 0.12F * eased);

  if (death_type == BakerDeathType::Mounted) {
    pose.pelvis_pos.setX(pose.pelvis_pos.x() + side_sign * 0.22F * eased);
    pose.neck_base.setX(pose.neck_base.x() + side_sign * 0.26F * eased);
    pose.head_pos.setX(pose.head_pos.x() + side_sign * 0.34F * eased);
    pose.foot_l.setX(pose.foot_l.x() + side_sign * 0.16F * eased);
    pose.foot_r.setX(pose.foot_r.x() + side_sign * 0.12F * eased);
  }
}

void bake_humanoid_clip_frame(HumanoidBakeProfile profile,
                              const HumanoidClipSpec& clip,
                              std::uint32_t frame_index,
                              std::vector<QMatrix4x4>& out_palettes,
                              std::vector<QMatrix4x4>& out_sockets) {
  Render::GL::VariationParams variation{};
  variation.height_scale = 1.0F;
  variation.bulk_scale = 1.0F;
  variation.stance_width = 1.0F;
  variation.arm_swing_amp = 1.0F;
  variation.walk_speed_mult = 1.0F;
  variation.posture_slump = 0.0F;
  variation.shoulder_tilt = 0.0F;

  float const phase =
      (!clip.loops && clip.frames > 1U)
          ? static_cast<float>(frame_index) / static_cast<float>(clip.frames - 1U)
          : static_cast<float>(frame_index) / static_cast<float>(clip.frames);

  Render::GL::HumanoidPose pose{};

  if (clip.death_type != BakerDeathType::None) {
    Render::GL::HumanoidGaitDescriptor gait{};
    gait.state = Render::GL::HumanoidMotionState::Idle;
    gait.cycle_time = 1.6F;
    gait.cycle_phase = 0.0F;
    gait.speed = 0.0F;
    gait.normalized_speed = 0.0F;
    Render::GL::HumanoidRendererBase::compute_locomotion_pose(
        0U, 0.0F, gait, variation, pose);
    float const death_blend = clip.loops ? 1.0F : phase;
    bake_death_pose(clip.death_type, death_blend, pose);
  } else if (clip.attack_type != BakerAttackType::None) {

    Render::GL::HumanoidGaitDescriptor hold_gait{};
    hold_gait.state = Render::GL::HumanoidMotionState::Hold;
    hold_gait.cycle_time = 1.8F;
    hold_gait.cycle_phase = 0.0F;
    hold_gait.speed = 0.0F;
    hold_gait.normalized_speed = 0.0F;
    Render::GL::HumanoidRendererBase::compute_locomotion_pose(
        0U, 0.0F, hold_gait, variation, pose);

    Render::GL::HumanoidAnimationContext anim_ctx{};
    anim_ctx.gait = hold_gait;
    anim_ctx.gait.state = Render::GL::HumanoidMotionState::Attacking;
    anim_ctx.attack_phase = phase;
    anim_ctx.jitter_seed = 0U;
    anim_ctx.inputs.is_attacking = true;
    anim_ctx.inputs.is_melee = !is_ranged_attack_type(clip.attack_type);
    anim_ctx.inputs.attack_variant = clip.attack_variant;

    Render::GL::HumanoidPoseController ctrl(pose, anim_ctx);
    float const sword_reach_scale =
        profile == HumanoidBakeProfile::Skeleton ? 0.88F : 1.0F;
    switch (clip.attack_type) {
    case BakerAttackType::Sword:
      if (is_rpg_sword_clip(clip)) {
        apply_authored_rpg_sword_pose(clip.attack_variant, phase, pose);
      } else if (profile == HumanoidBakeProfile::SwordReady ||
                 profile == HumanoidBakeProfile::Skeleton) {
        ctrl.combat_sword_slash_variant(phase, clip.attack_variant, sword_reach_scale);
      } else {
        ctrl.sword_slash_variant(phase, clip.attack_variant, sword_reach_scale);
      }
      break;
    case BakerAttackType::Spear:
      ctrl.spear_thrust_variant(phase, clip.attack_variant);
      break;
    case BakerAttackType::Bow:
      ctrl.aim_bow(phase);
      break;
    case BakerAttackType::BowMelee:
      ctrl.bow_melee_strike(phase);
      break;
    case BakerAttackType::SpearFromHold:
      ctrl.kneel(0.875F);
      ctrl.spear_thrust_from_hold(phase, 0.875F);
      break;
    case BakerAttackType::BowFromHold:
      ctrl.kneel(1.125F);
      ctrl.aim_bow(phase);
      break;
    default:
      break;
    }
  } else if (clip.riding_type != BakerRidingType::None) {
    Render::GL::HumanoidGaitDescriptor hold_gait{};
    hold_gait.state = Render::GL::HumanoidMotionState::Hold;
    hold_gait.cycle_time = 1.8F;
    hold_gait.cycle_phase = 0.0F;
    hold_gait.speed = 0.0F;
    hold_gait.normalized_speed = 0.0F;
    Render::GL::HumanoidRendererBase::compute_locomotion_pose(
        0U, 0.0F, hold_gait, variation, pose);

    Render::GL::HumanoidAnimationContext anim_ctx_r{};
    anim_ctx_r.variation = variation;
    anim_ctx_r.gait = hold_gait;
    anim_ctx_r.gait.state = Render::GL::HumanoidMotionState::Hold;
    anim_ctx_r.inputs.movement_state =
        (clip.riding_type != BakerRidingType::Idle)
            ? Render::Creature::MovementAnimationState::Walk
            : Render::Creature::MovementAnimationState::Idle;

    auto horse_profile = Render::GL::make_horse_profile(0U, {}, {});
    auto mount = Render::GL::compute_mount_frame(horse_profile);
    Render::GL::tune_mounted_knight_frame(horse_profile.dims, mount);

    Render::GL::MountedPoseController ctrl(pose, anim_ctx_r);
    if (profile == HumanoidBakeProfile::SwordReady &&
        clip.riding_type != BakerRidingType::BowShot &&
        clip.riding_type != BakerRidingType::SwordStrike &&
        clip.riding_type != BakerRidingType::SpearThrust) {
      Render::GL::MountedPoseController::MountedRiderPoseRequest request{};
      request.dims = horse_profile.dims;
      request.weapon_pose =
          Render::GL::MountedPoseController::MountedWeaponPose::SwordIdle;
      request.shield_pose = Render::GL::MountedPoseController::MountedShieldPose::Guard;
      request.left_hand_on_reins = false;
      request.right_hand_on_reins = false;

      switch (clip.riding_type) {
      case BakerRidingType::Idle:
        request.seat_pose = Render::GL::MountedPoseController::MountedSeatPose::Neutral;
        break;
      case BakerRidingType::Charge:
        request.seat_pose = Render::GL::MountedPoseController::MountedSeatPose::Forward;
        request.forward_bias = 0.30F;
        request.clearance_forward = 1.15F;
        break;
      case BakerRidingType::Reining:
        request.seat_pose =
            Render::GL::MountedPoseController::MountedSeatPose::Defensive;
        request.forward_bias = -0.15F;
        request.rein_tension_left = 0.55F;
        request.rein_tension_right = 0.55F;
        break;
      case BakerRidingType::BowShot:
      default:
        break;
      }

      ctrl.apply_pose(mount, request);
      ctrl.finalize_head_sync(mount, "sword_ready_riding");
    } else {
      ctrl.mount_on_horse(mount);

      switch (clip.riding_type) {
      case BakerRidingType::Idle:
        ctrl.riding_idle(mount);
        break;
      case BakerRidingType::Charge:
        ctrl.riding_charging(mount, 1.0F);
        break;
      case BakerRidingType::Reining:
        ctrl.riding_reining(mount, 0.7F, 0.7F);
        break;
      case BakerRidingType::BowShot:
        ctrl.riding_bow_shot(mount, phase);
        break;
      case BakerRidingType::SwordStrike:
        ctrl.riding_melee_strike(mount, phase);
        break;
      case BakerRidingType::SpearThrust:
        ctrl.riding_spear_thrust(mount, phase);
        break;
      default:
        break;
      }
    }
  } else {
    Render::GL::HumanoidGaitDescriptor gait{};
    gait.state = clip.state;
    gait.cycle_time = clip.cycle_time;
    float idle_time = phase * clip.cycle_time;

    switch (clip.state) {
    case Render::GL::HumanoidMotionState::Idle:
      gait.speed = 0.0F;
      gait.normalized_speed = 0.0F;
      break;
    case Render::GL::HumanoidMotionState::Walk:
      gait.speed = 1.5F;
      gait.normalized_speed = 1.0F;
      break;
    case Render::GL::HumanoidMotionState::Run:
      gait.speed = 4.0F;
      gait.normalized_speed = 1.6F;
      break;
    case Render::GL::HumanoidMotionState::Hold:
    default:
      gait.speed = 0.0F;
      gait.normalized_speed = 0.0F;
      break;
    }

    if (clip.hold_type != BakerHoldType::None) {
      gait.cycle_phase = 0.0F;
      Render::GL::HumanoidRendererBase::compute_locomotion_pose(
          0U, 0.0F, gait, variation, pose);
      bake_hold_pose(
          profile, clip.hold_type, phase, pose);
    } else if (clip.ambient_idle_type != BakerAmbientIdleType::None) {
      gait.cycle_phase = 0.0F;
      Render::GL::HumanoidRendererBase::compute_locomotion_pose(
          0U, 0.0F, gait, variation, pose);
      float const ambient_phase = transition_phase(frame_index, clip.frames);
      idle_time = ambient_phase * clip.cycle_time;
      Render::GL::HumanoidAnimationContext anim_ctx{};
      anim_ctx.gait = gait;
      anim_ctx.gait.state = Render::GL::HumanoidMotionState::Idle;
      Render::GL::HumanoidPoseController ctrl(pose, anim_ctx);
      if (profile == HumanoidBakeProfile::SwordReady) {
        ctrl.carry_sword_and_shield();
      } else if (profile == HumanoidBakeProfile::SpearReady) {
        ctrl.hold_spear_idle();
      }
      auto to_ambient_type = [](BakerAmbientIdleType t) -> Render::GL::AmbientIdleType {
        switch (t) {
        case BakerAmbientIdleType::SitDown:
          return Render::GL::AmbientIdleType::SitDown;
        case BakerAmbientIdleType::Jump:
          return Render::GL::AmbientIdleType::Jump;
        case BakerAmbientIdleType::RaiseWeapon:
          return Render::GL::AmbientIdleType::RaiseWeapon;
        case BakerAmbientIdleType::ShiftWeight:
          return Render::GL::AmbientIdleType::ShiftWeight;
        default:
          return Render::GL::AmbientIdleType::None;
        }
      };
      ctrl.apply_ambient_idle_explicit(to_ambient_type(clip.ambient_idle_type),
                                       ambient_phase);
    } else {
      gait.cycle_phase = phase;
      Render::GL::HumanoidRendererBase::compute_locomotion_pose(
          0U, phase * clip.cycle_time, gait, variation, pose);
    }
    if (profile == HumanoidBakeProfile::SwordReady &&
        clip.hold_type == BakerHoldType::None &&
        clip.ambient_idle_type == BakerAmbientIdleType::None) {
      Render::GL::HumanoidAnimationContext anim_ctx{};
      anim_ctx.gait = gait;
      anim_ctx.gait.state = clip.state;
      anim_ctx.inputs.movement_state =
          (gait.speed > 0.1F) ? Render::Creature::MovementAnimationState::Walk
                              : Render::Creature::MovementAnimationState::Idle;
      Render::GL::HumanoidPoseController ctrl(pose, anim_ctx);
      ctrl.carry_sword_and_shield();
    }
    if (clip.state == Render::GL::HumanoidMotionState::Idle &&
        clip.riding_type == BakerRidingType::Idle &&
        clip.hold_type == BakerHoldType::None &&
        clip.ambient_idle_type == BakerAmbientIdleType::None) {
      Render::GL::HumanoidAnimationContext anim_ctx{};
      anim_ctx.gait = gait;
      anim_ctx.gait.state = Render::GL::HumanoidMotionState::Idle;
      Render::GL::HumanoidPoseController ctrl(pose, anim_ctx);
      ctrl.apply_micro_idle(idle_time, 0U);
    }
  }

  if (is_rpg_sword_clip(clip) || clip.attack_type == BakerAttackType::Spear ||
      clip.attack_type == BakerAttackType::SpearFromHold ||
      clip.hold_type == BakerHoldType::Spear ||
      clip.riding_type == BakerRidingType::SwordStrike ||
      clip.riding_type == BakerRidingType::SpearThrust) {
    Render::Humanoid::rebuild_humanoid_frames(pose, QVector3D(1.0F, 1.0F, 1.0F), 1.0F);
  }

  Render::Humanoid::BonePalette palette{};
  Render::Humanoid::evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), palette);

  for (std::size_t b = 0; b < Render::Humanoid::k_bone_count; ++b) {
    out_palettes.push_back(palette[b]);
  }

  Render::GL::AnimationInputs socket_inputs{};
  if (clip.attack_type != BakerAttackType::None) {
    socket_inputs.is_attacking = true;
    socket_inputs.is_melee = !is_ranged_attack_type(clip.attack_type);
    socket_inputs.attack_variant = clip.attack_variant;
  }
  if (clip.riding_type == BakerRidingType::SpearThrust) {

    socket_inputs.is_mounted = true;
    socket_inputs.is_attacking = true;
    socket_inputs.is_melee = true;
  }
  if (clip.hold_type != BakerHoldType::None) {
    socket_inputs.is_in_hold_mode = true;
    socket_inputs.hold_entry_progress = 1.0F;
  }

  for (auto const& spec : k_humanoid_sockets) {
    switch (spec.kind) {
    case HumanoidSocketSpec::Kind::GripFrame:
      if (spec.socket == Render::Humanoid::HumanoidSocket::GripR &&
          pose.body_frames.grip_r.radius > 0.0F) {
        out_sockets.push_back(attachment_frame_matrix(pose.body_frames.grip_r));
        continue;
      }
      if (spec.socket == Render::Humanoid::HumanoidSocket::GripL &&
          pose.body_frames.grip_l.radius > 0.0F) {
        out_sockets.push_back(attachment_frame_matrix(pose.body_frames.grip_l));
        continue;
      }
      break;
    case HumanoidSocketSpec::Kind::SwordBladeBase:
      if (pose.body_frames.grip_r.radius > 0.0F) {
        out_sockets.push_back(
            baked_sword_blade_socket_matrix(pose.body_frames.grip_r, false));
        continue;
      }
      break;
    case HumanoidSocketSpec::Kind::SwordBladeTip:
      if (pose.body_frames.grip_r.radius > 0.0F) {
        out_sockets.push_back(
            baked_sword_blade_socket_matrix(pose.body_frames.grip_r, true));
        continue;
      }
      break;
    case HumanoidSocketSpec::Kind::SpearShaftBase:
    case HumanoidSocketSpec::Kind::SpearShaftTip:
    case HumanoidSocketSpec::Kind::SpearHeadTip:
      if (pose.body_frames.grip_r.radius > 0.0F) {
        out_sockets.push_back(baked_spear_socket_matrix(
            pose.body_frames.grip_r, socket_inputs, phase, spec.kind));
        continue;
      }
      break;
    case HumanoidSocketSpec::Kind::TopologySocket:
      break;
    }
    out_sockets.push_back(Render::Humanoid::socket_transform(palette, spec.socket));
  }
}

bool bake_humanoid(const std::filesystem::path& out_dir,
                   std::uint32_t species_id,
                   std::string_view file_name,
                   HumanoidBakeProfile profile) {
  bpat::BpatWriter writer(species_id,
                          static_cast<std::uint32_t>(Render::Humanoid::k_bone_count));
  for (auto const& spec : k_humanoid_sockets) {
    Render::Humanoid::SocketDef const& def = Render::Humanoid::socket_def(spec.socket);
    bpat::SocketDescriptor s{};
    s.name = spec.name;
    s.anchor_bone = static_cast<std::uint32_t>(def.bone);
    s.local_offset = def.local_offset;
    writer.add_socket(std::move(s));
  }

  for (std::size_t clip_index = 0; clip_index < k_humanoid_clips.size(); ++clip_index) {
    auto const& clip = k_humanoid_clips[clip_index];
    bpat::ClipDescriptor desc{};
    desc.name = clip.name;
    desc.frame_count = clip.frames;
    desc.fps = clip.fps;
    desc.loops = clip.loops;
    apply_humanoid_markers(static_cast<std::uint16_t>(clip_index), profile, desc);
    writer.add_clip(std::move(desc));

    std::vector<QMatrix4x4> palettes;
    palettes.reserve(static_cast<std::size_t>(clip.frames) *
                     Render::Humanoid::k_bone_count);
    std::vector<QMatrix4x4> sockets;
    sockets.reserve(static_cast<std::size_t>(clip.frames) * k_humanoid_sockets.size());
    for (std::uint32_t f = 0; f < clip.frames; ++f) {
      bake_humanoid_clip_frame(profile, clip, f, palettes, sockets);
    }
    writer.append_clip_palettes(palettes);
    writer.append_clip_socket_transforms(sockets);
  }

  std::filesystem::create_directories(out_dir);
  std::filesystem::path const out_path = out_dir / std::string(file_name);
  std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
  if (!out) {
    std::cerr << "[bpat_baker] cannot open " << out_path << " for writing\n";
    return false;
  }
  if (!writer.write(out)) {
    std::cerr << "[bpat_baker] write failed for " << out_path << "\n";
    return false;
  }
  out.flush();
  std::cout << "[bpat_baker] wrote " << out_path << " (" << writer.frame_total()
            << " frames, " << k_humanoid_clips.size() << " clips, "
            << Render::Humanoid::k_bone_count << " bones, " << k_humanoid_sockets.size()
            << " sockets)\n";
  return true;
}

bool bake_species_manifest(const std::filesystem::path& out_dir,
                           const Render::Creature::SpeciesManifest& manifest) {
  if (manifest.bind_palette == nullptr || manifest.creature_spec == nullptr ||
      manifest.bake_clip_palette == nullptr) {
    std::cerr << "[bpat_baker] manifest for " << manifest.species_name
              << " is incomplete\n";
    return false;
  }

  auto const bind_palette = manifest.bind_palette();
  std::vector<QMatrix4x4> inverse_bind;
  inverse_bind.reserve(bind_palette.size());
  for (const QMatrix4x4& m : bind_palette) {
    inverse_bind.push_back(m.inverted());
  }
  bpat::BpatWriter writer(manifest.species_id,
                          static_cast<std::uint32_t>(bind_palette.size()));

  for (std::size_t i = 0; i < manifest.clips.size(); ++i) {
    auto const& clip = manifest.clips[i];
    bpat::ClipDescriptor desc{};
    desc.name = clip.name;
    desc.frame_count = clip.frame_count;
    desc.fps = clip.fps;
    desc.loops = clip.loops;
    apply_generic_markers(desc);
    writer.add_clip(std::move(desc));

    std::vector<QMatrix4x4> palettes;
    palettes.reserve(static_cast<std::size_t>(clip.frame_count) * bind_palette.size());
    for (std::uint32_t f = 0; f < clip.frame_count; ++f) {
      manifest.bake_clip_palette(i, f, palettes);
    }
    writer.append_clip_palettes(palettes);
  }

  std::filesystem::create_directories(out_dir);
  std::filesystem::path const out_path = out_dir / std::string(manifest.bpat_file_name);
  std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
  if (!out) {
    std::cerr << "[bpat_baker] cannot open " << out_path << " for writing\n";
    return false;
  }
  if (!writer.write(out)) {
    std::cerr << "[bpat_baker] write failed for " << out_path << "\n";
    return false;
  }
  out.flush();
  std::cout << "[bpat_baker] wrote " << out_path << " (" << writer.frame_total()
            << " frames, " << manifest.clips.size() << " clips, " << bind_palette.size()
            << " bones)\n";

  Render::Creature::BakeInput mesh_input{};
  mesh_input.graph = &Render::Creature::part_graph_for(
      manifest.creature_spec(), Render::Creature::CreatureLOD::Minimal);
  mesh_input.bind_pose = bind_palette;
  auto source = Render::Creature::bake_rigged_mesh_cpu(mesh_input);
  snapshot::SnapshotMeshWriter snapshot_writer(
      manifest.species_id,
      Render::Creature::CreatureLOD::Minimal,
      static_cast<std::uint32_t>(source.vertices.size()),
      source.indices);
  for (std::size_t i = 0; i < manifest.clips.size(); ++i) {
    auto const& clip = manifest.clips[i];
    snapshot::ClipDescriptor desc{};
    desc.name = clip.name;
    desc.frame_count = clip.frame_count;
    snapshot_writer.add_clip(std::move(desc));

    std::vector<Render::GL::RiggedVertex> clip_vertices;
    clip_vertices.reserve(static_cast<std::size_t>(clip.frame_count) *
                          source.vertices.size());
    for (std::uint32_t f = 0; f < clip.frame_count; ++f) {
      std::vector<QMatrix4x4> frame_palette;
      frame_palette.reserve(bind_palette.size());
      manifest.bake_clip_palette(i, f, frame_palette);
      std::size_t const n = std::min(frame_palette.size(), inverse_bind.size());
      for (std::size_t b = 0; b < n; ++b) {
        frame_palette[b] = frame_palette[b] * inverse_bind[b];
      }
      auto baked = Render::GL::bake_snapshot_vertices(source.vertices, frame_palette);
      clip_vertices.insert(clip_vertices.end(), baked.begin(), baked.end());
    }
    snapshot_writer.append_clip_vertices(clip_vertices);
  }

  if (source.vertices.empty() || source.indices.empty()) {
    std::cerr << "[bpat_baker] warning: no geometry baked for " << manifest.species_name
              << " minimal snapshot; skipping write of "
              << (out_dir / std::string(manifest.minimal_snapshot_file_name))
              << " (pre-baked asset on disk is preserved)\n";
    return true;
  }

  std::filesystem::path const snapshot_out_path =
      out_dir / std::string(manifest.minimal_snapshot_file_name);
  std::ofstream snapshot_out(snapshot_out_path, std::ios::binary | std::ios::trunc);
  if (!snapshot_out) {
    std::cerr << "[bpat_baker] cannot open " << snapshot_out_path << " for writing\n";
    return false;
  }
  if (!snapshot_writer.write(snapshot_out)) {
    std::cerr << "[bpat_baker] write failed for " << snapshot_out_path << "\n";
    return false;
  }
  snapshot_out.flush();
  std::cout << "[bpat_baker] wrote " << snapshot_out_path << " ("
            << source.vertices.size() << " verts/frame, " << source.indices.size()
            << " indices, " << static_cast<int>(Render::Creature::CreatureLOD::Minimal)
            << " lod)\n";
  return true;
}

struct HorseClipSpec {
  const char* name{};
  Render::GL::GaitType gait;
  std::uint32_t frames{};
  float fps{};
  bool loops{};
  bool is_moving{};
  bool is_fighting{false};
  float bob_scale{0.0F};
};

constexpr std::array<HorseClipSpec, 6> k_horse_clips{{
    {"idle", Render::GL::GaitType::IDLE, 24U, 24.0F, true, false, false, 0.0F},
    {"walk", Render::GL::GaitType::WALK, 24U, 24.0F, true, true, false, 0.50F},
    {"trot", Render::GL::GaitType::TROT, 16U, 24.0F, true, true, false, 0.85F},
    {"canter", Render::GL::GaitType::CANTER, 16U, 24.0F, true, true, false, 1.00F},
    {"gallop", Render::GL::GaitType::GALLOP, 12U, 24.0F, true, true, false, 1.12F},
    {"fight", Render::GL::GaitType::IDLE, 24U, 24.0F, true, false, true, 0.0F},
}};

void bake_horse_clip_frame(const HorseClipSpec& clip,
                           std::uint32_t frame_index,
                           const Render::GL::HorseDimensions& dims,
                           std::vector<QMatrix4x4>& out_palettes) {
  float const phase = static_cast<float>(frame_index) / static_cast<float>(clip.frames);

  Render::GL::HorseGait const base{};
  Render::GL::HorseGait const gait = Render::GL::gait_for_type(clip.gait, base);

  Render::Horse::HorsePoseMotion motion{};
  motion.phase = phase;
  motion.is_moving = clip.is_moving;
  motion.is_fighting = clip.is_fighting;
  if (clip.is_fighting) {

    float const fight_bob = std::sin(phase * 2.0F * std::numbers::pi_v<float> + 0.5F) *
                            dims.idle_bob_amplitude * 0.45F;
    motion.bob = fight_bob;
  } else {
    float const amp =
        clip.is_moving ? dims.move_bob_amplitude : dims.idle_bob_amplitude;
    float const scale = clip.is_moving ? clip.bob_scale : 0.8F;
    motion.bob = std::sin(phase * 2.0F * std::numbers::pi_v<float>) * amp * scale;
  }

  Render::Horse::HorseSpecPose pose{};
  Render::Horse::make_horse_spec_pose_animated(dims, gait, motion, pose);

  Render::Horse::BonePalette palette{};
  Render::Horse::evaluate_horse_skeleton(pose, palette);

  for (std::size_t b = 0; b < Render::Horse::k_horse_bone_count; ++b) {
    out_palettes.push_back(palette[b]);
  }
}

bool bake_horse(const std::filesystem::path& out_dir) {
  bpat::BpatWriter writer(
      bpat::k_species_horse,
      static_cast<std::uint32_t>(Render::Horse::k_horse_bone_count));

  Render::GL::HorseDimensions const dims = Render::GL::make_horse_dimensions(0U);

  for (auto const& clip : k_horse_clips) {
    bpat::ClipDescriptor desc{};
    desc.name = clip.name;
    desc.frame_count = clip.frames;
    desc.fps = clip.fps;
    desc.loops = clip.loops;
    apply_generic_markers(desc);
    writer.add_clip(std::move(desc));

    std::vector<QMatrix4x4> palettes;
    palettes.reserve(static_cast<std::size_t>(clip.frames) *
                     Render::Horse::k_horse_bone_count);
    for (std::uint32_t f = 0; f < clip.frames; ++f) {
      bake_horse_clip_frame(clip, f, dims, palettes);
    }
    writer.append_clip_palettes(palettes);
  }

  std::filesystem::create_directories(out_dir);
  std::filesystem::path const out_path = out_dir / "horse.bpat";
  std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
  if (!out) {
    std::cerr << "[bpat_baker] cannot open " << out_path << " for writing\n";
    return false;
  }
  if (!writer.write(out)) {
    std::cerr << "[bpat_baker] write failed for " << out_path << "\n";
    return false;
  }
  out.flush();
  std::cout << "[bpat_baker] wrote " << out_path << " (" << writer.frame_total()
            << " frames, " << k_horse_clips.size() << " clips, "
            << Render::Horse::k_horse_bone_count << " bones)\n";

  Render::Creature::BakeInput mesh_input{};
  mesh_input.graph = &Render::Creature::part_graph_for(
      Render::Horse::horse_creature_spec(), Render::Creature::CreatureLOD::Minimal);
  mesh_input.bind_pose = Render::Horse::horse_bind_palette();
  auto source = Render::Creature::bake_rigged_mesh_cpu(mesh_input);
  snapshot::SnapshotMeshWriter snapshot_writer(
      bpat::k_species_horse,
      Render::Creature::CreatureLOD::Minimal,
      static_cast<std::uint32_t>(source.vertices.size()),
      source.indices);
  for (auto const& clip : k_horse_clips) {
    snapshot::ClipDescriptor desc{};
    desc.name = clip.name;
    desc.frame_count = clip.frames;
    snapshot_writer.add_clip(std::move(desc));

    std::vector<Render::GL::RiggedVertex> clip_vertices;
    clip_vertices.reserve(static_cast<std::size_t>(clip.frames) *
                          source.vertices.size());
    for (std::uint32_t f = 0; f < clip.frames; ++f) {
      std::vector<QMatrix4x4> frame_palette;
      frame_palette.reserve(Render::Horse::k_horse_bone_count);
      bake_horse_clip_frame(clip, f, dims, frame_palette);
      auto baked = Render::GL::bake_snapshot_vertices(source.vertices, frame_palette);
      clip_vertices.insert(clip_vertices.end(), baked.begin(), baked.end());
    }
    snapshot_writer.append_clip_vertices(clip_vertices);
  }

  std::filesystem::path const snapshot_out_path = out_dir / "horse_minimal.bpsm";
  std::ofstream snapshot_out(snapshot_out_path, std::ios::binary | std::ios::trunc);
  if (!snapshot_out) {
    std::cerr << "[bpat_baker] cannot open " << snapshot_out_path << " for writing\n";
    return false;
  }
  if (!snapshot_writer.write(snapshot_out)) {
    std::cerr << "[bpat_baker] write failed for " << snapshot_out_path << "\n";
    return false;
  }
  snapshot_out.flush();
  std::cout << "[bpat_baker] wrote " << snapshot_out_path << " ("
            << source.vertices.size() << " verts/frame, " << source.indices.size()
            << " indices, " << static_cast<int>(Render::Creature::CreatureLOD::Minimal)
            << " lod)\n";
  return true;
}

struct ElephantClipSpec {
  const char* name{};
  std::uint32_t frames{};
  float fps{};
  bool loops{};
  bool is_moving{};
  Render::GL::ElephantGait gait;
  bool is_fighting{false};
  float bob_scale{0.0F};
};

const std::array<ElephantClipSpec, 4> k_elephant_clips{{
    {"idle",
     24U,
     24.0F,
     true,
     false,
     Render::GL::ElephantGait{2.0F, 0.0F, 0.0F, 0.02F, 0.01F},
     false,
     0.0F},
    {"walk",
     24U,
     24.0F,
     true,
     true,
     Render::GL::ElephantGait{1.2F, 0.25F, 0.0F, 0.30F, 0.10F},
     false,
     0.62F},
    {"run",
     16U,
     24.0F,
     true,
     true,
     Render::GL::ElephantGait{0.6F, 0.5F, 0.5F, 0.70F, 0.25F},
     false,
     0.75F},
    {"fight",
     24U,
     24.0F,
     true,
     false,
     Render::GL::ElephantGait{1.15F, 0.0F, 0.0F, 0.30F, 0.06F},
     true,
     0.0F},
}};

void bake_elephant_clip_frame(const ElephantClipSpec& clip,
                              std::uint32_t frame_index,
                              const Render::GL::ElephantDimensions& dims,
                              std::vector<QMatrix4x4>& out_palettes) {
  float const phase = static_cast<float>(frame_index) / static_cast<float>(clip.frames);

  Render::Elephant::ElephantPoseMotion motion{};
  motion.phase = phase;
  motion.is_moving = clip.is_moving;
  motion.is_fighting = clip.is_fighting;
  motion.anim_time = phase * clip.gait.cycle_time;
  motion.combat_phase = Render::GL::CombatAnimPhase::Idle;
  if (clip.is_fighting) {

    float const fight_bob = std::sin(phase * 2.0F * std::numbers::pi_v<float>) *
                            dims.idle_bob_amplitude * 0.5F;
    motion.bob = fight_bob;
  } else {
    float const amp =
        clip.is_moving ? dims.move_bob_amplitude : dims.idle_bob_amplitude;
    float const scale = clip.is_moving ? clip.bob_scale : 0.8F;
    motion.bob = std::sin(phase * 2.0F * std::numbers::pi_v<float>) * amp * scale;
  }

  Render::Elephant::ElephantSpecPose pose{};
  Render::Elephant::make_elephant_spec_pose_animated(dims, clip.gait, motion, pose);

  Render::Elephant::BonePalette palette{};
  Render::Elephant::evaluate_elephant_skeleton(pose, palette);

  for (std::size_t b = 0; b < Render::Elephant::k_elephant_bone_count; ++b) {
    out_palettes.push_back(palette[b]);
  }
}

bool bake_elephant(const std::filesystem::path& out_dir) {
  bpat::BpatWriter writer(
      bpat::k_species_elephant,
      static_cast<std::uint32_t>(Render::Elephant::k_elephant_bone_count));

  Render::GL::ElephantDimensions const dims = Render::GL::make_elephant_dimensions(0U);

  for (auto const& clip : k_elephant_clips) {
    bpat::ClipDescriptor desc{};
    desc.name = clip.name;
    desc.frame_count = clip.frames;
    desc.fps = clip.fps;
    desc.loops = clip.loops;
    apply_generic_markers(desc);
    writer.add_clip(std::move(desc));

    std::vector<QMatrix4x4> palettes;
    palettes.reserve(static_cast<std::size_t>(clip.frames) *
                     Render::Elephant::k_elephant_bone_count);
    for (std::uint32_t f = 0; f < clip.frames; ++f) {
      bake_elephant_clip_frame(clip, f, dims, palettes);
    }
    writer.append_clip_palettes(palettes);
  }

  std::filesystem::create_directories(out_dir);
  std::filesystem::path const out_path = out_dir / "elephant.bpat";
  std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
  if (!out) {
    std::cerr << "[bpat_baker] cannot open " << out_path << " for writing\n";
    return false;
  }
  if (!writer.write(out)) {
    std::cerr << "[bpat_baker] write failed for " << out_path << "\n";
    return false;
  }
  out.flush();
  std::cout << "[bpat_baker] wrote " << out_path << " (" << writer.frame_total()
            << " frames, " << k_elephant_clips.size() << " clips, "
            << Render::Elephant::k_elephant_bone_count << " bones)\n";

  Render::Creature::BakeInput mesh_input{};
  mesh_input.graph =
      &Render::Creature::part_graph_for(Render::Elephant::elephant_creature_spec(),
                                        Render::Creature::CreatureLOD::Minimal);
  mesh_input.bind_pose = Render::Elephant::elephant_bind_palette();
  auto source = Render::Creature::bake_rigged_mesh_cpu(mesh_input);
  snapshot::SnapshotMeshWriter snapshot_writer(
      bpat::k_species_elephant,
      Render::Creature::CreatureLOD::Minimal,
      static_cast<std::uint32_t>(source.vertices.size()),
      source.indices);
  for (auto const& clip : k_elephant_clips) {
    snapshot::ClipDescriptor desc{};
    desc.name = clip.name;
    desc.frame_count = clip.frames;
    snapshot_writer.add_clip(std::move(desc));

    std::vector<Render::GL::RiggedVertex> clip_vertices;
    clip_vertices.reserve(static_cast<std::size_t>(clip.frames) *
                          source.vertices.size());
    for (std::uint32_t f = 0; f < clip.frames; ++f) {
      std::vector<QMatrix4x4> frame_palette;
      frame_palette.reserve(Render::Elephant::k_elephant_bone_count);
      bake_elephant_clip_frame(clip, f, dims, frame_palette);
      auto baked = Render::GL::bake_snapshot_vertices(source.vertices, frame_palette);
      clip_vertices.insert(clip_vertices.end(), baked.begin(), baked.end());
    }
    snapshot_writer.append_clip_vertices(clip_vertices);
  }

  std::filesystem::path const snapshot_out_path = out_dir / "elephant_minimal.bpsm";
  std::ofstream snapshot_out(snapshot_out_path, std::ios::binary | std::ios::trunc);
  if (!snapshot_out) {
    std::cerr << "[bpat_baker] cannot open " << snapshot_out_path << " for writing\n";
    return false;
  }
  if (!snapshot_writer.write(snapshot_out)) {
    std::cerr << "[bpat_baker] write failed for " << snapshot_out_path << "\n";
    return false;
  }
  snapshot_out.flush();
  std::cout << "[bpat_baker] wrote " << snapshot_out_path << " ("
            << source.vertices.size() << " verts/frame, " << source.indices.size()
            << " indices, " << static_cast<int>(Render::Creature::CreatureLOD::Minimal)
            << " lod)\n";
  return true;
}

} // namespace

int main(int argc, char** argv) {
  static_assert(Render::Creature::k_humanoid_idle_clip == 0U);
  static_assert(Render::Creature::k_humanoid_idle_squat_clip == 1U);
  static_assert(Render::Creature::k_humanoid_idle_jump_clip == 2U);
  static_assert(Render::Creature::k_humanoid_idle_weapon_clip == 3U);
  static_assert(Render::Creature::k_humanoid_idle_weave_clip == 4U);
  static_assert(Render::Creature::k_humanoid_hold_clip == 7U);
  static_assert(Render::Creature::k_humanoid_hold_bow_clip == 8U);
  static_assert(Render::Creature::k_humanoid_attack_sword_a_clip == 9U);
  static_assert(Render::Creature::k_humanoid_attack_spear_a_clip == 12U);
  static_assert(Render::Creature::k_humanoid_attack_bow_clip == 15U);
  static_assert(Render::Creature::k_humanoid_riding_idle_clip == 16U);
  static_assert(Render::Creature::k_humanoid_riding_bow_shot_clip == 19U);
  static_assert(Render::Creature::k_humanoid_riding_sword_strike_clip == 20U);
  static_assert(Render::Creature::k_humanoid_riding_spear_thrust_clip == 21U);
  static_assert(Render::Creature::k_humanoid_die_infantry_clip == 22U);
  static_assert(Render::Creature::k_humanoid_dead_mounted_clip == 25U);
  static_assert(Render::Creature::k_humanoid_rpg_sword_slash_left_clip == 26U);
  static_assert(Render::Creature::k_humanoid_rpg_sword_slash_right_clip == 27U);
  static_assert(Render::Creature::k_humanoid_rpg_sword_overhead_clip == 28U);
  static_assert(Render::Creature::k_humanoid_rpg_sword_thrust_clip == 29U);
  static_assert(Render::Creature::k_humanoid_rpg_sword_finisher_clip == 30U);
  std::filesystem::path out_dir = "assets/creatures";
  if (argc >= 2) {
    out_dir = argv[1];
  }
  bool ok = true;
  ok = bake_humanoid(out_dir,
                     bpat::k_species_humanoid,
                     "humanoid.bpat",
                     HumanoidBakeProfile::Default) &&
       ok;
  ok = bake_humanoid(out_dir,
                     bpat::k_species_humanoid_sword,
                     "humanoid_sword.bpat",
                     HumanoidBakeProfile::SwordReady) &&
       ok;
  ok = bake_humanoid(out_dir,
                     bpat::k_species_humanoid_spear,
                     "humanoid_spear.bpat",
                     HumanoidBakeProfile::SpearReady) &&
       ok;
  ok = bake_humanoid(out_dir,
                     bpat::k_species_humanoid_skeleton,
                     "humanoid_skeleton.bpat",
                     HumanoidBakeProfile::Skeleton) &&
       ok;
  ok = bake_species_manifest(out_dir, Render::Horse::horse_manifest()) && ok;
  ok = bake_species_manifest(out_dir, Render::Elephant::elephant_manifest()) && ok;
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
