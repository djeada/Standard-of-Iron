#pragma once

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <span>
#include <vector>

#include "entity/formation_instance_layout.h"
#include "game/core/component.h"
#include "game/units/spawn_type.h"
#include "humanoid/runtime/unit_layout_spacing.h"

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
[[nodiscard]] inline auto selection_ring_spacing(const Game::Units::TroopConfig& config,
                                                 Game::Units::SpawnType spawn_type,
                                                 float configured_spacing) -> float {
  return resolve_formation_spacing(config, spawn_type, configured_spacing);
}

[[nodiscard]] inline auto
selection_ring_visual_size(const Game::Units::TroopConfig& config,
                           Game::Units::SpawnType spawn_type,
                           int individuals_per_unit,
                           float unit_ring_size,
                           float formation_spacing = 0.0F) -> float {
  if (individuals_per_unit <= 1) {
    return unit_ring_size;
  }

  float const max_visual_size =
      selection_ring_spacing(config, spawn_type, formation_spacing) * 0.48F;
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
  auto const frame =
      Render::Entity::formation_world_frame(input.position, input.yaw_degrees);
  for (auto const& soldier : input.soldiers) {
    if (!soldier.alive) {
      continue;
    }

    auto const anchor = frame.map(QVector3D(soldier.local_x, 0.0F, soldier.local_z));
    placements.push_back({anchor.x(), anchor.z(), input.ring_size});
  }

  return placements;
}

} // namespace Render::GL
