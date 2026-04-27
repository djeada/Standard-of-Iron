#pragma once

#include "../game/systems/formation_system.h"
#include "../game/systems/nation_id.h"
#include "../game/units/spawn_type.h"
#include "humanoid/formation_calculator.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Render::GL {

struct SelectionRingLayoutInput {
  Game::Units::SpawnType spawn_type{Game::Units::SpawnType::Archer};
  Game::Systems::NationID nation_id{Game::Systems::NationID::RomanRepublic};
  int individuals_per_unit{1};
  int max_units_per_row{1};
  float health_ratio{1.0F};
  float ring_size{0.5F};
  std::uint32_t seed{0U};
  QVector3D position{0.0F, 0.0F, 0.0F};
  QVector3D rotation{0.0F, 0.0F, 0.0F};
  QVector3D scale{1.0F, 1.0F, 1.0F};
};

struct SelectionRingPlacement {
  float world_x{0.0F};
  float world_z{0.0F};
  float ring_size{0.5F};
};

namespace Detail {

[[nodiscard]] inline auto selection_ring_nation(
    Game::Systems::NationID nation_id) -> FormationCalculatorFactory::Nation {
  switch (nation_id) {
  case Game::Systems::NationID::Carthage:
    return FormationCalculatorFactory::Nation::Carthage;
  case Game::Systems::NationID::RomanRepublic:
    return FormationCalculatorFactory::Nation::Roman;
  }
  return FormationCalculatorFactory::Nation::Roman;
}

[[nodiscard]] inline auto
selection_ring_category(Game::Units::SpawnType spawn_type)
    -> FormationCalculatorFactory::UnitCategory {
  switch (spawn_type) {
  case Game::Units::SpawnType::MountedKnight:
  case Game::Units::SpawnType::HorseArcher:
  case Game::Units::SpawnType::HorseSpearman:
    return FormationCalculatorFactory::UnitCategory::Cavalry;
  default:
    return FormationCalculatorFactory::UnitCategory::Infantry;
  }
}

[[nodiscard]] inline auto selection_ring_spacing(
    FormationCalculatorFactory::UnitCategory category) -> float {
  return (category == FormationCalculatorFactory::UnitCategory::Cavalry)
             ? cavalry_formation_spacing()
             : 0.75F;
}

[[nodiscard]] inline auto
selection_ring_visual_size(Game::Units::SpawnType spawn_type,
                           int individuals_per_unit,
                           float unit_ring_size) -> float {
  if (individuals_per_unit <= 1) {
    return unit_ring_size;
  }

  auto const category = selection_ring_category(spawn_type);
  float const max_visual_size = selection_ring_spacing(category) * 0.48F;
  return std::min(unit_ring_size * 0.25F, max_visual_size);
}

[[nodiscard]] inline auto fast_random(std::uint32_t &state) -> float {
  state = state * 1664525U + 1013904223U;
  return float(state & 0x7FFFFFU) / float(0x7FFFFFU);
}

} // namespace Detail

[[nodiscard]] inline auto
build_selection_ring_layout(const SelectionRingLayoutInput &input)
    -> std::vector<SelectionRingPlacement> {
  int const total_units = std::max(1, input.individuals_per_unit);
  float const health_ratio = std::clamp(input.health_ratio, 0.0F, 1.0F);
  int const visible_count = std::min(
      total_units,
      std::max(1,
               static_cast<int>(std::ceil(health_ratio * float(total_units)))));

  std::vector<SelectionRingPlacement> placements;
  placements.reserve(static_cast<std::size_t>(visible_count));

  auto const category = Detail::selection_ring_category(input.spawn_type);
  auto const *calculator = FormationCalculatorFactory::get_calculator(
      Detail::selection_ring_nation(input.nation_id), category);
  if (calculator == nullptr) {
    placements.push_back(
        {input.position.x(), input.position.z(), input.ring_size});
    return placements;
  }

  int const cols = std::max(1, input.max_units_per_row);
  int const rows = std::max(1, (total_units + cols - 1) / cols);
  float const spacing = Detail::selection_ring_spacing(category);

  for (int idx = 0; idx < visible_count; ++idx) {
    int const row = idx / cols;
    int const col = idx % cols;
    auto const offset = calculator->calculate_offset(idx, row, col, rows, cols,
                                                     spacing, input.seed);

    float yaw = input.rotation.y();
    if (total_units > 1) {
      std::uint32_t rng_state =
          input.seed ^ static_cast<std::uint32_t>(idx * 9176U);
      yaw += (Detail::fast_random(rng_state) - 0.5F) * 5.0F;
    }

    QMatrix4x4 model;
    model.translate(input.position);
    model.rotate(yaw, 0.0F, 1.0F, 0.0F);
    model.scale(input.scale);
    model.translate(offset.offset_x, 0.0F, offset.offset_z);

    QVector3D const world = model.map(QVector3D(0.0F, 0.0F, 0.0F));
    placements.push_back({world.x(), world.z(), input.ring_size});
  }

  return placements;
}

} // namespace Render::GL
