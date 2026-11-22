#pragma once

#include "render/entity/barracks_activity.h"

namespace Render::GL {

struct DrawContext;
class ISubmitter;

// Draws the barracks yard life: the drill, the forge fire, the off-duty soldiers.
void submit_barracks_activity(const DrawContext& ctx,
                              ISubmitter& out,
                              bool carthage,
                              const BarracksGrounds& grounds);

} // namespace Render::GL
