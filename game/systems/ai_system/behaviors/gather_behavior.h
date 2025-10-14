#pragma once

#include "../ai_behavior.h"

namespace Game::Systems::AI {

class GatherBehavior : public AIBehavior {
public:
  void execute(const AISnapshot &snapshot, AIContext &context, float deltaTime,
               std::vector<AICommand> &outCommands) override;

  bool shouldExecute(const AISnapshot &snapshot,
                     const AIContext &context) const override;

  BehaviorPriority getPriority() const override {
    return BehaviorPriority::Low;
  }

  bool canRunConcurrently() const override { return false; }

private:
  float m_gatherTimer = 0.0f;
};

} // namespace Game::Systems::AI
