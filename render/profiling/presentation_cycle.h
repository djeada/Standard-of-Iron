#pragma once

#include <atomic>
#include <cmath>
#include <cstdint>

namespace Render::Profiling {

struct PresentationCyclePosition {
  double x;
  double z;
  double zoom;
};

inline auto presentation_cycle_position(double seconds) -> PresentationCyclePosition {
  constexpr double pi = 3.14159265358979323846;
  const double angle = std::fmod(seconds, 20.0) * pi / 10.0;
  return {
      20.0 * std::sin(angle), 10.0 * (1.0 - std::cos(angle)), 2.0 * std::sin(angle)};
}

struct PresentationCycleProgress {
  std::atomic<std::uint64_t> updates{0};
  std::atomic<std::uint64_t> completed_cycles{0};
};

inline auto presentation_cycle_progress() -> PresentationCycleProgress& {
  static PresentationCycleProgress progress;
  return progress;
}

} // namespace Render::Profiling
