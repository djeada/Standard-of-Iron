

#pragma once

#include "../creature/part_graph.h"
#include "humanoid_full_builder.h"
#include "skeleton.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <span>

namespace Render::GL {
struct HumanoidVariant;
struct HumanoidPose;
class Mesh;
class ISubmitter;
} // namespace Render::GL

namespace Render::Creature {
struct CreatureSpec;
}

namespace Render::Humanoid {

[[nodiscard]] auto
humanoid_creature_spec() noexcept -> const Render::Creature::CreatureSpec &;

auto compute_bone_palette(const Render::GL::HumanoidPose &pose,
                          std::span<QMatrix4x4> out_bones) noexcept
    -> std::uint32_t;

auto humanoid_bind_palette() noexcept -> std::span<const QMatrix4x4>;

void submit_humanoid_reduced_rigged(const Render::GL::HumanoidPose &pose,
                                    const Render::GL::HumanoidVariant &variant,
                                    const QMatrix4x4 &world_from_unit,
                                    Render::GL::ISubmitter &out) noexcept;

void submit_humanoid_full_rigged(const Render::GL::HumanoidPose &pose,
                                 const Render::GL::HumanoidVariant &variant,
                                 const QMatrix4x4 &world_from_unit,
                                 Render::GL::ISubmitter &out) noexcept;

void submit_humanoid_minimal_rigged(const Render::GL::HumanoidPose &pose,
                                    const Render::GL::HumanoidVariant &variant,
                                    const QMatrix4x4 &world_from_unit,
                                    Render::GL::ISubmitter &out) noexcept;

} // namespace Render::Humanoid
