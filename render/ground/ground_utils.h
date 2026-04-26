#pragma once

#include "../gl/render_constants.h"
#include <cmath>
#include <cstdint>

namespace Render::Ground {

using std::uint32_t;

namespace MathConstants {
inline constexpr float k_two_pi = ::Render::GL::MathConstants::k_two_pi;
}

namespace HashConstants {
inline constexpr uint32_t k_spatial_hash_prime_1 = 73856093U;
inline constexpr uint32_t k_spatial_hash_prime_2 = 19349663U;
inline constexpr uint32_t k_spatial_hash_prime_3 = 83492791U;
inline constexpr uint32_t k_linear_congruential_multiplier = 1664525U;
inline constexpr uint32_t k_linear_congruential_increment = 1013904223U;
inline constexpr uint32_t k_xor_shift_amount_17 = 17;
inline constexpr uint32_t k_xor_shift_amount_11 = 11;
inline constexpr uint32_t k_xor_shift_amount_15 = 15;
inline constexpr uint32_t k_xor_shift_amount_14 = 14;
inline constexpr uint32_t k_hash_mix_multiplier_1 = 0xed5ad4bbU;
inline constexpr uint32_t k_hash_mix_multiplier_2 = 0xac4c1b51U;
inline constexpr uint32_t k_hash_mix_multiplier_3 = 0x31848babU;
inline constexpr float k_noise_frequency_x = 127.1F;
inline constexpr float k_noise_frequency_y = 311.7F;
inline constexpr float k_noise_amplitude = 43758.5453123F;
inline constexpr float k_temporal_variation_frequency = 37.0F;
} // namespace HashConstants

inline auto hash_coords(int x, int z, uint32_t salt = 0U) -> uint32_t {
  auto const ux =
      static_cast<uint32_t>(x * HashConstants::k_spatial_hash_prime_1);
  auto const uz =
      static_cast<uint32_t>(z * HashConstants::k_spatial_hash_prime_2);
  return ux ^ uz ^ (salt * HashConstants::k_spatial_hash_prime_3);
}

inline auto rand_01(uint32_t &state) -> float {
  state = state * HashConstants::k_linear_congruential_multiplier +
          HashConstants::k_linear_congruential_increment;
  return static_cast<float>((state >> ::Render::GL::BitShift::Shift8) &
                            ::Render::GL::BitShift::Mask24Bit) /
         ::Render::GL::BitShift::Mask24BitFloat;
}

inline auto remap(float value, float min_out, float max_out) -> float {
  return min_out + (max_out - min_out) * value;
}

inline auto hash_to_01(uint32_t h) -> float {
  h ^= h >> HashConstants::k_xor_shift_amount_17;
  h *= HashConstants::k_hash_mix_multiplier_1;
  h ^= h >> HashConstants::k_xor_shift_amount_11;
  h *= HashConstants::k_hash_mix_multiplier_2;
  h ^= h >> HashConstants::k_xor_shift_amount_15;
  h *= HashConstants::k_hash_mix_multiplier_3;
  h ^= h >> HashConstants::k_xor_shift_amount_14;
  return static_cast<float>((h >> ::Render::GL::BitShift::Shift8) &
                            ::Render::GL::BitShift::Mask24Bit) /
         ::Render::GL::BitShift::Mask24BitFloat;
}

inline auto noise_hash(float x, float y) -> float {
  float const n = std::sin(x * HashConstants::k_noise_frequency_x +
                           y * HashConstants::k_noise_frequency_y) *
                  HashConstants::k_noise_amplitude;
  return n - std::floor(n);
}

inline auto value_noise(float x, float z, uint32_t salt = 0U) -> float {
  int const x0 = int(std::floor(x));
  int const z0 = int(std::floor(z));
  int const x1 = x0 + 1;
  int const z1 = z0 + 1;
  float const tx = x - float(x0);
  float const tz = z - float(z0);
  float const n00 = hash_to_01(hash_coords(x0, z0, salt));
  float const n10 = hash_to_01(hash_coords(x1, z0, salt));
  float const n01 = hash_to_01(hash_coords(x0, z1, salt));
  float const n11 = hash_to_01(hash_coords(x1, z1, salt));
  float const nx0 = n00 * (1.0F - tx) + n10 * tx;
  float const nx1 = n01 * (1.0F - tx) + n11 * tx;
  return nx0 * (1.0F - tz) + nx1 * tz;
}

} // namespace Render::Ground
