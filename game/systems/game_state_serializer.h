#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVector3D>

namespace Engine::Core {
class World;
using EntityID = unsigned int;
} // namespace Engine::Core

namespace Render::GL {
class Camera;
}

namespace Game::Systems {

struct LevelSnapshot {
  QString map_path;
  QString map_name;
  Engine::Core::EntityID playerUnitId = 0;
  float camFov = 45.0F;
  float camNear = 0.1F;
  float camFar = 1000.0F;
  int max_troops_per_player = 50;
};

struct RuntimeSnapshot {
  bool paused = false;
  float timeScale = 1.0F;
  int localOwnerId = 1;
  QString victoryState = "";
  int cursorMode = 0;
  int selectedPlayerId = 1;
  bool followSelection = false;
};

class GameStateSerializer {
public:
  static auto buildMetadata(const Engine::Core::World &world,
                            const Render::GL::Camera *camera,
                            const LevelSnapshot &level,
                            const RuntimeSnapshot &runtime) -> QJsonObject;

  static void restoreCameraFromMetadata(const QJsonObject &metadata,
                                        Render::GL::Camera *camera,
                                        int viewportWidth, int viewportHeight);

  static void restoreRuntimeFromMetadata(const QJsonObject &metadata,
                                         RuntimeSnapshot &runtime);

  static void restoreLevelFromMetadata(const QJsonObject &metadata,
                                       LevelSnapshot &level);
};

} // namespace Game::Systems
