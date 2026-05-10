

#pragma once

#include <QMatrix4x4>
#include <cstddef>
#include <cstdint>

namespace Render::GL {

class BonePaletteArena {
public:
  static constexpr std::size_t k_palette_width = 64;
  static constexpr std::size_t k_matrix_floats = 16;
  static constexpr std::size_t k_palette_floats =
      k_palette_width * k_matrix_floats;
  static constexpr std::size_t k_palette_bytes =
      k_palette_floats * sizeof(float);

  static void pack_palette_for_gpu(const QMatrix4x4 *src, float *dst) noexcept;

  BonePaletteArena() = delete;
  ~BonePaletteArena() = delete;
};

} // namespace Render::GL
