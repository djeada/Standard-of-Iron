#include "audio_clock.h"

#include <atomic>
#include <cstdint>

namespace Game::Audio::Clock {
namespace {

std::atomic<bool> g_rendered{false};

std::atomic<std::int64_t> g_rendered_ticks{0};

} // namespace

void use_rendered_time(bool rendered) noexcept {
  if (rendered && !g_rendered.load(std::memory_order_acquire)) {
    g_rendered_ticks.store(std::chrono::steady_clock::now().time_since_epoch().count(),
                           std::memory_order_release);
  }
  g_rendered.store(rendered, std::memory_order_release);
}

auto uses_rendered_time() noexcept -> bool {
  return g_rendered.load(std::memory_order_acquire);
}

void advance_rendered(double seconds) noexcept {
  if (seconds <= 0.0) {
    return;
  }
  auto const step = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
      std::chrono::duration<double>(seconds));
  g_rendered_ticks.fetch_add(step.count(), std::memory_order_acq_rel);
}

auto now() noexcept -> TimePoint {
  if (!g_rendered.load(std::memory_order_acquire)) {
    return std::chrono::steady_clock::now();
  }
  return TimePoint(std::chrono::steady_clock::duration(
      g_rendered_ticks.load(std::memory_order_acquire)));
}

} // namespace Game::Audio::Clock
