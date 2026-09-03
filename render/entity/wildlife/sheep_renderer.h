#pragma once

#include "render/creature/animation_state_components.h"
#include "wildlife_draw_state.h"

namespace Render::GL {
class EntityRendererRegistry;
}

namespace Render::GL::Wildlife {

[[nodiscard]] auto
resolve_sheep_clip(const DrawState& state,
                   GaitTier tier,
                   bool grazing) -> Render::Creature::AnimationStateId;

void register_sheep_renderer(EntityRendererRegistry& registry);

} // namespace Render::GL::Wildlife
