#pragma once

#include <cstdint>

#include "render/creature/pipeline/creature_render_graph.h"

namespace Render::GL {
struct ElephantVariant;
} // namespace Render::GL

namespace Render::Creature::Pipeline {

struct PreparedElephantBodyState;

void add_quadruped(CreatureRenderBatch& batch,
                   const CreatureGraphOutput& output,
                   const Render::GL::ElephantVariant& variant,
                   Render::Creature::AnimationStateId state,
                   float phase,
                   std::uint32_t clip_variant = 0U);

void add_quadruped(CreatureRenderBatch& batch, const PreparedElephantBodyState& state);

} // namespace Render::Creature::Pipeline
