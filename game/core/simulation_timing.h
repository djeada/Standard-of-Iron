#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

namespace Engine::Core::Timing {

// Microsecond accumulators the simulation publishes for diagnostics.
//
// The renderer's frame profile reports these, but the simulation must not write
// into a renderer-owned struct to provide them: that would make gameplay depend
// on render.  The simulation accumulates here and the profiler reads it, which
// keeps the dependency pointing render -> game like everything else.
class MicrosecondAccumulator {
public:
  void add(std::uint64_t microseconds) noexcept {
    m_value.fetch_add(microseconds, std::memory_order_relaxed);
  }

  [[nodiscard]] auto value() const noexcept -> std::uint64_t {
    return m_value.load(std::memory_order_relaxed);
  }

  void reset() noexcept { m_value.store(0, std::memory_order_relaxed); }

private:
  std::atomic<std::uint64_t> m_value{0};
};

[[nodiscard]] auto combat_state_update() noexcept -> MicrosecondAccumulator&;

// RAII timer that folds its lifetime into an accumulator.
class ScopedAccumulator {
public:
  explicit ScopedAccumulator(MicrosecondAccumulator& accumulator) noexcept
      : m_accumulator(&accumulator)
      , m_start(std::chrono::steady_clock::now()) {}

  ScopedAccumulator(const ScopedAccumulator&) = delete;
  auto operator=(const ScopedAccumulator&) -> ScopedAccumulator& = delete;
  ScopedAccumulator(ScopedAccumulator&&) = delete;
  auto operator=(ScopedAccumulator&&) -> ScopedAccumulator& = delete;

  ~ScopedAccumulator() {
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                             std::chrono::steady_clock::now() - m_start)
                             .count();
    m_accumulator->add(static_cast<std::uint64_t>(elapsed));
  }

private:
  MicrosecondAccumulator* m_accumulator;
  std::chrono::steady_clock::time_point m_start;
};

} // namespace Engine::Core::Timing
