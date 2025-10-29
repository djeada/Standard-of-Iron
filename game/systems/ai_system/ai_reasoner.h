#pragma once

#include "ai_types.h"

namespace Game::Systems::AI {

class AIReasoner {
public:
  AIReasoner() = default;
  ~AIReasoner() = default;

  AIReasoner(const AIReasoner &) = delete;
  auto operator=(const AIReasoner &) -> AIReasoner & = delete;

  static void updateContext(const AISnapshot &snapshot, AIContext &ctx);

  static void updateStateMachine(AIContext &ctx, float deltaTime);
};

} // namespace Game::Systems::AI
