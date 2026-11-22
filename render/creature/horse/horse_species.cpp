#include "render/creature/horse/horse_species.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "animation/bpat/bpat_format.h"
#include "render/creature/horse/dimensions.h"
#include "render/creature/horse/horse_spec.h"
#include "render/creature/pipeline/creature_visual_definition.h"
#include "render/creature/spec.h"

namespace Render::Creature::Pipeline {

namespace {

auto horse_part_graph(Render::Creature::CreatureLOD lod) noexcept
    -> const Render::Creature::PartGraph& {
  return Render::Creature::part_graph_for(Render::Horse::horse_creature_spec(), lod);
}

const CreatureMeshRecipe k_horse_mesh_recipe{
    "horse.procedural.low_poly",
    mesh_lod_mask({CreatureMeshLod::Full, CreatureMeshLod::Minimal}),
    &Render::Horse::horse_creature_spec,
    &horse_part_graph,
    {1, 1},
};

const CreatureRigDefinition k_horse_rig_definition{
    "horse.runtime_rig",
    static_cast<std::uint8_t>(Render::Horse::k_horse_bone_count),
    &Render::Horse::horse_bind_palette,
    &Render::Horse::horse_topology(),
    {static_cast<std::uint8_t>(Render::Horse::HorseBone::FootFL),
     static_cast<std::uint8_t>(Render::Horse::HorseBone::FootFR),
     static_cast<std::uint8_t>(Render::Horse::HorseBone::FootBL),
     static_cast<std::uint8_t>(Render::Horse::HorseBone::FootBR)},
    static_cast<std::uint8_t>(Render::Horse::HorseBone::Body),
};

const CreatureGroundingModel k_horse_grounding_model{
    "horse.hoof_grounding",
    static_cast<std::uint8_t>(k_large_creature_leg_count),
    0.58F,
    1.0F,
};

const CreatureMotionController k_horse_motion_controller{
    "horse.gait_controller",
    CreatureLocomotionMode::Idle,
};

const CreatureAttachmentFrameExtractor k_horse_attachment_frame_extractor{
    "horse.mount_frames",
};

const CreatureLodStrategy k_horse_lod_strategy{
    "horse.large_creature_lod",
    k_horse_mesh_recipe.lod_mask,
};

const CreatureVisualDefinition k_horse_visual_definition{
    "horse.large_creature.v1",
    CreatureKind::Horse,
    &k_horse_mesh_recipe,
    &k_horse_rig_definition,
    &k_horse_motion_controller,
    &k_horse_grounding_model,
    &k_horse_attachment_frame_extractor,
    &k_horse_lod_strategy,
    8,
};

} // namespace

auto horse_creature_visual_definition() noexcept -> const CreatureVisualDefinition& {
  return k_horse_visual_definition;
}

} // namespace Render::Creature::Pipeline

namespace Render::Horse {

namespace {

namespace RCP = Render::Creature::Pipeline;

auto horse_bind() noexcept -> std::span<const QMatrix4x4> {
  return Render::Horse::horse_bind_palette();
}

auto horse_fill_roles(const void* variant,
                      QVector3D* out,
                      std::size_t max_roles) -> std::uint32_t {
  const auto& v = *static_cast<const Render::GL::HorseVariant*>(variant);
  std::array<QVector3D, 8> filled{};
  Render::Horse::fill_horse_role_colors(v, filled);
  const auto n = std::min<std::size_t>(filled.size(), max_roles);
  for (std::size_t i = 0; i < n; ++i) {
    out[i] = filled[i];
  }
  return static_cast<std::uint32_t>(n);
}

void describe_horse_assets(std::span<RCP::CreatureAsset> assets) {
  RCP::CreatureAsset horse{};
  horse.id = RCP::k_horse_asset;
  horse.debug_name = "horse.v1";
  horse.kind = RCP::CreatureKind::Horse;
  horse.bpat_species_id = Render::Creature::Bpat::k_species_horse;
  horse.spec = &Render::Horse::horse_creature_spec();
  horse.topology = &horse.spec->topology;
  horse.role_count = 8;
  horse.max_bones = static_cast<std::uint8_t>(Render::Horse::k_horse_bone_count);
  horse.bind_palette = &horse_bind;
  horse.fill_role_colors = &horse_fill_roles;
  horse.snapshot_mesh_species_id = Render::Creature::Bpat::k_species_horse;
  horse.snapshot_mesh_lod_mask = static_cast<std::uint8_t>(
      1U << static_cast<std::uint8_t>(Render::Creature::CreatureLOD::Minimal));
  horse.visual_definition = &RCP::horse_creature_visual_definition();
  assets[RCP::k_horse_asset] = horse;
}

constexpr std::array<std::size_t, 4> k_horse_foot_bones{
    static_cast<std::size_t>(HorseBone::FootFL),
    static_cast<std::size_t>(HorseBone::FootFR),
    static_cast<std::size_t>(HorseBone::FootBL),
    static_cast<std::size_t>(HorseBone::FootBR)};

constexpr auto leg_bones(HorseBone shoulder,
                         HorseBone knee,
                         HorseBone foot) noexcept -> RCP::WholeMeshLegBones {
  return {static_cast<std::uint8_t>(shoulder),
          static_cast<std::uint8_t>(knee),
          static_cast<std::uint8_t>(foot)};
}

constexpr RCP::WholeMeshBones k_horse_whole_mesh_bones{
    .root = static_cast<std::uint8_t>(HorseBone::Root),
    .neck = static_cast<std::uint8_t>(HorseBone::NeckTop),
    .head = static_cast<std::uint8_t>(HorseBone::Head),
    .trunk_tip = static_cast<std::uint8_t>(HorseBone::Head),
    .legs = {{leg_bones(HorseBone::ShoulderFL, HorseBone::KneeFL, HorseBone::FootFL),
              leg_bones(HorseBone::ShoulderFR, HorseBone::KneeFR, HorseBone::FootFR),
              leg_bones(HorseBone::ShoulderBL, HorseBone::KneeBL, HorseBone::FootBL),
              leg_bones(HorseBone::ShoulderBR, HorseBone::KneeBR, HorseBone::FootBR)}},
};

} // namespace

auto horse_species_contract() noexcept -> const RCP::SpeciesContract& {
  static const RCP::SpeciesContract contract{
      .kind = RCP::CreatureKind::Horse,
      .role_count = 8U,
      .material_id = k_horse_material_id,
      .bind_palette = &horse_bind,
      .foot_bones = k_horse_foot_bones,
      .describe_assets = &describe_horse_assets,
      .default_asset = RCP::k_horse_asset,
      .initialize_lod_geometry = &initialize_horse_asset,
      .whole_mesh_bones = &k_horse_whole_mesh_bones,
  };
  return contract;
}

} // namespace Render::Horse
