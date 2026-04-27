#pragma once

#include <cstdint>

namespace Render::GL {

void advance_pose_cache_frame();
void clear_humanoid_caches();
auto humanoid_current_frame() -> std::uint32_t;

} // namespace Render::GL
