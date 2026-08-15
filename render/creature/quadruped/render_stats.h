#pragma once

#include <atomic>
#include <cstdint>

namespace Render::GL {

struct QuadrupedRenderStats {
  std::atomic<std::uint32_t> total{0};
  std::atomic<std::uint32_t> rendered{0};
  std::atomic<std::uint32_t> skipped_lod{0};
  std::atomic<std::uint32_t> profiles_computed{0};
  std::atomic<std::uint32_t> profiles_cached{0};
  std::atomic<std::uint32_t> lod_full{0};
  std::atomic<std::uint32_t> lod_minimal{0};

  void reset() {
    total.store(0, std::memory_order_relaxed);
    rendered.store(0, std::memory_order_relaxed);
    skipped_lod.store(0, std::memory_order_relaxed);
    profiles_computed.store(0, std::memory_order_relaxed);
    profiles_cached.store(0, std::memory_order_relaxed);
    lod_full.store(0, std::memory_order_relaxed);
    lod_minimal.store(0, std::memory_order_relaxed);
  }
};

using HorseRenderStats = QuadrupedRenderStats;
using ElephantRenderStats = QuadrupedRenderStats;

auto get_horse_render_stats() -> const HorseRenderStats&;
void reset_horse_render_stats();

auto get_elephant_render_stats() -> const ElephantRenderStats&;
void reset_elephant_render_stats();

} // namespace Render::GL
