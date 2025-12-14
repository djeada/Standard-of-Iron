#pragma once

#include "projectile.h"
#include <QVector3D>

namespace Game::Systems {

class HealingBeam : public Projectile {
public:
  HealingBeam(const QVector3D &healer_pos, const QVector3D &target_pos,
              const QVector3D &color, float duration = 0.8F);

  auto get_start() const -> QVector3D override { return m_healer_pos; }
  auto get_end() const -> QVector3D override { return m_target_pos; }
  auto get_color() const -> QVector3D override { return m_color; }
  auto get_speed() const -> float override { return 1.0F / m_duration; }
  auto get_arc_height() const -> float override;
  auto get_progress() const -> float override { return m_progress; }
  auto get_scale() const -> float override { return m_beam_width; }
  auto is_active() const -> bool override { return m_active; }

  auto should_apply_damage() const -> bool override { return false; }
  auto get_damage() const -> int override { return 0; }
  auto get_target_id() const -> Engine::Core::EntityID override { return 0; }
  auto get_attacker_id() const -> Engine::Core::EntityID override { return 0; }
  auto get_target_locked_position() const -> QVector3D override {
    return m_target_pos;
  }

  void update(float delta_time) override;
  void deactivate() override { m_active = false; }

  auto get_duration() const -> float { return m_duration; }
  auto get_beam_width() const -> float { return m_beam_width; }
  auto get_intensity() const -> float { return m_intensity; }

  void set_beam_width(float width) { m_beam_width = width; }
  void set_intensity(float intensity) { m_intensity = intensity; }

private:
  QVector3D m_healer_pos;
  QVector3D m_target_pos;
  QVector3D m_color;
  float m_duration;
  float m_progress{0.0F};
  float m_beam_width{0.15F};
  float m_intensity{1.0F};
  bool m_active{true};
};

} // namespace Game::Systems
