#pragma once

#include <QVector3D>

#include <utility>
#include <vector>

#include "core/component_combat.h"
#include "core/world.h"
#include "movement_system.h"
#include "systems/navigation/pathfinding.h"

namespace Game::Systems {

class MovementSystem::Assignment {
public:
  static void assign_direct_target(Engine::Core::MovementComponent& movement,
                                   const QVector3D& target);

  [[nodiscard]] static auto
  assign_waypoints_to_movement(Pathfinding& pathfinder,
                               const std::vector<QVector3D>& waypoints,
                               const QVector3D& resolved_goal,
                               const Engine::Core::TransformComponent& transform,
                               Engine::Core::MovementComponent& movement) -> bool;

  static void
  assign_navigation_target(Pathfinding* pathfinder,
                           const Engine::Core::TransformComponent& transform,
                           Engine::Core::MovementComponent& movement,
                           const QVector3D& requested_target);

  [[nodiscard]] static auto
  assign_escape_if_sealed(Pathfinding& pathfinder,
                          const Engine::Core::TransformComponent& transform,
                          Engine::Core::MovementComponent& movement,
                          const QVector3D& target) -> bool;

  [[nodiscard]] static auto
  assign_local_recovery_move(const QVector3D& current_position,
                             const QVector3D& goal,
                             Engine::Core::MovementComponent* movement) -> bool;

  static auto retarget_unit(Engine::Core::World& world,
                            Engine::Core::EntityID entity_id,
                            const QVector3D& goal) -> bool;

  static void stamp_route_revision(Engine::Core::MovementComponent& movement);

  static void adopt_formation_group(Engine::Core::World& world,
                                    Engine::Core::EntityID unit_id,
                                    const Engine::Core::TransformComponent& transform,
                                    Engine::Core::MovementComponent& movement);

private:
  static auto keep_route_toward(Pathfinding* pathfinder,
                                const Engine::Core::TransformComponent& transform,
                                Engine::Core::MovementComponent& movement,
                                const QVector3D& requested_target) -> bool;
  static void route_to_planned_target(Pathfinding& pathfinder,
                                      const Engine::Core::TransformComponent& transform,
                                      Engine::Core::MovementComponent& movement,
                                      const QVector3D& current_pos,
                                      const QVector3D& planned_target);
  static void append_aligned_waypoints(const std::vector<QVector3D>& waypoints,
                                       Engine::Core::MovementComponent& movement);
  static void straighten_path(Pathfinding& pathfinder,
                              const Engine::Core::TransformComponent& transform,
                              Engine::Core::MovementComponent& movement);
  static void skip_reached_waypoints(const Engine::Core::TransformComponent& transform,
                                     Engine::Core::MovementComponent& movement);
};

} // namespace Game::Systems
