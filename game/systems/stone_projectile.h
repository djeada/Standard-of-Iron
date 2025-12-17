#pragma once
#include "projectile.h"

namespace Game::Systems {

class StoneProjectile : public Projectile {
public:
  StoneProjectile(const QVector3D &start, const QVector3D &end,
                  const QVector3D &color, float speed, float arc_height,
                  float inv_dist, float scale = 1.0F,
                  bool should_apply_damage = false, int damage = 0,
                  Engine::Core::EntityID attacker_id = 0,
                  Engine::Core::EntityID target_id = 0);

  auto get_start() const -> QVector3D override { return m_start; }
  auto get_end() const -> QVector3D override { return m_end; }
  auto get_color() const -> QVector3D override { return m_color; }
  auto get_speed() const -> float override { return m_speed; }
  auto get_arc_height() const -> float override { return m_arc_height; }
  auto get_progress() const -> float override { return m_t; }
  auto get_scale() const -> float override { return m_scale; }
  auto is_active() const -> bool override { return m_active; }

  auto should_apply_damage() const -> bool override {
    return m_should_apply_damage;
  }
  auto get_damage() const -> int override { return m_damage; }
  auto get_target_id() const -> Engine::Core::EntityID override {
    return m_target_id;
  }
  auto get_attacker_id() const -> Engine::Core::EntityID override {
    return m_attacker_id;
  }
  auto get_target_locked_position() const -> QVector3D override {
    return m_target_locked_position;
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
  bool m_should_apply_damage{};
  int m_damage{};
  Engine::Core::EntityID m_target_id{0};
  Engine::Core::EntityID m_attacker_id{0};
  QVector3D m_target_locked_position;
};

} // namespace Game::Systems
