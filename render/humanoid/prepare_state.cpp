#include "prepare_internal.h"

namespace Render::GL {

uint32_t s_current_frame = 0;
HumanoidRenderStats s_render_stats;

auto humanoid_current_frame() -> std::uint32_t {
  return s_current_frame;
}

void advance_pose_cache_frame() {
  ++s_current_frame;
}

void clear_humanoid_caches() {
  s_current_frame = 0;
}

auto get_humanoid_render_stats() -> const HumanoidRenderStats& {
  return s_render_stats;
}

void reset_humanoid_render_stats() {
  s_render_stats.reset();
}

namespace detail {
void increment_facial_hair_skipped_distance() {
  ++s_render_stats.facial_hair_skipped_distance;
}
} // namespace detail

} // namespace Render::GL
