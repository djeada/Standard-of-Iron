#pragma once

#include "render/creature/humanoid/schema/visual_capabilities.h"
#include "render/creature/render_request.h"

namespace Render::Humanoid {

[[nodiscard]] auto resolve_humanoid_capabilities(
    Render::Creature::ArchetypeId archetype_id) noexcept -> HumanoidCapabilities;

} // namespace Render::Humanoid
