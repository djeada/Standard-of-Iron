#include "creature_render_state.h"

namespace Render::Creature::Pipeline {

auto make_prepared_humanoid_row(
    UnitVisualSpec spec, const Render::GL::HumanoidPose &pose,
    const Render::GL::HumanoidVariant &variant,
    const Render::GL::HumanoidAnimationContext &anim,
    const QMatrix4x4 &world_from_unit, std::uint32_t seed, CreatureLOD lod,
    const Render::GL::DrawContext *legacy_ctx, EntityId entity_id,
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
  row.legacy_ctx = legacy_ctx;
  return row;
}

auto make_prepared_horse_row(
    UnitVisualSpec spec, const Render::Horse::HorseSpecPose &pose,
    const Render::GL::HorseVariant &variant, const QMatrix4x4 &world_from_unit,
    std::uint32_t seed, CreatureLOD lod, EntityId entity_id,
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
  return row;
}

auto make_prepared_elephant_row(
    UnitVisualSpec spec, const Render::Elephant::ElephantSpecPose &pose,
    const Render::GL::ElephantVariant &variant,
    const QMatrix4x4 &world_from_unit, std::uint32_t seed, CreatureLOD lod,
    EntityId entity_id,
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
  return row;
}

void append_prepared_row(CreatureFrame &frame,
                         const PreparedCreatureRenderRow &row,
                         SpecId spec_id) noexcept {
  switch (row.spec.kind) {
  case CreatureKind::Humanoid:
    frame.push_humanoid(row.entity_id, row.world_from_unit, spec_id, row.seed,
                        row.lod, row.humanoid_pose, row.humanoid_variant,
                        row.humanoid_anim);
    if (row.legacy_ctx != nullptr && !frame.legacy_ctx.empty()) {
      frame.legacy_ctx.back() = row.legacy_ctx;
    }
    break;

  case CreatureKind::Horse:
    frame.push_horse(row.entity_id, row.world_from_unit, spec_id, row.seed,
                     row.lod, row.horse_pose, row.horse_variant);
    break;

  case CreatureKind::Elephant:
    frame.push_elephant(row.entity_id, row.world_from_unit, spec_id, row.seed,
                        row.lod, row.elephant_pose, row.elephant_variant);
    break;

  case CreatureKind::Mounted:
    break;
  }
}

} // namespace Render::Creature::Pipeline
