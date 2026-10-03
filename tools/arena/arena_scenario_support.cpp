#include "arena_scenario_internal.h"

namespace Arena::scenario_internal {

auto shortest_degrees(float to_degrees, float from_degrees) -> float {
  float diff = to_degrees - from_degrees;
  while (diff > 180.0F) {
    diff -= 360.0F;
  }
  while (diff < -180.0F) {
    diff += 360.0F;
  }
  return diff;
}

auto vector_from_transform(const Engine::Core::TransformComponent& transform)
    -> QVector3D {
  return {transform.position.x, transform.position.y, transform.position.z};
}

auto horizontal_distance(const QVector3D& lhs, const QVector3D& rhs) -> float {
  QVector3D delta = lhs - rhs;
  delta.setY(0.0F);
  return delta.length();
}

} // namespace Arena::scenario_internal
