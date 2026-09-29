#pragma once

#include <QVector3D>

#include <cstdint>
#include <utility>
#include <vector>

namespace App::Controllers {

struct FormationDragStep {
  bool follow_auto_facing = false;
  bool frontage_changed = false;
};

class FormationPlacement {
public:
  using EntityID = std::uint64_t;

  void begin(std::vector<EntityID> units, const QVector3D& position, bool right_drag);
  void end();
  void set_units(std::vector<EntityID> units) { m_units = std::move(units); }

  [[nodiscard]] auto placing() const -> bool { return m_placing; }
  [[nodiscard]] auto right_drag() const -> bool { return m_right_drag; }
  [[nodiscard]] auto units() const -> const std::vector<EntityID>& { return m_units; }
  [[nodiscard]] auto position() const -> const QVector3D& { return m_position; }
  [[nodiscard]] auto facing_degrees() const -> float { return m_facing_degrees; }
  [[nodiscard]] auto facing_explicit() const -> bool { return m_facing_explicit; }
  [[nodiscard]] auto aim_distance() const -> float { return m_aim_distance; }
  [[nodiscard]] auto frontage() const -> float { return m_frontage; }
  [[nodiscard]] auto dragging() const -> bool { return m_drag_active; }
  [[nodiscard]] auto drag_start() const -> const QVector3D& { return m_drag_start; }
  [[nodiscard]] auto single_unit() const -> bool {
    return m_placing && m_units.size() == 1;
  }

  void move_to(const QVector3D& position);
  void clear_frontage() { m_frontage = 0.0F; }
  void clear_facing_choice() { m_facing_explicit = false; }
  void clear_aim() { m_aim_distance = 0.0F; }
  void set_facing(float degrees, bool explicit_choice);
  void follow_auto_facing(float auto_facing_degrees);
  [[nodiscard]] auto aim_at(const QVector3D& aim_point) -> bool;

  void begin_drag(const QVector3D& start);
  void end_drag() { m_drag_active = false; }
  [[nodiscard]] auto drag_to(const QVector3D& current) -> FormationDragStep;

  void finish_deployment();
  void end_right_drag() { m_right_drag = false; }

private:
  bool m_placing = false;
  bool m_right_drag = false;
  QVector3D m_position;
  float m_facing_degrees = 0.0F;
  bool m_facing_explicit = false;
  float m_aim_distance = 0.0F;
  float m_frontage = 0.0F;
  std::vector<EntityID> m_units;
  bool m_drag_active = false;
  QVector3D m_drag_start;
};

} // namespace App::Controllers
