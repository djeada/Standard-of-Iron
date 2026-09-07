#pragma once

#include <atomic>
#include <cstdint>

namespace Render::Profiling {

class FrameSwapClock {
public:
  void observe(std::int64_t nanoseconds) noexcept {
    m_last_ns.store(nanoseconds, std::memory_order_release);
  }
  [[nodiscard]] auto latest() const noexcept -> std::int64_t {
    return m_last_ns.load(std::memory_order_acquire);
  }

  [[nodiscard]] auto interval_ms(std::int64_t& previous) const noexcept -> double {
    const auto now = latest();
    const double interval = previous > 0 && now > previous
                                ? static_cast<double>(now - previous) / 1000000.0
                                : 0.0;
    previous = now;
    return interval;
  }

private:
  std::atomic<std::int64_t> m_last_ns{0};
};

inline auto frame_swap_clock() -> FrameSwapClock& {
  static FrameSwapClock clock;
  return clock;
}

} // namespace Render::Profiling
