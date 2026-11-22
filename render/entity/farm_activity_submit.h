#pragma once

#include "render/entity/farm_activity.h"

namespace Render::GL {

struct DrawContext;
class ISubmitter;

// Draws what FarmActivity decided for one farm: field workers and their props.
void submit_farm_activity(const DrawContext& ctx, ISubmitter& out, bool carthage);

} // namespace Render::GL
