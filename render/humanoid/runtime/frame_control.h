#pragma once

#include <cstdint>

namespace Render::GL {

void begin_humanoid_frame();

void reset_humanoid_runtime_context();

[[nodiscard]] auto humanoid_current_frame() -> std::uint32_t;

} // namespace Render::GL
