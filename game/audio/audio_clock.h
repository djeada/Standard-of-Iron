#pragma once

#include <chrono>

namespace Game::Audio::Clock {

using TimePoint = std::chrono::steady_clock::time_point;

void use_rendered_time(bool rendered) noexcept;
[[nodiscard]] auto uses_rendered_time() noexcept -> bool;
void advance_rendered(double seconds) noexcept;
[[nodiscard]] auto now() noexcept -> TimePoint;

} // namespace Game::Audio::Clock
