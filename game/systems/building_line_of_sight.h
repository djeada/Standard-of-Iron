#pragma once

#include <QVector3D>

namespace Game::Systems {

[[nodiscard]] auto
first_building_intersection_fraction(const QVector3D& start,
                                     const QVector3D& end,
                                     unsigned int ignore_entity_id = 0) -> float;

[[nodiscard]] auto has_clear_building_los(const QVector3D& start,
                                          const QVector3D& end,
                                          unsigned int ignore_entity_id = 0) -> bool;

[[nodiscard]] auto nearest_building_body_clearance(const QVector3D& point) -> float;

[[nodiscard]] auto
first_building_body_intersection_fraction(const QVector3D& start,
                                          const QVector3D& end,
                                          float radius,
                                          unsigned int ignore_entity_id = 0) -> float;

[[nodiscard]] auto depenetrate_from_building_bodies(const QVector3D& point,
                                                    float radius) -> QVector3D;

} // namespace Game::Systems
