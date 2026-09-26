#pragma once

#include <vector>

#include "../ai_behavior.h"

namespace Game::Systems::AI {

class GoldVeinBehavior : public AIBehavior {
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
  Engine::Core::EntityID m_vein = 0;
  std::vector<Engine::Core::EntityID> m_party;
  float m_sent_at = 0.0F;
  Engine::Core::EntityID m_given_up = 0;
  float m_given_up_until = 0.0F;
};

} // namespace Game::Systems::AI
