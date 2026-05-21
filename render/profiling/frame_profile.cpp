#include "frame_profile.h"

#include <cstdio>
#include <cstring>

namespace Render::Profiling {

auto global_profile() -> FrameProfile& {
  static FrameProfile g_profile;
  return g_profile;
}

auto format_overlay(const FrameProfile& profile) -> std::string {
  std::string out;
  out.reserve(256);

  char line[192];

  std::uint64_t const total = profile.total_us();
  double const total_ms = static_cast<double>(total) / 1000.0;
  std::snprintf(line,
                sizeof(line),
                "frame #%llu  total %6.2f ms\n",
                static_cast<unsigned long long>(profile.frame_index),
                total_ms);
  out += line;

  for (std::size_t i = 0; i < profile.phase_us.size(); ++i) {
    auto const phase = static_cast<Phase>(i);
    double const ms = static_cast<double>(profile.phase_us[i]) / 1000.0;
    double const pct = total > 0 ? (100.0 * static_cast<double>(profile.phase_us[i]) /
                                    static_cast<double>(total))
                                 : 0.0;
    std::snprintf(
        line, sizeof(line), "  %-8s %6.2f ms  %5.1f%%\n", phase_name(phase), ms, pct);
    out += line;
  }

  std::snprintf(line,
                sizeof(line),
                "draws=%llu tris=%llu inst=%llu  head %+5.2f ms\n",
                static_cast<unsigned long long>(profile.draw_calls),
                static_cast<unsigned long long>(profile.triangles),
                static_cast<unsigned long long>(profile.instances),
                profile.budget_headroom_ms);
  out += line;

  std::snprintf(line,
                sizeof(line),
                "avg/p95 %5.2f/%5.2f ms  soldiers=%llu  cache h/m=%llu/%llu\n",
                profile.average_frame_ms,
                profile.p95_frame_ms,
                static_cast<unsigned long long>(profile.visible_soldiers),
                static_cast<unsigned long long>(profile.render_asset_cache_hits),
                static_cast<unsigned long long>(profile.render_asset_cache_misses));
  out += line;

  std::snprintf(line,
                sizeof(line),
                "combat %5.2f  sample %5.2f  prep %5.2f  bpat %5.2f  cache %5.2f  "
                "layout %5.2f ms\n",
                static_cast<double>(profile.combat_state_update_us) / 1000.0,
                static_cast<double>(profile.animation_input_sampling_us) / 1000.0,
                static_cast<double>(profile.humanoid_preparation_us) / 1000.0,
                static_cast<double>(profile.bpat_playback_us) / 1000.0,
                static_cast<double>(profile.render_asset_cache_lookup_us) / 1000.0,
                static_cast<double>(profile.soldier_layout_generation_us) / 1000.0);
  out += line;

  return out;
}

} // namespace Render::Profiling
