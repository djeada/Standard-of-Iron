#pragma once

#include <cmath>
#include <cstdint>

namespace Game::Map::terrain_value_noise {

[[nodiscard]] inline auto
hash_coords(int x, int z, std::uint32_t seed) -> std::uint32_t {
  std::uint32_t const ux = static_cast<std::uint32_t>(x) * 73856093U;
  std::uint32_t const uz = static_cast<std::uint32_t>(z) * 19349663U;
  std::uint32_t const s = seed * 83492791U + 0x9e3779b9U;
  return ux ^ uz ^ s;
}

[[nodiscard]] inline auto hash_to_float01(std::uint32_t h) -> float {
  h ^= h >> 17;
  h *= 0xed5ad4bbU;
  h ^= h >> 11;
  h *= 0xac4c1b51U;
  h ^= h >> 15;
  h *= 0x31848babU;
  h ^= h >> 14;
  return (h & 0x00FFFFFFU) / float(0x01000000);
}

[[nodiscard]] inline auto
value_noise_2d(float x, float z, std::uint32_t seed) -> float {
  int const ix0 = static_cast<int>(std::floor(x));
  int const iz0 = static_cast<int>(std::floor(z));
  int const ix1 = ix0 + 1;
  int const iz1 = iz0 + 1;

  float const tx = x - static_cast<float>(ix0);
  float const tz = z - static_cast<float>(iz0);
  const auto fade = [](float value) {
    return value * value * value * (value * (value * 6.0F - 15.0F) + 10.0F);
  };
  const float sx = fade(tx);
  const float sz = fade(tz);

  float const n00 = hash_to_float01(hash_coords(ix0, iz0, seed));
  float const n10 = hash_to_float01(hash_coords(ix1, iz0, seed));
  float const n01 = hash_to_float01(hash_coords(ix0, iz1, seed));
  float const n11 = hash_to_float01(hash_coords(ix1, iz1, seed));

  float const nx0 = n00 * (1.0F - sx) + n10 * sx;
  float const nx1 = n01 * (1.0F - sx) + n11 * sx;
  return nx0 * (1.0F - sz) + nx1 * sz;
}

} // namespace Game::Map::terrain_value_noise
