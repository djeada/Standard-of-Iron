#pragma once

#include "../../game/map/terrain.h"
#include "../gl/render_constants.h"
#include <algorithm>
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

struct CurvatureShadingResponse {
  float curvature_emphasis = 0.0F;
  float ridge_response = 0.0F;
  float gully_response = 0.0F;
};

inline auto smoothstep(float edge0, float edge1, float x) -> float {
  float const width = std::max(1e-6F, edge1 - edge0);
  float const t = std::clamp((x - edge0) / width, 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

inline auto compute_curvature_shading_response(
    Game::Map::TerrainType type, float avg_curvature, float avg_slope,
    float edge_factor, float plateau_factor,
    float entrance_factor) -> CurvatureShadingResponse {
  if (type != Game::Map::TerrainType::Hill &&
      type != Game::Map::TerrainType::Mountain) {
    return {};
  }

  float const terrain_scale =
      (type == Game::Map::TerrainType::Mountain) ? 1.0F : 0.78F;
  float const curvature_signal = smoothstep(0.01F, 0.12F, avg_curvature);
  float const slope_signal = smoothstep(0.12F, 0.45F, avg_slope);
  float const exposed_signal = std::max(edge_factor, slope_signal);
  float const sheltered_signal =
      0.60F * plateau_factor + 0.40F * entrance_factor;

  CurvatureShadingResponse response;
  response.curvature_emphasis = std::clamp(
      terrain_scale * curvature_signal * (0.55F + 0.45F * exposed_signal) *
          (1.0F - 0.45F * sheltered_signal),
      0.0F, 1.0F);
  response.ridge_response =
      std::clamp(response.curvature_emphasis * (0.45F + 0.55F * edge_factor) *
                     (1.0F - 0.35F * entrance_factor),
                 0.0F, 1.0F);
  response.gully_response = std::clamp(
      response.curvature_emphasis * (0.50F + 0.50F * (1.0F - plateau_factor)) *
          (0.85F + 0.15F * (1.0F - entrance_factor)),
      0.0F, 1.0F);
  return response;
}

inline auto compute_entry_shading_factor(float avg_entry_weight,
                                         float avg_slope, float plateau_factor,
                                         float concavity_hint) -> float {
  float const coverage = smoothstep(0.02F, 0.22F, avg_entry_weight);
  float const sheltered = 1.0F - smoothstep(0.20F, 0.58F, avg_slope);
  float const plateau_support =
      0.35F + 0.65F * std::max(plateau_factor, sheltered);
  float const carved_support = concavity_hint * (0.30F + 0.30F * sheltered);
  return std::clamp(coverage * plateau_support + carved_support, 0.0F, 1.0F);
}

} // namespace Render::Ground
