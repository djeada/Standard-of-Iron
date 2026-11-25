#pragma once

#include "render/entity/barracks_activity.h"

namespace Render::GL {

struct DrawContext;
class ISubmitter;

struct BarracksActivityPlan {
  bool active{false};
  bool training{false};
};

[[nodiscard]] auto
resolve_barracks_activity_plan(const DrawContext& ctx) -> BarracksActivityPlan;

void submit_barracks_activity(const DrawContext& ctx,
                              ISubmitter& out,
                              bool carthage,
                              const BarracksGrounds& grounds,
                              const BarracksActivityPlan& plan);

void submit_barracks_activity(const DrawContext& ctx,
                              ISubmitter& out,
                              bool carthage,
                              const BarracksGrounds& grounds);

} // namespace Render::GL
