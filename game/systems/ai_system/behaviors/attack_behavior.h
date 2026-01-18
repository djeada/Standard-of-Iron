#pragma once

#include "../ai_behavior.h"

namespace Game::Systems::AI {

class AttackBehavior : public AIBehavior {
public:
  void execute(const AISnapshot &snapshot, AIContext &context, float delta_time,
               std::vector<AICommand> &out_commands) override;

  [[nodiscard]] auto
  should_execute(const AISnapshot &snapshot,
                 const AIContext &context) const -> bool override;

  [[nodiscard]] auto get_priority() const -> BehaviorPriority override {
    return BehaviorPriority::Normal;
  }

  [[nodiscard]] auto can_run_concurrently() const -> bool override {
    return false;
  }

private:
  float m_attack_timer = 0.0F;
  Engine::Core::EntityID m_last_target = 0;
  float m_target_lock_duration = 0.0F;
  int m_scout_direction = 0;
  float m_last_scout_time = 0.0F;
};

} // namespace Game::Systems::AI
