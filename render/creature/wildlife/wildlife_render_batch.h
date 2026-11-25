#pragma once

#include "render/creature/pipeline/creature_render_graph.h"

namespace Render::Creature::Pipeline {

struct PreparedWildlifeBodyState;

void add_quadruped(CreatureRenderBatch& batch, const PreparedWildlifeBodyState& state);

} // namespace Render::Creature::Pipeline
