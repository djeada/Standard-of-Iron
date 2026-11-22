#pragma once

#include "render/creature/pipeline/creature_render_graph.h"

namespace Render::GL {
struct HumanoidPose;
struct HumanoidVariant;
struct HumanoidAnimationContext;
} // namespace Render::GL

namespace Render::Creature::Pipeline {

struct PreparedHumanoidBodyState;

// Adds a humanoid body to `batch`: the render request with its animation
// selection, blend layers, role colours and wear, and a shadow row when the
// output is for the shadow pass. Nothing is added for a culled body.
void add_humanoid(CreatureRenderBatch& batch,
                  const CreatureGraphOutput& output,
                  const Render::GL::HumanoidPose& pose,
                  const Render::GL::HumanoidVariant& variant,
                  const Render::GL::HumanoidAnimationContext& anim);

void add_humanoid(CreatureRenderBatch& batch, const PreparedHumanoidBodyState& state);

} // namespace Render::Creature::Pipeline
