#include "creature_render_state.h"

namespace Render::Creature::Pipeline {

auto make_prepared_humanoid_row(
    UnitVisualSpec spec, const Render::GL::HumanoidPose &pose,
    const Render::GL::HumanoidVariant &variant,
    const Render::GL::HumanoidAnimationContext &anim,
    const QMatrix4x4 &world_from_unit, std::uint32_t seed,
    Render::Creature::CreatureLOD lod, EntityId entity_id,
    RenderPassIntent pass) noexcept -> PreparedCreatureRenderRow {
  spec.kind = CreatureKind::Humanoid;

  PreparedCreatureRenderRow row{};
  row.spec = spec;
  row.entity_id = entity_id;
  row.world_from_unit = world_from_unit;
  row.seed = seed;
  row.lod = lod;
  row.pass = pass;
  row.humanoid_pose = pose;
  row.humanoid_variant = variant;
  row.humanoid_anim = anim;
  row.bpat_playback = BpatPlayback{0U, 0U};
  return row;
}

auto make_prepared_horse_row(
    UnitVisualSpec spec, const Render::Horse::HorseSpecPose &pose,
    const Render::GL::HorseVariant &variant, const QMatrix4x4 &world_from_unit,
    std::uint32_t seed, Render::Creature::CreatureLOD lod, EntityId entity_id,
    RenderPassIntent pass) noexcept -> PreparedCreatureRenderRow {
  spec.kind = CreatureKind::Horse;

  PreparedCreatureRenderRow row{};
  row.spec = spec;
  row.entity_id = entity_id;
  row.world_from_unit = world_from_unit;
  row.seed = seed;
  row.lod = lod;
  row.pass = pass;
  row.horse_pose = pose;
  row.horse_variant = variant;
  row.bpat_playback = BpatPlayback{0U, 0U};
  return row;
}

auto make_prepared_elephant_row(
    UnitVisualSpec spec, const Render::Elephant::ElephantSpecPose &pose,
    const Render::GL::ElephantVariant &variant,
    const QMatrix4x4 &world_from_unit, std::uint32_t seed,
    Render::Creature::CreatureLOD lod, EntityId entity_id,
    RenderPassIntent pass) noexcept -> PreparedCreatureRenderRow {
  spec.kind = CreatureKind::Elephant;

  PreparedCreatureRenderRow row{};
  row.spec = spec;
  row.entity_id = entity_id;
  row.world_from_unit = world_from_unit;
  row.seed = seed;
  row.lod = lod;
  row.pass = pass;
  row.elephant_pose = pose;
  row.elephant_variant = variant;
  row.bpat_playback = BpatPlayback{0U, 0U};
  return row;
}

} // namespace Render::Creature::Pipeline
