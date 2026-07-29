#pragma once

#include <QVector3D>

#include <cstdint>

namespace Engine::Core {
class World;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace App::Controllers {

class ActionVFX {
public:
  static void spawn_attack_arrow(Engine::Core::World* world,
                                 Engine::Core::EntityID target_id);
};

} // namespace App::Controllers
