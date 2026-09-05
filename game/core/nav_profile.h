#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "utils/percentile.h"

namespace Engine::Core {

enum class NavCounter : std::uint8_t {
  ElapsedUs = 0,
  PositionTests,
  SegmentTests,
  StandabilityTests,
  StandabilityCellsScanned,
  NearestStandableSearches,
  NearestStandableCellsScanned,
  GroupRoutes,
  IndividualRoutes,
  RouteCacheHits,
  RouteCacheMisses,
  CellsExpanded,
  HeapOperations,
  DirtyCellsRebuilt,
  GridRebuilds,
  RequestsQueued,
  RequestsDropped,
  LockWaitUs,
  RouteCacheEvictions,
  RouteCacheKept,
  RouteCacheFlushes,
  RegionMapRebuilds,
  _Count
};

[[nodiscard]] auto nav_counter_name(NavCounter counter) noexcept -> std::string_view;

class NavProfile {
public:
  static constexpr std::size_t k_count = static_cast<std::size_t>(NavCounter::_Count);
  static constexpr std::size_t k_tick_window = 600U;

  void set_enabled(bool enabled) noexcept {
    m_enabled.store(enabled, std::memory_order_relaxed);
  }
  [[nodiscard]] auto enabled() const noexcept -> bool {
    return m_enabled.load(std::memory_order_relaxed);
  }

  void add(NavCounter counter, std::uint64_t amount = 1) noexcept {
    if (!enabled()) {
      return;
    }
    m_tick[static_cast<std::size_t>(counter)].fetch_add(amount,
                                                        std::memory_order_relaxed);
  }

  void begin_tick() noexcept;
  void end_tick() noexcept;

  [[nodiscard]] auto last_tick(NavCounter counter) const noexcept -> std::uint64_t {
    return m_last_tick[static_cast<std::size_t>(counter)];
  }
  [[nodiscard]] auto total(NavCounter counter) const noexcept -> std::uint64_t {
    return m_total[static_cast<std::size_t>(counter)];
  }
  [[nodiscard]] auto per_tick_average(NavCounter counter) const noexcept -> double;

  [[nodiscard]] auto ticks() const noexcept -> std::uint64_t { return m_ticks; }

  [[nodiscard]] auto tick_time_ms() const -> Utils::Stats::Distribution {
    return m_tick_ms.distribution();
  }

  [[nodiscard]] auto max_queue_age_ticks() const noexcept -> std::uint64_t {
    return m_max_queue_age_ticks;
  }
  void observe_queue_age_ticks(std::uint64_t age) noexcept;

  void clear() noexcept;

  [[nodiscard]] auto format_report() const -> std::string;

private:
  std::atomic_bool m_enabled{false};
  std::array<std::atomic<std::uint64_t>, k_count> m_tick{};
  std::array<std::uint64_t, k_count> m_last_tick{};
  std::array<std::uint64_t, k_count> m_total{};
  std::uint64_t m_ticks{0};
  std::uint64_t m_max_queue_age_ticks{0};
  Utils::Stats::SampleWindow<k_tick_window> m_tick_ms;
};

[[nodiscard]] auto nav_profile() noexcept -> NavProfile&;

inline void count_nav(NavCounter counter, std::uint64_t amount = 1) noexcept {
  nav_profile().add(counter, amount);
}

inline thread_local std::uint64_t* g_nav_child_sink = nullptr;

class NavScope {
public:
  explicit NavScope(NavCounter counter) noexcept
      : m_counter(counter)
      , m_enabled(nav_profile().enabled())
      , m_start(m_enabled ? std::chrono::steady_clock::now()
                          : std::chrono::steady_clock::time_point{}) {
    if (m_enabled) {
      m_parent_sink = g_nav_child_sink;
      g_nav_child_sink = &m_child_us;
    }
  }

  NavScope(const NavScope&) = delete;
  auto operator=(const NavScope&) -> NavScope& = delete;
  NavScope(NavScope&&) = delete;
  auto operator=(NavScope&&) -> NavScope& = delete;

  ~NavScope() {
    if (!m_enabled) {
      return;
    }
    NavProfile& profile = nav_profile();
    profile.add(m_counter);
    const auto elapsed = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - m_start)
            .count());
    g_nav_child_sink = m_parent_sink;
    profile.add(NavCounter::ElapsedUs,
                elapsed > m_child_us ? elapsed - m_child_us : 0U);
    if (m_parent_sink != nullptr) {
      *m_parent_sink += elapsed;
    }
  }

private:
  NavCounter m_counter;
  bool m_enabled;
  std::chrono::steady_clock::time_point m_start;
  std::uint64_t m_child_us{0};
  std::uint64_t* m_parent_sink{nullptr};
};

class NavTickScope {
public:
  NavTickScope() noexcept
      : m_enabled(nav_profile().enabled()) {
    if (m_enabled) {
      g_nav_child_sink = nullptr;
      nav_profile().begin_tick();
    }
  }

  NavTickScope(const NavTickScope&) = delete;
  auto operator=(const NavTickScope&) -> NavTickScope& = delete;
  NavTickScope(NavTickScope&&) = delete;
  auto operator=(NavTickScope&&) -> NavTickScope& = delete;

  ~NavTickScope() {
    if (m_enabled) {
      nav_profile().end_tick();
    }
  }

private:
  bool m_enabled;
};

} // namespace Engine::Core
