#include "arrow_system.h"

#include <qvectornd.h>

#include <algorithm>
#include <cmath>

namespace Game::Systems {
namespace {

void apply_visual_profile(ArrowInstance& arrow,
                          ArrowVisualStyle style,
                          std::uint32_t sequence) {
  arrow.style = style;
  auto const profile = make_arrow_visual_profile(style, sequence);
  arrow.t = profile.initial_progress;
  arrow.scale = profile.scale;
  arrow.roll_deg = profile.roll_deg;
  arrow.spin_rate_deg = profile.spin_rate_deg;
  arrow.trail_alpha = profile.trail_alpha;
  arrow.trail_length = profile.trail_length;
  arrow.brightness = profile.brightness;
}

} // namespace

ArrowSystem::ArrowSystem()
    : m_config(GameConfig::instance().arrow()) {
}

void ArrowSystem::spawn_arrow(const QVector3D& start,
                              const QVector3D& end,
                              const QVector3D& color,
                              float speed,
                              ArrowVisualStyle style) {
  ArrowInstance a;
  a.start = start;
  a.end = end;
  a.color = color;
  a.speed = speed;
  a.active = true;
  QVector3D const delta = end - start;
  float const dist = delta.length();
  a.arc_height = std::clamp(m_config.arc_height_multiplier * dist,
                            m_config.arc_height_min,
                            m_config.arc_height_max);

  a.inv_dist = (dist > 0.001F) ? (1.0F / dist) : 1.0F;
  apply_visual_profile(a, style, ++m_spawn_sequence);
  m_arrows.push_back(a);
}

void ArrowSystem::update(Engine::Core::World*, float delta_time) {
  for (auto& arrow : m_arrows) {
    if (!arrow.active) {
      continue;
    }

    arrow.t += delta_time * arrow.speed * arrow.inv_dist;
    if (arrow.t >= 1.0F) {
      arrow.t = 1.0F;
      arrow.active = false;
    }
  }

  m_arrows.erase(std::remove_if(m_arrows.begin(),
                                m_arrows.end(),
                                [](const ArrowInstance& a) { return !a.active; }),
                 m_arrows.end());
}

} // namespace Game::Systems
