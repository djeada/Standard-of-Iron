#include "creature_render_state.h"

#include "../archetype_registry.h"
#include "creature_asset.h"
#include "creature_prepared_state.h"
#include "preparation_common.h"

namespace Render::Creature::Pipeline {

namespace {

[[nodiscard]] auto
compose_world_key(std::uint32_t entity_id,
                  std::uint32_t seed) noexcept -> Render::Creature::WorldKey {
  auto const key = (static_cast<Render::Creature::WorldKey>(entity_id) << 32U) |
                   static_cast<Render::Creature::WorldKey>(seed);
  return key != 0U ? key : Render::Creature::WorldKey{1U};
}

[[nodiscard]] auto default_archetype_for(CreatureKind kind) noexcept
    -> Render::Creature::ArchetypeId {
  switch (kind) {
  case CreatureKind::Horse:
    return Render::Creature::ArchetypeRegistry::kHorseBase;
  case CreatureKind::Elephant:
    return Render::Creature::ArchetypeRegistry::kElephantBase;
  case CreatureKind::Humanoid:
    return Render::Creature::ArchetypeRegistry::kHumanoidBase;
  case CreatureKind::Mounted:
    break;
  }
  return Render::Creature::kInvalidArchetype;
}

void initialize_row_request(PreparedCreatureRenderRow &row) noexcept {
  row.request.variant = Render::Creature::kCanonicalVariant;
  row.request.world = row.world_from_unit;
  row.request.entity_id = row.entity_id;
  row.request.seed = row.seed;
  row.request.world_key = compose_world_key(row.entity_id, row.seed);
  row.request.lod = row.lod;
  row.request.pass = row.pass;
  row.request.world_already_grounded = row.world_already_grounded;
  row.request.archetype =
      row.spec.archetype_id != Render::Creature::kInvalidArchetype
          ? row.spec.archetype_id
          : default_archetype_for(row.kind);

  if (const auto *asset = CreatureAssetRegistry::instance().resolve(row.spec);
      asset != nullptr) {
    row.request.creature_asset_id = asset->id;
  }
}

} // namespace

auto make_prepared_render_row(
    const PreparedCreatureVisualState &visual_state) noexcept
    -> PreparedCreatureRenderRow {
  PreparedCreatureRenderRow row{};
  row.spec = visual_state.spec;
  row.spec.kind = visual_state.kind;
  row.kind = visual_state.kind;
  row.seed = visual_state.seed;
  row.lod = visual_state.lod;
  row.pass = visual_state.pass;
  row.world_from_unit = visual_state.world_from_unit;
  row.world_already_grounded = visual_state.world_already_grounded;
  row.entity_id = visual_state.entity_id;
  initialize_row_request(row);
  return row;
}

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

  PreparedCreatureVisualState visual_state{};
  visual_state.spec = spec;
  visual_state.kind = CreatureKind::Humanoid;
  visual_state.lod = lod;
  visual_state.pass = pass;
  visual_state.seed = seed;
  visual_state.world_from_unit = world_from_unit;
  visual_state.entity_id = entity_id;
  auto row = make_prepared_render_row(visual_state);
  row.request.state = humanoid_state_for_anim(anim);
  row.request.phase = humanoid_phase_for_anim(anim);
  row.request.clip_variant = humanoid_clip_variant_for_anim(anim);
  return row;
}

auto make_prepared_creature_row(
    UnitVisualSpec spec, CreatureKind kind, const QMatrix4x4 &world_from_unit,
    std::uint32_t seed, Render::Creature::CreatureLOD lod, EntityId entity_id,
    RenderPassIntent pass) noexcept -> PreparedCreatureRenderRow {
  PreparedCreatureVisualState visual_state{};
  visual_state.spec = spec;
  visual_state.kind = kind;
  visual_state.lod = lod;
  visual_state.pass = pass;
  visual_state.seed = seed;
  visual_state.world_from_unit = world_from_unit;
  visual_state.entity_id = entity_id;
  return make_prepared_render_row(visual_state);
}

} // namespace Render::Creature::Pipeline
