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
                                 bool friendly_fire,
                                 ArrowVisualStyle visual_style,
                                 ArrowVisualProfile visual_profile,
                                 QVector3D target_origin_at_launch)
    : m_start(start)
    , m_end(end)
    , m_color(color)
    , m_t(visual_profile.initial_progress)
    , m_speed(speed)
    , m_arc_height(arc_height)
    , m_inv_dist(inv_dist)
    , m_scale(visual_profile.scale)
    , m_is_ballista_bolt(is_ballista_bolt)
    , m_kind(kind)
    , m_should_apply_damage(should_apply_damage)
    , m_damage(damage)
    , m_attacker_id(attacker_id)
    , m_target_id(target_id)
    , m_target_origin_at_launch(target_origin_at_launch)
    , m_impact_radius(impact_radius)
    , m_splash_damage_multiplier(splash_damage_multiplier)
    , m_friendly_fire(friendly_fire)
    , m_visual_style(visual_style)
    , m_roll_deg(visual_profile.roll_deg)
    , m_spin_rate_deg(visual_profile.spin_rate_deg)
    , m_trail_alpha(visual_profile.trail_alpha)
    , m_trail_length(visual_profile.trail_length)
    , m_brightness(visual_profile.brightness) {
}

void ArrowProjectile::update(float delta_time) {
  if (!m_active) {
    return;
  }

  m_t += delta_time * m_speed * m_inv_dist;
  if (m_t >= 1.0F) {
    m_t = 1.0F;
  }
}

} // namespace Game::Systems
