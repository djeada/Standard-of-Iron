#pragma once

#include <cstdint>

namespace Render::GL {

inline auto hash_01(std::uint32_t x) -> float {
  constexpr int k_shift_13 = 13;
  constexpr int k_shift_17 = 17;
  constexpr int k_shift_5 = 5;
  constexpr std::uint32_t k_mask_24_bit = 0xFFFFFFU;
  constexpr std::uint32_t k_range_24_bit = 0x01000000U;
  x ^= x << k_shift_13;
  x ^= x >> k_shift_17;
  x ^= x << k_shift_5;
  return (x & k_mask_24_bit) / float(k_range_24_bit);
}

} // namespace Render::GL
