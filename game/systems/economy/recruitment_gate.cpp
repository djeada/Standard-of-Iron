#include "recruitment_gate.h"

#include <cmath>

#include "core/component.h"
#include "core/component_structures.h"
#include "systems/building_collision_registry.h"
#include "systems/movement/body_profile.h"
#include "systems/navigation/walkability.h"

namespace Game::Systems {
namespace {

constexpr float k_gate_margin = 0.45F;
constexpr float k_assembly_distance = 4.2F;
constexpr float k_assembly_side_shift = 1.2F;
constexpr float k_assembly_search = 12.0F;
constexpr float k_slot_spacing = 1.5F;
constexpr int k_slots_per_row = 3;
constexpr float k_degrees_to_radians = 3.14159265F / 180.0F;

auto to_world(const Engine::Core::TransformComponent& transform,
              float local_x,
              float local_z) -> QVector3D {
  const float yaw = transform.rotation.y * k_degrees_to_radians;
  const float c = std::cos(yaw);
  const float s = std::sin(yaw);
  return {transform.position.x + local_x * c + local_z * s,
          0.0F,
          transform.position.z - local_x * s + local_z * c};
}

} // namespace

auto barracks_gate(const Engine::Core::TransformComponent& transform)
    -> RecruitmentGate {
  const auto body = BuildingCollisionRegistry::get_building_body("barracks");
  const float front = body.offset_z + body.depth * 0.5F + k_gate_margin;
  RecruitmentGate gate;
  gate.position = to_world(transform, body.offset_x, front);
  const QVector3D ahead = to_world(transform, body.offset_x, front + 1.0F);
  gate.outward = (ahead - gate.position).normalized();
  return gate;
}

auto barracks_default_rally(const Engine::Core::TransformComponent& transform)
    -> QVector3D {
  const RecruitmentGate gate = barracks_gate(transform);
  const QVector3D away_from_stockpile(-gate.outward.z(), 0.0F, gate.outward.x());
  return gate.position + gate.outward * k_assembly_distance +
         away_from_stockpile * k_assembly_side_shift;
}

auto barracks_assembly_point(const Engine::Core::TransformComponent& transform,
                             Engine::Core::ProductionComponent& production)
    -> QVector3D {
  if (production.assembly_cached) {
    return {production.assembly_x, 0.0F, production.assembly_z};
  }
  const QVector3D wanted = barracks_default_rally(transform);
  const QVector3D found =
      Walkability::nearest_standable(wanted, BodyProfile{}, k_assembly_search)
          .value_or(wanted);
  production.assembly_x = found.x();
  production.assembly_z = found.z();
  production.assembly_cached = true;
  return {found.x(), 0.0F, found.z()};
}

auto barracks_assembly_slot(const QVector3D& assembly,
                            const RecruitmentGate& gate,
                            int index) -> QVector3D {
  const QVector3D across(gate.outward.z(), 0.0F, -gate.outward.x());
  const int column = index % k_slots_per_row;
  const int row = (index / k_slots_per_row) % 2;
  return assembly + across * (static_cast<float>(column - 1) * k_slot_spacing) +
         gate.outward * (static_cast<float>(row) * k_slot_spacing);
}

} // namespace Game::Systems
