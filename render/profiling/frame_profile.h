// Stage 14 — structured frame-profiling data + text formatter.
//
// The existing HumanoidRenderStats / HorseRenderStats / … are per-
// rig-type counters read ad-hoc by debug code. This header adds a
// single `FrameProfile` struct that aggregates the phase timings a
// render HUD wants to expose (collection, culling, sort, submit,
// playback) plus top-level counters (draw calls, triangles, budget
// headroom) across any number of rigs.
//
// The profile is populated from inside the renderer each frame via a
// lightweight `FrameTimer` scope (RAII + `steady_clock`). QML/Qt
// overlays read it via `format_overlay()`.
//
// Zero cost when disabled: `FrameProfile::enabled=false` short-circuits
// every phase accumulator. Toggle via `set_enabled()` from the F3 key
// handler (wiring into QML is the host's job).

#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

namespace Render::Profiling {

enum class Phase : std::uint8_t {
  Collection = 0,
  Culling = 1,
  Sort = 2,
  Submit = 3,
  Playback = 4,
  Present = 5,
  _Count
};

[[nodiscard]] inline auto phase_name(Phase p) noexcept -> const char * {
  switch (p) {
  case Phase::Collection:
    return "collect";
  case Phase::Culling:
    return "cull";
  case Phase::Sort:
    return "sort";
  case Phase::Submit:
    return "submit";
  case Phase::Playback:
    return "play";
  case Phase::Present:
    return "present";
  case Phase::_Count:
    break;
  }
  return "?";
}

struct FrameProfile {
  // All times in microseconds. 0 when the phase did not run.
  std::array<std::uint64_t,
             static_cast<std::size_t>(Phase::_Count)>
      phase_us{};

  std::uint64_t draw_calls{0};
  std::uint64_t triangles{0};
  std::uint64_t instances{0};

  // Budget headroom is "ms left in the frame before we overshoot 16.7ms".
  // Negative values mean we already overshot.
  double budget_headroom_ms{16.67};

  std::uint64_t frame_index{0};

  bool enabled{true};

  void reset() {
    for (auto &v : phase_us) {
      v = 0;
    }
    draw_calls = 0;
    triangles = 0;
    instances = 0;
    budget_headroom_ms = 16.67;
  }

  void add_phase_us(Phase p, std::uint64_t us) {
    if (!enabled) {
      return;
    }
    phase_us[static_cast<std::size_t>(p)] += us;
  }

  [[nodiscard]] auto total_us() const noexcept -> std::uint64_t {
    std::uint64_t total = 0;
    for (auto v : phase_us) {
      total += v;
    }
    return total;
  }
};

// RAII scope that measures elapsed time and reports to a FrameProfile
// phase on destruction. Cheap (one steady_clock sample at ctor/dtor)
// and disabled when the profile is disabled.
class PhaseScope {
public:
  PhaseScope(FrameProfile *profile, Phase phase)
      : m_profile(profile), m_phase(phase),
        m_start(profile != nullptr && profile->enabled
                    ? std::chrono::steady_clock::now()
                    : std::chrono::steady_clock::time_point{}) {}

  PhaseScope(const PhaseScope &) = delete;
  auto operator=(const PhaseScope &) -> PhaseScope & = delete;
  PhaseScope(PhaseScope &&) = delete;
  auto operator=(PhaseScope &&) -> PhaseScope & = delete;

  ~PhaseScope() {
    if (m_profile == nullptr || !m_profile->enabled) {
      return;
    }
    auto const now = std::chrono::steady_clock::now();
    auto const us = std::chrono::duration_cast<std::chrono::microseconds>(
                        now - m_start)
                        .count();
    m_profile->add_phase_us(m_phase, static_cast<std::uint64_t>(us));
  }

private:
  FrameProfile *m_profile;
  Phase m_phase;
  std::chrono::steady_clock::time_point m_start;
};

// Format the profile as a multi-line overlay string suitable for a
// debug HUD. Deterministic output — safe to snapshot in tests.
[[nodiscard]] auto format_overlay(const FrameProfile &profile) -> std::string;

// Process-global profile. Renderer writes to it; HUD reads from it.
// Atomic pointer lets the HUD swap in its own profile for scoped
// measurements without disturbing the renderer.
[[nodiscard]] auto global_profile() -> FrameProfile &;

} // namespace Render::Profiling
