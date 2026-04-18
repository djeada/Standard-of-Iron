#pragma once

// Stage 4 seam — pose-keyed cache of local-space DrawPartCmd templates.
//
// The design in plan.md: caches keyed on pose-shape only, world transform
// applied at submit time. PoseDrawTemplate is a vector of DrawPartCmds whose
// `world` matrix is LOCAL to the unit root. expand_to_world() applies the
// caller's world transform and pushes fully-resolved commands into an
// output vector in one tight loop — this is where the matrix-op reduction
// comes from (one multiply per part, not ~45 per unit).
//
// The cache is a plain unordered_map with a generation counter for cheap
// eviction. It is deliberately NOT thread-safe: the render loop is
// single-threaded (see repo memory "threading model").

#include "../draw_part.h"
#include "pose_key.h"

#include <QMatrix4x4>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Render::GL {

struct PoseDrawTemplate {
  // Local-space DrawPartCmd templates. `world` on each template is in the
  // unit's root frame; expand_to_world() multiplies it by the caller's
  // world matrix.
  std::vector<DrawPartCmd> parts;
  std::uint64_t last_touched_frame = 0;
};

class PoseTemplateCache {
public:
  // Observability accessors — useful for tests and for surfacing hit-rate
  // in the frame-stats overlay later.
  struct Stats {
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    std::uint64_t inserts = 0;
    std::uint64_t evictions = 0;
  };

  void begin_frame(std::uint64_t frame_index) noexcept {
    m_current_frame = frame_index;
  }

  // Find an existing template. Bumps the entry's last-touched frame so that
  // the next evict() pass sees it as live. Returns nullptr on miss.
  [[nodiscard]] auto find(const PoseKey &key) -> PoseDrawTemplate * {
    auto it = m_entries.find(key);
    if (it == m_entries.end()) {
      ++m_stats.misses;
      return nullptr;
    }
    it->second.last_touched_frame = m_current_frame;
    ++m_stats.hits;
    return &it->second;
  }

  // Insert / replace a template. Always updates last_touched to the current
  // frame; callers don't have to bother.
  auto insert(const PoseKey &key, std::vector<DrawPartCmd> parts)
      -> PoseDrawTemplate & {
    PoseDrawTemplate tpl;
    tpl.parts = std::move(parts);
    tpl.last_touched_frame = m_current_frame;
    auto [it, inserted] = m_entries.insert_or_assign(key, std::move(tpl));
    if (inserted) {
      ++m_stats.inserts;
    }
    return it->second;
  }

  // Evict any entry whose last_touched_frame is older than
  // m_current_frame - max_age_frames. Cheap; called once per frame by the
  // renderer's end-of-frame housekeeping.
  void evict_older_than(std::uint64_t max_age_frames) {
    if (m_current_frame <= max_age_frames) {
      return;
    }
    std::uint64_t const cutoff = m_current_frame - max_age_frames;
    for (auto it = m_entries.begin(); it != m_entries.end();) {
      if (it->second.last_touched_frame < cutoff) {
        it = m_entries.erase(it);
        ++m_stats.evictions;
      } else {
        ++it;
      }
    }
  }

  [[nodiscard]] auto size() const noexcept -> std::size_t {
    return m_entries.size();
  }
  [[nodiscard]] auto stats() const noexcept -> const Stats & {
    return m_stats;
  }

  void clear() noexcept {
    m_entries.clear();
    m_stats = {};
  }

  // Fold a local-space template into world space and append the results to
  // `out`. This is the matrix-reduction path: N local parts × 1 world
  // multiply = N world matrices. No allocation if `out` is pre-reserved.
  static void expand_to_world(const PoseDrawTemplate &tpl,
                              const QMatrix4x4 &world,
                              std::vector<DrawPartCmd> &out) {
    out.reserve(out.size() + tpl.parts.size());
    for (const auto &local : tpl.parts) {
      DrawPartCmd cmd = local;
      cmd.world = world * local.world;
      out.push_back(std::move(cmd));
    }
  }

private:
  std::unordered_map<PoseKey, PoseDrawTemplate> m_entries;
  std::uint64_t m_current_frame = 0;
  Stats m_stats{};
};

} // namespace Render::GL
