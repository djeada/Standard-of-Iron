#pragma once

#include <atomic>
#include <cstdint>

namespace Render::GL {

struct HumanoidRenderStats {
  std::atomic<uint32_t> soldiers_total{0};
  std::atomic<uint32_t> soldiers_rendered{0};
  std::atomic<uint32_t> soldiers_skipped_frustum{0};
  std::atomic<uint32_t> soldiers_skipped_fog{0};
  std::atomic<uint32_t> soldiers_skipped_lod{0};
  std::atomic<uint32_t> soldiers_skipped_lens_gap{0};
  std::atomic<uint32_t> facial_hair_skipped_distance{0};
  std::atomic<uint32_t> lod_full{0};
  std::atomic<uint32_t> lod_minimal{0};

  void reset() {
    soldiers_total.store(0, std::memory_order_relaxed);
    soldiers_rendered.store(0, std::memory_order_relaxed);
    soldiers_skipped_frustum.store(0, std::memory_order_relaxed);
    soldiers_skipped_fog.store(0, std::memory_order_relaxed);
    soldiers_skipped_lod.store(0, std::memory_order_relaxed);
    soldiers_skipped_lens_gap.store(0, std::memory_order_relaxed);
    facial_hair_skipped_distance.store(0, std::memory_order_relaxed);
    lod_full.store(0, std::memory_order_relaxed);
    lod_minimal.store(0, std::memory_order_relaxed);
  }
};

auto get_humanoid_render_stats() -> const HumanoidRenderStats&;

void reset_humanoid_render_stats();

} // namespace Render::GL
