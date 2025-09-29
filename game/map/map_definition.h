#pragma once

#include <QString>
#include <QVector3D>
#include <vector>

namespace Game::Map {

struct GridDefinition {
    int width = 50;      // number of cells in X
    int height = 50;     // number of cells in Z
    float tileSize = 1.0f;
};

struct CameraDefinition {
    QVector3D center{0.0f, 0.0f, 0.0f};
    float distance = 15.0f; // RTS orbit distance
    float tiltDeg = 45.0f;  // RTS tilt angle
    float fovY = 45.0f;     // degrees
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
};

struct UnitSpawn {
    QString type; // e.g., "archer"
    float x = 0.0f; // world X (or grid x * tileSize)
    float z = 0.0f; // world Z
    int playerId = 0;
};

struct MapDefinition {
    QString name;
    GridDefinition grid;
    CameraDefinition camera;
    std::vector<UnitSpawn> spawns;
};

} // namespace Game::Map
