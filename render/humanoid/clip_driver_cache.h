

#pragma once

#include "humanoid_clip_registry.h"

#include <cstdint>
#include <unordered_map>

namespace Render::Humanoid {

class ClipDriverCache {
public:
  struct Entry {
    HumanoidClipDriver driver;
    float last_time{0.0F};
    std::uint32_t last_seen_frame{0};
    bool initialised{false};
  };

  auto get_or_create(std::uint32_t entity_id) -> Entry &;

  void advance_frame(std::uint32_t current_frame, std::uint32_t max_age = 120);

  void clear() { m_entries.clear(); }
  [[nodiscard]] auto size() const noexcept -> std::size_t {
    return m_entries.size();
  }
  [[nodiscard]] auto current_frame() const noexcept -> std::uint32_t {
    return m_current_frame;
  }

  static auto instance() -> ClipDriverCache &;

private:
  std::unordered_map<std::uint32_t, Entry> m_entries;
  std::uint32_t m_current_frame{0};
};

void apply_overlays_to_pose(Render::GL::HumanoidPose &pose,
                            const HumanoidOverlays &overlays,
                            const QVector3D &right_axis) noexcept;

} // namespace Render::Humanoid
