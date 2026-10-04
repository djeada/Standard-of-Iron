#pragma once

#include <unordered_map>
#include <vector>

#include "../ai_behavior.h"

namespace Game::Systems::AI {

class RampartBehavior : public AIBehavior {
public:
  static constexpr float k_decision_seconds = 3.0F;
  static constexpr float k_post_spacing = 6.0F;
  static constexpr float k_climb_patience_seconds = 45.0F;
  static constexpr float k_unreachable_post_seconds = 120.0F;
  static constexpr const char* k_task_name = "manning-the-wall";

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
  struct Posting {
    float x = 0.0F;
    float z = 0.0F;
    float since = 0.0F;
  };

  struct ShunnedPost {
    float x = 0.0F;
    float z = 0.0F;
    float until = 0.0F;
  };

  float m_timer = 0.0F;
  std::unordered_map<Engine::Core::EntityID, Posting> m_postings;
  std::vector<ShunnedPost> m_shunned;
};

} // namespace Game::Systems::AI
