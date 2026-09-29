#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "movement_trace_analysis.h"

namespace Engine::Core::movement_analysis {

struct OpenFinding {
  bool open{false};
  std::size_t index{0};
};

class FindingSink {
public:
  explicit FindingSink(MovementAnalysis& analysis)
      : m_analysis(analysis) {}

  void add(MovementFindingKind kind,
           EntityID entity,
           std::uint32_t slot,
           std::uint64_t tick,
           float magnitude,
           std::string detail);

  void extend(OpenFinding& run,
              MovementFindingKind kind,
              EntityID entity,
              std::uint32_t slot,
              std::uint64_t tick,
              float magnitude,
              std::string detail);

  static void close(OpenFinding& run) { run.open = false; }

  [[nodiscard]] auto finding_count() const -> std::size_t {
    return m_analysis.findings.size();
  }

private:
  MovementAnalysis& m_analysis;
};

template <typename... Args>
auto text(const char* fmt, Args... args) -> std::string {
  std::array<char, 256> buffer{};
  int const written = std::snprintf(buffer.data(), buffer.size(), fmt, args...);
  if (written <= 0) {
    return {};
  }
  return std::string(buffer.data(),
                     std::min(static_cast<std::size_t>(written), buffer.size() - 1U));
}

[[nodiscard]] constexpr auto
state_claims_travel(std::uint8_t presentation_state) -> bool {
  return presentation_state == 2U || presentation_state == 3U ||
         presentation_state == 6U;
}

[[nodiscard]] constexpr auto
state_claims_stillness(std::uint8_t presentation_state) -> bool {
  return presentation_state == 0U;
}

[[nodiscard]] auto is_declared_hold(MovementOrderState state) -> bool;

[[nodiscard]] auto accepted_speed(const MovementTroopSample& sample) -> float;

void analyze_troops(const std::vector<MovementTroopSample>& troops,
                    const MovementGateThresholds& thresholds,
                    MovementAnalysis& analysis,
                    FindingSink& sink);

void analyze_soldiers(const std::vector<MovementSoldierSample>& soldiers,
                      const MovementGateThresholds& thresholds,
                      MovementAnalysis& analysis,
                      FindingSink& sink);

} // namespace Engine::Core::movement_analysis
