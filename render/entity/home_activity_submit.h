#pragma once

#include "render/entity/home_activity.h"

namespace Render::GL {

struct DrawContext;
class ISubmitter;

// Draws what HomeActivity decided for one home: doorstep residents, laundry,
// shutters, curtains and hens.
void submit_home_activity(const DrawContext& ctx, ISubmitter& out, bool carthage);

} // namespace Render::GL
