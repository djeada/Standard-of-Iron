#include "formation_calculator.h"

#include "../../game/systems/formation_system.h"
#include "../../game/units/troop_config.h"
#include "../horse/dimensions.h"

#include <algorithm>

namespace Render::GL {

auto cavalry_formation_spacing(float mount_scale) -> float {
  HorseDimensions dims = make_horse_dimensions(0U);
  scale_horse_dimensions(dims, std::max(0.1F, mount_scale));
  float const horse_length =
      dims.body_length + dims.head_length * 0.85F + dims.tail_length * 0.10F;
  return horse_length * 1.10F;
}

auto resolve_formation_spacing(Game::Units::SpawnType spawn_type,
                               float configured_spacing,
                               float mount_scale) -> float {
  switch (spawn_type) {
  case Game::Units::SpawnType::MountedKnight:
  case Game::Units::SpawnType::HorseArcher:
  case Game::Units::SpawnType::HorseSpearman:
    return cavalry_formation_spacing(mount_scale);
  default:
    break;
  }

  if (configured_spacing > 0.0F) {
    return configured_spacing;
  }
  return Game::Units::TroopConfig::instance().get_formation_spacing(spawn_type);
}

auto RomanInfantryFormation::calculate_offset(
    int idx, int row, int col, int rows, int cols, float spacing,
    uint32_t seed) const -> FormationOffset {
  auto const offset =
      Game::Systems::FormationSystem::instance().get_local_offset(
          Game::Systems::FormationType::Roman,
          Game::Systems::FormationUnitCategory::Infantry, idx, row, col, rows,
          cols, spacing, seed);
  return {offset.offset_x, offset.offset_z, offset.yaw_offset};
}

auto RomanCavalryFormation::calculate_offset(
    int idx, int row, int col, int rows, int cols, float spacing,
    uint32_t seed) const -> FormationOffset {
  auto const offset =
      Game::Systems::FormationSystem::instance().get_local_offset(
          Game::Systems::FormationType::Roman,
          Game::Systems::FormationUnitCategory::Cavalry, idx, row, col, rows,
          cols, spacing, seed);
  return {offset.offset_x, offset.offset_z, offset.yaw_offset};
}

auto CarthageInfantryFormation::calculate_offset(
    int idx, int row, int col, int rows, int cols, float spacing,
    uint32_t seed) const -> FormationOffset {
  auto const offset =
      Game::Systems::FormationSystem::instance().get_local_offset(
          Game::Systems::FormationType::Carthage,
          Game::Systems::FormationUnitCategory::Infantry, idx, row, col, rows,
          cols, spacing, seed);
  return {offset.offset_x, offset.offset_z, offset.yaw_offset};
}

auto CarthageCavalryFormation::calculate_offset(
    int idx, int row, int col, int rows, int cols, float spacing,
    uint32_t seed) const -> FormationOffset {
  auto const offset =
      Game::Systems::FormationSystem::instance().get_local_offset(
          Game::Systems::FormationType::Carthage,
          Game::Systems::FormationUnitCategory::Cavalry, idx, row, col, rows,
          cols, spacing, seed);
  return {offset.offset_x, offset.offset_z, offset.yaw_offset};
}

auto BuilderCircleFormation::calculate_offset(
    int idx, int row, int col, int rows, int cols, float spacing,
    uint32_t seed) const -> FormationOffset {
  auto const offset =
      Game::Systems::FormationSystem::instance().get_local_offset(
          Game::Systems::FormationType::Roman,
          Game::Systems::FormationUnitCategory::BuilderConstruction, idx, row,
          col, rows, cols, spacing, seed);
  return {offset.offset_x, offset.offset_z, offset.yaw_offset};
}

RomanInfantryFormation FormationCalculatorFactory::s_roman_infantry;
RomanCavalryFormation FormationCalculatorFactory::s_roman_cavalry;
CarthageInfantryFormation FormationCalculatorFactory::s_carthage_infantry;
CarthageCavalryFormation FormationCalculatorFactory::s_carthage_cavalry;
BuilderCircleFormation FormationCalculatorFactory::s_builder_circle;

auto FormationCalculatorFactory::get_calculator(
    Nation nation, UnitCategory category) -> const IFormationCalculator * {

  if (category == UnitCategory::BuilderConstruction) {
    return &s_builder_circle;
  }

  switch (nation) {
  case Nation::Roman:
    switch (category) {
    case UnitCategory::Infantry:
      return &s_roman_infantry;
    case UnitCategory::Cavalry:
      return &s_roman_cavalry;
    case UnitCategory::BuilderConstruction:
      return &s_builder_circle;
    }
    break;

  case Nation::Carthage:
    switch (category) {
    case UnitCategory::Infantry:
      return &s_carthage_infantry;
    case UnitCategory::Cavalry:
      return &s_carthage_cavalry;
    case UnitCategory::BuilderConstruction:
      return &s_builder_circle;
    }
    break;
  }

  return &s_roman_infantry;
}

} // namespace Render::GL
