#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Game::Map {

struct MountainNoiseSettings {
  std::uint32_t seed = 1337U;
  float frequency = 0.07F;
  int octaves = 4;
};

inline auto clamp_noise_octaves(int octaves) -> int {
  return std::clamp(octaves, 1, 8);
}

inline auto smooth_lerp(float a, float b, float t) -> float {
  const float smooth = t * t * (3.0F - 2.0F * t);
  return a + (b - a) * smooth;
}

inline auto hash_noise_coords(int x, int z,
                              std::uint32_t seed) -> std::uint32_t {
  const std::uint32_t ux = static_cast<std::uint32_t>(x) * 73856093U;
  const std::uint32_t uz = static_cast<std::uint32_t>(z) * 19349663U;
  return ux ^ uz ^ (seed * 83492791U + 0x9e3779b9U);
}

inline auto hash_to_unit_float(std::uint32_t h) -> float {
  h ^= h >> 17;
  h *= 0xed5ad4bbU;
  h ^= h >> 11;
  h *= 0xac4c1b51U;
  h ^= h >> 15;
  h *= 0x31848babU;
  h ^= h >> 14;
  return static_cast<float>(h & 0x00FFFFFFU) / static_cast<float>(0x01000000U);
}

inline auto value_noise(float x, float z, std::uint32_t seed) -> float {
  const int ix0 = static_cast<int>(std::floor(x));
  const int iz0 = static_cast<int>(std::floor(z));
  const int ix1 = ix0 + 1;
  const int iz1 = iz0 + 1;

  const float tx = x - static_cast<float>(ix0);
  const float tz = z - static_cast<float>(iz0);

  const float n00 = hash_to_unit_float(hash_noise_coords(ix0, iz0, seed));
  const float n10 = hash_to_unit_float(hash_noise_coords(ix1, iz0, seed));
  const float n01 = hash_to_unit_float(hash_noise_coords(ix0, iz1, seed));
  const float n11 = hash_to_unit_float(hash_noise_coords(ix1, iz1, seed));

  const float nx0 = smooth_lerp(n00, n10, tx);
  const float nx1 = smooth_lerp(n01, n11, tx);
  return smooth_lerp(nx0, nx1, tz);
}

inline auto fbm_noise(float x, float z, std::uint32_t seed,
                      int octaves) -> float {
  float sum = 0.0F;
  float amplitude = 1.0F;
  float frequency = 1.0F;
  float normalization = 0.0F;

  for (int i = 0; i < clamp_noise_octaves(octaves); ++i) {
    sum +=
        value_noise(x * frequency, z * frequency, seed + i * 97U) * amplitude;
    normalization += amplitude;
    amplitude *= 0.5F;
    frequency *= 2.0F;
  }

  return normalization > 0.0F ? (sum / normalization) : 0.0F;
}

inline auto
sample_mountain_region(float world_x, float world_z,
                       const MountainNoiseSettings &settings) -> float {
  const float frequency = std::clamp(settings.frequency, 0.01F, 2.0F);
  const int octaves = clamp_noise_octaves(settings.octaves);
  const float primary = fbm_noise(world_x * frequency, world_z * frequency,
                                  settings.seed, octaves);
  const float detail =
      fbm_noise(world_x * frequency * 2.4F, world_z * frequency * 2.4F,
                settings.seed + 31U, std::max(1, octaves - 1));
  const float ridge = 1.0F - std::abs(primary * 2.0F - 1.0F);
  return std::clamp(primary * 0.62F + detail * 0.23F + ridge * 0.15F, 0.0F,
                    1.0F);
}

} // namespace Game::Map
