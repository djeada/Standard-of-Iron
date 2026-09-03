#pragma once

#include "render/creature/animation_state_components.h"
#include "wildlife_draw_state.h"

namespace Render::GL {
class EntityRendererRegistry;
}

namespace Render::GL::Wildlife {

[[nodiscard]] auto resolve_wolf_clip(const DrawState& state, GaitTier tier)
    -> Render::Creature::AnimationStateId;

void register_wolf_renderer(EntityRendererRegistry& registry);

} // namespace Render::GL::Wildlife
