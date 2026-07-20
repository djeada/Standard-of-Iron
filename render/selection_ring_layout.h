#pragma once

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <span>
#include <vector>

#include "../game/core/component.h"
#include "../game/units/spawn_type.h"
#include "humanoid/formation_calculator.h"

namespace Render::GL {

struct SelectionRingLayoutInput {

  std::span<const Engine::Core::FormationSoldierPresentation> soldiers{};
  float ring_size{0.5F};
  QVector3D position{0.0F, 0.0F, 0.0F};
  float yaw_degrees{0.0F};
};

struct SelectionRingPlacement {
  float world_x{0.0F};
  float world_z{0.0F};
  float ring_size{0.5F};
};

namespace Detail {
[[nodiscard]] inline auto selection_ring_spacing(Game::Units::SpawnType spawn_type,
                                                 float configured_spacing) -> float {
  return resolve_formation_spacing(spawn_type, configured_spacing);
}

[[nodiscard]] inline auto
selection_ring_visual_size(Game::Units::SpawnType spawn_type,
                           int individuals_per_unit,
                           float unit_ring_size,
                           float formation_spacing = 0.0F) -> float {
  if (individuals_per_unit <= 1) {
    return unit_ring_size;
  }

  float const max_visual_size =
      selection_ring_spacing(spawn_type, formation_spacing) * 0.48F;
  return std::min(unit_ring_size * 0.25F, max_visual_size);
}

} // namespace Detail

[[nodiscard]] inline auto build_selection_ring_layout(
    const SelectionRingLayoutInput& input) -> std::vector<SelectionRingPlacement> {
  std::vector<SelectionRingPlacement> placements;
  if (input.soldiers.empty()) {
    placements.push_back({input.position.x(), input.position.z(), input.ring_size});
    return placements;
  }

  placements.reserve(input.soldiers.size());
  float const yaw = input.yaw_degrees * std::numbers::pi_v<float> / 180.0F;
  float const sin_yaw = std::sin(yaw);
  float const cos_yaw = std::cos(yaw);
  for (auto const& soldier : input.soldiers) {
    if (!soldier.alive) {
      continue;
    }
    float const world_x =
        input.position.x() + cos_yaw * soldier.local_x + sin_yaw * soldier.local_z;
    float const world_z =
        input.position.z() - sin_yaw * soldier.local_x + cos_yaw * soldier.local_z;
    placements.push_back({world_x, world_z, input.ring_size});
  }

  return placements;
}

} // namespace Render::GL
