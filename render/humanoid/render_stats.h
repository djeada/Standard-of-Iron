#pragma once

#include <cstdint>

namespace Render::GL {

struct HumanoidRenderStats {
  uint32_t soldiers_total{0};
  uint32_t soldiers_rendered{0};
  uint32_t soldiers_skipped_frustum{0};
  uint32_t soldiers_skipped_lod{0};
  uint32_t soldiers_skipped_temporal{0};
  uint32_t facial_hair_skipped_distance{0};
  uint32_t lod_full{0};
  uint32_t lod_minimal{0};

  void reset() {
    soldiers_total = 0;
    soldiers_rendered = 0;
    soldiers_skipped_frustum = 0;
    soldiers_skipped_lod = 0;
    soldiers_skipped_temporal = 0;
    facial_hair_skipped_distance = 0;
    lod_full = 0;
    lod_minimal = 0;
  }
};

auto get_humanoid_render_stats() -> const HumanoidRenderStats &;

void reset_humanoid_render_stats();

} // namespace Render::GL
