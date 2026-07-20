#pragma once

#include "../core/system.h"
#include "combat_system/auto_engagement.h"
#include "combat_system/combat_utils.h"
#include "target_commitment_system.h"

namespace Game::Systems {

class CombatSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;

private:
  Combat::AutoEngagement m_auto_engagement;
  Combat::CombatQueryContext m_query_context;
  TargetCommitmentSystem m_target_commitment;
};

} // namespace Game::Systems
