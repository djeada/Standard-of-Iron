#pragma once

#include <cstdint>

#include "render/frame/terrain_scene_types.h"

namespace Render::Ground::Scatter {

inline auto visibility_filter_enabled_ref() -> bool& {
  static thread_local bool enabled = true;
  return enabled;
}

inline void set_visibility_filter_enabled_for_current_thread(bool enabled) {
  visibility_filter_enabled_ref() = enabled;
}

[[nodiscard]] inline auto visibility_filter_enabled_for_current_thread() -> bool {
  return visibility_filter_enabled_ref();
}

} // namespace Render::Ground::Scatter
