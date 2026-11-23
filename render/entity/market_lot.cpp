#include "render/entity/market_lot.h"

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

} // namespace

auto lot_run_seed(float world_x,
                  float world_z,
                  float front_x,
                  float front_z) -> std::uint32_t {
  const float length = std::hypot(front_x, front_z);
  if (!std::isfinite(world_x) || !std::isfinite(world_z) || !(length > 1.0e-4F)) {
    return 0U;
  }
  // How far along its facing the run's line lies, and which way that is. The
  // odd quantum keeps lot-pitch fractions off a rounding edge.
  constexpr float k_line_quantum = 0.83F;
  constexpr float k_degrees = 57.2957795F;
  const float across = ((world_x * front_x) + (world_z * front_z)) / length;
  const auto line = static_cast<std::int32_t>(std::lround(across / k_line_quantum));
  const auto facing =
      static_cast<std::int32_t>(std::lround(std::atan2(front_x, front_z) * k_degrees));
  return cell_hash(line, facing, 0x6A09E667U);
}

auto portico_run_variant(std::uint32_t run_seed) -> int {
  return static_cast<int>(run_seed %
                          static_cast<std::uint32_t>(k_portico_walk_variants));
}

auto portico_visual_variant(float world_x, float world_z) -> int {
  return portico_run_variant(lot_run_seed(world_x, world_z, 0.0F, 1.0F));
}

auto dock_visual_variant(float world_x, float world_z) -> int {
  if (!std::isfinite(world_x) || !std::isfinite(world_z)) {
    return 0;
  }
  constexpr float k_lot = 5.5F;
  const auto lot_x = static_cast<std::int32_t>(std::lround(world_x / k_lot));
  const auto lot_z = static_cast<std::int32_t>(std::lround(world_z / k_lot));
  // Every other lot along a quay moors the ship; the rest take a boat or
  // stand empty for the next arrival.
  if (((lot_x + lot_z) & 1) == 0) {
    return 0;
  }
  return 1 + static_cast<int>(cell_hash(lot_x, lot_z, 0xBB67AE85U) & 1U);
}

} // namespace Render::GL
