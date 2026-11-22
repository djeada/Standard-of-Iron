#pragma once

#include "render/creature/humanoid/humanoid_types.h"
#include "render/creature/pipeline/creature_prepared_state.h"

namespace Render::GL {

struct DrawContext;

auto resolve_visual_movement_state(const DrawContext& ctx) -> VisualMovementState;
auto visual_movement_for_animation_inputs(
    const DrawContext& ctx, const AnimationInputs& anim) -> VisualMovementState;

auto sample_anim_state(const DrawContext& ctx) -> AnimationInputs;

} // namespace Render::GL

namespace Render::Creature::Pipeline {

// The entity's animation inputs, sampled once for the prepare pass. Elephants
// share the humanoid sampler.
[[nodiscard]] auto resolve_humanoid_animation_state(const Render::GL::DrawContext& ctx)
    -> PreparedAnimationState;

[[nodiscard]] auto resolve_elephant_animation_state(const Render::GL::DrawContext& ctx)
    -> PreparedAnimationState;

} // namespace Render::Creature::Pipeline
