#pragma once

#include "render/entity/farm_activity.h"

namespace Render::GL {

struct DrawContext;
class ISubmitter;

// What the farm activity presentation decides for one farm before anything is
// drawn: the field it works (null for none) and the screen metrics the workers
// are drawn with.
struct FarmActivityPlan {
  const FarmActivity::Field* field{nullptr};
  float scale{0.0F};
  float pixels{0.0F};
};

[[nodiscard]] auto resolve_farm_activity_plan(const DrawContext& ctx,
                                              bool carthage) -> FarmActivityPlan;

// Draws what FarmActivity decided for one farm: field workers and their props.
void submit_farm_activity(const DrawContext& ctx,
                          ISubmitter& out,
                          bool carthage,
                          const FarmActivityPlan& plan);
// Resolves the plan and draws it, for callers outside the registry.
void submit_farm_activity(const DrawContext& ctx, ISubmitter& out, bool carthage);

} // namespace Render::GL
