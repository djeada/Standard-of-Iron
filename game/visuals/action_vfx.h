#pragma once

#include <QVector3D>

namespace Engine {
namespace Core {
class World;
class Entity;
}
}

namespace Game {
namespace Visuals {

class ActionVFX {
public:
  static void spawnAttackArrow(Engine::Core::World &world,
                              Engine::Core::Entity *targetEntity);
};

} // namespace Visuals
} // namespace Game
