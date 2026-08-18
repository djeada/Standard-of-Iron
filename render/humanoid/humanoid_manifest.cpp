#include "humanoid_manifest.h"

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
#include "animation/death_pose_manifest.h"
#include "animation/showcase_pose_manifest.h"
#include "grip_axis.h"
#include "humanoid_full_builder.h"
#include "humanoid_renderer_base.h"
#include "humanoid_spec.h"
#include "mounted_pose_controller.h"
#include "pose_controller.h"
#include "render/creature/humanoid_clip_ids.h"
#include "render/creature/movement_state.h"
#include "render/entity/mounted_knight_pose.h"
#include "render/equipment/weapons/spear_renderer.h"
#include "render/equipment/weapons/sword_renderer.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/horse/dimensions.h"
#include "render/horse/horse_motion.h"
#include "skeleton.h"
#include "spear_pose_utils.h"

namespace Render::Humanoid {

namespace {

enum class BakerAttackType : std::uint8_t {
  None,
  Unarmed,
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
  Bow,
  TestudoFront,
  TestudoTop,
  TestudoLeft,
  TestudoRight,
  TestudoRear,
  CarthageShieldWallFront,
  CarthageShieldWallLeft,
  CarthageShieldWallRight
};

[[nodiscard]] constexpr auto
is_defensive_shield_hold(BakerHoldType type) noexcept -> bool {
  return type == BakerHoldType::TestudoFront || type == BakerHoldType::TestudoTop ||
         type == BakerHoldType::TestudoLeft || type == BakerHoldType::TestudoRight ||
         type == BakerHoldType::TestudoRear ||
         type == BakerHoldType::CarthageShieldWallFront ||
         type == BakerHoldType::CarthageShieldWallLeft ||
         type == BakerHoldType::CarthageShieldWallRight;
}

[[nodiscard]] constexpr auto
defensive_shield_pose(BakerHoldType type) noexcept -> Animation::ShieldFormationPose {
  switch (type) {
  case BakerHoldType::TestudoTop:
    return Animation::ShieldFormationPose::RomanTop;
  case BakerHoldType::TestudoLeft:
    return Animation::ShieldFormationPose::RomanLeft;
  case BakerHoldType::TestudoRight:
    return Animation::ShieldFormationPose::RomanRight;
  case BakerHoldType::TestudoRear:
    return Animation::ShieldFormationPose::RomanRear;
  case BakerHoldType::CarthageShieldWallFront:
    return Animation::ShieldFormationPose::CarthageFront;
  case BakerHoldType::CarthageShieldWallLeft:
    return Animation::ShieldFormationPose::CarthageLeft;
  case BakerHoldType::CarthageShieldWallRight:
    return Animation::ShieldFormationPose::CarthageRight;
  case BakerHoldType::TestudoFront:
  case BakerHoldType::None:
  case BakerHoldType::Spear:
  case BakerHoldType::Bow:
    break;
  }
  return Animation::ShieldFormationPose::RomanFront;
}
enum class BakerRidingType : std::uint8_t {
  None,
  Idle,
  Charge,
  Reining,
  BowShot,
  SwordStrike,
  SpearThrust
};
enum class BakerShowcaseType : std::uint8_t {
  None,
  Jump,
  FrontFlip,
  Handstand,
  SideAerial,
  SwordFlourish,
  SpearThrow,
  RestSit,
  RestSitKnees,
  RestKneel,
  RestSitDown,
  RestSitKneesDown,
  TauntDismissive,
  TauntCynical,
};
enum class BakerAmbientIdleType : std::uint8_t {
  None,
  SitDown,
  Jump,
  RaiseWeapon,
  ShiftWeight,
  PlantFlag
};
auto to_showcase_move(BakerShowcaseType t) noexcept -> Animation::HumanoidShowcaseMove {
  switch (t) {
  case BakerShowcaseType::Jump:
    return Animation::HumanoidShowcaseMove::Jump;
  case BakerShowcaseType::FrontFlip:
    return Animation::HumanoidShowcaseMove::FrontFlip;
  case BakerShowcaseType::Handstand:
    return Animation::HumanoidShowcaseMove::Handstand;
  case BakerShowcaseType::SideAerial:
    return Animation::HumanoidShowcaseMove::SideAerial;
  case BakerShowcaseType::SwordFlourish:
    return Animation::HumanoidShowcaseMove::SwordFlourish;
  case BakerShowcaseType::SpearThrow:
    return Animation::HumanoidShowcaseMove::SpearThrow;
  case BakerShowcaseType::TauntDismissive:
    return Animation::HumanoidShowcaseMove::TauntDismissive;
  case BakerShowcaseType::TauntCynical:
    return Animation::HumanoidShowcaseMove::TauntCynical;
  case BakerShowcaseType::RestSit:
    return Animation::HumanoidShowcaseMove::RestSit;
  case BakerShowcaseType::RestSitKnees:
    return Animation::HumanoidShowcaseMove::RestSitKnees;
  case BakerShowcaseType::RestKneel:
    return Animation::HumanoidShowcaseMove::RestKneel;
  case BakerShowcaseType::RestSitDown:
    return Animation::HumanoidShowcaseMove::RestSitDown;
  case BakerShowcaseType::RestSitKneesDown:
    return Animation::HumanoidShowcaseMove::RestSitKneesDown;
  case BakerShowcaseType::None:
    break;
  }
  return Animation::HumanoidShowcaseMove::None;
}

auto animation_profile_for_bake(BakeProfile profile) noexcept
    -> Animation::HumanoidClipProfile {
  switch (profile) {
  case BakeProfile::Default:
    return Animation::HumanoidClipProfile::Default;
  case BakeProfile::SwordReady:
    return Animation::HumanoidClipProfile::SwordReady;
  case BakeProfile::SpearReady:
    return Animation::HumanoidClipProfile::SpearReady;
  case BakeProfile::Skeleton:
    return Animation::HumanoidClipProfile::Skeleton;
  case BakeProfile::Caster:
    return Animation::HumanoidClipProfile::Caster;
  case BakeProfile::StaveCaster:
    return Animation::HumanoidClipProfile::StaveCaster;
  }
  return Animation::HumanoidClipProfile::Default;
}

struct HumanoidClipSpec {
  const char* name{};
  Render::GL::HumanoidMotionState state;
  BakerAttackType attack_type{BakerAttackType::None};
  std::uint8_t attack_variant{0};
  Animation::HumanoidDeathCollapse death_collapse{
      Animation::HumanoidDeathCollapse::None};
  BakerRidingType riding_type{BakerRidingType::None};
  BakerHoldType hold_type{BakerHoldType::None};
  BakerAmbientIdleType ambient_idle_type{BakerAmbientIdleType::None};
  BakerShowcaseType showcase_type{BakerShowcaseType::None};
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
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     Animation::k_humanoid_idle_breath_frames,
     Animation::k_humanoid_idle_breath_fps,
     Animation::k_humanoid_idle_breath_cycle_time,
     true},
    {"idle_squat",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::SitDown,
     BakerShowcaseType::None,
     72U,
     24.0F,
     3.0F,
     false},
    {"idle_jump",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::Jump,
     BakerShowcaseType::None,
     72U,
     24.0F,
     3.0F,
     false},
    {"idle_weapon",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::RaiseWeapon,
     BakerShowcaseType::None,
     72U,
     24.0F,
     3.0F,
     false},
    {"idle_weave",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::ShiftWeight,
     BakerShowcaseType::None,
     72U,
     24.0F,
     3.0F,
     false},
    {"idle_plant_flag",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::PlantFlag,
     BakerShowcaseType::None,
     72U,
     24.0F,
     3.0F,
     false},
    {"walk",
     Render::GL::HumanoidMotionState::Walk,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     32U,
     24.0F,
     0.92F,
     true},
    {"run",
     Render::GL::HumanoidMotionState::Run,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     32U,
     24.0F,
     0.56F,
     true},
    {"hold",
     Render::GL::HumanoidMotionState::Hold,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::Spear,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     16U,
     24.0F,
     1.8F,
     true},
    {"hold_bow",
     Render::GL::HumanoidMotionState::Hold,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::Bow,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     16U,
     24.0F,
     1.8F,
     true},
    {"attack_sword_a",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Sword,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     32U,
     24.0F,
     1.0F,
     false},
    {"attack_sword_b",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Sword,
     1,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     32U,
     24.0F,
     1.0F,
     false},
    {"attack_sword_c",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Sword,
     2,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     32U,
     24.0F,
     1.0F,
     false},
    {"attack_spear_a",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Spear,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     32U,
     24.0F,
     1.0F,
     false},
    {"attack_spear_b",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Spear,
     1,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     32U,
     24.0F,
     1.0F,
     false},
    {"attack_spear_c",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Spear,
     2,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     32U,
     24.0F,
     1.0F,
     false},
    {"attack_bow",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Bow,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     32U,
     24.0F,
     1.0F,
     false},
    {"riding_idle",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::Idle,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     Animation::k_humanoid_idle_breath_frames,
     Animation::k_humanoid_idle_breath_fps,
     Animation::k_humanoid_idle_breath_cycle_time,
     true},
    {"riding_charge",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::Charge,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     24U,
     24.0F,
     1.0F,
     false},
    {"riding_reining",
     Render::GL::HumanoidMotionState::Hold,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::Reining,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     24U,
     24.0F,
     1.0F,
     false},
    {"riding_bow_shot",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::BowShot,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     24U,
     24.0F,
     1.0F,
     false},
    {"riding_sword_strike",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::SwordStrike,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     32U,
     24.0F,
     1.2F,
     false},
    {"riding_spear_thrust",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::SpearThrust,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     32U,
     24.0F,
     1.2F,
     false},
    {"die_infantry",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::BackSprawl,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     Animation::humanoid_death_collapse_frames(
         Animation::HumanoidDeathCollapse::BackSprawl),
     Animation::k_humanoid_death_bake_fps,
     1.0F,
     false},
    {"die_infantry_face",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::FacePlant,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     Animation::humanoid_death_collapse_frames(
         Animation::HumanoidDeathCollapse::FacePlant),
     Animation::k_humanoid_death_bake_fps,
     1.0F,
     false},
    {"die_infantry_side",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::SideCrumple,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     Animation::humanoid_death_collapse_frames(
         Animation::HumanoidDeathCollapse::SideCrumple),
     Animation::k_humanoid_death_bake_fps,
     1.0F,
     false},
    {"dead_infantry",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::BackSprawl,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     1U,
     1.0F,
     1.0F,
     true},
    {"dead_infantry_face",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::FacePlant,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     1U,
     1.0F,
     1.0F,
     true},
    {"dead_infantry_side",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::SideCrumple,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     1U,
     1.0F,
     1.0F,
     true},
    {"die_mounted",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::MountedUnseat,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     Animation::humanoid_death_collapse_frames(
         Animation::HumanoidDeathCollapse::MountedUnseat),
     Animation::k_humanoid_death_bake_fps,
     1.0F,
     false},
    {"dead_mounted",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::MountedUnseat,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     1U,
     1.0F,
     1.0F,
     true},
    {"rpg_sword_slash_left",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Sword,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     36U,
     24.0F,
     1.0F,
     false},
    {"rpg_sword_slash_right",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Sword,
     1,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     36U,
     24.0F,
     1.0F,
     false},
    {"rpg_sword_overhead",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Sword,
     3,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     40U,
     24.0F,
     1.1F,
     false},
    {"rpg_sword_thrust",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Sword,
     4,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     34U,
     24.0F,
     0.9F,
     false},
    {"rpg_sword_finisher",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Sword,
     5,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     44U,
     24.0F,
     1.25F,
     false},
    {"archer_melee",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::BowMelee,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     32U,
     24.0F,
     1.0F,
     false},
    {"hold_spear_attack",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::SpearFromHold,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     32U,
     24.0F,
     1.0F,
     false},
    {"hold_bow_attack",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::BowFromHold,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     32U,
     24.0F,
     1.0F,
     false},

