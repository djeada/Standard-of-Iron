#pragma once

#include "../ai_behavior.h"

namespace Game::Systems::AI {

class HarassBehavior : public AIBehavior {
public:
  void execute(const AISnapshot& snapshot,
               AIContext& context,
               float delta_time,
               std::vector<AICommand>& out_commands) override;

  [[nodiscard]] auto should_execute(const AISnapshot& snapshot,
                                    const AIContext& context) const -> bool override;

  [[nodiscard]] auto get_priority() const -> BehaviorPriority override {
    return BehaviorPriority::Low;
  }

  [[nodiscard]] auto can_run_concurrently() const -> bool override { return true; }

private:
  float m_harass_timer = 0.0F;
  Engine::Core::EntityID m_last_target = 0;
};

} // namespace Game::Systems::AI
