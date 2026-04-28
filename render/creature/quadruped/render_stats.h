#pragma once

#include <cstdint>

namespace Render::GL {

struct QuadrupedRenderStats {
  std::uint32_t total{0};
  std::uint32_t rendered{0};
  std::uint32_t skipped_lod{0};
  std::uint32_t profiles_computed{0};
  std::uint32_t profiles_cached{0};
  std::uint32_t lod_full{0};
  std::uint32_t lod_minimal{0};

  void reset() {
    total = 0;
    rendered = 0;
    skipped_lod = 0;
    profiles_computed = 0;
    profiles_cached = 0;
    lod_full = 0;
    lod_minimal = 0;
  }
};

using HorseRenderStats = QuadrupedRenderStats;
using ElephantRenderStats = QuadrupedRenderStats;

auto get_horse_render_stats() -> const HorseRenderStats &;
void reset_horse_render_stats();

auto get_elephant_render_stats() -> const ElephantRenderStats &;
void reset_elephant_render_stats();

} // namespace Render::GL
