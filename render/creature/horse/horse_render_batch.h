#pragma once

#include <cstdint>

#include "render/creature/pipeline/creature_render_graph.h"

namespace Render::GL {
struct HorseVariant;
} // namespace Render::GL

namespace Render::Creature::Pipeline {

struct PreparedHorseBodyState;

// Adds a horse body to `batch`: its render request with role colours.
// Nothing is added for a culled body or a species without an asset.
void add_quadruped(CreatureRenderBatch& batch,
                   const CreatureGraphOutput& output,
                   const Render::GL::HorseVariant& variant,
                   Render::Creature::AnimationStateId state,
                   float phase,
                   std::uint32_t clip_variant = 0U);

void add_quadruped(CreatureRenderBatch& batch, const PreparedHorseBodyState& state);

} // namespace Render::Creature::Pipeline
