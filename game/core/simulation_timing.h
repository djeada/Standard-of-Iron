#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

namespace Engine::Core::Timing {

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

[[nodiscard]] auto commander_motor() noexcept -> MicrosecondAccumulator&;
[[nodiscard]] auto commander_targeting() noexcept -> MicrosecondAccumulator&;
[[nodiscard]] auto commander_weapon_trace() noexcept -> MicrosecondAccumulator&;
[[nodiscard]] auto commander_engagement() noexcept -> MicrosecondAccumulator&;
[[nodiscard]] auto commander_camera() noexcept -> MicrosecondAccumulator&;

struct RpgCostSample {
  std::uint64_t motor_us{0};
  std::uint64_t targeting_us{0};
  std::uint64_t weapon_trace_us{0};
  std::uint64_t engagement_us{0};
  std::uint64_t camera_us{0};
};

[[nodiscard]] auto sample_and_reset_rpg_costs() noexcept -> RpgCostSample;

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
