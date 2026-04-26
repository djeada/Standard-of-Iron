#pragma once

#include "../../elephant/elephant_spec.h"
#include "../../gl/humanoid/humanoid_types.h"
#include "../../horse/horse_spec.h"
#include "bpat_playback.h"
#include "unit_visual_spec.h"

#include <QMatrix4x4>
#include <cstdint>

namespace Render::GL {
struct DrawContext;
}

namespace Render::Creature::Pipeline {

using EntityId = std::uint32_t;

enum class RenderPassIntent : std::uint8_t {
  Main = 0,
  Shadow = 1,
};

struct PreparedCreatureRenderRow {
  UnitVisualSpec spec{};
  EntityId entity_id{0};
  QMatrix4x4 world_from_unit{};
  std::uint32_t seed{0};
  Render::Creature::CreatureLOD lod{Render::Creature::CreatureLOD::Full};
  RenderPassIntent pass{RenderPassIntent::Main};

  Render::GL::HumanoidPose humanoid_pose{};
  Render::GL::HumanoidVariant humanoid_variant{};
  Render::GL::HumanoidAnimationContext humanoid_anim{};

  Render::Horse::HorseSpecPose horse_pose{};
  Render::GL::HorseVariant horse_variant{};

  Render::Elephant::ElephantSpecPose elephant_pose{};
  Render::GL::ElephantVariant elephant_variant{};

  BpatPlayback bpat_playback{};
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
