#pragma once

#include <vector>

#include "../ai_behavior.h"

namespace Game::Systems::AI {

class LocalEngagementBehavior : public AIBehavior {
public:
  static constexpr const char* k_task_name = "local-engagement";
  static constexpr float k_update_interval = 1.0F;
  static constexpr float k_threat_cluster_radius = 10.0F;
  static constexpr float k_min_force_ratio = 0.8F;

  void execute(const AISnapshot& snapshot,
               AIContext& context,
               float delta_time,
               std::vector<AICommand>& out_commands) override;

  [[nodiscard]] auto should_execute(const AISnapshot& snapshot,
                                    const AIContext& context) const -> bool override;

  [[nodiscard]] auto get_priority() const -> BehaviorPriority override {
    return BehaviorPriority::High;
  }

  [[nodiscard]] auto can_run_concurrently() const -> bool override { return true; }

  [[nodiscard]] static auto
  fresh_responder_slots(int already_fighting, const AIStrategyConfig& strategy) -> int;

private:
  float m_timer = 0.0F;
};

} // namespace Game::Systems::AI
