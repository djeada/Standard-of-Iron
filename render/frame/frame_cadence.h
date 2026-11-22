#pragma once

#include <chrono>

namespace Render {

class FrameCadence {
public:
  using Clock = std::chrono::steady_clock;

  static constexpr std::chrono::milliseconds k_background_frame_interval{33};

  void set_window_active(bool active) noexcept { m_window_active = active; }
  [[nodiscard]] auto window_active() const noexcept -> bool { return m_window_active; }

  [[nodiscard]] auto
  wait_before_next_frame(Clock::time_point frame_started,
                         Clock::time_point now) const noexcept -> Clock::duration {
    if (m_window_active) {
      return Clock::duration::zero();
    }
    const auto elapsed = now - frame_started;
    if (elapsed >= k_background_frame_interval) {
      return Clock::duration::zero();
    }
    return k_background_frame_interval - elapsed;
  }

private:
  bool m_window_active = true;
};

} // namespace Render
