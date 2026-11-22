#pragma once

// Shared by the humanoid_manifest*.cpp translation units; not a public header.

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <span>
#include <string_view>
#include <vector>

#include "animation/bpat/bpat_format.h"
#include "animation/clip_manifest.h"
#include "animation/commander_spear_manifest.h"
#include "animation/death_pose_manifest.h"
#include "animation/pose_curve.h"
#include "animation/rig/humanoid_proportions.h"
#include "animation/showcase_pose_manifest.h"
#include "render/creature/horse/dimensions.h"
#include "render/creature/horse/horse_motion.h"
#include "render/creature/humanoid/asset/humanoid_clip_table.h"
#include "render/creature/humanoid/asset/humanoid_manifest.h"
#include "render/creature/humanoid/asset/humanoid_spec.h"
#include "render/creature/humanoid/humanoid_types.h"
#include "render/creature/humanoid/runtime/body_frame_resolver.h"
#include "render/creature/humanoid/runtime/grip_axis.h"
#include "render/creature/humanoid/runtime/humanoid_renderer.h"
#include "render/creature/humanoid/runtime/mounted_pose_controller.h"
#include "render/creature/humanoid/runtime/mounted_swordsman_pose.h"
#include "render/creature/humanoid/runtime/pose_controller.h"
#include "render/creature/humanoid/runtime/pose_primitives.h"
#include "render/creature/humanoid/runtime/skeleton_evaluator.h"
#include "render/creature/humanoid/runtime/spear_pose_utils.h"
#include "render/creature/humanoid/runtime/sword_pose_utils.h"
#include "render/creature/humanoid_clip_ids.h"
#include "render/creature/movement_state.h"

