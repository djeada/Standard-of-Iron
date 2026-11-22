#pragma once

#include "render/entity/barracks_activity.h"

namespace Render::GL {

struct DrawContext;
class ISubmitter;

// What the barracks activity presentation decides for one barracks before
// anything is drawn: whether its yard is alive (intact and near enough) and
// whether it is training.
struct BarracksActivityPlan {
  bool active{false};
  bool training{false};
};

[[nodiscard]] auto
resolve_barracks_activity_plan(const DrawContext& ctx) -> BarracksActivityPlan;

// Draws the barracks yard life: the drill, the forge fire, the off-duty soldiers.
void submit_barracks_activity(const DrawContext& ctx,
                              ISubmitter& out,
                              bool carthage,
                              const BarracksGrounds& grounds,
                              const BarracksActivityPlan& plan);
// Resolves the plan and draws it, for callers outside the registry.
void submit_barracks_activity(const DrawContext& ctx,
                              ISubmitter& out,
                              bool carthage,
                              const BarracksGrounds& grounds);

} // namespace Render::GL
