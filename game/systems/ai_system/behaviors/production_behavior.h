#pragma once

#include "../ai_behavior.h"

namespace Game::Systems::AI {

class ProductionBehavior : public AIBehavior {
public:
  void execute(const AISnapshot &snapshot, AIContext &context, float deltaTime,
               std::vector<AICommand> &outCommands) override;

  [[nodiscard]] auto
  should_execute(const AISnapshot &snapshot,
                 const AIContext &context) const -> bool override;

  [[nodiscard]] auto getPriority() const -> BehaviorPriority override {
    return BehaviorPriority::High;
  }

  [[nodiscard]] auto canRunConcurrently() const -> bool override {
    return true;
  }

private:
  float m_productionTimer = 0.0F;
  int m_productionCounter = 0;
};

} // namespace Game::Systems::AI
