#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "utils/percentile.h"

namespace Engine::Core {

class StartupProfiler {
public:
  struct PhaseRecord {
    std::string name;
    double duration_ms = 0.0;
    std::uint64_t entries = 0;
    std::int64_t items = 0;
  };

  struct Counter {
    std::string name;
    std::int64_t value = 0;
  };

  struct FrameWindow {
    Utils::Stats::Distribution distribution;
    double window_seconds = 0.0;
  };

  struct Report {
    std::string label;
    std::vector<PhaseRecord> phases;
    std::vector<Counter> counters;
    double overlay_released_ms = -1.0;
    double first_playable_frame_ms = -1.0;
    double first_frame_duration_ms = -1.0;
    FrameWindow first_second;
    FrameWindow first_five_seconds;
  };

  [[nodiscard]] static auto instance() -> StartupProfiler&;

  [[nodiscard]] static auto tracing_enabled() -> bool;

  [[nodiscard]] static auto reporting_enabled() -> bool;

  void begin_run(std::string label);

  void record_phase(std::string_view name, double duration_ms, std::int64_t items = 0);

  void add_counter(std::string_view name, std::int64_t value);

  void mark_overlay_released();

  void record_playable_frame(double frame_duration_ms);

  [[nodiscard]] auto active() const -> bool;

  [[nodiscard]] auto report() const -> Report;

  [[nodiscard]] auto format_report() const -> std::string;

  [[nodiscard]] auto report_as_json() const -> std::string;

  void log_report() const;

  [[nodiscard]] static auto trace_file_path() -> std::string;

  auto write_report(const std::string& path) const -> bool;

private:
  struct FrameSample {
    double offset_ms = 0.0;
    double duration_ms = 0.0;
  };

  [[nodiscard]] auto elapsed_ms_locked() const -> double;

  mutable std::mutex m_mutex;
  bool m_active = false;
  std::string m_label;
  std::chrono::steady_clock::time_point m_run_started;
  std::vector<PhaseRecord> m_phases;
  std::vector<Counter> m_counters;
  double m_overlay_released_ms = -1.0;
  double m_first_playable_frame_ms = -1.0;
  std::vector<FrameSample> m_frames;
};

class ScopedStartupPhase {
public:
  explicit ScopedStartupPhase(std::string_view name);
  ~ScopedStartupPhase();

  ScopedStartupPhase(const ScopedStartupPhase&) = delete;
  ScopedStartupPhase(ScopedStartupPhase&&) = delete;
  auto operator=(const ScopedStartupPhase&) -> ScopedStartupPhase& = delete;
  auto operator=(ScopedStartupPhase&&) -> ScopedStartupPhase& = delete;

  void add_items(std::int64_t count) { m_items += count; }

private:
  std::string m_name;
  std::int64_t m_items = 0;
  std::chrono::steady_clock::time_point m_started;
};

} // namespace Engine::Core
