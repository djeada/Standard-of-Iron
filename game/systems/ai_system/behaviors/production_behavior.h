#pragma once

#include "../ai_behavior.h"

namespace Game::Systems::AI {

class ProductionBehavior : public AIBehavior {
public:
  void execute(const AISnapshot &snapshot, AIContext &context, float deltaTime,
               std::vector<AICommand> &outCommands) override;

  bool shouldExecute(const AISnapshot &snapshot,
                     const AIContext &context) const override;

  BehaviorPriority getPriority() const override {
    return BehaviorPriority::High;
  }

  bool canRunConcurrently() const override { return true; }

private:
  float m_productionTimer = 0.0f;
  int m_productionCounter = 0;
};

} // namespace Game::Systems::AI
