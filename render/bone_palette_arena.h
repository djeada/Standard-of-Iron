

#pragma once

#include <QMatrix4x4>
#include <cstddef>
#include <cstdint>

namespace Render::GL {

class BonePaletteArena {
public:
  static constexpr std::size_t kPaletteWidth = 64;
  static constexpr std::size_t kMatrixFloats = 16;
  static constexpr std::size_t kPaletteFloats = kPaletteWidth * kMatrixFloats;
  static constexpr std::size_t kPaletteBytes = kPaletteFloats * sizeof(float);

  static void pack_palette_for_gpu(const QMatrix4x4 *src, float *dst) noexcept;

  BonePaletteArena() = delete;
  ~BonePaletteArena() = delete;
};

} // namespace Render::GL
