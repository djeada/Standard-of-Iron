#include "nav_profile.h"

#include <algorithm>
#include <cstdio>

namespace Engine::Core {

namespace {

constexpr std::array<std::string_view, NavProfile::k_count> k_names{
    "elapsed_us",
    "position_tests",
    "segment_tests",
    "standability_tests",
    "standability_cells_scanned",
    "nearest_standable_searches",
    "nearest_standable_cells_scanned",
    "group_routes",
    "individual_routes",
    "route_cache_hits",
    "route_cache_misses",
    "cells_expanded",
    "heap_operations",
    "dirty_cells_rebuilt",
    "grid_rebuilds",
    "requests_queued",
    "requests_dropped",
    "lock_wait_us"};

} // namespace

auto nav_counter_name(NavCounter counter) noexcept -> std::string_view {
  const auto index = static_cast<std::size_t>(counter);
  if (index >= k_names.size()) {
    return "unknown";
  }
  return k_names[index];
}

void NavProfile::begin_tick() noexcept {
  for (auto& value : m_tick) {
    value.store(0, std::memory_order_relaxed);
  }
}

void NavProfile::end_tick() noexcept {
  for (std::size_t i = 0; i < k_count; ++i) {
    const std::uint64_t value = m_tick[i].load(std::memory_order_relaxed);
    m_last_tick[i] = value;
    m_total[i] += value;
  }
  ++m_ticks;
  m_tick_ms.push(static_cast<double>(
                     m_last_tick[static_cast<std::size_t>(NavCounter::ElapsedUs)]) /
                 1000.0);
}

auto NavProfile::per_tick_average(NavCounter counter) const noexcept -> double {
  if (m_ticks == 0U) {
    return 0.0;
  }
  return static_cast<double>(total(counter)) / static_cast<double>(m_ticks);
}

void NavProfile::observe_queue_age_ticks(std::uint64_t age) noexcept {
  if (!enabled()) {
    return;
  }
  m_max_queue_age_ticks = std::max(m_max_queue_age_ticks, age);
}

void NavProfile::clear() noexcept {
  for (auto& value : m_tick) {
    value.store(0, std::memory_order_relaxed);
  }
  m_last_tick.fill(0);
  m_total.fill(0);
  m_ticks = 0;
  m_max_queue_age_ticks = 0;
  m_tick_ms.clear();
}

auto NavProfile::format_report() const -> std::string {
  std::string out;
  char line[192];

  const auto time = tick_time_ms();
  std::snprintf(line,
                sizeof(line),
                "navigation over %llu ticks: avg %.3f ms, p50 %.3f, p95 %.3f, "
                "p99 %.3f, max %.3f ms\n",
                static_cast<unsigned long long>(m_ticks),
                time.average,
                time.p50,
                time.p95,
                time.p99,
                time.maximum);
  out += line;

  std::snprintf(
      line, sizeof(line), "%-34s %14s %14s\n", "counter", "per tick", "total");
  out += line;
  out += std::string(64, '-');
  out += '\n';

  for (std::size_t i = 0; i < k_count; ++i) {
    const auto counter = static_cast<NavCounter>(i);
    if (counter == NavCounter::ElapsedUs) {
      continue;
    }
    const std::uint64_t sum = total(counter);
    if (sum == 0U) {
      continue;
    }
    const std::string_view name = nav_counter_name(counter);
    std::snprintf(line,
                  sizeof(line),
                  "%-34.*s %14.2f %14llu\n",
                  static_cast<int>(name.size()),
                  name.data(),
                  per_tick_average(counter),
                  static_cast<unsigned long long>(sum));
    out += line;
  }

  std::snprintf(line,
                sizeof(line),
                "max queue age: %llu ticks\n",
                static_cast<unsigned long long>(m_max_queue_age_ticks));
  out += line;
  return out;
}

auto nav_profile() noexcept -> NavProfile& {
  static NavProfile g_profile;
  return g_profile;
}

} // namespace Engine::Core
