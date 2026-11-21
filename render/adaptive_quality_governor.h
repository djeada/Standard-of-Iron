#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

#include "render/graphics_settings.h"

namespace Render {

// Watches presented frame intervals during play and asks for the next lower
// graphics preset when the game clearly cannot keep up. It only ever steps
// down, one preset at a time, and leaves the choice alone once the player has
// picked a preset in Settings.
//
// The judgement uses the median interval over a few seconds of play, so a
// loading hitch, a shader compile or an autosave never triggers it; only a
// battle that is slow most of the time does.
class AdaptiveQualityGovernor {
public:
  struct Policy {
    // Seconds of play to ignore after a match appears or after a change, so
    // first-use uploads and shader recompiles settle first.
    double warmup_seconds = 6.0;
    // Seconds of play judged at once.
    double window_seconds = 6.0;
    // A median frame slower than this is "too slow" (about 24 fps).
    double slow_frame_ms = 1000.0 / 24.0;
    // Single intervals longer than this are stalls, not frame rate; they are
    // clamped so one long hitch cannot dominate the window.
    double stall_clamp_ms = 250.0;
  };

  AdaptiveQualityGovernor() = default;
  explicit AdaptiveQualityGovernor(Policy policy)
      : m_policy(policy) {}

  // Feed one presented frame. `eligible` is false whenever the interval says
  // nothing about gameplay speed (loading screen, window in the background,
  // film or benchmark capture); that restarts the judgement. Returns the
  // preset to switch to when a step down is warranted.
  auto observe(double frame_interval_ms,
               bool eligible,
               GraphicsQuality current,
               bool chosen_by_user) -> std::optional<GraphicsQuality> {
    if (!eligible || chosen_by_user || current == GraphicsQuality::Low ||
        frame_interval_ms <= 0.0) {
      reset();
      return std::nullopt;
    }
    if (m_current.has_value() && *m_current != current) {
      // The preset changed under us (Settings, or our own step): start over.
      reset();
    }
    m_current = current;

    const double interval = std::min(frame_interval_ms, m_policy.stall_clamp_ms);
    m_elapsed_ms += interval;
    if (m_elapsed_ms <= m_policy.warmup_seconds * 1000.0) {
      return std::nullopt;
    }

    m_window.push_back(interval);
    m_window_ms += interval;
    if (m_window_ms < m_policy.window_seconds * 1000.0) {
      return std::nullopt;
    }

    const double median = median_of(m_window);
    m_window.clear();
    m_window_ms = 0.0;
    if (median <= m_policy.slow_frame_ms) {
      return std::nullopt;
    }

    const auto lower = static_cast<GraphicsQuality>(static_cast<int>(current) - 1);
    reset();
    return lower;
  }

  void reset() {
    m_elapsed_ms = 0.0;
    m_window.clear();
    m_window_ms = 0.0;
    m_current.reset();
  }

  [[nodiscard]] auto policy() const -> const Policy& { return m_policy; }

private:
  static auto median_of(std::vector<double> values) -> double {
    if (values.empty()) {
      return 0.0;
    }
    const auto middle = values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2);
    std::nth_element(values.begin(), middle, values.end());
    return *middle;
  }

  Policy m_policy{};
  double m_elapsed_ms = 0.0;
  std::vector<double> m_window;
  double m_window_ms = 0.0;
  std::optional<GraphicsQuality> m_current;
};

} // namespace Render
