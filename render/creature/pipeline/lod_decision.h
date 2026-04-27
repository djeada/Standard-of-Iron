#pragma once

#include "../part_graph.h"

#include <cstdint>
#include <optional>

namespace Render::Creature::Pipeline {

struct LodDistanceThresholds {
  float full{12.0F};
  float minimal{40.0F};
};

struct TemporalSkipParams {
  float distance_minimal{45.0F};
  std::uint32_t period_minimal{3};
};

struct CreatureLodDecisionInputs {
  std::optional<CreatureLOD> forced_lod{};
  bool has_camera{true};
  float distance{0.0F};

  LodDistanceThresholds thresholds{};

  bool apply_visibility_budget{false};
  bool budget_grant_full{true};

  TemporalSkipParams temporal{};
  std::uint32_t frame_index{0};
  std::uint32_t instance_seed{0};
};

enum class CullReason : std::uint8_t {
  None = 0,
  Billboard = 1,
  Temporal = 2,
};

struct CreatureLodDecision {
  CreatureLOD lod{CreatureLOD::Full};
  bool culled{false};
  CullReason reason{CullReason::None};

  [[nodiscard]] constexpr auto rendered() const noexcept -> bool {
    return !culled;
  }
};

[[nodiscard]] auto
select_distance_lod(float distance,
                    const LodDistanceThresholds &t) noexcept -> CreatureLOD;

[[nodiscard]] auto
should_render_temporal(std::uint32_t frame, std::uint32_t seed,
                       std::uint32_t period) noexcept -> bool;

[[nodiscard]] auto decide_creature_lod(
    const CreatureLodDecisionInputs &in) noexcept -> CreatureLodDecision;

} // namespace Render::Creature::Pipeline
