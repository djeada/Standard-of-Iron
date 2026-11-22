#pragma once

#include "render/creature/render_request.h"
#include "render/creature/schema/visual_capabilities.h"

namespace Render::Humanoid {

[[nodiscard]] auto resolve_humanoid_capabilities(
    Render::Creature::ArchetypeId archetype_id) noexcept -> HumanoidCapabilities;

} // namespace Render::Humanoid
