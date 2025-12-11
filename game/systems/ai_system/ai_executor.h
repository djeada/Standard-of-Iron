#pragma once

#include "ai_behavior_registry.h"
#include "ai_types.h"
#include <vector>

namespace Game::Systems::AI {

class AIExecutor {
public:
  AIExecutor() = default;
  ~AIExecutor() = default;

  AIExecutor(const AIExecutor &) = delete;
  auto operator=(const AIExecutor &) -> AIExecutor & = delete;

  static void run(const AISnapshot &snapshot, AIContext &context,
                  float delta_time, AIBehaviorRegistry &registry,
                  std::vector<AICommand> &outCommands);
};

} // namespace Game::Systems::AI
