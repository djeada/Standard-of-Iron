#pragma once

#include "render/creature/pipeline/creature_render_graph.h"

namespace Render::Creature::Pipeline {

struct PreparedWildlifeBodyState;

// Adds a sheep or wolf body to `batch`: its render request with role colours
// and the outgoing clip blend. Nothing is added for a culled body or a species
// without an asset.
void add_quadruped(CreatureRenderBatch& batch, const PreparedWildlifeBodyState& state);

} // namespace Render::Creature::Pipeline
