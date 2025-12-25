#pragma once

#include "../ai_behavior.h"

namespace Game::Systems::AI {

class ExpandBehavior : public AIBehavior {
public:
  void execute(const AISnapshot &snapshot, AIContext &context, float delta_time,
               std::vector<AICommand> &outCommands) override;

  [[nodiscard]] auto
  should_execute(const AISnapshot &snapshot,
                 const AIContext &context) const -> bool override;

  [[nodiscard]] auto get_priority() const -> BehaviorPriority override {
    return BehaviorPriority::High;
  }

  [[nodiscard]] auto can_run_concurrently() const -> bool override {
    return false;
  }

private:
  float m_expand_timer = 0.0F;
};

} 
