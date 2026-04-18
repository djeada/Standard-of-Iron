// Stage 15 — per-entity HumanoidClipDriver cache.
//
// Clip drivers carry per-unit state (current/previous state, blend timer)
// and must persist across frames for the state machine to work. This
// cache mirrors the entity-id keyed pattern used by UnitRenderCache /
// pose_palette_cache and is pruned each frame by advance_frame().
//
// The cache also tracks the last anim.time sampled for each entity so
// the driver tick receives a real dt even though the render call site
// does not carry a delta-time argument.

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

  // Mark the current frame. Entries not touched for `max_age` frames
  // are evicted. Intended to be called once per rendered frame from
  // the humanoid pipeline bootstrap (alongside advance_pose_cache_frame).
  void advance_frame(std::uint32_t current_frame, std::uint32_t max_age = 120);

  void clear() { m_entries.clear(); }
  [[nodiscard]] auto size() const noexcept -> std::size_t {
    return m_entries.size();
  }
  [[nodiscard]] auto current_frame() const noexcept -> std::uint32_t {
    return m_current_frame;
  }

  // Process-wide instance. Drivers are cheap POD + small vectors, but
  // one cache is enough: the humanoid pipeline is single-threaded today.
  static auto instance() -> ClipDriverCache &;

private:
  std::unordered_map<std::uint32_t, Entry> m_entries;
  std::uint32_t m_current_frame{0};
};

// Apply clip-driven overlays to a HumanoidPose in-place. Shifts the
// upper torso (chest-level) laterally along `right_axis` and vertically
// along world-up, and propagates the shift to shoulders/head so the
// upper body moves as a rigid unit. Hands and arms follow — if the
// caller wants arm IK to re-solve against the shifted torso, they
// should call this BEFORE running IK passes. The legs are not touched.
void apply_overlays_to_pose(Render::GL::HumanoidPose &pose,
                            const HumanoidOverlays &overlays,
                            const QVector3D &right_axis) noexcept;

} // namespace Render::Humanoid
