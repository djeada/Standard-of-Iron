#pragma once

#include <QVector3D>

#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include "pathfinding.h"

namespace Game::Systems {

struct RouteCorridorPlan {
  std::uint64_t id{0};
  std::vector<QVector3D> centerline;

  [[nodiscard]] auto reachable() const noexcept -> bool {
    return centerline.size() >= 2U;
  }
};

struct RouteLanePlan {
  std::vector<QVector3D> waypoints;
  float lateral_offset{0.0F};
  float minimum_lateral_scale{1.0F};
  std::optional<QVector3D> opening_point;
  std::optional<QVector3D> reform_point;

  [[nodiscard]] auto valid() const noexcept -> bool { return waypoints.size() >= 2U; }
  [[nodiscard]] auto requires_controlled_break() const noexcept -> bool {
    return std::abs(lateral_offset) > 0.1F && minimum_lateral_scale < 0.99F;
  }
};

class RouteCorridorPlanner {
public:
  [[nodiscard]] static auto identity(const std::vector<QVector3D>& points,
                                     std::uint64_t topology_revision) -> std::uint64_t;

  [[nodiscard]] static auto plan(Pathfinding& pathfinder,
                                 const QVector3D& start,
                                 const QVector3D& destination,
                                 Pathfinding::Passability passability,
                                 float clearance) -> RouteCorridorPlan;

  [[nodiscard]] static auto fit_lane(Pathfinding& pathfinder,
                                     const RouteCorridorPlan& corridor,
                                     const QVector3D& member_start,
                                     const QVector3D& member_destination,
                                     float lateral_offset,
                                     Pathfinding::Passability passability,
                                     float clearance) -> RouteLanePlan;
};

} // namespace Game::Systems
