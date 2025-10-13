#pragma once

#include "map_definition.h"
#include <QString>
#include <memory>

namespace Engine {
namespace Core {
class World;
using EntityID = unsigned int;
} // namespace Core
} // namespace Engine
namespace Render {
namespace GL {
class Renderer;
class Camera;
} // namespace GL
} // namespace Render

namespace Game {
namespace Map {

struct LevelLoadResult {
  bool ok = false;
  QString mapName;
  QString errorMessage;
  Engine::Core::EntityID playerUnitId = 0;
  float camFov = 45.0f;
  float camNear = 0.1f;
  float camFar = 1000.0f;
  int gridWidth = 50;
  int gridHeight = 50;
  float tileSize = 1.0f;
  int maxTroopsPerPlayer = 50;
  VictoryConfig victoryConfig;
};

class LevelLoader {
public:
  static LevelLoadResult loadFromAssets(const QString &mapPath,
                                        Engine::Core::World &world,
                                        Render::GL::Renderer &renderer,
                                        Render::GL::Camera &camera);
};

} // namespace Map
} // namespace Game
