#include "arrow_projectile.h"

namespace Game::Systems {

ArrowProjectile::ArrowProjectile(const QVector3D &start, const QVector3D &end,
                                 const QVector3D &color, float speed,
                                 float arc_height, float inv_dist,
                                 bool is_ballista_bolt)
    : m_start(start), m_end(end), m_color(color), m_speed(speed),
      m_arc_height(arc_height), m_inv_dist(inv_dist),
      m_is_ballista_bolt(is_ballista_bolt) {}

void ArrowProjectile::update(float delta_time) {
  if (!m_active) {
    return;
  }

  m_t += delta_time * m_speed * m_inv_dist;
  if (m_t >= 1.0F) {
    m_t = 1.0F;
    m_active = false;
  }
}

} 
