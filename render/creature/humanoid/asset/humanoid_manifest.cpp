#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "render/creature/humanoid/asset/humanoid_manifest_internal.h"

namespace Render::Humanoid {

namespace humanoid_manifest_detail {

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
  case BakerHoldType::ResourceCarry:
  case BakerHoldType::Wade:
    break;
  }
  return Animation::ShieldFormationPose::RomanFront;
}

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
  case BakerShowcaseType::GestureTalk:
    return Animation::HumanoidShowcaseMove::GestureTalk;
  case BakerShowcaseType::GesturePoint:
    return Animation::HumanoidShowcaseMove::GesturePoint;
  case BakerShowcaseType::GestureShrug:
    return Animation::HumanoidShowcaseMove::GestureShrug;
  case BakerShowcaseType::GestureNod:
    return Animation::HumanoidShowcaseMove::GestureNod;
  case BakerShowcaseType::GestureCrossed:
    return Animation::HumanoidShowcaseMove::GestureCrossed;
  case BakerShowcaseType::GestureSalute:
    return Animation::HumanoidShowcaseMove::GestureSalute;
  case BakerShowcaseType::GestureChin:
    return Animation::HumanoidShowcaseMove::GestureChin;
  case BakerShowcaseType::GestureOpen:
    return Animation::HumanoidShowcaseMove::GestureOpen;
  case BakerShowcaseType::GestureRead:
    return Animation::HumanoidShowcaseMove::GestureRead;
  case BakerShowcaseType::GesturePresent:
    return Animation::HumanoidShowcaseMove::GesturePresent;
  case BakerShowcaseType::GestureRecoil:
    return Animation::HumanoidShowcaseMove::GestureRecoil;
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

[[nodiscard]] auto is_rpg_sword_clip(const HumanoidClipSpec& clip) noexcept -> bool {
  return std::string_view{clip.name}.starts_with("rpg_sword_");
}

[[nodiscard]] auto is_rpg_spear_clip(const HumanoidClipSpec& clip) noexcept -> bool {
  return std::string_view{clip.name}.starts_with("rpg_spear_");
}

[[nodiscard]] auto commander_spear_move_for_variant(std::uint8_t variant) noexcept
    -> Animation::CommanderSpearMove {
  switch (variant) {
  case 1U:
    return Animation::CommanderSpearMove::Sweep;
  case 2U:
    return Animation::CommanderSpearMove::Launcher;
  case 3U:
    return Animation::CommanderSpearMove::Finisher;
  default:
    return Animation::CommanderSpearMove::Thrust;
  }
}

[[nodiscard]] auto is_ranged_attack_type(BakerAttackType type) noexcept -> bool {
  return type == BakerAttackType::Bow || type == BakerAttackType::BowFromHold ||
         type == BakerAttackType::Sling || type == BakerAttackType::Javelin;
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

auto build_runtime_manifest(const ProfileBinding& binding)
    -> Render::Creature::CreatureRuntimeManifest {
  Render::Creature::CreatureRuntimeManifest m;
  m.species_name = binding.species_name;
  m.species_id = binding.species_id;
  m.bpat_file_name = binding.bpat_file_name;

  m.minimal_snapshot_file_name = {};
  m.topology = &binding.creature_spec().topology;
  m.bind_palette = &bind_palette_provider;
  m.creature_spec = binding.creature_spec;
  return m;
}

auto runtime_manifests() noexcept
    -> const std::array<Render::Creature::CreatureRuntimeManifest, 6>& {
  static const auto manifests = [] {
    auto const bindings = profile_bindings();
    std::array<Render::Creature::CreatureRuntimeManifest, 6> out{};
    for (std::size_t i = 0; i < bindings.size(); ++i) {
      out[i] = build_runtime_manifest(bindings[i]);
    }
    return out;
  }();
  return manifests;
}

} // namespace humanoid_manifest_detail

using namespace humanoid_manifest_detail;

auto humanoid_runtime_manifest(BakeProfile profile) noexcept
    -> const Render::Creature::CreatureRuntimeManifest& {
  return runtime_manifests()[static_cast<std::size_t>(profile)];
}

auto humanoid_bake_recipe(BakeProfile profile) noexcept
    -> const Render::Creature::CreatureBakeRecipe& {
  static const auto recipes = [] {
    auto const bindings = profile_bindings();
    std::array<Render::Creature::CreatureBakeRecipe, 6> out{};
    for (std::size_t i = 0; i < bindings.size(); ++i) {
      out[i].runtime = &runtime_manifests()[i];
      out[i].clips = clip_descriptors();
      out[i].sockets = socket_descriptors();
      out[i].bake_clip_frame = bindings[i].bake_clip_frame;
      out[i].clip_markers = bindings[i].clip_markers;
    }
    return out;
  }();
  return recipes[static_cast<std::size_t>(profile)];
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
