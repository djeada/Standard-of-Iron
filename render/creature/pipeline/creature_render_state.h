#pragma once

#include "../../elephant/elephant_spec.h"
#include "../../gl/humanoid/humanoid_types.h"
#include "../../horse/horse_spec.h"
#include "render_pass_intent.h"
#include "unit_visual_spec.h"

#include <QMatrix4x4>
#include <cstdint>

namespace Render::GL {
struct DrawContext;
}

namespace Render::Creature::Pipeline {

using EntityId = std::uint32_t;

struct PreparedCreatureRenderRow {
  UnitVisualSpec spec{};
  std::uint32_t seed{0};
  Render::Creature::CreatureLOD lod{Render::Creature::CreatureLOD::Full};
  RenderPassIntent pass{RenderPassIntent::Main};

  Render::Elephant::ElephantSpecPose elephant_pose{};
};

[[nodiscard]] auto make_prepared_humanoid_row(
    UnitVisualSpec spec, const Render::GL::HumanoidPose &pose,
    const Render::GL::HumanoidVariant &variant,
    const Render::GL::HumanoidAnimationContext &anim,
    const QMatrix4x4 &world_from_unit, std::uint32_t seed,
    Render::Creature::CreatureLOD lod, EntityId entity_id = 0,
    RenderPassIntent pass = RenderPassIntent::Main) noexcept
    -> PreparedCreatureRenderRow;

[[nodiscard]] auto make_prepared_horse_row(
    UnitVisualSpec spec, const Render::Horse::HorseSpecPose &pose,
    const Render::GL::HorseVariant &variant, const QMatrix4x4 &world_from_unit,
    std::uint32_t seed, Render::Creature::CreatureLOD lod,
    EntityId entity_id = 0,
    RenderPassIntent pass = RenderPassIntent::Main) noexcept
    -> PreparedCreatureRenderRow;

[[nodiscard]] auto make_prepared_elephant_row(
    UnitVisualSpec spec, const Render::Elephant::ElephantSpecPose &pose,
    const Render::GL::ElephantVariant &variant,
    const QMatrix4x4 &world_from_unit, std::uint32_t seed,
    Render::Creature::CreatureLOD lod, EntityId entity_id = 0,
    RenderPassIntent pass = RenderPassIntent::Main) noexcept
    -> PreparedCreatureRenderRow;

} // namespace Render::Creature::Pipeline
