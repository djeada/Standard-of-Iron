#pragma once

#include "terrain.h"
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

  float nearPlane = 1.0f;
  float farPlane = 200.0f;

  float yawDeg = 225.0f;
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
  std::vector<TerrainFeature> terrain;
  BiomeSettings biome;
  CoordSystem coordSystem = CoordSystem::Grid;
  int maxTroopsPerPlayer = 50;
  QString victoryCondition = "barracks";
};

} // namespace Game::Map
