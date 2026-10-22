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

[[nodiscard]] auto
first_building_body_intersection_fraction(const BuildingCollisionRegistry& buildings,
                                          const QVector3D& start,
                                          const QVector3D& end,
                                          float radius,
                                          unsigned int ignore_entity_id = 0,
                                          bool skip_wall_links = false) -> float;

[[nodiscard]] auto
depenetrate_from_building_bodies(const BuildingCollisionRegistry& buildings,
                                 const QVector3D& point,
                                 float radius,
                                 bool skip_wall_links = false) -> QVector3D;

[[nodiscard]] auto
nearest_building_body_clearance(const BuildingCollisionRegistry& buildings,
                                const QVector3D& point,
                                bool skip_wall_links = false) -> float;

} // namespace Game::Systems
