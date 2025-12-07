#include "formation_calculator.h"
#include <cmath>

namespace Render::GL {

auto RomanInfantryFormation::calculateOffset(
    int idx, int row, int col, int rows, int cols, float spacing,
    uint32_t seed) const -> FormationOffset {

  float const offset_x = (col - (cols - 1) * 0.5F) * spacing;
  float const offset_z = (row - (rows - 1) * 0.5F) * spacing;

  return {offset_x, offset_z};
}

auto RomanCavalryFormation::calculateOffset(
    int idx, int row, int col, int rows, int cols, float spacing,
    uint32_t seed) const -> FormationOffset {

  float const spacing_multiplier = 1.05F;
  float const offset_x =
      (col - (cols - 1) * 0.5F) * spacing * spacing_multiplier;
  float const offset_z =
      (row - (rows - 1) * 0.5F) * spacing * spacing_multiplier;

  return {offset_x, offset_z};
}

auto CarthageInfantryFormation::calculateOffset(
    int idx, int row, int col, int rows, int cols, float spacing,
    uint32_t seed) const -> FormationOffset {

  float const base_spacing =
      spacing *
      (1.0F +
       std::sin(float(row) * 0.55F + float(seed & 0xFFU) * 0.01F) * 0.10F);

  float offset_x = (col - (cols - 1) * 0.5F) * base_spacing;
  float offset_z = (row - (rows - 1) * 0.5F) * base_spacing;

  uint32_t rng_state = seed ^ (uint32_t(idx) * 7919U);

  auto fast_random = [](uint32_t &state) -> float {
    state = state * 1664525U + 1013904223U;
    return float(state & 0x7FFFFFU) / float(0x7FFFFFU);
  };

  auto rand_range = [&](uint32_t &state, float magnitude) -> float {
    return (fast_random(state) - 0.5F) * magnitude;
  };

  float const jitter_x = rand_range(rng_state, spacing * 0.35F);
  float const jitter_z = rand_range(rng_state, spacing * 0.35F);

  int const cluster_c = (col >= 0) ? (col / 2) : ((col - 1) / 2);
  int const cluster_r = (row >= 0) ? (row / 2) : ((row - 1) / 2);
  uint32_t cluster_state =
      seed ^ (uint32_t(cluster_r * 97 + cluster_c * 271) * 23U + 0xBADU);

  float const cluster_shift_x = rand_range(cluster_state, spacing * 0.50F);
  float const cluster_shift_z = rand_range(cluster_state, spacing * 0.50F);
  float const cluster_arc =
      std::sin(float(cluster_r) * 0.9F + float(cluster_c) * 0.7F) * spacing *
      0.25F;

  float const slant_x = (row - (rows - 1) * 0.5F) * spacing * 0.12F;
  float const wave_z =
      std::sin(float(col) * 0.8F + float(row) * 0.4F) * spacing * 0.15F;

  offset_x += jitter_x + cluster_shift_x + cluster_arc + slant_x;
  offset_z += jitter_z + cluster_shift_z + wave_z;

  if (row % 2 == 1) {
    offset_x += spacing * 0.22F;
  }

  return {offset_x, offset_z};
}

auto CarthageCavalryFormation::calculateOffset(
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

RomanInfantryFormation FormationCalculatorFactory::s_romanInfantry;
RomanCavalryFormation FormationCalculatorFactory::s_romanCavalry;
CarthageInfantryFormation FormationCalculatorFactory::s_carthageInfantry;
CarthageCavalryFormation FormationCalculatorFactory::s_carthageCavalry;

auto FormationCalculatorFactory::getCalculator(
    Nation nation, UnitCategory category) -> const IFormationCalculator * {
  switch (nation) {
  case Nation::Roman:
    switch (category) {
    case UnitCategory::Infantry:
      return &s_romanInfantry;
    case UnitCategory::Cavalry:
      return &s_romanCavalry;
    }
    break;

  case Nation::Carthage:
    switch (category) {
    case UnitCategory::Infantry:
      return &s_carthageInfantry;
    case UnitCategory::Cavalry:
      return &s_carthageCavalry;
    }
    break;
  }

  return &s_romanInfantry;
}

} // namespace Render::GL
