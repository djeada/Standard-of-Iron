#include "creature_visual_definition.h"

#include "../../elephant/elephant_spec.h"
#include "../../horse/horse_spec.h"

#include <initializer_list>

namespace Render::Creature::Pipeline {

namespace {

constexpr auto
lod_mask(std::initializer_list<CreatureMeshLod> lods) -> std::uint8_t {
  std::uint8_t mask = 0;
  for (const CreatureMeshLod lod : lods) {
    mask = static_cast<std::uint8_t>(mask |
                                     (1U << static_cast<std::uint8_t>(lod)));
  }
  return mask;
}

auto horse_part_graph(Render::Creature::CreatureLOD lod) noexcept
    -> const Render::Creature::PartGraph & {
  return Render::Creature::part_graph_for(Render::Horse::horse_creature_spec(),
                                          lod);
}

auto elephant_part_graph(Render::Creature::CreatureLOD lod) noexcept
    -> const Render::Creature::PartGraph & {
  return Render::Creature::part_graph_for(
      Render::Elephant::elephant_creature_spec(), lod);
}

const CreatureMeshRecipe k_horse_mesh_recipe{
    "horse.procedural.low_poly",
    lod_mask({CreatureMeshLod::Full, CreatureMeshLod::Minimal}),
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
    LegacySlotMask::HorseAttachments,
};

const CreatureMeshRecipe k_elephant_mesh_recipe{
    "elephant.procedural.low_poly",
    lod_mask({CreatureMeshLod::Full, CreatureMeshLod::Minimal}),
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
    LegacySlotMask::ElephantHowdah,
};

} // namespace

auto horse_creature_visual_definition() noexcept
    -> const CreatureVisualDefinition & {
  return k_horse_visual_definition;
}

auto elephant_creature_visual_definition() noexcept
    -> const CreatureVisualDefinition & {
  return k_elephant_visual_definition;
}

auto resolve_creature_visual_definition(const UnitVisualSpec &spec) noexcept
    -> const CreatureVisualDefinition * {
  if (spec.creature_definition != nullptr) {
    return spec.creature_definition;
  }

  switch (spec.kind) {
  case CreatureKind::Horse:
    return &horse_creature_visual_definition();
  case CreatureKind::Elephant:
    return &elephant_creature_visual_definition();
  case CreatureKind::Humanoid:
  case CreatureKind::Mounted:
    return nullptr;
  }
  return nullptr;
}

} // namespace Render::Creature::Pipeline
