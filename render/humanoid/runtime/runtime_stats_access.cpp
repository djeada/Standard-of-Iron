#include <atomic>

#include "render/humanoid/runtime/frame_control.h"
#include "render/humanoid/runtime/runtime_context.h"
#include "render/humanoid/runtime/runtime_stats.h"
#include "render/humanoid/runtime/runtime_stats_shim.h"

namespace Render::GL {

namespace {
std::atomic<std::uint32_t> g_runtime_generation{0U};
} // namespace

auto humanoid_current_frame() -> std::uint32_t {
  return Render::Humanoid::current_humanoid_runtime_context().frame_index;
}

void begin_humanoid_frame() {
  Render::Humanoid::current_humanoid_runtime_context().begin_frame();
}

void reset_humanoid_runtime_context() {
  Render::Humanoid::current_humanoid_runtime_context().reset();
  g_runtime_generation.fetch_add(1U, std::memory_order_relaxed);
}

auto humanoid_runtime_generation() -> std::uint32_t {
  return g_runtime_generation.load(std::memory_order_relaxed);
}

auto get_humanoid_render_stats() -> const HumanoidRenderStats& {
  return Render::Humanoid::current_humanoid_runtime_context().stats;
}

void reset_humanoid_render_stats() {
  Render::Humanoid::current_humanoid_runtime_context().reset_stats();
}

namespace detail {
void increment_facial_hair_skipped_distance() {
  ++Render::Humanoid::current_humanoid_runtime_context()
        .stats.facial_hair_skipped_distance;
}
} // namespace detail

} // namespace Render::GL
