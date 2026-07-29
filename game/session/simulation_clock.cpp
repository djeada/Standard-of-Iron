#include "simulation_clock.h"

#include <algorithm>

namespace Game::Session {

SimulationClock::SimulationClock(double tick_seconds)
    : m_tick_seconds(tick_seconds > 0.0 ? tick_seconds : k_default_tick_seconds) {
}

void SimulationClock::reset() {
  m_accumulator = 0.0;
  m_tick = 0;
  m_pending_ticks = 0;
  m_paused = false;
  m_time_scale = 1.0;
}

void SimulationClock::set_time_scale(double scale) {
  m_time_scale = std::max(0.0, scale);
}

auto SimulationClock::advance(double real_delta_seconds) -> int {
  if (m_paused || real_delta_seconds <= 0.0) {
    m_pending_ticks = 0;
    return 0;
  }

  const double clamped = std::min(real_delta_seconds, k_max_frame_seconds);
  m_accumulator += clamped * m_time_scale;

  m_pending_ticks = static_cast<int>(m_accumulator / m_tick_seconds);
  m_accumulator -= static_cast<double>(m_pending_ticks) * m_tick_seconds;
  return m_pending_ticks;
}

auto SimulationClock::consume_tick() -> bool {
  if (m_pending_ticks <= 0) {
    return false;
  }
  --m_pending_ticks;
  ++m_tick;
  return true;
}

auto SimulationClock::now_seconds() const -> double {
  return static_cast<double>(m_tick) * m_tick_seconds;
}

void SimulationClock::restore(std::uint64_t tick_count) {
  m_tick = tick_count;
  m_accumulator = 0.0;
  m_pending_ticks = 0;
}

} // namespace Game::Session
