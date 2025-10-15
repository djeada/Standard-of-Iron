#pragma once

#include "../core/entity.h"
#include "../core/system.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

class CaptureSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World *world, float deltaTime) override;

private:
  void processBarrackCapture(Engine::Core::World *world, float deltaTime);
  int countNearbyTroops(Engine::Core::World *world, float barrackX,
                        float barrackZ, int ownerId, float radius);
  void transferBarrackOwnership(Engine::Core::World *world,
                                Engine::Core::Entity *barrack, int newOwnerId);
};

} // namespace Game::Systems
