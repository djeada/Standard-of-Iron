#pragma once

#include "ai_types.h"
#include <unordered_map>
#include <vector>

namespace Game::Systems::AI {

class AICommandFilter {
public:
  explicit AICommandFilter(float cooldown_period = 5.0F)
      : m_cooldown_period(cooldown_period) {}

  auto filter(const std::vector<AICommand> &commands,
              float current_time) -> std::vector<AICommand>;

  void update(float current_time);

  void reset();

private:
  struct CommandHistory {
    Engine::Core::EntityID unit_id;
    AICommandType type;

    Engine::Core::EntityID target_id;

    float move_target_x;
    float move_target_y;
    float move_target_z;

    float issued_time;

    [[nodiscard]] auto is_similar_to(const AICommandType &cmd_type,
                                     Engine::Core::EntityID unit,
                                     Engine::Core::EntityID target, float x,
                                     float y, float z, float current_time,
                                     float cooldown) const -> bool;
  };

  std::vector<CommandHistory> m_history;
  float m_cooldown_period;

  [[nodiscard]] auto is_duplicate(Engine::Core::EntityID unit_id,
                                  AICommandType type,
                                  Engine::Core::EntityID target_id,
                                  float move_x, float move_y, float move_z,
                                  float current_time) const -> bool;

  void record_command(const AICommand &cmd, float current_time);

  void cleanup_history(float current_time);
};

} // namespace Game::Systems::AI
