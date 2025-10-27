#pragma once

#include "../gl/render_constants.h"
#include <cmath>
#include <cstdint>

namespace Render::Ground {

using std::uint32_t;

namespace MathConstants {
inline constexpr float TwoPi = ::Render::GL::MathConstants::TwoPi;
} // namespace MathConstants

namespace HashConstants {
inline constexpr uint32_t SpatialHashPrime1 = 73856093U;
inline constexpr uint32_t SpatialHashPrime2 = 19349663U;
inline constexpr uint32_t SpatialHashPrime3 = 83492791U;
inline constexpr uint32_t LinearCongruentialMultiplier = 1664525U;
inline constexpr uint32_t LinearCongruentialIncrement = 1013904223U;
inline constexpr uint32_t XorShiftAmount17 = 17;
inline constexpr uint32_t XorShiftAmount11 = 11;
inline constexpr uint32_t XorShiftAmount15 = 15;
inline constexpr uint32_t XorShiftAmount14 = 14;
inline constexpr uint32_t HashMixMultiplier1 = 0xed5ad4bbU;
inline constexpr uint32_t HashMixMultiplier2 = 0xac4c1b51U;
inline constexpr uint32_t HashMixMultiplier3 = 0x31848babU;
inline constexpr float NoiseFrequencyX = 127.1F;
inline constexpr float NoiseFrequencyY = 311.7F;
inline constexpr float NoiseAmplitude = 43758.5453123F;
inline constexpr float TemporalVariationFrequency = 37.0F;
} // namespace HashConstants

inline auto hashCoords(int x, int z, uint32_t salt = 0U) -> uint32_t {
  auto const ux = static_cast<uint32_t>(x * HashConstants::SpatialHashPrime1);
  auto const uz = static_cast<uint32_t>(z * HashConstants::SpatialHashPrime2);
  return ux ^ uz ^ (salt * HashConstants::SpatialHashPrime3);
}

inline auto rand01(uint32_t &state) -> float {
  state = state * HashConstants::LinearCongruentialMultiplier +
          HashConstants::LinearCongruentialIncrement;
  return static_cast<float>((state >> ::Render::GL::BitShift::Shift8) &
                            ::Render::GL::BitShift::Mask24Bit) /
         ::Render::GL::BitShift::Mask24BitFloat;
}

inline auto remap(float value, float minOut, float maxOut) -> float {
  return minOut + (maxOut - minOut) * value;
}

inline auto hashTo01(uint32_t h) -> float {
  h ^= h >> HashConstants::XorShiftAmount17;
  h *= HashConstants::HashMixMultiplier1;
  h ^= h >> HashConstants::XorShiftAmount11;
  h *= HashConstants::HashMixMultiplier2;
  h ^= h >> HashConstants::XorShiftAmount15;
  h *= HashConstants::HashMixMultiplier3;
  h ^= h >> HashConstants::XorShiftAmount14;
  return static_cast<float>((h >> ::Render::GL::BitShift::Shift8) &
                            ::Render::GL::BitShift::Mask24Bit) /
         ::Render::GL::BitShift::Mask24BitFloat;
}

inline auto noiseHash(float x, float y) -> float {
  float const n = std::sin(x * HashConstants::NoiseFrequencyX +
                           y * HashConstants::NoiseFrequencyY) *
                  HashConstants::NoiseAmplitude;
  return n - std::floor(n);
}

} // namespace Render::Ground
