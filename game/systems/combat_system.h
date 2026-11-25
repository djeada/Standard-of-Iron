#pragma once

#include "../core/system.h"
#include "combat_system/auto_engagement.h"
#include "combat_system/combat_utils.h"
#include "target_commitment_system.h"

class QJsonObject;

namespace Game::Systems {

class CombatSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;
  [[nodiscard]] auto capture_checkpoint() const -> QJsonObject;
  [[nodiscard]] auto restore_checkpoint(const QJsonObject& state) -> bool;

private:
  Combat::AutoEngagement m_auto_engagement;
  Combat::CombatQueryContext m_query_context;
  TargetCommitmentSystem m_target_commitment;
};

} // namespace Game::Systems
