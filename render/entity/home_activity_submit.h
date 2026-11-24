#pragma once

#include <cstdint>

#include "render/entity/home_activity.h"

namespace Render::GL {

struct DrawContext;
class ISubmitter;

// Beyond this distance a home shows no activity at all.
inline constexpr float k_home_activity_far = 190.0F;

// What the home activity presentation decides for one home before anything is
// drawn: nothing, the residents who fell when the house was destroyed, or the
// live hearth, doorstep and window activity, with the screen metrics the live
// path draws with.
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
// The same for a home that draws with its own anchor instead of the nation's
// registered one (styled homes).
[[nodiscard]] auto
resolve_home_activity_plan(const DrawContext& ctx,
                           const HomeSmokeAnchor& anchor) -> HomeActivityPlan;

// Draws what HomeActivity decided for one home: doorstep residents, laundry,
// shutters, curtains and hens.
void submit_home_activity(const DrawContext& ctx,
                          ISubmitter& out,
                          bool carthage,
                          const HomeActivityPlan& plan);
void submit_home_activity(const DrawContext& ctx,
                          ISubmitter& out,
                          bool carthage,
                          const HomeActivityPlan& plan,
                          const HomeSmokeAnchor& anchor);
// Resolves the plan and draws it, for callers outside the registry.
void submit_home_activity(const DrawContext& ctx, ISubmitter& out, bool carthage);

} // namespace Render::GL
