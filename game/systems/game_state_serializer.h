#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVector3D>

namespace Engine {
namespace Core {
class World;
using EntityID = unsigned int;
}
} // namespace Engine

namespace Render {
namespace GL {
class Camera;
}
} // namespace Render

namespace Game {
namespace Systems {

struct LevelSnapshot {
  QString mapPath;
  QString mapName;
  Engine::Core::EntityID playerUnitId = 0;
  float camFov = 45.0f;
  float camNear = 0.1f;
  float camFar = 1000.0f;
  int maxTroopsPerPlayer = 50;
};

struct RuntimeSnapshot {
  bool paused = false;
  float timeScale = 1.0f;
  int localOwnerId = 1;
  QString victoryState = "";
  int cursorMode = 0;
  int selectedPlayerId = 1;
  bool followSelection = false;
};

class GameStateSerializer {
public:
  static QJsonObject buildMetadata(const Engine::Core::World &world,
                                   const Render::GL::Camera *camera,
                                   const LevelSnapshot &level,
                                   const RuntimeSnapshot &runtime);

  static void restoreCameraFromMetadata(const QJsonObject &metadata,
                                       Render::GL::Camera *camera,
                                       int viewportWidth, int viewportHeight);

  static void restoreRuntimeFromMetadata(const QJsonObject &metadata,
                                        RuntimeSnapshot &runtime);

  static void restoreLevelFromMetadata(const QJsonObject &metadata,
                                      LevelSnapshot &level);
};

} // namespace Systems
} // namespace Game
