#pragma once

#include <QJsonObject>

#include <cstdint>
#include <vector>

#include "game/core/system_profiler.h"

namespace Render::Profiling {

struct PerformanceBudget {
  double frame_p50_ms{12.0};
  double frame_p95_ms{16.67};
  double frame_p99_ms{20.0};
  double frame_max_ms{33.3};
  double update_average_ms{2.0};
  double update_p95_ms{4.0};
  double navigation_average_ms{0.25};
  double navigation_p95_ms{1.0};
  double render_submit_p95_ms{3.0};
  double gpu_total_p95_ms{12.0};

  [[nodiscard]] static auto release_gate() noexcept -> PerformanceBudget { return {}; }

  [[nodiscard]] static auto
  scale_gate(double frame_p95_ms) noexcept -> PerformanceBudget;
};

struct PerformanceMeasurement {
  std::size_t frames{0};
  double frame_p50_ms{0.0};
  double frame_p95_ms{0.0};
  double frame_p99_ms{0.0};
  double frame_max_ms{0.0};
  double update_average_ms{0.0};
  double update_p95_ms{0.0};
  double render_submit_p95_ms{0.0};
  double gpu_shadow_p95_ms{0.0};
  double gpu_color_p95_ms{0.0};
  bool gpu_timed{false};
  bool full_creature_lod{false};
  bool ultra_preset{false};
};

[[nodiscard]] auto asset_counters_json() -> QJsonObject;

[[nodiscard]] auto navigation_counters_json() -> QJsonObject;

[[nodiscard]] auto
system_profiler_json(const Engine::Core::SystemProfiler& profiler) -> QJsonObject;

[[nodiscard]] auto
budget_verdict_json(const PerformanceBudget& budget,
                    const PerformanceMeasurement& measured) -> QJsonObject;

} // namespace Render::Profiling
