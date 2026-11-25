#pragma once

#include <unordered_map>

#include "../../core/entity.h"
#include "combat_utils.h"

namespace Game::Systems {
class CombatSystem;
}

namespace Engine::Core {
class World;
}

namespace Game::Systems::Combat {

class AutoEngagement {
public:
  void process(Engine::Core::World* world,
               const CombatQueryContext& query_context,
               float delta_time);

private:
  friend class Game::Systems::CombatSystem;

  std::unordered_map<Engine::Core::EntityID, float> m_scan_cooldowns;
};

} // namespace Game::Systems::Combat
