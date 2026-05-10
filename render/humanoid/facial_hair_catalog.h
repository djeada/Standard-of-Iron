#pragma once

#include "../creature/archetype_registry.h"
#include "../gl/humanoid/humanoid_types.h"

#include <QVector3D>
#include <cstddef>
#include <cstdint>

namespace Render::Humanoid {

inline constexpr std::uint32_t k_facial_hair_role_count = 3;

auto facial_hair_role_colors(const Render::GL::HumanoidVariant &variant,
                             QVector3D *out, std::uint32_t base_count,
                             std::size_t max_count) -> std::uint32_t;

auto facial_hair_make_static_attachment(Render::GL::FacialHairStyle style,
                                        std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec;

auto resolve_facial_hair_archetype(Render::Creature::ArchetypeId base_archetype,
                                   const Render::GL::HumanoidVariant &variant)
    -> Render::Creature::ArchetypeId;

} // namespace Render::Humanoid
