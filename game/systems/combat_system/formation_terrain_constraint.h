#pragma once

#include <QVector3D>

#include "../navigation/pathfinding.h"

namespace Game::Systems::Combat {

auto constrain_step_to_ground(const Pathfinding& pathfinder,
                              QVector3D origin,
                              float& step_x,
                              float& step_z,
                              Pathfinding::Passability passability) -> bool;

auto terrain_walkable_at(const Pathfinding& pathfinder, float x, float z) -> bool;

void pull_onto_terrain(const Pathfinding& pathfinder,
                       float anchor_x,
                       float anchor_z,
                       QVector3D& destination);

auto constrain_step_to_terrain(const Pathfinding& pathfinder,
                               float from_x,
                               float from_z,
                               float& step_x,
                               float& step_z) -> bool;

} // namespace Game::Systems::Combat
