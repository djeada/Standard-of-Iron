#pragma once

#include <QVector3D>

namespace Engine::Core {
class World;
using EntityID = unsigned int;
} 

namespace App::Controllers {

class ActionVFX {
public:
  static void spawnAttackArrow(Engine::Core::World *world,
                               Engine::Core::EntityID target_id);
};

} 
