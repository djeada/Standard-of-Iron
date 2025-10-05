#pragma once

#include <QString>
#include <QVector3D>
#include <vector>

namespace Game::Map {

struct GridDefinition {
  int width = 50;
  int height = 50;
  float tileSize = 1.0f;
};

struct CameraDefinition {
  QVector3D center{0.0f, 0.0f, 0.0f};
  float distance = 15.0f;
  float tiltDeg = 45.0f;
  float fovY = 45.0f;
  float nearPlane = 0.1f;
  float farPlane = 1000.0f;
};

struct UnitSpawn {
  QString type;
  float x = 0.0f;
  float z = 0.0f;
  int playerId = 0;
};

enum class CoordSystem { Grid, World };

struct MapDefinition {
  QString name;
  GridDefinition grid;
  CameraDefinition camera;
  std::vector<UnitSpawn> spawns;
  CoordSystem coordSystem = CoordSystem::Grid;
  int maxTroopsPerPlayer = 50;
};

} // namespace Game::Map
