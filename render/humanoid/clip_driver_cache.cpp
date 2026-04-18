#include "clip_driver_cache.h"

#include <QVector3D>

namespace Render::Humanoid {

auto ClipDriverCache::get_or_create(std::uint32_t entity_id) -> Entry & {
  auto it = m_entries.find(entity_id);
  if (it == m_entries.end()) {
    it = m_entries.emplace(entity_id, Entry{}).first;
  }
  it->second.last_seen_frame = m_current_frame;
  return it->second;
}

void ClipDriverCache::advance_frame(std::uint32_t current_frame,
                                    std::uint32_t max_age) {
  m_current_frame = current_frame;
  for (auto it = m_entries.begin(); it != m_entries.end();) {
    if (current_frame - it->second.last_seen_frame > max_age) {
      it = m_entries.erase(it);
    } else {
      ++it;
    }
  }
}

auto ClipDriverCache::instance() -> ClipDriverCache & {
  static ClipDriverCache k_instance;
  return k_instance;
}

void apply_overlays_to_pose(Render::GL::HumanoidPose &pose,
                            const HumanoidOverlays &overlays,
                            const QVector3D &right_axis) noexcept {
  QVector3D right = right_axis;
  if (right.lengthSquared() < 1e-6F) {
    right = QVector3D(1.0F, 0.0F, 0.0F);
  } else {
    right.normalize();
  }
  QVector3D const up(0.0F, 1.0F, 0.0F);

  QVector3D const lateral = right * overlays.torso_sway_x;
  QVector3D const breath = up * overlays.breathing_y;

  // Shift the upper torso as a rigid unit. Neck_base gets both shifts;
  // head gets both. Shoulders get the lateral only (breathing looks
  // odd when shoulders bob). Chest body frame is not touched here —
  // the skin code reads it off neck_base/head_pos for its own basis.
  pose.neck_base += lateral + breath;
  pose.head_pos += lateral + breath;
  pose.shoulder_l += lateral;
  pose.shoulder_r += lateral;
}

} // namespace Render::Humanoid
