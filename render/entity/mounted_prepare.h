#pragma once

#include "../creature/pipeline/creature_render_state.h"
#include "../creature/pipeline/unit_visual_spec.h"
#include "../gl/humanoid/humanoid_types.h"
#include "../horse/horse_spec.h"

#include <QMatrix4x4>
#include <cstdint>
#include <vector>

namespace Render::GL {
struct DrawContext;
} // namespace Render::GL

namespace Render::GL {

struct MountedPreparedSet {
  Render::Creature::Pipeline::PreparedCreatureRenderRow mount_row{};
  Render::Creature::Pipeline::PreparedCreatureRenderRow rider_row{};
};

[[nodiscard]] auto prepare_mounted_rows(
    const Render::Creature::Pipeline::MountedSpec &mounted_spec,
    const QMatrix4x4 &mount_world_from_unit,
    const QMatrix4x4 &rider_world_from_unit,
    const Render::Horse::HorseSpecPose &mount_pose,
    const Render::GL::HorseVariant &mount_variant,
    const Render::GL::HumanoidPose &rider_pose,
    const Render::GL::HumanoidVariant &rider_variant,
    const Render::GL::HumanoidAnimationContext &rider_anim, std::uint32_t seed,
    Render::Creature::CreatureLOD lod,
    Render::Creature::Pipeline::RenderPassIntent pass =
        Render::Creature::Pipeline::RenderPassIntent::Main) noexcept
    -> MountedPreparedSet;

} // namespace Render::GL