    {"showcase_jump",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::Jump,
     48U,
     30.0F,
     1.6F,
     false},
    {"showcase_front_flip",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::FrontFlip,
     51U,
     30.0F,
     1.7F,
     false},
    {"showcase_handstand",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::Handstand,
     102U,
     30.0F,
     3.4F,
     false},
    {"showcase_side_aerial",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::SideAerial,
     57U,
     30.0F,
     1.9F,
     false},
    {"showcase_sword_flourish",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::SwordFlourish,
     78U,
     30.0F,
     2.6F,
     false},
    {"showcase_spear_throw",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::SpearThrow,
     66U,
     30.0F,
     2.2F,
     false},
    {"showcase_rest_sit",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::RestSit,
     180U,
     30.0F,
     6.0F,
     true},
    {"showcase_rest_sit_knees",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::RestSitKnees,
     210U,
     30.0F,
     7.0F,
     true},
    {"showcase_rest_kneel",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::RestKneel,
     150U,
     30.0F,
     5.0F,
     true},
    {"showcase_rest_sit_down",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::RestSitDown,
     48U,
     30.0F,
     1.6F,
     false},
    {"showcase_rest_sit_knees_down",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::RestSitKneesDown,
     51U,
     30.0F,
     1.7F,
     false},
    {"unarmed_jab",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Unarmed,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     28U,
     28.0F,
     1.0F,
     false},
    {"unarmed_cross",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Unarmed,
     1,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     28U,
     28.0F,
     1.0F,
     false},
    {"unarmed_hook",
     Render::GL::HumanoidMotionState::Attacking,
     BakerAttackType::Unarmed,
     2,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     30U,
     28.0F,
     1.05F,
     false},
    {"testudo_front",
     Render::GL::HumanoidMotionState::Hold,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::TestudoFront,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     16U,
     24.0F,
     1.8F,
     true},
    {"testudo_top",
     Render::GL::HumanoidMotionState::Hold,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::TestudoTop,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     16U,
     24.0F,
     1.8F,
     true},
    {"testudo_left",
     Render::GL::HumanoidMotionState::Hold,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::TestudoLeft,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     16U,
     24.0F,
     1.8F,
     true},
    {"testudo_right",
     Render::GL::HumanoidMotionState::Hold,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::TestudoRight,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     16U,
     24.0F,
     1.8F,
     true},
    {"testudo_rear",
     Render::GL::HumanoidMotionState::Hold,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::TestudoRear,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     16U,
     24.0F,
     1.8F,
     true},
    {"carthage_shield_wall_front",
     Render::GL::HumanoidMotionState::Hold,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::CarthageShieldWallFront,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     16U,
     24.0F,
     1.8F,
     true},
    {"carthage_shield_wall_left",
     Render::GL::HumanoidMotionState::Hold,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::CarthageShieldWallLeft,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     16U,
     24.0F,
     1.8F,
     true},
    {"carthage_shield_wall_right",
     Render::GL::HumanoidMotionState::Hold,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::CarthageShieldWallRight,
     BakerAmbientIdleType::None,
     BakerShowcaseType::None,
     16U,
     24.0F,
     1.8F,
     true},
    {"showcase_taunt_dismissive",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::TauntDismissive,
     72U,
     30.0F,
     2.4F,
     false},
    {"showcase_taunt_cynical",
     Render::GL::HumanoidMotionState::Idle,
     BakerAttackType::None,
     0,
     Animation::HumanoidDeathCollapse::None,
     BakerRidingType::None,
     BakerHoldType::None,
     BakerAmbientIdleType::None,
     BakerShowcaseType::TauntCynical,
     96U,
     30.0F,
     3.2F,
     true},
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

auto rpg_sword_guard_key(float phase) -> AuthoredSwordPoseKey {
  return {.phase = phase,
          .right_hand = k_rpg_sword_guard_right_hand,
          .left_hand = k_rpg_sword_guard_left_hand,
          .blade_dir = k_rpg_sword_guard_blade_dir};
}

auto rpg_sword_pose_keys(std::uint8_t variant) -> const AuthoredSwordPoseKeys& {
  static const AuthoredSwordPoseKeys slash_left{{
      rpg_sword_guard_key(0.00F),
      {.phase = 0.16F,
       .right_hand = {0.46F, 1.44F, -0.26F},
       .left_hand = {-0.26F, 1.10F, 0.20F},
       .blade_dir = {0.46F, 0.74F, -0.49F}},
      {.phase = 0.38F,
       .right_hand = {0.42F, 1.48F, 0.02F},
       .left_hand = {-0.22F, 1.08F, 0.26F},
       .blade_dir = {0.32F, 0.90F, -0.30F},
       .pelvis_delta = {0.02F, -0.03F, 0.04F},
       .shoulder_r_delta = {0.12F, 0.02F, -0.08F},
       .shoulder_l_delta = {-0.06F, -0.01F, 0.07F}},
      {.phase = 0.54F,
       .right_hand = {-0.30F, 0.86F, 0.82F},
       .left_hand = {0.04F, 0.98F, 0.66F},
       .blade_dir = {-0.56F, -0.32F, 0.76F},
       .pelvis_delta = {0.02F, -0.06F, 0.16F},
       .shoulder_r_delta = {-0.12F, -0.12F, 0.26F},
       .shoulder_l_delta = {-0.12F, 0.04F, 0.02F},
       .neck_delta = {0.00F, -0.04F, 0.10F},
       .head_delta = {0.00F, -0.03F, 0.06F},
       .foot_r_delta = {0.00F, 0.00F, 0.18F},
       .knee_r_delta = {0.00F, 0.00F, 0.10F}},
      {.phase = 0.76F,
       .right_hand = {-0.34F, 0.72F, 0.76F},
       .left_hand = {-0.10F, 0.94F, 0.54F},
       .blade_dir = {-0.84F, -0.44F, 0.32F},
       .pelvis_delta = {0.00F, -0.04F, 0.10F},
       .shoulder_r_delta = {-0.08F, -0.08F, 0.14F}},
      rpg_sword_guard_key(1.00F),
  }};
  static const AuthoredSwordPoseKeys slash_right{{
      rpg_sword_guard_key(0.00F),
      {.phase = 0.16F,
       .right_hand = {-0.28F, 1.42F, -0.22F},
       .left_hand = {-0.26F, 1.10F, 0.20F},
       .blade_dir = {-0.46F, 0.74F, -0.49F}},
      {.phase = 0.38F,
       .right_hand = {-0.22F, 1.46F, 0.02F},
       .left_hand = {-0.24F, 1.08F, 0.24F},
       .blade_dir = {-0.32F, 0.90F, -0.30F},
       .pelvis_delta = {-0.02F, -0.03F, 0.03F},
       .shoulder_r_delta = {-0.08F, 0.02F, 0.06F},
       .shoulder_l_delta = {0.10F, -0.02F, -0.08F}},
      {.phase = 0.54F,
       .right_hand = {0.66F, 0.88F, 0.80F},
       .left_hand = {0.10F, 0.98F, 0.66F},
       .blade_dir = {0.56F, -0.32F, 0.76F},
       .pelvis_delta = {-0.02F, -0.06F, 0.15F},
       .shoulder_r_delta = {0.16F, -0.10F, 0.24F},
       .shoulder_l_delta = {0.12F, 0.04F, 0.00F},
       .neck_delta = {0.00F, -0.04F, 0.08F},
       .head_delta = {0.00F, -0.03F, 0.05F},
       .foot_r_delta = {0.00F, 0.00F, -0.02F},
       .knee_r_delta = {0.00F, 0.00F, -0.01F},
       .foot_l_delta = {0.00F, 0.00F, 0.12F},
       .knee_l_delta = {0.00F, 0.00F, 0.08F}},
      {.phase = 0.76F,
       .right_hand = {0.64F, 0.78F, 0.74F},
       .left_hand = {0.04F, 0.94F, 0.54F},
       .blade_dir = {0.84F, -0.44F, 0.32F},
       .pelvis_delta = {0.00F, -0.04F, 0.08F},
       .shoulder_r_delta = {0.12F, -0.06F, 0.12F}},
      rpg_sword_guard_key(1.00F),
  }};
  static const AuthoredSwordPoseKeys overhead{{
      rpg_sword_guard_key(0.00F),
      {.phase = 0.18F,
       .right_hand = {0.18F, 1.58F, -0.32F},
       .left_hand = {-0.18F, 1.18F, 0.20F},
       .blade_dir = {0.10F, 0.97F, -0.22F}},
      {.phase = 0.42F,
       .right_hand = {0.10F, 1.72F, -0.10F},
       .left_hand = {-0.10F, 1.22F, 0.30F},
       .blade_dir = {0.04F, 0.99F, -0.10F},
       .pelvis_delta = {0.00F, -0.03F, 0.02F},
       .shoulder_r_delta = {0.06F, 0.10F, -0.04F},
       .shoulder_l_delta = {-0.04F, 0.08F, -0.02F}},
      {.phase = 0.60F,
       .right_hand = {0.02F, 0.78F, 1.22F},
       .left_hand = {-0.02F, 0.96F, 0.74F},
       .blade_dir = {-0.04F, -0.50F, 0.87F},
       .pelvis_delta = {0.00F, -0.10F, 0.20F},
       .shoulder_r_delta = {0.04F, -0.18F, 0.20F},
       .shoulder_l_delta = {-0.04F, -0.06F, 0.12F},
       .neck_delta = {0.00F, -0.08F, 0.14F},
       .head_delta = {0.00F, -0.06F, 0.10F},
       .foot_r_delta = {0.00F, 0.00F, 0.20F},
       .knee_r_delta = {0.00F, 0.00F, 0.12F}},
      {.phase = 0.82F,
       .right_hand = {-0.02F, 0.64F, 0.70F},
       .left_hand = {-0.06F, 0.90F, 0.54F},
       .blade_dir = {-0.10F, -0.78F, 0.62F}},
      rpg_sword_guard_key(1.00F),
  }};
  static const AuthoredSwordPoseKeys thrust{{
      rpg_sword_guard_key(0.00F),
      {.phase = 0.14F,
       .right_hand = {0.28F, 1.18F, -0.30F},
       .left_hand = {-0.18F, 1.08F, 0.18F},
       .blade_dir = {0.22F, 0.66F, 0.72F}},
      {.phase = 0.34F,
       .right_hand = {0.18F, 1.10F, 0.28F},
       .left_hand = {-0.06F, 1.06F, 0.34F},
       .blade_dir = {0.10F, 0.32F, 0.94F}},
      {.phase = 0.48F,
       .right_hand = {0.06F, 1.16F, 1.60F},
       .left_hand = {0.00F, 1.02F, 0.74F},
       .blade_dir = {0.03F, 0.08F, 1.00F},
       .pelvis_delta = {0.00F, -0.08F, 0.30F},
       .shoulder_r_delta = {0.00F, -0.06F, 0.40F},
       .shoulder_l_delta = {0.00F, -0.02F, 0.10F},
       .neck_delta = {0.00F, -0.04F, 0.12F},
       .head_delta = {0.00F, -0.02F, 0.08F},
       .foot_r_delta = {0.00F, 0.00F, 0.26F},
       .knee_r_delta = {0.00F, 0.00F, 0.16F},
       .foot_l_delta = {0.00F, 0.00F, -0.08F}},
      {.phase = 0.72F,
       .right_hand = {0.08F, 1.02F, 1.02F},
       .left_hand = {-0.04F, 1.00F, 0.58F},
       .blade_dir = {0.06F, 0.28F, 0.96F}},
      rpg_sword_guard_key(1.00F),
  }};
  static const AuthoredSwordPoseKeys finisher{{
      rpg_sword_guard_key(0.00F),
      {.phase = 0.18F,
       .right_hand = {0.26F, 1.68F, -0.42F},
       .left_hand = {-0.20F, 1.22F, 0.18F},
       .blade_dir = {0.14F, 0.96F, -0.24F}},
      {.phase = 0.48F,
       .right_hand = {0.12F, 1.84F, -0.18F},
       .left_hand = {-0.08F, 1.28F, 0.30F},
       .blade_dir = {0.06F, 0.99F, -0.12F},
       .pelvis_delta = {0.00F, -0.04F, 0.04F},
       .shoulder_r_delta = {0.08F, 0.12F, -0.08F}},
      {.phase = 0.66F,
       .right_hand = {-0.10F, 0.58F, 1.42F},
       .left_hand = {-0.02F, 0.86F, 0.82F},
       .blade_dir = {-0.06F, -0.62F, 0.78F},
       .pelvis_delta = {0.00F, -0.16F, 0.28F},
       .shoulder_r_delta = {0.02F, -0.26F, 0.30F},
       .shoulder_l_delta = {-0.06F, -0.10F, 0.16F},
       .neck_delta = {0.00F, -0.12F, 0.20F},
       .head_delta = {0.00F, -0.10F, 0.14F},
       .foot_r_delta = {0.00F, 0.00F, 0.30F},
       .knee_r_delta = {0.00F, 0.00F, 0.18F}},
      {.phase = 0.86F,
       .right_hand = {-0.24F, 0.52F, 0.74F},
       .left_hand = {-0.08F, 0.82F, 0.54F},
       .blade_dir = {-0.14F, -0.86F, 0.48F}},
      rpg_sword_guard_key(1.00F),
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

auto slerp_dir(const QVector3D& from, const QVector3D& to, float t) -> QVector3D {
  if (from.lengthSquared() < 1.0e-8F || to.lengthSquared() < 1.0e-8F) {
    return blend_vec(from, to, t);
  }
  QVector3D const a = from.normalized();
  QVector3D const b = to.normalized();
  float const dot = std::clamp(QVector3D::dotProduct(a, b), -1.0F, 1.0F);
  if (dot > 0.9995F) {
    QVector3D const straight = blend_vec(a, b, t);
    return straight.lengthSquared() > 1.0e-8F ? straight.normalized() : a;
  }
  QVector3D ortho = b - (a * dot);
  if (ortho.lengthSquared() < 1.0e-8F) {

    ortho = QVector3D::crossProduct(a, QVector3D(0.0F, 1.0F, 0.0F));
    if (ortho.lengthSquared() < 1.0e-8F) {
      ortho = QVector3D::crossProduct(a, QVector3D(1.0F, 0.0F, 0.0F));
    }
  }
  ortho.normalize();
  float const theta = std::acos(dot) * std::clamp(t, 0.0F, 1.0F);
  return (a * std::cos(theta)) + (ortho * std::sin(theta));
}

auto sample_authored_sword_pose_key(const AuthoredSwordPoseKeys& keys,
                                    float phase) -> AuthoredSwordPoseKey {
  float const clamped = std::clamp(phase, 0.0F, 1.0F);
  std::size_t segment = 1U;
  while (segment + 1U < keys.size() && clamped > keys[segment].phase) {
    ++segment;
  }
  auto const& from = keys[segment - 1U];
  auto const& to = keys[segment];
  float const span = std::max(0.001F, to.phase - from.phase);
  float const t = std::clamp((clamped - from.phase) / span, 0.0F, 1.0F);

  using Channel = QVector3D AuthoredSwordPoseKey::*;
  auto tangent = [&keys](std::size_t index, Channel channel) -> QVector3D {
    if (index == 0U || index + 1U >= keys.size()) {
      return {};
    }
    float const window =
        std::max(0.001F, keys[index + 1U].phase - keys[index - 1U].phase);
    return (keys[index + 1U].*channel - keys[index - 1U].*channel) / window;
  };

  float const t2 = t * t;
  float const t3 = t2 * t;
  float const h00 = (2.0F * t3) - (3.0F * t2) + 1.0F;
  float const h10 = t3 - (2.0F * t2) + t;
  float const h01 = (-2.0F * t3) + (3.0F * t2);
  float const h11 = t3 - t2;
  auto hermite = [&](Channel channel) -> QVector3D {
    return (from.*channel * h00) + (tangent(segment - 1U, channel) * span * h10) +
           (to.*channel * h01) + (tangent(segment, channel) * span * h11);
  };

  bool const terminal_segment = segment == 1U || segment + 1U == keys.size();
  float const blade_t = terminal_segment ? (t * t * (3.0F - 2.0F * t)) : t;

  return {
      .phase = clamped,
      .right_hand = hermite(&AuthoredSwordPoseKey::right_hand),
      .left_hand = hermite(&AuthoredSwordPoseKey::left_hand),
      .blade_dir = slerp_dir(from.blade_dir, to.blade_dir, blade_t),
      .pelvis_delta = hermite(&AuthoredSwordPoseKey::pelvis_delta),
      .shoulder_r_delta = hermite(&AuthoredSwordPoseKey::shoulder_r_delta),
      .shoulder_l_delta = hermite(&AuthoredSwordPoseKey::shoulder_l_delta),
      .neck_delta = hermite(&AuthoredSwordPoseKey::neck_delta),
      .head_delta = hermite(&AuthoredSwordPoseKey::head_delta),
      .foot_r_delta = hermite(&AuthoredSwordPoseKey::foot_r_delta),
      .knee_r_delta = hermite(&AuthoredSwordPoseKey::knee_r_delta),
      .foot_l_delta = hermite(&AuthoredSwordPoseKey::foot_l_delta),
      .knee_l_delta = hermite(&AuthoredSwordPoseKey::knee_l_delta),
  };
}

void apply_authored_rpg_sword_pose(Render::GL::HumanoidPoseController& ctrl,
                                   std::uint8_t variant,
                                   float phase,
                                   Render::GL::HumanoidPose& pose) {
  auto const sample =
      sample_authored_sword_pose_key(rpg_sword_pose_keys(variant), phase);

  pose.pelvis_pos += sample.pelvis_delta;
  pose.shoulder_r += sample.shoulder_r_delta;
  pose.shoulder_l += sample.shoulder_l_delta;
  pose.neck_base += sample.neck_delta;
  pose.head_pos += sample.head_delta;
  pose.foot_r += sample.foot_r_delta;
  pose.knee_r += sample.knee_r_delta;
  pose.foot_l += sample.foot_l_delta;
  pose.knee_l += sample.knee_l_delta;

  ctrl.place_hand_at(Render::GL::Side::Right, sample.right_hand);
  ctrl.place_hand_at(Render::GL::Side::Left, sample.left_hand);
  pose.grip_axis_r = Render::Humanoid::hand_axis_for_weapon_direction(
      sample.blade_dir, Render::GL::baked_sword_direction(), true);
}

void bake_hold_pose(BakeProfile profile,
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
  bool kneels = true;
  if (is_defensive_shield_hold(hold_type)) {
    bool const carthaginian = hold_type == BakerHoldType::CarthageShieldWallFront ||
                              hold_type == BakerHoldType::CarthageShieldWallLeft ||
                              hold_type == BakerHoldType::CarthageShieldWallRight;
    kneels = !carthaginian;
    kneel_depth = hold_type == BakerHoldType::TestudoFront ? 0.70F : 0.95F;
  } else if (profile == BakeProfile::SwordReady) {
    kneel_depth = 0.825F;
  } else if (hold_type == BakerHoldType::Bow) {
    kneel_depth = 1.125F;
  }

  if (kneels) {
    ctrl.kneel(kneel_depth);
  }
  if (is_defensive_shield_hold(hold_type)) {
    ctrl.guard_sword_and_shield_formation(defensive_shield_pose(hold_type), 1.0F);
  } else if (profile == BakeProfile::SwordReady) {
    ctrl.guard_sword_and_shield_for_defense();
  } else if (hold_type == BakerHoldType::Bow) {
    ctrl.hold_bow_ready();
  } else {
    ctrl.brace_spear_for_hold();
  }
}

void bake_death_pose(Animation::HumanoidDeathCollapse collapse,
                     float blend,
                     Render::GL::HumanoidPose& pose) {
  Render::GL::HumanoidAnimationContext anim_ctx{};
  anim_ctx.gait.state = Render::GL::HumanoidMotionState::Idle;
  Render::GL::HumanoidPoseController ctrl(pose, anim_ctx);
  ctrl.apply_death_collapse(collapse, std::clamp(blend, 0.0F, 1.0F));
}

[[nodiscard]] auto
to_ambient_idle_type(BakerAmbientIdleType t) noexcept -> Render::GL::AmbientIdleType {
  switch (t) {
  case BakerAmbientIdleType::SitDown:
    return Render::GL::AmbientIdleType::SitDown;
  case BakerAmbientIdleType::Jump:
    return Render::GL::AmbientIdleType::Jump;
  case BakerAmbientIdleType::RaiseWeapon:
    return Render::GL::AmbientIdleType::RaiseWeapon;
  case BakerAmbientIdleType::ShiftWeight:
    return Render::GL::AmbientIdleType::ShiftWeight;
  case BakerAmbientIdleType::PlantFlag:
    return Render::GL::AmbientIdleType::PlantFlag;
  case BakerAmbientIdleType::None:
    break;
  }
  return Render::GL::AmbientIdleType::None;
}

void apply_ground_stance_for_profile(Render::GL::HumanoidPoseController& ctrl,
                                     BakeProfile profile) {
  switch (profile) {
  case BakeProfile::SwordReady:
    ctrl.carry_sword_and_shield();
    break;
  case BakeProfile::SpearReady:
    ctrl.hold_spear_idle();
    break;
  case BakeProfile::Caster:
    ctrl.channel_spell_idle();
    break;
  case BakeProfile::StaveCaster:
    ctrl.carry_stave();
    break;
  case BakeProfile::Default:
  case BakeProfile::Skeleton:
    break;
  }
}

void bake_humanoid_clip_frame(BakeProfile profile,
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

  if (clip.death_collapse != Animation::HumanoidDeathCollapse::None) {

    Render::GL::HumanoidGaitDescriptor gait{};
    gait.state = Render::GL::HumanoidMotionState::Idle;
    gait.cycle_time = 1.6F;
    gait.cycle_phase = 0.0F;
    gait.speed = 0.0F;
    gait.normalized_speed = 0.0F;
    Render::GL::HumanoidRendererBase::compute_locomotion_pose(
        0U, 0.0F, gait, variation, pose);

    float const death_blend = clip.loops ? 1.0F : phase;
    bake_death_pose(clip.death_collapse, death_blend, pose);
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
    float const sword_reach_scale = profile == BakeProfile::Skeleton ? 0.88F : 1.0F;
    switch (clip.attack_type) {
    case BakerAttackType::Unarmed:
      ctrl.unarmed_strike(phase, clip.attack_variant);
      break;
    case BakerAttackType::Sword:
      if (is_rpg_sword_clip(clip)) {
        apply_authored_rpg_sword_pose(ctrl, clip.attack_variant, phase, pose);
      } else if (profile == BakeProfile::SwordReady ||
                 profile == BakeProfile::Skeleton) {
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
    if (profile == BakeProfile::SwordReady &&
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
    if (clip.riding_type == BakerRidingType::Idle) {

      Render::GL::HumanoidPoseController breath_ctrl(pose, anim_ctx_r);
      breath_ctrl.apply_idle_breath(phase, true);
    }
  } else {
    Render::GL::HumanoidGaitDescriptor gait{};
    gait.state = clip.state;
    gait.cycle_time = clip.cycle_time;

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

    if (clip.showcase_type != BakerShowcaseType::None) {
      gait.cycle_phase = 0.0F;
      Render::GL::HumanoidRendererBase::compute_locomotion_pose(
          0U, 0.0F, gait, variation, pose);
      Render::GL::HumanoidAnimationContext anim_ctx{};
      anim_ctx.gait = gait;
      anim_ctx.gait.state = Render::GL::HumanoidMotionState::Idle;
      Render::GL::HumanoidPoseController ctrl(pose, anim_ctx);
      ctrl.apply_showcase_move(to_showcase_move(clip.showcase_type),
                               transition_phase(frame_index, clip.frames));
    } else if (clip.hold_type != BakerHoldType::None) {
      gait.cycle_phase = 0.0F;
      Render::GL::HumanoidRendererBase::compute_locomotion_pose(
          0U, 0.0F, gait, variation, pose);
      bake_hold_pose(profile, clip.hold_type, phase, pose);
    } else if (clip.ambient_idle_type != BakerAmbientIdleType::None) {
      gait.cycle_phase = 0.0F;
      Render::GL::HumanoidRendererBase::compute_locomotion_pose(
          0U, 0.0F, gait, variation, pose);
      float const ambient_phase = transition_phase(frame_index, clip.frames);
      Render::GL::HumanoidAnimationContext anim_ctx{};
      anim_ctx.gait = gait;
      anim_ctx.gait.state = Render::GL::HumanoidMotionState::Idle;
      Render::GL::HumanoidPoseController ctrl(pose, anim_ctx);

      apply_ground_stance_for_profile(ctrl, profile);

      ctrl.apply_idle_breath(ambient_phase * clip.cycle_time /
                                 Animation::k_humanoid_idle_breath_cycle_time,
                             false);
      ctrl.apply_ambient_idle_explicit(to_ambient_idle_type(clip.ambient_idle_type),
                                       ambient_phase);
    } else {
      gait.cycle_phase = phase;
      Render::GL::HumanoidRendererBase::compute_locomotion_pose(
          0U, phase * clip.cycle_time, gait, variation, pose);
      Render::GL::HumanoidAnimationContext anim_ctx{};
      anim_ctx.gait = gait;
      anim_ctx.gait.state = clip.state;
      anim_ctx.inputs.movement_state =
          (gait.speed > 0.1F) ? Render::Creature::MovementAnimationState::Walk
                              : Render::Creature::MovementAnimationState::Idle;
      Render::GL::HumanoidPoseController ctrl(pose, anim_ctx);
      apply_ground_stance_for_profile(ctrl, profile);
      if (clip.state == Render::GL::HumanoidMotionState::Idle) {
        ctrl.apply_idle_breath(phase, false);
      }
    }
  }

  if (clip.showcase_type != BakerShowcaseType::None || is_rpg_sword_clip(clip) ||
      clip.attack_type == BakerAttackType::Unarmed ||
      clip.attack_type == BakerAttackType::Sword ||
      clip.attack_type == BakerAttackType::BowMelee ||
      clip.attack_type == BakerAttackType::Spear ||
      clip.attack_type == BakerAttackType::SpearFromHold ||
      clip.attack_type == BakerAttackType::Bow ||
      clip.attack_type == BakerAttackType::BowFromHold ||
      clip.hold_type == BakerHoldType::Spear || clip.hold_type == BakerHoldType::Bow ||
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

auto bind_palette_provider() noexcept -> std::span<const QMatrix4x4> {
  return humanoid_bind_palette();
}

auto clip_descriptors() noexcept
    -> std::span<const Render::Creature::BakeClipDescriptor> {
  static const auto descriptors = [] {
    std::array<Render::Creature::BakeClipDescriptor, k_humanoid_clips.size()> out{};
    for (std::size_t i = 0; i < k_humanoid_clips.size(); ++i) {
      out[i].name = k_humanoid_clips[i].name;
      out[i].frame_count = k_humanoid_clips[i].frames;
      out[i].fps = k_humanoid_clips[i].fps;
      out[i].loops = k_humanoid_clips[i].loops;
    }
    return out;
  }();
  return descriptors;
}

auto socket_descriptors() noexcept
    -> std::span<const Render::Creature::BakeSocketDescriptor> {
  static const auto descriptors = [] {
    std::array<Render::Creature::BakeSocketDescriptor, k_humanoid_sockets.size()> out{};
    for (std::size_t i = 0; i < k_humanoid_sockets.size(); ++i) {
      SocketDef const& def = socket_def(k_humanoid_sockets[i].socket);
      out[i].name = k_humanoid_sockets[i].name;
      out[i].anchor_bone = static_cast<std::uint32_t>(def.bone);
      out[i].local_offset = def.local_offset;
    }
    return out;
  }();
  return descriptors;
}

struct ProfileBinding {
  BakeProfile profile;
  std::uint32_t species_id;
  std::string_view species_name;
  std::string_view bpat_file_name;
  Render::Creature::BakeClipFrameFn bake_clip_frame;
  Render::Creature::BakeClipMarkersFn clip_markers;
  Render::Creature::CreatureSpecProviderFn creature_spec;
};

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

auto profile_bindings() noexcept -> std::span<const ProfileBinding> {
  namespace bpat = Render::Creature::Bpat;
  static const std::array<ProfileBinding, 6> bindings{{
      make_binding<BakeProfile::Default>(bpat::k_species_humanoid,
                                         "humanoid",
                                         "humanoid.bpat",
                                         &humanoid_creature_spec),
      make_binding<BakeProfile::SwordReady>(bpat::k_species_humanoid_sword,
                                            "humanoid.sword_ready",
                                            "humanoid_sword.bpat",
                                            &humanoid_creature_spec),
      make_binding<BakeProfile::SpearReady>(bpat::k_species_humanoid_spear,
                                            "humanoid.spear_ready",
                                            "humanoid_spear.bpat",
                                            &humanoid_creature_spec),
      make_binding<BakeProfile::Skeleton>(bpat::k_species_humanoid_skeleton,
                                          "humanoid.skeleton",
                                          "humanoid_skeleton.bpat",
                                          &skeleton_humanoid_creature_spec),
      make_binding<BakeProfile::Caster>(bpat::k_species_humanoid_caster,
                                        "humanoid.caster",
                                        "humanoid_caster.bpat",
                                        &humanoid_creature_spec),
      make_binding<BakeProfile::StaveCaster>(bpat::k_species_humanoid_stave_caster,
                                             "humanoid.stave_caster",
                                             "humanoid_stave_caster.bpat",
                                             &humanoid_creature_spec),
  }};
  return bindings;
}

auto build_manifest(const ProfileBinding& binding)
    -> Render::Creature::SpeciesManifest {
  Render::Creature::SpeciesManifest m;
  m.species_name = binding.species_name;
  m.species_id = binding.species_id;
  m.bpat_file_name = binding.bpat_file_name;

  m.minimal_snapshot_file_name = {};
  m.topology = &binding.creature_spec().topology;
  m.clips = clip_descriptors();
  m.sockets = socket_descriptors();
  m.bind_palette = &bind_palette_provider;
  m.creature_spec = binding.creature_spec;
  m.bake_clip_frame = binding.bake_clip_frame;
  m.clip_markers = binding.clip_markers;
  return m;
}

} // namespace

auto humanoid_manifest(BakeProfile profile) noexcept
    -> const Render::Creature::SpeciesManifest& {
  static const auto manifests = [] {
    auto const bindings = profile_bindings();
    std::array<Render::Creature::SpeciesManifest, 6> out{};
    for (std::size_t i = 0; i < bindings.size(); ++i) {
      out[i] = build_manifest(bindings[i]);
    }
    return out;
  }();
  return manifests[static_cast<std::size_t>(profile)];
}

auto humanoid_bake_profiles() noexcept -> std::span<const BakeProfile> {
  static constexpr std::array<BakeProfile, 6> k_profiles{{
      BakeProfile::Default,
      BakeProfile::SwordReady,
      BakeProfile::SpearReady,
      BakeProfile::Skeleton,
      BakeProfile::Caster,
      BakeProfile::StaveCaster,
  }};
  return k_profiles;
}

} // namespace Render::Humanoid
