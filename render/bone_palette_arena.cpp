#include "bone_palette_arena.h"

#include <cstring>

namespace Render::GL {

void BonePaletteArena::pack_palette_for_gpu(const QMatrix4x4 *src,
                                            float *dst) noexcept {
  if (dst == nullptr) {
    return;
  }
  if (src == nullptr) {
    for (std::size_t b = 0; b < kPaletteWidth; ++b) {
      QMatrix4x4 ident;
      std::memcpy(dst + b * kMatrixFloats, ident.constData(),
                  sizeof(float) * kMatrixFloats);
    }
    return;
  }
  for (std::size_t b = 0; b < kPaletteWidth; ++b) {
    std::memcpy(dst + b * kMatrixFloats, src[b].constData(),
                sizeof(float) * kMatrixFloats);
  }
}

} // namespace Render::GL
