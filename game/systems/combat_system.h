#pragma once

#include "../core/system.h"
#include "combat_system/auto_engagement.h"

namespace Game::Systems {

class CombatSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World *world, float delta_time) override;

private:
  Combat::AutoEngagement m_auto_engagement;
};

} // namespace Game::Systems
