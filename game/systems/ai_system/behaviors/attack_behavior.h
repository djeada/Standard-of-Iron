#pragma once

#include "../ai_behavior.h"

namespace Game::Systems::AI {

class AttackBehavior : public AIBehavior {
public:
  void execute(const AISnapshot &snapshot, AIContext &context, float delta_time,
               std::vector<AICommand> &outCommands) override;

  [[nodiscard]] auto
  should_execute(const AISnapshot &snapshot,
                 const AIContext &context) const -> bool override;

  [[nodiscard]] auto get_priority() const -> BehaviorPriority override {
    return BehaviorPriority::Normal;
  }

  [[nodiscard]] auto can_run_concurrently() const -> bool override {
    return false;
  }

private:
  float m_attackTimer = 0.0F;
  Engine::Core::EntityID m_lastTarget = 0;
  float m_targetLockDuration = 0.0F;
  int m_scoutDirection = 0;
  float m_lastScoutTime = 0.0F;
};

} 
