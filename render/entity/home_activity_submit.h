#pragma once

#include <cstdint>

#include "render/entity/home_activity.h"

namespace Render::GL {

struct DrawContext;
class ISubmitter;

inline constexpr float k_home_activity_far = 190.0F;

struct HomeActivityPlan {
  enum class Mode : std::uint8_t {
    None,
    FallenResidents,
    Live
  };
  Mode mode{Mode::None};
  float fallen_for{0.0F};
  float scale{0.0F};
  float pixels{0.0F};
  float distance{0.0F};
};

[[nodiscard]] auto resolve_home_activity_plan(const DrawContext& ctx,
                                              bool carthage) -> HomeActivityPlan;

[[nodiscard]] auto
resolve_home_activity_plan(const DrawContext& ctx,
                           const HomeSmokeAnchor& anchor) -> HomeActivityPlan;

void submit_home_activity(const DrawContext& ctx,
                          ISubmitter& out,
                          bool carthage,
                          const HomeActivityPlan& plan);
void submit_home_activity(const DrawContext& ctx,
                          ISubmitter& out,
                          bool carthage,
                          const HomeActivityPlan& plan,
                          const HomeSmokeAnchor& anchor);

void submit_home_activity(const DrawContext& ctx, ISubmitter& out, bool carthage);

} // namespace Render::GL
