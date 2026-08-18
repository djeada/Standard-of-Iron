#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVector3D>

#include <cstdint>
#include <vector>

#include "../systems/match_snapshot.h"

namespace Engine::Core {
class World;
} // namespace Engine::Core

namespace Render::GL {
class Camera;
}

namespace Game::Systems {

class GameStateSerializer {
public:
  static auto build_metadata(const Engine::Core::World& world,
                             const Render::GL::Camera* camera,
                             const LevelSnapshot& level,
                             const RuntimeSnapshot& runtime) -> QJsonObject;

  static void restore_camera_from_metadata(const QJsonObject& metadata,
                                           Render::GL::Camera* camera,
                                           int viewport_width,
                                           int viewport_height);

  static void restore_runtime_from_metadata(const QJsonObject& metadata,
                                            RuntimeSnapshot& runtime);

  static void restore_level_from_metadata(const QJsonObject& metadata,
                                          LevelSnapshot& level);

  static void restore_player_nations_from_metadata(const QJsonObject& metadata);

  static void restore_visibility_from_metadata(const QJsonObject& metadata);
};

} // namespace Game::Systems
