#pragma once

#include "../ai_behavior.h"

namespace Game::Systems::AI {

// Carthage turns gold into an army (NATION_ASYMMETRY.md section 9): hire
// mercenary bands at the Mercenary Camp and elephants at the Elephant Stables,
// in bursts, while a treasury model keeps the upkeep payable: no band is hired
// whose upkeep would leave the treasury empty within two minutes.
class MercenaryBehavior : public AIBehavior {
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
  float m_timer = 0.0F;
  int m_hires = 0;
};

} // namespace Game::Systems::AI
