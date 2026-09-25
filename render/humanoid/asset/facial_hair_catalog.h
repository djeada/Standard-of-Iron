#pragma once

#include "render/creature/archetype_registry.h"
#include "render/gl/humanoid/humanoid_types.h"

namespace Render::Humanoid {

[[nodiscard]] auto facial_hair_body_archetype(
    Render::Creature::ArchetypeId base_archetype,
    Render::GL::FacialHairStyle style) -> Render::Creature::ArchetypeId;

auto resolve_facial_hair_archetype(Render::Creature::ArchetypeId base_archetype,
                                   const Render::GL::HumanoidVariant& variant)
    -> Render::Creature::ArchetypeId;

} // namespace Render::Humanoid
