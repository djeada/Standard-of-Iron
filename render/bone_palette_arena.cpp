#include "bone_palette_arena.h"

#include <cstring>

namespace Render::GL {

void BonePaletteArena::pack_palette_for_gpu(const QMatrix4x4 *src,
                                            float *dst) noexcept {
  if (dst == nullptr) {
    return;
  }
  if (src == nullptr) {
    for (std::size_t b = 0; b < k_palette_width; ++b) {
      QMatrix4x4 ident;
      std::memcpy(dst + b * k_matrix_floats, ident.constData(),
                  sizeof(float) * k_matrix_floats);
    }
    return;
  }
  for (std::size_t b = 0; b < k_palette_width; ++b) {
    std::memcpy(dst + b * k_matrix_floats, src[b].constData(),
                sizeof(float) * k_matrix_floats);
  }
}

} // namespace Render::GL
