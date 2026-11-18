#pragma once

#include <chrono>

namespace Game::Audio::Clock {

using TimePoint = std::chrono::steady_clock::time_point;

// Every cooldown, instance lifetime and music hold in the audio code reads this
// clock. In play it is the wall clock. An offline render (the arena's promo capture
// pulls the mix for every simulated step, at a few frames a second) switches it to
// the time the rendered audio has reached: with the wall clock, every cooldown had
// lapsed between two rendered frames and a fight played the same sword clash on a
// half-second metronome.
void use_rendered_time(bool rendered) noexcept;
[[nodiscard]] auto uses_rendered_time() noexcept -> bool;
void advance_rendered(double seconds) noexcept;
[[nodiscard]] auto now() noexcept -> TimePoint;

} // namespace Game::Audio::Clock
