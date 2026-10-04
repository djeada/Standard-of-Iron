#pragma once

#include <unordered_map>
#include <vector>

#include "../ai_behavior.h"

namespace Game::Systems::AI {

class GarrisonBehavior : public AIBehavior {
public:
  static constexpr float k_decision_seconds = 1.0F;

  static constexpr float k_sally_reach = 22.0F;

  static constexpr int k_lonely_escort = 1;
  static constexpr float k_escort_radius = 10.0F;
  static constexpr int k_sally_party = 3;

  static constexpr float k_sally_cooldown_seconds = 20.0F;
  static constexpr float k_post_slack = 2.5F;
  static constexpr const char* k_task_name = "holding-the-town";

  void execute(const AISnapshot& snapshot,
               AIContext& context,
               float delta_time,
               std::vector<AICommand>& out_commands) override;

  [[nodiscard]] auto should_execute(const AISnapshot& snapshot,
                                    const AIContext& context) const -> bool override;

  [[nodiscard]] auto get_priority() const -> BehaviorPriority override {
    return BehaviorPriority::Critical;
  }

  [[nodiscard]] auto can_run_concurrently() const -> bool override { return true; }

  [[nodiscard]] static auto is_garrison_infantry(const EntitySnapshot& unit) -> bool;

  static constexpr float k_ward_apron = 6.0F;
  static constexpr float k_gateway_depth = 1.5F;
  [[nodiscard]] static constexpr auto inner_half(float ward_half) -> float {
    return ward_half > k_ward_apron ? ward_half - k_ward_apron + k_gateway_depth
                                    : ward_half;
  }

private:
  struct Post {
    float x = 0.0F;
    float z = 0.0F;
  };

  auto counterattack(const AISnapshot& snapshot,
                     const std::vector<const EntitySnapshot*>& infantry,
                     std::vector<AICommand>& out_commands) -> bool;
  void sally(const AISnapshot& snapshot,
             const std::vector<const EntitySnapshot*>& infantry,
             std::vector<AICommand>& out_commands);
  void hold_posts(const AISnapshot& snapshot,
                  const std::vector<const EntitySnapshot*>& infantry,
                  AIContext& context,
                  std::vector<AICommand>& out_commands);

  float m_timer = 0.0F;
  std::unordered_map<Engine::Core::EntityID, Post> m_posts;
  Engine::Core::EntityID m_sally_target = 0;
  std::vector<Engine::Core::EntityID> m_sally_party;
  float m_sally_cooldown_until = 0.0F;
};

} // namespace Game::Systems::AI
