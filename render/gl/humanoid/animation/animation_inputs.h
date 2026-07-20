#pragma once

#include "../humanoid_types.h"

namespace Render::GL {

struct DrawContext;

auto resolve_visual_movement_state(const DrawContext& ctx) -> VisualMovementState;
auto visual_movement_for_animation_inputs(
    const DrawContext& ctx, const AnimationInputs& anim) -> VisualMovementState;

auto sample_anim_state(const DrawContext& ctx) -> AnimationInputs;

} // namespace Render::GL
