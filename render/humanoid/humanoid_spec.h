

#pragma once

#include "../creature/part_graph.h"
#include "humanoid_full_builder.h"
#include "skeleton.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
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

inline constexpr std::size_t k_humanoid_role_count = 7;

[[nodiscard]] auto
humanoid_creature_spec() noexcept -> const Render::Creature::CreatureSpec &;

auto humanoid_bind_palette() noexcept -> std::span<const QMatrix4x4>;

[[nodiscard]] auto
humanoid_bind_body_frames() noexcept -> const Render::GL::BodyFrames &;

void fill_humanoid_role_colors(
    const Render::GL::HumanoidVariant &variant,
    std::array<QVector3D, k_humanoid_role_count> &out_roles) noexcept;

} // namespace Render::Humanoid
