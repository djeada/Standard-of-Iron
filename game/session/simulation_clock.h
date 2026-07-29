#pragma once

#include <cstdint>

namespace Game::Session {

class SimulationClock {
public:
  static constexpr double k_default_tick_seconds = 1.0 / 60.0;

  static constexpr double k_max_frame_seconds = 0.25;

  explicit SimulationClock(double tick_seconds = k_default_tick_seconds);

  void reset();

  auto advance(double real_delta_seconds) -> int;

  auto consume_tick() -> bool;

  [[nodiscard]] auto pending_ticks() const -> int { return m_pending_ticks; }

  void drop_pending_ticks() { m_pending_ticks = 0; }

  [[nodiscard]] auto tick() const -> std::uint64_t { return m_tick; }
  [[nodiscard]] auto tick_seconds() const -> double { return m_tick_seconds; }

  [[nodiscard]] auto now_seconds() const -> double;

  void set_paused(bool paused) { m_paused = paused; }
  [[nodiscard]] auto paused() const -> bool { return m_paused; }

  void set_time_scale(double scale);
  [[nodiscard]] auto time_scale() const -> double { return m_time_scale; }

  void restore(std::uint64_t tick_count);

private:
  double m_tick_seconds;
  double m_accumulator = 0.0;
  double m_time_scale = 1.0;
  std::uint64_t m_tick = 0;
  int m_pending_ticks = 0;
  bool m_paused = false;
};

} // namespace Game::Session
