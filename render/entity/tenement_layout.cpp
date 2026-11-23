#include "render/entity/tenement_layout.h"

#include <cmath>

namespace Render::GL {
namespace {

auto mix(std::uint32_t value) -> std::uint32_t {
  value ^= value >> 16U;
  value *= 0x7FEB352DU;
  value ^= value >> 15U;
  value *= 0x846CA68BU;
  value ^= value >> 16U;
  return value;
}

auto cell_hash(std::int32_t x, std::int32_t z, std::uint32_t salt) -> std::uint32_t {
  return mix((static_cast<std::uint32_t>(x) * 0x9E3779B1U) ^
             (static_cast<std::uint32_t>(z) * 0x85EBCA77U) ^ salt);
}

auto unit(std::uint32_t hash) -> float {
  return static_cast<float>(hash & 0xFFFFU) / 65535.0F;
}

} // namespace

auto tenement_visual_variant(float world_x, float world_z) -> int {
  if (!std::isfinite(world_x) || !std::isfinite(world_z)) {
    return 0;
  }

  constexpr float k_height_cell = 15.0F;
  const auto cell_x = static_cast<std::int32_t>(std::floor(world_x / k_height_cell));
  const auto cell_z = static_cast<std::int32_t>(std::floor(world_z / k_height_cell));
  const float district = unit(cell_hash(cell_x, cell_z, 0x51ED270BU));
  int storeys = district < 0.30F ? 2 : (district < 0.82F ? 3 : 4);

  const auto lot_x = static_cast<std::int32_t>(std::lround(world_x * 2.0F));
  const auto lot_z = static_cast<std::int32_t>(std::lround(world_z * 2.0F));
  const std::uint32_t lot = cell_hash(lot_x, lot_z, 0x2545F491U);
  const float stray = unit(lot);
  if (stray < 0.16F) {
    --storeys;
  } else if (stray > 0.86F) {
    ++storeys;
  }
  storeys = storeys < 2 ? 2 : (storeys > 4 ? 4 : storeys);

  std::array<int, k_tenement_variant_count> matching{};
  int count = 0;
  for (int variant = 0; variant < k_tenement_variant_count; ++variant) {
    if (k_tenement_traits[static_cast<std::size_t>(variant)].storeys == storeys) {
      matching[static_cast<std::size_t>(count++)] = variant;
    }
  }
  if (count == 0) {
    return 0;
  }
  return matching[static_cast<std::size_t>((lot >> 16U) %
                                           static_cast<std::uint32_t>(count))];
}

} // namespace Render::GL
