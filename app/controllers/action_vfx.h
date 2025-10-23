#pragma once

#include <QVector3D>

namespace Engine {
namespace Core {
class World;
using EntityID = unsigned int;
} // namespace Core
} // namespace Engine

namespace App::Controllers {

class ActionVFX {
public:
  static void spawnAttackArrow(Engine::Core::World *world,
                               Engine::Core::EntityID targetId);
};

} // namespace App::Controllers
