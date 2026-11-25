#pragma once

#include "render/entity/farm_activity.h"

namespace Render::GL {

struct DrawContext;
class ISubmitter;

struct FarmActivityPlan {
  const FarmActivity::Field* field{nullptr};
  float scale{0.0F};
  float pixels{0.0F};
};

[[nodiscard]] auto resolve_farm_activity_plan(const DrawContext& ctx,
                                              bool carthage) -> FarmActivityPlan;

void submit_farm_activity(const DrawContext& ctx,
                          ISubmitter& out,
                          bool carthage,
                          const FarmActivityPlan& plan);

void submit_farm_activity(const DrawContext& ctx, ISubmitter& out, bool carthage);

} // namespace Render::GL