namespace Render::Humanoid {

namespace humanoid_manifest_detail {

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

struct AuthoredSwordPoseKey {
  float phase{0.0F};
  QVector3D right_hand;
  QVector3D left_hand;
  QVector3D blade_dir{0.12F, 0.86F, 0.50F};
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

constexpr QVector3D k_rpg_sword_guard_right_hand{0.22F, 1.18F, 0.20F};

constexpr QVector3D k_rpg_sword_guard_left_hand{-0.22F, 1.13F, 0.18F};

constexpr QVector3D k_rpg_sword_guard_blade_dir{0.12F, 0.86F, 0.50F};

struct ProfileBinding {
  BakeProfile profile;
  std::uint32_t species_id;
  std::string_view species_name;
  std::string_view bpat_file_name;
  Render::Creature::BakeClipFrameFn bake_clip_frame;
  Render::Creature::BakeClipMarkersFn clip_markers;
  Render::Creature::CreatureSpecProviderFn creature_spec;
};

[[nodiscard]] constexpr auto
is_defensive_shield_hold(BakerHoldType type) noexcept -> bool;

[[nodiscard]] constexpr auto
defensive_shield_pose(BakerHoldType type) noexcept -> Animation::ShieldFormationPose;

auto to_showcase_move(BakerShowcaseType t) noexcept -> Animation::HumanoidShowcaseMove;

auto animation_profile_for_bake(BakeProfile profile) noexcept
    -> Animation::HumanoidClipProfile;

[[nodiscard]] auto is_rpg_sword_clip(const HumanoidClipSpec& clip) noexcept -> bool;

[[nodiscard]] auto is_rpg_spear_clip(const HumanoidClipSpec& clip) noexcept -> bool;

[[nodiscard]] auto commander_spear_move_for_variant(std::uint8_t variant) noexcept
    -> Animation::CommanderSpearMove;

[[nodiscard]] auto is_ranged_attack_type(BakerAttackType type) noexcept -> bool;

auto blend_vec(const QVector3D& from, const QVector3D& to, float t) -> QVector3D;

auto normalized_or(const QVector3D& value, const QVector3D& fallback) -> QVector3D;

auto attachment_frame_matrix(const Render::GL::AttachmentFrame& frame) -> QMatrix4x4;

auto sword_local_pose(const QVector3D& blade_axis_local) -> QMatrix4x4;

auto baked_sword_blade_socket_matrix(const Render::GL::AttachmentFrame& grip,
                                     bool tip) -> QMatrix4x4;

auto spear_endpoint_distance(HumanoidSocketSpec::Kind kind,
                             const Render::GL::SpearRenderConfig& config) -> float;

auto baked_spear_socket_matrix(const Render::GL::AttachmentFrame& grip,
                               const Render::GL::AnimationInputs& inputs,
                               float attack_phase,
                               HumanoidSocketSpec::Kind kind) -> QMatrix4x4;

auto transition_phase(std::uint32_t frame_index, std::uint32_t frame_count) -> float;

auto hold_gait_descriptor() -> Render::GL::HumanoidGaitDescriptor;

auto rpg_sword_guard_key(float phase) -> AuthoredSwordPoseKey;

auto rpg_sword_pose_keys(std::uint8_t variant) -> const AuthoredSwordPoseKeys&;

auto slerp_dir(const QVector3D& from, const QVector3D& to, float t) -> QVector3D;

auto sample_authored_sword_pose_key(const AuthoredSwordPoseKeys& keys,
                                    float phase) -> AuthoredSwordPoseKey;

void apply_authored_rpg_sword_pose(Render::GL::HumanoidPoseController& ctrl,
                                   std::uint8_t variant,
                                   float phase,
                                   Render::GL::HumanoidPose& pose);

void apply_authored_rpg_spear_pose(Render::GL::HumanoidPoseController& ctrl,
                                   std::uint8_t variant,
                                   float phase,
                                   Render::GL::HumanoidPose& pose);

void bake_hold_pose(BakeProfile profile,
                    BakerHoldType hold_type,
                    float sample_phase,
                    Render::GL::HumanoidPose& pose);

void bake_death_pose(Animation::HumanoidDeathCollapse collapse,
                     float blend,
                     Render::GL::HumanoidPose& pose);

[[nodiscard]] auto
to_ambient_idle_type(BakerAmbientIdleType t) noexcept -> Render::GL::AmbientIdleType;

[[nodiscard]] auto ready_weapon_for_profile(BakeProfile profile) noexcept
    -> Animation::HumanoidReadyWeapon;

auto apply_ground_stance_for_profile(Render::GL::HumanoidPoseController& ctrl,
                                     BakeProfile profile,
                                     bool moving = false) -> bool;

void restore_leg_lengths(Render::GL::HumanoidPose& pose, bool knees_over_toes);

auto shield_raised_to_front(BakeProfile profile, const HumanoidClipSpec& clip) -> bool;

auto hold_shield_upright(BakeProfile profile, Render::GL::HumanoidPose& pose) -> bool;

void bake_humanoid_clip_frame(BakeProfile profile,
                              const HumanoidClipSpec& clip,
                              std::uint32_t frame_index,
                              std::vector<QMatrix4x4>& out_palettes,
                              std::vector<QMatrix4x4>& out_sockets);

auto bind_palette_provider() noexcept -> std::span<const QMatrix4x4>;

auto clip_descriptors() noexcept
    -> std::span<const Render::Creature::BakeClipDescriptor>;

auto socket_descriptors() noexcept
    -> std::span<const Render::Creature::BakeSocketDescriptor>;

auto profile_bindings() noexcept -> std::span<const ProfileBinding>;

auto build_runtime_manifest(const ProfileBinding& binding)
    -> Render::Creature::CreatureRuntimeManifest;

auto runtime_manifests() noexcept
    -> const std::array<Render::Creature::CreatureRuntimeManifest, 6>&;

template <BakeProfile P>
void bake_clip_frame_for(std::size_t clip_index,
                         std::uint32_t frame_index,
                         std::vector<QMatrix4x4>& out_palettes,
                         std::vector<QMatrix4x4>* out_socket_transforms) {

  std::vector<QMatrix4x4> discarded_sockets;
  bake_humanoid_clip_frame(P,
                           k_humanoid_clips[clip_index],
                           frame_index,
                           out_palettes,
                           out_socket_transforms != nullptr ? *out_socket_transforms
                                                            : discarded_sockets);
}

template <BakeProfile P>
void clip_markers_for(std::size_t clip_index,
                      std::string_view clip_name,
                      Animation::ClipMarkers& out) {
  (void)clip_name;
  out = Animation::authored_humanoid_clip_markers(
      static_cast<std::uint16_t>(clip_index), animation_profile_for_bake(P));
}

template <BakeProfile P>
constexpr auto
make_binding(std::uint32_t species_id,
             std::string_view species_name,
             std::string_view bpat_file_name,
             Render::Creature::CreatureSpecProviderFn spec) -> ProfileBinding {
  return {P,
          species_id,
          species_name,
          bpat_file_name,
          &bake_clip_frame_for<P>,
          &clip_markers_for<P>,
          spec};
}

} // namespace humanoid_manifest_detail

} // namespace Render::Humanoid
