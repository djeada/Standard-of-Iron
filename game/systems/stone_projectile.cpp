#include "stone_projectile.h"

namespace Game::Systems {

StoneProjectile::StoneProjectile(const QVector3D &start, const QVector3D &end,
                                 const QVector3D &color, float speed,
                                 float arc_height, float inv_dist, float scale,
                                 bool should_apply_damage, int damage,
                                 Engine::Core::EntityID attacker_id,
                                 Engine::Core::EntityID target_id)
    : m_start(start), m_end(end), m_color(color), m_speed(speed),
      m_arc_height(arc_height), m_inv_dist(inv_dist), m_scale(scale),
      m_should_apply_damage(should_apply_damage), m_damage(damage),
      m_target_id(target_id), m_attacker_id(attacker_id),
      m_target_locked_position(end) {}

void StoneProjectile::update(float delta_time) {
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
