#pragma once

#include <QVector3D>

#include <optional>

#include "pathfinding.h"
#include "systems/nav_grid_types.h"

namespace Game::Systems {

struct BodyProfile {
  float radius{0.0F};
  Pathfinding::Passability passability{Pathfinding::Passability::Light};

  bool stops_at_building_facade{false};

  bool scrambles_hill_slopes{false};

  [[nodiscard]] auto clearance() const -> float {
    return radius <= 0.0F || stops_at_building_facade
               ? 0.0F
               : Pathfinding::traversal_clearance_for_body(radius);
  }
};

namespace Walkability {

void refresh();

[[nodiscard]] auto can_stand(const QVector3D& position,
                             const BodyProfile& profile) -> bool;

[[nodiscard]] auto can_traverse(const QVector3D& from,
                                const QVector3D& to,
                                const BodyProfile& profile) -> bool;

[[nodiscard]] auto penetration(const QVector3D& position,
                               const BodyProfile& profile) -> float;

struct SlopeFooting {
  bool on_slope{false};
  float grade{0.0F};
  QVector3D downhill{0.0F, 0.0F, 0.0F};
};

[[nodiscard]] auto slope_footing(const QVector3D& position) -> SlopeFooting;

[[nodiscard]] auto nearest_standable(const QVector3D& position,
                                     const BodyProfile& profile,
                                     float max_search_radius,
                                     const std::optional<QVector3D>& approach_from =
                                         std::nullopt) -> std::optional<QVector3D>;

[[nodiscard]] auto
standing_point_around(const QVector3D& target,
                      float preferred_bearing_radians,
                      float distance,
                      const BodyProfile& profile) -> std::optional<QVector3D>;

} // namespace Walkability

} // namespace Game::Systems
