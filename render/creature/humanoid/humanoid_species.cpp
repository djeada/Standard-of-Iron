#include "render/creature/humanoid/humanoid_species.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "animation/bpat/bpat_format.h"
#include "render/creature/humanoid/asset/humanoid_spec.h"
#include "render/creature/humanoid/humanoid_types.h"
#include "render/creature/humanoid/runtime/humanoid_pose_diagnostics.h"
#include "render/creature/humanoid/schema/blend_profile.h"
#include "render/creature/humanoid/schema/skeleton_schema.h"

namespace Render::Humanoid {

namespace {

namespace RCP = Render::Creature::Pipeline;

auto humanoid_bind() noexcept -> std::span<const QMatrix4x4> {
  return Render::Humanoid::humanoid_bind_palette();
}

auto humanoid_fill_roles(const void* variant,
                         QVector3D* out,
                         std::size_t max_roles) -> std::uint32_t {
  const auto& v = *static_cast<const Render::GL::HumanoidVariant*>(variant);
  std::array<QVector3D, Render::Humanoid::k_humanoid_role_count> filled{};
  Render::Humanoid::fill_humanoid_role_colors(v, filled);
  const auto n = std::min(filled.size(), max_roles);
  for (std::size_t i = 0; i < n; ++i) {
    out[i] = filled[i];
  }
  return static_cast<std::uint32_t>(n);
}

void describe_humanoid_assets(std::span<RCP::CreatureAsset> assets) {
  RCP::CreatureAsset humanoid{};
  humanoid.id = RCP::k_humanoid_asset;
  humanoid.debug_name = "humanoid.v1";
  humanoid.kind = RCP::CreatureKind::Humanoid;
  humanoid.bpat_species_id = Render::Creature::Bpat::k_species_humanoid;
  humanoid.spec = &Render::Humanoid::humanoid_creature_spec();
  humanoid.body_variant_spec =
      &Render::Humanoid::humanoid_creature_spec_for_body_variant;
  humanoid.topology = &humanoid.spec->topology;
  humanoid.blend_profile = &Render::Humanoid::k_humanoid_blend_profile;
  humanoid.body_pose_probe = &Render::Humanoid::record_humanoid_body_pose;
  humanoid.role_count =
      static_cast<std::uint8_t>(Render::Humanoid::k_humanoid_role_count);
  humanoid.max_bones = static_cast<std::uint8_t>(Render::Humanoid::k_bone_count);
  humanoid.bind_palette = &humanoid_bind;
  humanoid.fill_role_colors = &humanoid_fill_roles;
  assets[RCP::k_humanoid_asset] = humanoid;

  RCP::CreatureAsset sword{};
  sword.id = RCP::k_humanoid_sword_asset;
  sword.debug_name = "humanoid.sword_ready.v1";
  sword.kind = RCP::CreatureKind::Humanoid;
  sword.bpat_species_id = Render::Creature::Bpat::k_species_humanoid_sword;
  sword.spec = &Render::Humanoid::humanoid_creature_spec();
  sword.body_variant_spec = &Render::Humanoid::humanoid_creature_spec_for_body_variant;
  sword.topology = &sword.spec->topology;
  sword.blend_profile = &Render::Humanoid::k_humanoid_blend_profile;
  sword.body_pose_probe = &Render::Humanoid::record_humanoid_body_pose;
  sword.role_count = static_cast<std::uint8_t>(Render::Humanoid::k_humanoid_role_count);
  sword.max_bones = static_cast<std::uint8_t>(Render::Humanoid::k_bone_count);
  sword.bind_palette = &humanoid_bind;
  sword.fill_role_colors = &humanoid_fill_roles;
  assets[RCP::k_humanoid_sword_asset] = sword;

  RCP::CreatureAsset spear{};
  spear.id = RCP::k_humanoid_spear_asset;
  spear.debug_name = "humanoid.spear_ready.v1";
  spear.kind = RCP::CreatureKind::Humanoid;
  spear.bpat_species_id = Render::Creature::Bpat::k_species_humanoid_spear;
  spear.spec = &Render::Humanoid::humanoid_creature_spec();
  spear.body_variant_spec = &Render::Humanoid::humanoid_creature_spec_for_body_variant;
  spear.topology = &spear.spec->topology;
  spear.blend_profile = &Render::Humanoid::k_humanoid_blend_profile;
  spear.body_pose_probe = &Render::Humanoid::record_humanoid_body_pose;
  spear.role_count = static_cast<std::uint8_t>(Render::Humanoid::k_humanoid_role_count);
  spear.max_bones = static_cast<std::uint8_t>(Render::Humanoid::k_bone_count);
  spear.bind_palette = &humanoid_bind;
  spear.fill_role_colors = &humanoid_fill_roles;
  assets[RCP::k_humanoid_spear_asset] = spear;

  RCP::CreatureAsset skeleton{};
  skeleton.id = RCP::k_skeleton_humanoid_asset;
  skeleton.debug_name = "humanoid.skeleton.v1";
  skeleton.kind = RCP::CreatureKind::Humanoid;
  skeleton.bpat_species_id = Render::Creature::Bpat::k_species_humanoid_skeleton;
  skeleton.spec = &Render::Humanoid::skeleton_humanoid_creature_spec();
  skeleton.topology = &skeleton.spec->topology;
  skeleton.blend_profile = &Render::Humanoid::k_humanoid_blend_profile;
  skeleton.body_pose_probe = &Render::Humanoid::record_humanoid_body_pose;
  skeleton.role_count =
      static_cast<std::uint8_t>(Render::Humanoid::k_humanoid_role_count);
  skeleton.max_bones = static_cast<std::uint8_t>(Render::Humanoid::k_bone_count);
  skeleton.bind_palette = &humanoid_bind;
  skeleton.fill_role_colors = &humanoid_fill_roles;
  assets[RCP::k_skeleton_humanoid_asset] = skeleton;

  RCP::CreatureAsset caster = humanoid;
  caster.id = RCP::k_caster_humanoid_asset;
  caster.debug_name = "humanoid.caster.v1";
  caster.bpat_species_id = Render::Creature::Bpat::k_species_humanoid_caster;
  assets[RCP::k_caster_humanoid_asset] = caster;

  RCP::CreatureAsset stave_caster = humanoid;
  stave_caster.id = RCP::k_stave_caster_humanoid_asset;
  stave_caster.debug_name = "humanoid.stave_caster.v1";
  stave_caster.bpat_species_id =
      Render::Creature::Bpat::k_species_humanoid_stave_caster;
  assets[RCP::k_stave_caster_humanoid_asset] = stave_caster;
}

constexpr std::array<std::size_t, 2> k_humanoid_foot_bones{
    static_cast<std::size_t>(HumanoidBone::FootL),
    static_cast<std::size_t>(HumanoidBone::FootR)};

} // namespace

auto humanoid_species_contract() noexcept -> const RCP::SpeciesContract& {
  static const RCP::SpeciesContract contract{
      .kind = RCP::CreatureKind::Humanoid,
      .role_count = static_cast<std::uint8_t>(k_humanoid_role_count),
      .material_id = 0,
      .bind_palette = &humanoid_bind,
      .foot_bones = k_humanoid_foot_bones,
      .describe_assets = &describe_humanoid_assets,
      .default_asset = RCP::k_humanoid_asset,
  };
  return contract;
}

} // namespace Render::Humanoid
