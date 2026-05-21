#include "arrow_projectile.h"

namespace Game::Systems {

ArrowProjectile::ArrowProjectile(const QVector3D& start,
                                 const QVector3D& end,
                                 const QVector3D& color,
                                 float speed,
                                 float arc_height,
                                 float inv_dist,
                                 bool is_ballista_bolt,
                                 ProjectileKind kind,
                                 bool should_apply_damage,
                                 int damage,
                                 Engine::Core::EntityID attacker_id,
                                 Engine::Core::EntityID target_id,
                                 float impact_radius,
                                 float splash_damage_multiplier,
                                 bool friendly_fire)
    : m_start(start)
    , m_end(end)
    , m_color(color)
    , m_speed(speed)
    , m_arc_height(arc_height)
    , m_inv_dist(inv_dist)
    , m_is_ballista_bolt(is_ballista_bolt)
    , m_kind(kind)
    , m_should_apply_damage(should_apply_damage)
    , m_damage(damage)
    , m_attacker_id(attacker_id)
    , m_target_id(target_id)
    , m_impact_radius(impact_radius)
    , m_splash_damage_multiplier(splash_damage_multiplier)
    , m_friendly_fire(friendly_fire) {
}

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

} // namespace Game::Systems
