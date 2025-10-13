#pragma once

#include "ai_types.h"

namespace Game::Systems::AI {

class AIReasoner {
public:
  AIReasoner() = default;
  ~AIReasoner() = default;

  AIReasoner(const AIReasoner &) = delete;
  AIReasoner &operator=(const AIReasoner &) = delete;

  void updateContext(const AISnapshot &snapshot, AIContext &context);

  void updateStateMachine(AIContext &context, float deltaTime);
};

} // namespace Game::Systems::AI
