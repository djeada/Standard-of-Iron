#pragma once

#include "../ai_behavior.h"

namespace Game::Systems::AI {

// Positions AI commanders near the army centre and triggers their rally
// ability. Runs concurrently alongside other behaviours so that regular
// troops are still managed while commanders are repositioned.
class CommanderBehavior : public AIBehavior {
public:
  void execute(const AISnapshot &snapshot, AIContext &context, float delta_time,
               std::vector<AICommand> &out_commands) override;

  [[nodiscard]] auto
  should_execute(const AISnapshot &snapshot,
                 const AIContext &context) const -> bool override;

  [[nodiscard]] auto get_priority() const -> BehaviorPriority override {
    return BehaviorPriority::High;
  }

  [[nodiscard]] auto can_run_concurrently() const -> bool override {
    return true;
  }

private:
  float m_update_timer = 0.0F;
  float m_rally_timer = 0.0F;

  static constexpr float k_update_interval = 2.0F;
  static constexpr float k_rally_interval = 5.0F;
  static constexpr float k_protected_offset = 4.0F;
  static constexpr float k_snap_threshold_sq = 9.0F; // 3 units radius
};

} // namespace Game::Systems::AI
