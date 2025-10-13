#pragma once

#include "ai_types.h"
#include <unordered_map>
#include <vector>

namespace Game::Systems::AI {

class AICommandFilter {
public:
  explicit AICommandFilter(float cooldownPeriod = 5.0f)
      : m_cooldownPeriod(cooldownPeriod) {}

  std::vector<AICommand> filter(const std::vector<AICommand> &commands,
                                float currentTime);

  void update(float currentTime);

  void reset();

private:
  struct CommandHistory {
    Engine::Core::EntityID unitId;
    AICommandType type;

    Engine::Core::EntityID targetId;

    float moveTargetX;
    float moveTargetY;
    float moveTargetZ;

    float issuedTime;

    bool isSimilarTo(const AICommandType &cmdType, Engine::Core::EntityID unit,
                     Engine::Core::EntityID target, float x, float y, float z,
                     float currentTime, float cooldown) const;
  };

  std::vector<CommandHistory> m_history;
  float m_cooldownPeriod;

  bool isDuplicate(Engine::Core::EntityID unitId, AICommandType type,
                   Engine::Core::EntityID targetId, float moveX, float moveY,
                   float moveZ, float currentTime) const;

  void recordCommand(const AICommand &cmd, float currentTime);

  void cleanupHistory(float currentTime);
};

} // namespace Game::Systems::AI
