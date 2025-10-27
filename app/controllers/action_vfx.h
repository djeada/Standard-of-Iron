#pragma once

#include <QVector3D>

namespace Engine::Core {
class World;
using EntityID = unsigned int;
} // namespace Engine::Core

namespace App::Controllers {

class ActionVFX {
public:
  static void spawnAttackArrow(Engine::Core::World *world,
                               Engine::Core::EntityID target_id);
};

} // namespace App::Controllers
