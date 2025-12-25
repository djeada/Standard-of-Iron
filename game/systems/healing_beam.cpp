#include "healing_beam.h"
#include <cmath>

namespace Game::Systems {

HealingBeam::HealingBeam(const QVector3D &healer_pos,
                         const QVector3D &target_pos, const QVector3D &color,
                         float duration)
    : m_healer_pos(healer_pos), m_target_pos(target_pos), m_color(color),
      m_duration(duration) {

  float dist = (target_pos - healer_pos).length();
  m_beam_width = 0.1F + dist * 0.02F;
}

auto HealingBeam::get_arc_height() const -> float {

  float dist = (m_target_pos - m_healer_pos).length();
  return dist * 0.25F;
}

void HealingBeam::update(float delta_time) {
  if (!m_active) {
    return;
  }

  m_progress += delta_time / m_duration;

  if (m_progress >= 1.3F) {
    m_active = false;
  }

  float fade = 1.0F;
  if (m_progress > 1.0F) {
    fade = 1.0F - (m_progress - 1.0F) / 0.3F;
  }
  m_intensity = fade * (0.8F + 0.2F * std::sin(m_progress * 20.0F));
}

} 
