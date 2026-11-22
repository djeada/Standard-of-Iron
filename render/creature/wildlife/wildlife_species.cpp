#include "render/creature/wildlife/wildlife_species.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "animation/bpat/bpat_format.h"
#include "render/creature/pipeline/creature_visual_definition.h"
#include "render/creature/spec.h"
#include "render/creature/wildlife/sheep_spec.h"
#include "render/creature/wildlife/wildlife_rig.h"
#include "render/creature/wildlife/wildlife_variant.h"
#include "render/creature/wildlife/wolf_spec.h"

namespace Render::Creature::Pipeline {

namespace {

auto sheep_part_graph(Render::Creature::CreatureLOD lod) noexcept
    -> const Render::Creature::PartGraph& {
  return Render::Creature::part_graph_for(Render::Wildlife::sheep_creature_spec(), lod);
}

auto wolf_part_graph(Render::Creature::CreatureLOD lod) noexcept
    -> const Render::Creature::PartGraph& {
  return Render::Creature::part_graph_for(Render::Wildlife::wolf_creature_spec(), lod);
}

const CreatureMeshRecipe k_sheep_mesh_recipe{
    "sheep.procedural.low_poly",
    mesh_lod_mask({CreatureMeshLod::Full, CreatureMeshLod::Minimal}),
    &Render::Wildlife::sheep_creature_spec,
    &sheep_part_graph,
    {1, 1},
};

const CreatureRigDefinition k_sheep_rig_definition{
    "sheep.baked_rig",
    static_cast<std::uint8_t>(Render::Wildlife::k_bone_count),
    &Render::Wildlife::sheep_bind_palette,
    &Render::Wildlife::wildlife_topology(),
    {static_cast<std::uint8_t>(Render::Wildlife::Bone::FootFL),
     static_cast<std::uint8_t>(Render::Wildlife::Bone::FootFR),
     static_cast<std::uint8_t>(Render::Wildlife::Bone::FootBL),
     static_cast<std::uint8_t>(Render::Wildlife::Bone::FootBR)},
    static_cast<std::uint8_t>(Render::Wildlife::Bone::Body),
};

const CreatureGroundingModel k_wildlife_grounding_model{
    "wildlife.foot_grounding",
    4U,
    0.55F,
    0.60F,
};

const CreatureMotionController k_wildlife_motion_controller{
    "wildlife.gait_controller",
    CreatureLocomotionMode::Idle,
};

const CreatureAttachmentFrameExtractor k_wildlife_attachment_frame_extractor{
    "wildlife.no_attachments",
};

const CreatureLodStrategy k_sheep_lod_strategy{
    "sheep.ambient_lod",
    k_sheep_mesh_recipe.lod_mask,
};

const CreatureVisualDefinition k_sheep_visual_definition{
    "sheep.ambient_quadruped.v1",
    CreatureKind::Sheep,
    &k_sheep_mesh_recipe,
    &k_sheep_rig_definition,
    &k_wildlife_motion_controller,
    &k_wildlife_grounding_model,
    &k_wildlife_attachment_frame_extractor,
    &k_sheep_lod_strategy,
    static_cast<std::uint8_t>(Render::Wildlife::k_sheep_role_count),
};

const CreatureMeshRecipe k_wolf_mesh_recipe{
    "wolf.procedural.low_poly",
    mesh_lod_mask({CreatureMeshLod::Full, CreatureMeshLod::Minimal}),
    &Render::Wildlife::wolf_creature_spec,
    &wolf_part_graph,
    {1, 1},
};

const CreatureRigDefinition k_wolf_rig_definition{
    "wolf.baked_rig",
    static_cast<std::uint8_t>(Render::Wildlife::k_bone_count),
    &Render::Wildlife::wolf_bind_palette,
    &Render::Wildlife::wildlife_topology(),
    {static_cast<std::uint8_t>(Render::Wildlife::Bone::FootFL),
     static_cast<std::uint8_t>(Render::Wildlife::Bone::FootFR),
     static_cast<std::uint8_t>(Render::Wildlife::Bone::FootBL),
     static_cast<std::uint8_t>(Render::Wildlife::Bone::FootBR)},
    static_cast<std::uint8_t>(Render::Wildlife::Bone::Body),
};

const CreatureLodStrategy k_wolf_lod_strategy{
    "wolf.ambient_lod",
    k_wolf_mesh_recipe.lod_mask,
};

const CreatureVisualDefinition k_wolf_visual_definition{
    "wolf.ambient_quadruped.v1",
    CreatureKind::Wolf,
    &k_wolf_mesh_recipe,
    &k_wolf_rig_definition,
    &k_wildlife_motion_controller,
    &k_wildlife_grounding_model,
    &k_wildlife_attachment_frame_extractor,
    &k_wolf_lod_strategy,
    static_cast<std::uint8_t>(Render::Wildlife::k_wolf_role_count),
};

} // namespace

auto sheep_creature_visual_definition() noexcept -> const CreatureVisualDefinition& {
  return k_sheep_visual_definition;
}

auto wolf_creature_visual_definition() noexcept -> const CreatureVisualDefinition& {
  return k_wolf_visual_definition;
}

} // namespace Render::Creature::Pipeline

