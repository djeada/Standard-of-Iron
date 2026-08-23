#pragma once

#include <cstdint>
#include <optional>

#include "render/creature/part_graph.h"

namespace Render::Creature::Pipeline {

struct LodDistanceThresholds {
  float full{12.0F};
  float cull{200.0F};
};

struct CreatureLodDecisionInputs {
  std::optional<CreatureLOD> forced_lod{};
  bool has_camera{true};
  float distance{0.0F};

  float apparent_size_scale{1.0F};

  LodDistanceThresholds thresholds{};

  bool apply_visibility_budget{false};
  bool budget_grant_full{true};
};

enum class CullReason : std::uint8_t {
  None = 0,
  Distance = 1,
};

struct CreatureLodDecision {
  CreatureLOD lod{CreatureLOD::Full};
  bool culled{false};
  CullReason reason{CullReason::None};

  [[nodiscard]] constexpr auto rendered() const noexcept -> bool { return !culled; }
};

[[nodiscard]] auto
select_distance_lod(float distance,
                    const LodDistanceThresholds& t) noexcept -> CreatureLOD;

[[nodiscard]] auto
select_distance_lod(float distance,
                    const LodDistanceThresholds& t,
                    float apparent_size_scale) noexcept -> CreatureLOD;

[[nodiscard]] auto lod_reference_distance(float distance,
                                          float apparent_size_scale) noexcept -> float;

[[nodiscard]] auto decide_creature_lod(const CreatureLodDecisionInputs& in) noexcept
    -> CreatureLodDecision;

} // namespace Render::Creature::Pipeline
