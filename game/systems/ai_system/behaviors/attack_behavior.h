#pragma once

#include "../ai_behavior.h"

namespace Game::Systems::AI {

class AttackBehavior : public AIBehavior {
public:
  void execute(const AISnapshot &snapshot, AIContext &context, float deltaTime,
               std::vector<AICommand> &outCommands) override;

  bool shouldExecute(const AISnapshot &snapshot,
                     const AIContext &context) const override;

  BehaviorPriority getPriority() const override {
    return BehaviorPriority::Normal;
  }

  bool canRunConcurrently() const override { return false; }

private:
  float m_attackTimer = 0.0f;
  Engine::Core::EntityID m_lastTarget = 0;
  float m_targetLockDuration = 0.0f;
};

} // namespace Game::Systems::AI
