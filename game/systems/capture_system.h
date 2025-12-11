#pragma once

#include "../core/entity.h"
#include "../core/system.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

class CaptureSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World *world, float delta_time) override;

private:
  static void processBarrackCapture(Engine::Core::World *world,
                                    float delta_time);
  static auto countNearbyTroops(Engine::Core::World *world, float barrack_x,
                                float barrack_z, int owner_id,
                                float radius) -> int;
  static void transferBarrackOwnership(Engine::Core::World *world,
                                       Engine::Core::Entity *barrack,
                                       int new_owner_id);
};

} // namespace Game::Systems
