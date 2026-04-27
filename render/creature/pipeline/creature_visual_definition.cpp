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

const CreatureMeshRecipe kHorseMeshRecipe{
    "horse.procedural.low_poly",
    lod_mask({CreatureMeshLod::Full, CreatureMeshLod::Minimal}),
    &Render::Horse::horse_creature_spec,
    &horse_part_graph,
    {43, 5},
};

const CreatureRigDefinition kHorseRigDefinition{
    "horse.runtime_rig",
    static_cast<std::uint8_t>(Render::Horse::kHorseBoneCount),
    &Render::Horse::horse_bind_palette,
    &Render::Horse::horse_topology(),
    {static_cast<std::uint8_t>(Render::Horse::HorseBone::FootFL),
     static_cast<std::uint8_t>(Render::Horse::HorseBone::FootFR),
     static_cast<std::uint8_t>(Render::Horse::HorseBone::FootBL),
     static_cast<std::uint8_t>(Render::Horse::HorseBone::FootBR)},
    static_cast<std::uint8_t>(Render::Horse::HorseBone::Body),
};

const CreatureGroundingModel kHorseGroundingModel{
    "horse.hoof_grounding",
    static_cast<std::uint8_t>(kLargeCreatureLegCount),
    0.58F,
    1.0F,
};

const CreatureMotionController kHorseMotionController{
    "horse.gait_controller",
    CreatureLocomotionMode::Idle,
};

const CreatureAttachmentFrameExtractor kHorseAttachmentFrameExtractor{
    "horse.mount_frames",
};

const CreatureLodStrategy kHorseLodStrategy{
    "horse.large_creature_lod",
    kHorseMeshRecipe.lod_mask,
};

const CreatureVisualDefinition kHorseVisualDefinition{
    "horse.large_creature.v1",
    CreatureKind::Horse,
    &kHorseMeshRecipe,
    &kHorseRigDefinition,
    &kHorseMotionController,
    &kHorseGroundingModel,
    &kHorseAttachmentFrameExtractor,
    &kHorseLodStrategy,
    8,
    LegacySlotMask::HorseAttachments,
};

const CreatureMeshRecipe kElephantMeshRecipe{
    "elephant.procedural.low_poly",
    lod_mask({CreatureMeshLod::Full, CreatureMeshLod::Minimal}),
    &Render::Elephant::elephant_creature_spec,
    &elephant_part_graph,
    {41, 5},
};

const CreatureRigDefinition kElephantRigDefinition{
    "elephant.runtime_rig",
    static_cast<std::uint8_t>(Render::Elephant::kElephantBoneCount),
    &Render::Elephant::elephant_bind_palette,
    &Render::Elephant::elephant_topology(),
    {static_cast<std::uint8_t>(Render::Elephant::ElephantBone::FootFL),
     static_cast<std::uint8_t>(Render::Elephant::ElephantBone::FootFR),
     static_cast<std::uint8_t>(Render::Elephant::ElephantBone::FootBL),
     static_cast<std::uint8_t>(Render::Elephant::ElephantBone::FootBR)},
    static_cast<std::uint8_t>(Render::Elephant::ElephantBone::Body),
};

const CreatureGroundingModel kElephantGroundingModel{
    "elephant.foot_grounding",
    static_cast<std::uint8_t>(kLargeCreatureLegCount),
    0.70F,
    0.75F,
};

const CreatureMotionController kElephantMotionController{
    "elephant.gait_controller",
    CreatureLocomotionMode::Idle,
};

const CreatureAttachmentFrameExtractor kElephantAttachmentFrameExtractor{
    "elephant.howdah_frames",
};

const CreatureLodStrategy kElephantLodStrategy{
    "elephant.large_creature_lod",
    kElephantMeshRecipe.lod_mask,
};

const CreatureVisualDefinition kElephantVisualDefinition{
    "elephant.large_creature.v1",
    CreatureKind::Elephant,
    &kElephantMeshRecipe,
    &kElephantRigDefinition,
    &kElephantMotionController,
    &kElephantGroundingModel,
    &kElephantAttachmentFrameExtractor,
    &kElephantLodStrategy,
    static_cast<std::uint8_t>(Render::Elephant::kElephantRoleCount),
    LegacySlotMask::ElephantHowdah,
};

} // namespace

auto horse_creature_visual_definition() noexcept
    -> const CreatureVisualDefinition & {
  return kHorseVisualDefinition;
}

auto elephant_creature_visual_definition() noexcept
    -> const CreatureVisualDefinition & {
  return kElephantVisualDefinition;
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