namespace Render::Wildlife {

namespace {

namespace RCP = Render::Creature::Pipeline;

auto sheep_bind() noexcept -> std::span<const QMatrix4x4> {
  return Render::Wildlife::sheep_bind_palette();
}

auto wolf_bind() noexcept -> std::span<const QMatrix4x4> {
  return Render::Wildlife::wolf_bind_palette();
}

auto wildlife_fill_roles(const void* variant,
                         QVector3D* out,
                         std::size_t max_roles) -> std::uint32_t {
  const auto& v = *static_cast<const Render::GL::WildlifeVariant*>(variant);
  const auto n = std::min(static_cast<std::size_t>(v.role_count), max_roles);
  for (std::size_t i = 0; i < n; ++i) {
    out[i] = v.roles[i];
  }
  return static_cast<std::uint32_t>(n);
}

void describe_sheep_assets(std::span<RCP::CreatureAsset> assets) {
  RCP::CreatureAsset sheep{};
  sheep.id = RCP::k_sheep_asset;
  sheep.debug_name = "sheep.v1";
  sheep.kind = RCP::CreatureKind::Sheep;
  sheep.bpat_species_id = Render::Creature::Bpat::k_species_sheep;
  sheep.spec = &Render::Wildlife::sheep_creature_spec();
  sheep.topology = &sheep.spec->topology;
  sheep.role_count = static_cast<std::uint8_t>(Render::Wildlife::k_sheep_role_count);
  sheep.max_bones = static_cast<std::uint8_t>(Render::Wildlife::k_bone_count);
  sheep.bind_palette = &sheep_bind;
  sheep.fill_role_colors = &wildlife_fill_roles;
  sheep.snapshot_mesh_species_id = Render::Creature::Bpat::k_species_sheep;
  sheep.snapshot_mesh_lod_mask = static_cast<std::uint8_t>(
      1U << static_cast<std::uint8_t>(Render::Creature::CreatureLOD::Minimal));
  sheep.visual_definition = &RCP::sheep_creature_visual_definition();
  assets[RCP::k_sheep_asset] = sheep;
}

void describe_wolf_assets(std::span<RCP::CreatureAsset> assets) {
  RCP::CreatureAsset wolf{};
  wolf.id = RCP::k_wolf_asset;
  wolf.debug_name = "wolf.v1";
  wolf.kind = RCP::CreatureKind::Wolf;
  wolf.bpat_species_id = Render::Creature::Bpat::k_species_wolf;
  wolf.spec = &Render::Wildlife::wolf_creature_spec();
  wolf.topology = &wolf.spec->topology;
  wolf.role_count = static_cast<std::uint8_t>(Render::Wildlife::k_wolf_role_count);
  wolf.max_bones = static_cast<std::uint8_t>(Render::Wildlife::k_bone_count);
  wolf.bind_palette = &wolf_bind;
  wolf.fill_role_colors = &wildlife_fill_roles;
  wolf.snapshot_mesh_species_id = Render::Creature::Bpat::k_species_wolf;
  wolf.snapshot_mesh_lod_mask = static_cast<std::uint8_t>(
      1U << static_cast<std::uint8_t>(Render::Creature::CreatureLOD::Minimal));
  wolf.visual_definition = &RCP::wolf_creature_visual_definition();
  assets[RCP::k_wolf_asset] = wolf;
}

constexpr std::array<std::size_t, 4> k_wildlife_foot_bones{
    static_cast<std::size_t>(Bone::FootFL),
    static_cast<std::size_t>(Bone::FootFR),
    static_cast<std::size_t>(Bone::FootBL),
    static_cast<std::size_t>(Bone::FootBR)};

} // namespace

auto sheep_species_contract() noexcept -> const RCP::SpeciesContract& {
  static const RCP::SpeciesContract contract{
      .kind = RCP::CreatureKind::Sheep,
      .role_count = static_cast<std::uint8_t>(k_sheep_role_count),
      .material_id = k_wildlife_material_id,
      .bind_palette = &sheep_bind,
      .foot_bones = k_wildlife_foot_bones,
      .describe_assets = &describe_sheep_assets,
      .default_asset = RCP::k_sheep_asset,
      .initialize_lod_geometry = &initialize_sheep_asset,
  };
  return contract;
}

auto wolf_species_contract() noexcept -> const RCP::SpeciesContract& {
  static const RCP::SpeciesContract contract{
      .kind = RCP::CreatureKind::Wolf,
      .role_count = static_cast<std::uint8_t>(k_wolf_role_count),
      .material_id = k_wildlife_material_id,
      .bind_palette = &wolf_bind,
      .foot_bones = k_wildlife_foot_bones,
      .describe_assets = &describe_wolf_assets,
      .default_asset = RCP::k_wolf_asset,
      .initialize_lod_geometry = &initialize_wolf_asset,
  };
  return contract;
}

} // namespace Render::Wildlife
