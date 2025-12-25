#include "formation_calculator.h"
#include <cmath>

namespace Render::GL {

auto RomanInfantryFormation::calculate_offset(
    int idx, int row, int col, int rows, int cols, float spacing,
    uint32_t seed) const -> FormationOffset {

  float const offset_x = (col - (cols - 1) * 0.5F) * spacing;
  float const offset_z = (row - (rows - 1) * 0.5F) * spacing;

  return {offset_x, offset_z};
}

auto RomanCavalryFormation::calculate_offset(
    int idx, int row, int col, int rows, int cols, float spacing,
    uint32_t seed) const -> FormationOffset {

  float const spacing_multiplier = 1.05F;
  float const offset_x =
      (col - (cols - 1) * 0.5F) * spacing * spacing_multiplier;
  float const offset_z =
      (row - (rows - 1) * 0.5F) * spacing * spacing_multiplier;

  return {offset_x, offset_z};
}

auto CarthageInfantryFormation::calculate_offset(
    int idx, int row, int col, int rows, int cols, float spacing,
    uint32_t seed) const -> FormationOffset {

  float const row_normalized = float(row) / float(rows > 1 ? rows - 1 : 1);
  float const col_normalized =
      float(col - (cols - 1) * 0.5F) / float(cols > 1 ? (cols - 1) * 0.5F : 1);

  float const spread_factor = 1.0F + row_normalized * 0.3F;
  float const row_spacing = spacing * (1.0F + row_normalized * 0.15F);

  float offset_x = (col - (cols - 1) * 0.5F) * spacing * spread_factor;
  float offset_z = (row - (rows - 1) * 0.5F) * row_spacing;

  if (row % 2 == 1) {
    offset_x += spacing * 0.35F;
  }

  float const rank_wave = std::sin(col_normalized * 3.14159F) * spacing *
                          0.12F * (1.0F + row_normalized);
  offset_z += rank_wave;

  float const center_push = (1.0F - std::abs(col_normalized)) * spacing * 0.2F;
  offset_z -= center_push;

  uint32_t variation_seed = seed ^ (uint32_t(idx) * 2654435761U);
  float const phase = float(variation_seed & 0xFFU) / 255.0F * 6.28318F;

  float const jitter_scale = spacing * 0.08F * (1.0F + row_normalized * 0.5F);
  float const jitter_x = std::sin(phase) * jitter_scale;
  float const jitter_z = std::cos(phase * 1.3F) * jitter_scale * 0.7F;

  offset_x += jitter_x;
  offset_z += jitter_z;

  int const cluster_id = idx / 4;
  float const cluster_phase = float(cluster_id * 137 + (seed & 0xFFU)) * 0.1F;
  float const cluster_pull = spacing * 0.06F;
  offset_x += std::sin(cluster_phase) * cluster_pull;
  offset_z += std::cos(cluster_phase * 0.7F) * cluster_pull;

  return {offset_x, offset_z};
}

auto CarthageCavalryFormation::calculate_offset(
    int idx, int row, int col, int rows, int cols, float spacing,
    uint32_t seed) const -> FormationOffset {

  float const spacing_multiplier = 1.2F;

  float offset_x = (col - (cols - 1) * 0.5F) * spacing * spacing_multiplier;
  float offset_z = (row - (rows - 1) * 0.5F) * spacing * spacing_multiplier;

  uint32_t rng_state = seed ^ (uint32_t(idx) * 7919U);

  auto fast_random = [](uint32_t &state) -> float {
    state = state * 1664525U + 1013904223U;
    return float(state & 0x7FFFFFU) / float(0x7FFFFFU);
  };

  float const jitter_x =
      (fast_random(rng_state) - 0.5F) * spacing * spacing_multiplier * 0.25F;
  float const jitter_z =
      (fast_random(rng_state) - 0.5F) * spacing * spacing_multiplier * 0.25F;

  offset_x += jitter_x;
  offset_z += jitter_z;

  float const cluster_x =
      std::sin(float(idx) * 0.7F) * spacing * spacing_multiplier * 0.10F;
  float const cluster_z =
      std::cos(float(idx) * 0.5F) * spacing * spacing_multiplier * 0.10F;

  offset_x += cluster_x;
  offset_z += cluster_z;

  return {offset_x, offset_z};
}

auto BuilderCircleFormation::calculate_offset(
    int idx, int row, int col, int rows, int cols, float spacing,
    uint32_t seed) const -> FormationOffset {

  int const total_units = rows * cols;
  float const angle =
      (float(idx) / float(total_units)) * 2.0F * 3.14159265358979F;
  float const radius = spacing * 1.8F;

  float offset_x = std::cos(angle) * radius;
  float offset_z = std::sin(angle) * radius;

  uint32_t rng_state = seed ^ (uint32_t(idx) * 2654435761U);
  auto fast_random = [](uint32_t &state) -> float {
    state = state * 1664525U + 1013904223U;
    return float(state & 0x7FFFFFU) / float(0x7FFFFFU);
  };

  float const jitter = spacing * 0.08F;
  offset_x += (fast_random(rng_state) - 0.5F) * jitter;
  offset_z += (fast_random(rng_state) - 0.5F) * jitter;

  return {offset_x, offset_z};
}

RomanInfantryFormation FormationCalculatorFactory::s_romanInfantry;
RomanCavalryFormation FormationCalculatorFactory::s_romanCavalry;
CarthageInfantryFormation FormationCalculatorFactory::s_carthageInfantry;
CarthageCavalryFormation FormationCalculatorFactory::s_carthageCavalry;
BuilderCircleFormation FormationCalculatorFactory::s_builderCircle;

auto FormationCalculatorFactory::getCalculator(
    Nation nation, UnitCategory category) -> const IFormationCalculator * {

  if (category == UnitCategory::BuilderConstruction) {
    return &s_builderCircle;
  }

  switch (nation) {
  case Nation::Roman:
    switch (category) {
    case UnitCategory::Infantry:
      return &s_romanInfantry;
    case UnitCategory::Cavalry:
      return &s_romanCavalry;
    case UnitCategory::BuilderConstruction:
      return &s_builderCircle;
    }
    break;

  case Nation::Carthage:
    switch (category) {
    case UnitCategory::Infantry:
      return &s_carthageInfantry;
    case UnitCategory::Cavalry:
      return &s_carthageCavalry;
    case UnitCategory::BuilderConstruction:
      return &s_builderCircle;
    }
    break;
  }

  return &s_romanInfantry;
}

} 
