#pragma once
#include "arrow_visual_profile.h"
#include "projectile.h"

namespace Game::Systems {

class ArrowProjectile : public Projectile {
public:
  ArrowProjectile(const QVector3D& start,
                  const QVector3D& end,
                  const QVector3D& color,
                  float speed,
                  float arc_height,
                  float inv_dist,
                  bool is_ballista_bolt = false,
                  ProjectileKind kind = ProjectileKind::Arrow,
                  bool should_apply_damage = false,
                  int damage = 0,
                  Engine::Core::EntityID attacker_id = 0,
                  Engine::Core::EntityID target_id = 0,
                  float impact_radius = 0.0F,
                  float splash_damage_multiplier = 0.6F,
                  bool friendly_fire = false,
                  ArrowVisualStyle visual_style = ArrowVisualStyle::Focused,
                  ArrowVisualProfile visual_profile = {},
                  QVector3D target_origin_at_launch = {});

  auto get_start() const -> QVector3D override { return m_start; }
  auto get_end() const -> QVector3D override { return m_end; }
  auto get_color() const -> QVector3D override { return m_color; }
  auto get_speed() const -> float override { return m_speed; }
  auto get_arc_height() const -> float override { return m_arc_height; }
  auto get_progress() const -> float override { return m_t; }
  auto get_scale() const -> float override { return m_scale; }
  auto get_kind() const -> ProjectileKind override { return m_kind; }
  auto is_active() const -> bool override { return m_active; }
  auto is_ballista_bolt() const -> bool { return m_is_ballista_bolt; }
  auto visual_style() const -> ArrowVisualStyle { return m_visual_style; }
  auto length_scale() const -> float { return m_length_scale; }
  auto roll_deg() const -> float { return m_roll_deg; }
  auto spin_rate_deg() const -> float { return m_spin_rate_deg; }
  auto trail_alpha() const -> float { return m_trail_alpha; }
  auto trail_length() const -> float { return m_trail_length; }
  auto brightness() const -> float { return m_brightness; }
  auto impact_radius() const -> float { return m_impact_radius; }
  auto splash_damage_multiplier() const -> float { return m_splash_damage_multiplier; }
  auto friendly_fire() const -> bool { return m_friendly_fire; }

  auto should_apply_damage() const -> bool override { return m_should_apply_damage; }
  auto get_damage() const -> int override { return m_damage; }
  auto get_target_id() const -> Engine::Core::EntityID override { return m_target_id; }
  auto get_attacker_id() const -> Engine::Core::EntityID override {
    return m_attacker_id;
  }
  auto get_target_locked_position() const -> QVector3D override {
    return m_target_origin_at_launch;
  }

  void update(float delta_time) override;
  void deactivate() override {
    m_active = false;
    m_should_apply_damage = false;
  }

private:
  QVector3D m_start;
  QVector3D m_end;
  QVector3D m_color;
  float m_t{0.0F};
  float m_speed{};
  float m_arc_height{};
  float m_inv_dist{};
  float m_scale{1.0F};
  bool m_active{true};
  bool m_is_ballista_bolt{false};
  ProjectileKind m_kind{ProjectileKind::Arrow};
  bool m_should_apply_damage{false};
  int m_damage{0};
  Engine::Core::EntityID m_attacker_id{0};
  Engine::Core::EntityID m_target_id{0};
  QVector3D m_target_origin_at_launch;
  float m_impact_radius{0.0F};
  float m_splash_damage_multiplier{0.6F};
  bool m_friendly_fire{false};
  ArrowVisualStyle m_visual_style{ArrowVisualStyle::Focused};
  float m_length_scale{1.0F};
  float m_roll_deg{0.0F};
  float m_spin_rate_deg{0.0F};
  float m_trail_alpha{0.0F};
  float m_trail_length{0.0F};
  float m_brightness{1.0F};
};

} // namespace Game::Systems
