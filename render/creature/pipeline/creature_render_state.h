#pragma once

#include "../../gl/humanoid/humanoid_types.h"
#include "../render_request.h"
#include "render_pass_intent.h"
#include "unit_visual_spec.h"

#include <QMatrix4x4>
#include <cstdint>

namespace Render::GL {
struct DrawContext;
}

namespace Render::Creature::Pipeline {

using EntityId = std::uint32_t;

struct PreparedCreatureVisualState;

struct PreparedCreatureRenderRow {
  UnitVisualSpec spec{};
  CreatureKind kind{CreatureKind::Humanoid};
  std::uint32_t seed{0};
  Render::Creature::CreatureLOD lod{Render::Creature::CreatureLOD::Full};
  RenderPassIntent pass{RenderPassIntent::Main};
  QMatrix4x4 world_from_unit{};
  bool world_already_grounded{true};
  EntityId entity_id{0};
  Render::Creature::CreatureRenderRequest request{};
};

[[nodiscard]] auto make_prepared_render_row(
    const PreparedCreatureVisualState &visual_state) noexcept
    -> PreparedCreatureRenderRow;

[[nodiscard]] auto make_prepared_humanoid_row(
    UnitVisualSpec spec, const Render::GL::HumanoidPose &pose,
    const Render::GL::HumanoidVariant &variant,
    const Render::GL::HumanoidAnimationContext &anim,
    const QMatrix4x4 &world_from_unit, std::uint32_t seed,
    Render::Creature::CreatureLOD lod, EntityId entity_id = 0,
    RenderPassIntent pass = RenderPassIntent::Main) noexcept
    -> PreparedCreatureRenderRow;

[[nodiscard]] auto make_prepared_creature_row(
    UnitVisualSpec spec, CreatureKind kind, const QMatrix4x4 &world_from_unit,
    std::uint32_t seed, Render::Creature::CreatureLOD lod,
    EntityId entity_id = 0,
    RenderPassIntent pass = RenderPassIntent::Main) noexcept
    -> PreparedCreatureRenderRow;

} // namespace Render::Creature::Pipeline
