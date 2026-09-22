#pragma once

#include <QVector3D>

namespace Engine::Core {
class World;
}

namespace Render::GL {
class Camera;
}

namespace Game::Session {
class SelectionService;
}

namespace Game::Systems {

class CameraFollowSystem {
public:
  static void update(Engine::Core::World& world,
                     Game::Session::SelectionService& selection,
                     Render::GL::Camera& camera);

  static void snap_to_selection(Engine::Core::World& world,
                                Game::Session::SelectionService& selection,
                                Render::GL::Camera& camera);
};

} // namespace Game::Systems
