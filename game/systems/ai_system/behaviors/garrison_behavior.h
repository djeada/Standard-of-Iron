#pragma once

#include <optional>
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

  static constexpr float k_breach_watch = 9.0F;
  static constexpr float k_engine_pressure = 4.0F;
  static constexpr float k_breach_pressure = 3.0F;
  static constexpr float k_breach_muster_depth = 7.0F;
  static constexpr float k_breach_linger_seconds = 10.0F;
  static constexpr float k_breach_switch_ratio = 1.5F;
  static constexpr std::size_t k_breach_files = 3;
  static constexpr float k_breach_file_width = 6.0F;
  static constexpr float k_breach_rank_depth = 5.0F;
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

private:
  struct Post {
    float x = 0.0F;
    float z = 0.0F;
  };

  struct Breach {
    Engine::Core::EntityID piece = 0;
    float x = 0.0F;
    float z = 0.0F;
    float pressure = 0.0F;
    int engines = 0;
    float attackers_x = 0.0F;
    float attackers_z = 0.0F;
  };

  [[nodiscard]] static auto
  find_breach(const AISnapshot& snapshot) -> std::optional<Breach>;
  void update_breach(const AISnapshot& snapshot,
                     const std::vector<const EntitySnapshot*>& infantry);
  [[nodiscard]] auto post_of(Engine::Core::EntityID unit) const -> const Post*;

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
  std::optional<Breach> m_breach;
  float m_breach_last_pressed = 0.0F;
  std::unordered_map<Engine::Core::EntityID, Post> m_breach_slots;
  Engine::Core::EntityID m_sally_target = 0;
  std::vector<Engine::Core::EntityID> m_sally_party;
  float m_sally_cooldown_until = 0.0F;
};

} // namespace Game::Systems::AI
