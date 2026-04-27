#pragma once

#include "../creature/archetype_registry.h"
#include "../gl/humanoid/humanoid_types.h"

#include <QVector3D>
#include <cstddef>
#include <cstdint>

namespace Render::Humanoid {

inline constexpr std::uint32_t kFacialHairRoleCount = 3;

auto facial_hair_role_colors(const Render::GL::HumanoidVariant &variant,
                             QVector3D *out, std::uint32_t base_count,
                             std::size_t max_count) -> std::uint32_t;

auto resolve_facial_hair_archetype(Render::Creature::ArchetypeId base_archetype,
                                   const Render::GL::HumanoidVariant &variant)
    -> Render::Creature::ArchetypeId;

} // namespace Render::Humanoid
