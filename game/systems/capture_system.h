#pragma once

#include "../core/entity.h"
#include "../core/system.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

class CaptureSystem : public Engine::Core::System {
public:
  explicit CaptureSystem(Engine::Core::World &world);

  void update(Engine::Core::World *world, float deltaTime) override;

private:
  Engine::Core::World &m_world;

  void processBarrackCapture(float deltaTime);
  int countNearbyTroops(float barrackX, float barrackZ, int ownerId,
                        float radius);
  void transferBarrackOwnership(Engine::Core::Entity *barrack, int newOwnerId);
};

} // namespace Game::Systems
