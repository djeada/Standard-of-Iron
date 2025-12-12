#pragma once
#include "projectile.h"

namespace Game::Systems {

class ArrowProjectile : public Projectile {
public:
  ArrowProjectile(const QVector3D &start, const QVector3D &end,
                  const QVector3D &color, float speed, float arc_height,
                  float inv_dist, bool is_ballista_bolt = false);

  auto get_start() const -> QVector3D override { return m_start; }
  auto get_end() const -> QVector3D override { return m_end; }
  auto get_color() const -> QVector3D override { return m_color; }
  auto get_speed() const -> float override { return m_speed; }
  auto get_arc_height() const -> float override { return m_arc_height; }
  auto get_progress() const -> float override { return m_t; }
  auto get_scale() const -> float override { return m_scale; }
  auto is_active() const -> bool override { return m_active; }
  auto is_ballista_bolt() const -> bool { return m_is_ballista_bolt; }

  auto should_apply_damage() const -> bool override { return false; }
  auto get_damage() const -> int override { return 0; }
  auto get_target_id() const -> Engine::Core::EntityID override { return 0; }
  auto get_attacker_id() const -> Engine::Core::EntityID override { return 0; }
  auto get_target_locked_position() const -> QVector3D override {
    return m_end;
  }

  void update(float delta_time) override;
  void deactivate() override { m_active = false; }

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
};

} // namespace Game::Systems
