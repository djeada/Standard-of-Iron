#pragma once

#include "../core/system.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

class DefenseTowerSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World *world, float delta_time) override;
};

} 
