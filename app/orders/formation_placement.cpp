#include "app/orders/formation_placement.h"

#include <cmath>
#include <numbers>

namespace App::Controllers {

namespace {
constexpr float k_min_aim_distance = 0.1F;
constexpr float k_min_drag_length = 0.5F;

auto heading_degrees(float x, float z) -> float {
  return std::atan2(x, z) * 180.0F / std::numbers::pi_v<float>;
}
} // namespace

void FormationPlacement::begin(std::vector<EntityID> units,
                               const QVector3D& position,
                               bool right_drag) {
  m_units = std::move(units);
  m_placing = true;
  m_right_drag = right_drag;
  m_position = position;
  m_frontage = 0.0F;
  m_aim_distance = 0.0F;
  m_drag_active = false;
  m_facing_explicit = false;
}

void FormationPlacement::end() {
  m_placing = false;
  m_drag_active = false;
  m_right_drag = false;
  m_position = QVector3D();
  m_facing_degrees = 0.0F;
  m_facing_explicit = false;
  m_aim_distance = 0.0F;
  m_frontage = 0.0F;
  m_units.clear();
}

void FormationPlacement::finish_deployment() {
  m_placing = false;
  m_drag_active = false;
  m_facing_explicit = false;
  m_aim_distance = 0.0F;
  m_units.clear();
}

void FormationPlacement::move_to(const QVector3D& position) {
  m_position = position;
  m_aim_distance = 0.0F;
}

void FormationPlacement::set_facing(float degrees, bool explicit_choice) {
  float normalized = std::fmod(degrees + 180.0F, 360.0F);
  if (normalized < 0.0F) {
    normalized += 360.0F;
  }
  m_facing_degrees = normalized - 180.0F;
  if (explicit_choice) {
    m_facing_explicit = true;
  }
}

void FormationPlacement::follow_auto_facing(float auto_facing_degrees) {
  if (m_facing_explicit) {
    return;
  }
  set_facing(auto_facing_degrees, false);
}

auto FormationPlacement::aim_at(const QVector3D& aim_point) -> bool {
  QVector3D delta = aim_point - m_position;
  delta.setY(0.0F);
  float const distance = delta.length();
  if (distance < k_min_aim_distance) {
    return false;
  }
  m_aim_distance = distance;
  set_facing(heading_degrees(delta.x(), delta.z()), true);
  return true;
}

void FormationPlacement::begin_drag(const QVector3D& start) {
  m_drag_active = true;
  m_drag_start = start;
  m_position = start;
  m_frontage = 0.0F;
  m_aim_distance = 0.0F;
}

auto FormationPlacement::drag_to(const QVector3D& current) -> FormationDragStep {
  QVector3D along = current - m_drag_start;
  along.setY(0.0F);
  float const length = along.length();

  if (m_units.size() == 1) {
    m_position = m_drag_start;
    if (length > k_min_drag_length) {
      (void)aim_at(current);
      return {};
    }
    m_aim_distance = 0.0F;
    return {.follow_auto_facing = true};
  }

  m_position = m_drag_start + (along * 0.5F);
  if (length > k_min_drag_length) {
    m_frontage = length;
    set_facing(heading_degrees(-along.z(), along.x()), true);
    return {.frontage_changed = true};
  }
  return {.follow_auto_facing = true};
}

} // namespace App::Controllers
