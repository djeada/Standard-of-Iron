#pragma once

#include <cstdint>

namespace Render::GL {

struct HorseRenderStats {
  uint32_t horses_total{0};
  uint32_t horses_rendered{0};
  uint32_t horses_skipped_lod{0};
  uint32_t profiles_computed{0};
  uint32_t profiles_cached{0};
  uint32_t lod_full{0};
  uint32_t lod_minimal{0};

  void reset() {
    horses_total = 0;
    horses_rendered = 0;
    horses_skipped_lod = 0;
    profiles_computed = 0;
    profiles_cached = 0;
    lod_full = 0;
    lod_minimal = 0;
  }
};

auto get_horse_render_stats() -> const HorseRenderStats &;

void reset_horse_render_stats();

} // namespace Render::GL
