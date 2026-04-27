#include "creature_render_state.h"

namespace Render::Creature::Pipeline {

auto make_prepared_humanoid_row(
    UnitVisualSpec spec, const Render::GL::HumanoidPose &pose,
    const Render::GL::HumanoidVariant &variant,
    const Render::GL::HumanoidAnimationContext &anim,
    const QMatrix4x4 &world_from_unit, std::uint32_t seed,
    Render::Creature::CreatureLOD lod, EntityId entity_id,
    RenderPassIntent pass) noexcept -> PreparedCreatureRenderRow {
  (void)pose;
  (void)variant;
  (void)anim;
  (void)world_from_unit;
  (void)entity_id;
  spec.kind = CreatureKind::Humanoid;

  PreparedCreatureRenderRow row{};
  row.spec = spec;
  row.seed = seed;
  row.lod = lod;
  row.pass = pass;
  return row;
}

auto make_prepared_horse_row(
    UnitVisualSpec spec, const Render::Horse::HorseSpecPose &pose,
    const Render::GL::HorseVariant &variant, const QMatrix4x4 &world_from_unit,
    std::uint32_t seed, Render::Creature::CreatureLOD lod, EntityId entity_id,
    RenderPassIntent pass) noexcept -> PreparedCreatureRenderRow {
  (void)pose;
  (void)variant;
  (void)world_from_unit;
  (void)entity_id;
  spec.kind = CreatureKind::Horse;

  PreparedCreatureRenderRow row{};
  row.spec = spec;
  row.seed = seed;
  row.lod = lod;
  row.pass = pass;
  return row;
}

auto make_prepared_elephant_row(
    UnitVisualSpec spec, const Render::Elephant::ElephantSpecPose &pose,
    const Render::GL::ElephantVariant &variant,
    const QMatrix4x4 &world_from_unit, std::uint32_t seed,
    Render::Creature::CreatureLOD lod, EntityId entity_id,
    RenderPassIntent pass) noexcept -> PreparedCreatureRenderRow {
  (void)variant;
  (void)world_from_unit;
  (void)entity_id;
  spec.kind = CreatureKind::Elephant;

  PreparedCreatureRenderRow row{};
  row.spec = spec;
  row.seed = seed;
  row.lod = lod;
  row.pass = pass;
  row.elephant_pose = pose;
  return row;
}

} // namespace Render::Creature::Pipeline
