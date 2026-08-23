#pragma once

#include <QVector3D>

namespace Game::Systems {

class BuildingCollisionRegistry;

[[nodiscard]] auto
first_building_intersection_fraction(const BuildingCollisionRegistry& buildings,
                                     const QVector3D& start,
                                     const QVector3D& end,
                                     unsigned int ignore_entity_id = 0) -> float;

[[nodiscard]] auto has_clear_building_los(const BuildingCollisionRegistry& buildings,
                                          const QVector3D& start,
                                          const QVector3D& end,
                                          unsigned int ignore_entity_id = 0) -> bool;

} // namespace Game::Systems
