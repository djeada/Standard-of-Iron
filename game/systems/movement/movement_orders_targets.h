#pragma once

#include <QVector3D>

#include <optional>
#include <utility>
#include <vector>

#include "core/component_combat.h"
#include "systems/navigation/nav_grid.h"
#include "systems/navigation/pathfinding.h"

namespace Game::Systems::MovementTargeting {

[[nodiscard]] auto passability_for(const Engine::Core::MovementComponent& movement)
    -> Pathfinding::Passability;

[[nodiscard]] auto is_direct_path_walkable(const QVector3D& from,
                                           const QVector3D& to,
                                           Pathfinding::Passability passability,
                                           float clearance_radius) -> bool;

[[nodiscard]] auto find_recovery_cell(const Pathfinding& pathfinder,
                                      const Point& origin,
                                      Pathfinding::Passability passability,
                                      Point& recovery_cell) -> bool;

[[nodiscard]] auto
resolve_walkable_direct_target(const QVector3D& target,
                               Pathfinding::Passability passability) -> QVector3D;

[[nodiscard]] auto
resolve_walkable_target_toward(const QVector3D& target,
                               const QVector3D& from,
                               Pathfinding::Passability passability) -> QVector3D;

[[nodiscard]] auto segment_traverses_navigation_portal(const QVector3D& from,
                                                       const QVector3D& to) -> bool;

[[nodiscard]] auto
align_portal_waypoint(const QVector3D& waypoint,
                      bool final_waypoint,
                      const std::optional<QVector3D>& previous) -> QVector3D;

[[nodiscard]] auto
path_legs_are_walkable(Pathfinding& pathfinder,
                       const Engine::Core::TransformComponent& transform,
                       Pathfinding::Passability passability,
                       const std::vector<std::pair<float, float>>& path) -> bool;

void pull_path_taut(Pathfinding& pathfinder,
                    const Engine::Core::TransformComponent& transform,
                    Pathfinding::Passability passability,
                    float clearance_radius,
                    std::vector<std::pair<float, float>>& path);

} // namespace Game::Systems::MovementTargeting
