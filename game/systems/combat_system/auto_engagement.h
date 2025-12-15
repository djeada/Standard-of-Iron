#pragma once

#include "../../core/entity.h"
#include <unordered_map>

namespace Engine::Core {
class World;
}

namespace Game::Systems::Combat {

class AutoEngagement {
public:
  void process(Engine::Core::World *world, float delta_time);

private:
  std::unordered_map<Engine::Core::EntityID, float> m_engagement_cooldowns;
};

} // namespace Game::Systems::Combat
