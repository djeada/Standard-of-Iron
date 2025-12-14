#pragma once

#include "ai_types.h"
#include <unordered_map>
#include <vector>

namespace Game::Systems::AI {

class AICommandFilter {
public:
  explicit AICommandFilter(float cooldownPeriod = 5.0F)
      : m_cooldownPeriod(cooldownPeriod) {}

  auto filter(const std::vector<AICommand> &commands,
              float currentTime) -> std::vector<AICommand>;

  void update(float currentTime);

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

    [[nodiscard]] auto isSimilarTo(const AICommandType &cmdType,
                                   Engine::Core::EntityID unit,
                                   Engine::Core::EntityID target, float x,
                                   float y, float z, float currentTime,
                                   float cooldown) const -> bool;
  };

  std::vector<CommandHistory> m_history;
  float m_cooldownPeriod;

  [[nodiscard]] auto isDuplicate(Engine::Core::EntityID unit_id,
                                 AICommandType type,
                                 Engine::Core::EntityID target_id, float move_x,
                                 float move_y, float move_z,
                                 float currentTime) const -> bool;

  void recordCommand(const AICommand &cmd, float currentTime);

  void cleanupHistory(float currentTime);
};

} // namespace Game::Systems::AI
