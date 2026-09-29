#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector3D>

#include "map/map_definition.h"

namespace Game::Map::loader_detail {

[[nodiscard]] auto authored_position(float raw_x,
                                     float raw_z,
                                     const GridDefinition& grid,
                                     CoordSystem coord_sys) -> QVector3D;

[[nodiscard]] auto read_map_header(const QJsonObject& root,
                                   MapDefinition& out_map,
                                   QString* out_error) -> bool;
void read_map_appearance(const QJsonObject& root, MapDefinition& out_map);
void read_map_clock_and_resources(const QJsonObject& root, MapDefinition& out_map);

[[nodiscard]] auto read_map_units(const QJsonObject& root,
                                  MapDefinition& out_map,
                                  QString* out_error) -> bool;
void read_map_scenery(const QJsonObject& root, MapDefinition& out_map);
void read_map_fog(const QJsonObject& root, MapDefinition& out_map);
[[nodiscard]] auto read_map_wildlife(const QJsonObject& root,
                                     MapDefinition& out_map) -> bool;
void finish_map_wildlife(MapDefinition& out_map, bool wildlife_authored);

void read_map_terrain(const QJsonObject& root, MapDefinition& out_map);

} // namespace Game::Map::loader_detail
