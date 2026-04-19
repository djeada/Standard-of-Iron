#pragma once

#include <cstdint>

namespace Render::GL {

struct ElephantRenderStats {
  uint32_t elephants_total{0};
  uint32_t elephants_rendered{0};
  uint32_t elephants_skipped_lod{0};
  uint32_t lod_full{0};
  uint32_t lod_reduced{0};
  uint32_t lod_minimal{0};

  void reset() {
    elephants_total = 0;
    elephants_rendered = 0;
    elephants_skipped_lod = 0;
    lod_full = 0;
    lod_reduced = 0;
    lod_minimal = 0;
  }
};

auto get_elephant_render_stats() -> const ElephantRenderStats &;

void reset_elephant_render_stats();

} // namespace Render::GL
