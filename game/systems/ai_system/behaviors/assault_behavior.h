#pragma once

#include <unordered_map>

#include "../ai_behavior.h"

namespace Game::Systems::AI {

class AssaultBehavior : public AIBehavior {
public:
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

private:
  struct AdvanceProgress {
    float best_distance = 0.0F;
    float last_gain_time = 0.0F;
    float last_observed_time = 0.0F;
    float objective_x = 0.0F;
    float objective_z = 0.0F;
  };

  [[nodiscard]] auto advance_is_stalled(Engine::Core::EntityID unit_id,
                                        float objective_x,
                                        float objective_z,
                                        float distance_to_objective,
                                        float game_time) -> bool;

  float m_assault_timer = 0.0F;
  std::unordered_map<Engine::Core::EntityID, AdvanceProgress> m_advance_progress;
};

} // namespace Game::Systems::AI
