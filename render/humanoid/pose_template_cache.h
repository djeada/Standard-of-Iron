#pragma once

#include "../draw_part.h"
#include "pose_key.h"

#include <QMatrix4x4>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Render::GL {

struct PoseDrawTemplate {

  std::vector<DrawPartCmd> parts;
  std::uint64_t last_touched_frame = 0;
};

class PoseTemplateCache {
public:
  struct Stats {
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    std::uint64_t inserts = 0;
    std::uint64_t evictions = 0;
  };

  void begin_frame(std::uint64_t frame_index) noexcept {
    m_current_frame = frame_index;
  }

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

  auto insert(const PoseKey &key,
              std::vector<DrawPartCmd> parts) -> PoseDrawTemplate & {
    PoseDrawTemplate tpl;
    tpl.parts = std::move(parts);
    tpl.last_touched_frame = m_current_frame;
    auto [it, inserted] = m_entries.insert_or_assign(key, std::move(tpl));
    if (inserted) {
      ++m_stats.inserts;
    }
    return it->second;
  }

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
  [[nodiscard]] auto stats() const noexcept -> const Stats & { return m_stats; }

  void clear() noexcept {
    m_entries.clear();
    m_stats = {};
  }

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
