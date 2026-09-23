#pragma once

#include "../ai_behavior.h"

namespace Game::Systems::AI {

class AllyAidBehavior : public AIBehavior {
public:
  void execute(const AISnapshot& snapshot,
               AIContext& context,
               float delta_time,
               std::vector<AICommand>& out_commands) override;

  [[nodiscard]] auto should_execute(const AISnapshot& snapshot,
                                    const AIContext& context) const -> bool override;

  [[nodiscard]] auto get_priority() const -> BehaviorPriority override {
    return BehaviorPriority::Normal;
  }

  [[nodiscard]] auto can_run_concurrently() const -> bool override { return true; }

private:
  float m_timer = 0.0F;
};

inline constexpr float k_ally_aid_reach = 170.0F;

} // namespace Game::Systems::AI
