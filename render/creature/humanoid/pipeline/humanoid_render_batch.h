#pragma once

#include "render/creature/pipeline/creature_render_graph.h"

namespace Render::GL {
struct HumanoidPose;
struct HumanoidVariant;
struct HumanoidAnimationContext;
} // namespace Render::GL

namespace Render::Creature::Pipeline {

struct PreparedHumanoidBodyState;

void add_humanoid(CreatureRenderBatch& batch,
                  const CreatureGraphOutput& output,
                  const Render::GL::HumanoidPose& pose,
                  const Render::GL::HumanoidVariant& variant,
                  const Render::GL::HumanoidAnimationContext& anim);

void add_humanoid(CreatureRenderBatch& batch, const PreparedHumanoidBodyState& state);

} // namespace Render::Creature::Pipeline
