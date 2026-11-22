#include "render/creature/elephant/elephant_species.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "animation/bpat/bpat_format.h"
#include "render/creature/elephant/dimensions.h"
#include "render/creature/elephant/elephant_spec.h"
#include "render/creature/pipeline/creature_visual_definition.h"
#include "render/creature/spec.h"

namespace Render::Creature::Pipeline {

namespace {

auto elephant_part_graph(Render::Creature::CreatureLOD lod) noexcept
    -> const Render::Creature::PartGraph& {
  return Render::Creature::part_graph_for(Render::Elephant::elephant_creature_spec(),
                                          lod);
}

const CreatureMeshRecipe k_elephant_mesh_recipe{
    "elephant.procedural.low_poly",
    mesh_lod_mask({CreatureMeshLod::Full, CreatureMeshLod::Minimal}),
    &Render::Elephant::elephant_creature_spec,
    &elephant_part_graph,
    {1, 1},
};

const CreatureRigDefinition k_elephant_rig_definition{
    "elephant.runtime_rig",
    static_cast<std::uint8_t>(Render::Elephant::k_elephant_bone_count),
    &Render::Elephant::elephant_bind_palette,
    &Render::Elephant::elephant_topology(),
    {static_cast<std::uint8_t>(Render::Elephant::ElephantBone::FootFL),
     static_cast<std::uint8_t>(Render::Elephant::ElephantBone::FootFR),
     static_cast<std::uint8_t>(Render::Elephant::ElephantBone::FootBL),
     static_cast<std::uint8_t>(Render::Elephant::ElephantBone::FootBR)},
    static_cast<std::uint8_t>(Render::Elephant::ElephantBone::Body),
};

const CreatureGroundingModel k_elephant_grounding_model{
    "elephant.foot_grounding",
    static_cast<std::uint8_t>(k_large_creature_leg_count),
    0.70F,
    0.75F,
};

const CreatureMotionController k_elephant_motion_controller{
    "elephant.gait_controller",
    CreatureLocomotionMode::Idle,
};

const CreatureAttachmentFrameExtractor k_elephant_attachment_frame_extractor{
    "elephant.howdah_frames",
};

const CreatureLodStrategy k_elephant_lod_strategy{
    "elephant.large_creature_lod",
    k_elephant_mesh_recipe.lod_mask,
};

const CreatureVisualDefinition k_elephant_visual_definition{
    "elephant.large_creature.v1",
    CreatureKind::Elephant,
    &k_elephant_mesh_recipe,
    &k_elephant_rig_definition,
    &k_elephant_motion_controller,
    &k_elephant_grounding_model,
    &k_elephant_attachment_frame_extractor,
    &k_elephant_lod_strategy,
    static_cast<std::uint8_t>(Render::Elephant::k_elephant_role_count),
};

} // namespace

auto elephant_creature_visual_definition() noexcept -> const CreatureVisualDefinition& {
  return k_elephant_visual_definition;
}

} // namespace Render::Creature::Pipeline

namespace Render::Elephant {

namespace {

namespace RCP = Render::Creature::Pipeline;

auto elephant_bind() noexcept -> std::span<const QMatrix4x4> {
  return Render::Elephant::elephant_bind_palette();
}

auto elephant_fill_roles(const void* variant,
                         QVector3D* out,
                         std::size_t max_roles) -> std::uint32_t {
  const auto& v = *static_cast<const Render::GL::ElephantVariant*>(variant);
  std::array<QVector3D, Render::Elephant::k_elephant_role_count> filled{};
  Render::Elephant::fill_elephant_role_colors(v, filled);
  const auto n = std::min(filled.size(), max_roles);
  for (std::size_t i = 0; i < n; ++i) {
    out[i] = filled[i];
  }
  return static_cast<std::uint32_t>(n);
}

void describe_elephant_assets(std::span<RCP::CreatureAsset> assets) {
  RCP::CreatureAsset elephant{};
  elephant.id = RCP::k_elephant_asset;
  elephant.debug_name = "elephant.v1";
  elephant.kind = RCP::CreatureKind::Elephant;
  elephant.bpat_species_id = Render::Creature::Bpat::k_species_elephant;
  elephant.spec = &Render::Elephant::elephant_creature_spec();
  elephant.topology = &elephant.spec->topology;
  elephant.role_count =
      static_cast<std::uint8_t>(Render::Elephant::k_elephant_role_count);
  elephant.max_bones =
      static_cast<std::uint8_t>(Render::Elephant::k_elephant_bone_count);
  elephant.bind_palette = &elephant_bind;
  elephant.fill_role_colors = &elephant_fill_roles;
  elephant.snapshot_mesh_species_id = Render::Creature::Bpat::k_species_elephant;
  elephant.snapshot_mesh_lod_mask = static_cast<std::uint8_t>(
      1U << static_cast<std::uint8_t>(Render::Creature::CreatureLOD::Minimal));
  elephant.visual_definition = &RCP::elephant_creature_visual_definition();
  assets[RCP::k_elephant_asset] = elephant;
}

constexpr std::array<std::size_t, 4> k_elephant_foot_bones{
    static_cast<std::size_t>(ElephantBone::FootFL),
    static_cast<std::size_t>(ElephantBone::FootFR),
    static_cast<std::size_t>(ElephantBone::FootBL),
    static_cast<std::size_t>(ElephantBone::FootBR)};

constexpr auto leg_bones(ElephantBone shoulder,
                         ElephantBone knee,
                         ElephantBone foot) noexcept -> RCP::WholeMeshLegBones {
  return {static_cast<std::uint8_t>(shoulder),
          static_cast<std::uint8_t>(knee),
          static_cast<std::uint8_t>(foot)};
}

constexpr RCP::WholeMeshBones k_elephant_whole_mesh_bones{
    .root = static_cast<std::uint8_t>(ElephantBone::Root),
    .neck = static_cast<std::uint8_t>(ElephantBone::Head),
    .head = static_cast<std::uint8_t>(ElephantBone::Head),
    .trunk_tip = static_cast<std::uint8_t>(ElephantBone::TrunkTip),
    .legs = {{leg_bones(
                  ElephantBone::ShoulderFL, ElephantBone::KneeFL, ElephantBone::FootFL),
              leg_bones(
                  ElephantBone::ShoulderFR, ElephantBone::KneeFR, ElephantBone::FootFR),
              leg_bones(
                  ElephantBone::ShoulderBL, ElephantBone::KneeBL, ElephantBone::FootBL),
              leg_bones(ElephantBone::ShoulderBR,
                        ElephantBone::KneeBR,
                        ElephantBone::FootBR)}},
};

} // namespace

auto elephant_species_contract() noexcept -> const RCP::SpeciesContract& {
  static const RCP::SpeciesContract contract{
      .kind = RCP::CreatureKind::Elephant,
      .role_count = static_cast<std::uint8_t>(k_elephant_role_count),
      .material_id = k_elephant_material_id,
      .bind_palette = &elephant_bind,
      .foot_bones = k_elephant_foot_bones,
      .describe_assets = &describe_elephant_assets,
      .default_asset = RCP::k_elephant_asset,
      .initialize_lod_geometry = &initialize_elephant_asset,
      .whole_mesh_bones = &k_elephant_whole_mesh_bones,
  };
  return contract;
}

} // namespace Render::Elephant
