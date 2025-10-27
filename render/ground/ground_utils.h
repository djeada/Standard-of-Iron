#pragma once

#include "../gl/render_constants.h"
#include <cmath>
#include <cstdint>

namespace Render::Ground {

using std::uint32_t;

namespace HashConstants {
inline constexpr uint32_t HashPrime1 = 73856093U;
inline constexpr uint32_t HashPrime2 = 19349663U;
inline constexpr uint32_t HashPrime3 = 83492791U;
inline constexpr uint32_t LcgMultiplier = 1664525U;
inline constexpr uint32_t LcgIncrement = 1013904223U;
inline constexpr uint32_t XorShift17 = 17;
inline constexpr uint32_t XorShift11 = 11;
inline constexpr uint32_t XorShift15 = 15;
inline constexpr uint32_t XorShift14 = 14;
inline constexpr uint32_t MixConstant1 = 0xed5ad4bbU;
inline constexpr uint32_t MixConstant2 = 0xac4c1b51U;
inline constexpr uint32_t MixConstant3 = 0x31848babU;
inline constexpr float NoiseHashX = 127.1F;
inline constexpr float NoiseHashY = 311.7F;
inline constexpr float NoiseHashScale = 43758.5453123F;
inline constexpr float TemporalHashFrequency = 37.0F;
} // namespace HashConstants

inline auto hashCoords(int x, int z, uint32_t salt = 0U) -> uint32_t {
  auto const ux = static_cast<uint32_t>(x * HashConstants::HashPrime1);
  auto const uz = static_cast<uint32_t>(z * HashConstants::HashPrime2);
  return ux ^ uz ^ (salt * HashConstants::HashPrime3);
}

inline auto rand01(uint32_t &state) -> float {
  state = state * HashConstants::LcgMultiplier + HashConstants::LcgIncrement;
  return static_cast<float>((state >> ::Render::GL::BitShift::Shift8) &
                            ::Render::GL::BitShift::Mask24Bit) /
         ::Render::GL::BitShift::Mask24BitFloat;
}

inline auto remap(float value, float minOut, float maxOut) -> float {
  return minOut + (maxOut - minOut) * value;
}

inline auto hashTo01(uint32_t h) -> float {
  h ^= h >> HashConstants::XorShift17;
  h *= HashConstants::MixConstant1;
  h ^= h >> HashConstants::XorShift11;
  h *= HashConstants::MixConstant2;
  h ^= h >> HashConstants::XorShift15;
  h *= HashConstants::MixConstant3;
  h ^= h >> HashConstants::XorShift14;
  return static_cast<float>((h >> ::Render::GL::BitShift::Shift8) &
                            ::Render::GL::BitShift::Mask24Bit) /
         ::Render::GL::BitShift::Mask24BitFloat;
}

inline auto noiseHash(float x, float y) -> float {
  float const n = std::sin(x * HashConstants::NoiseHashX +
                           y * HashConstants::NoiseHashY) *
                  HashConstants::NoiseHashScale;
  return n - std::floor(n);
}

} // namespace Render::Ground
