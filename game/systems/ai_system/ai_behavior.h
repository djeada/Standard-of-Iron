#pragma once

#include "ai_types.h"
#include <memory>
#include <vector>

namespace Game::Systems::AI {

class AIBehavior {
public:
  virtual ~AIBehavior() = default;

  virtual void execute(const AISnapshot &snapshot, AIContext &context,
                       float deltaTime,
                       std::vector<AICommand> &outCommands) = 0;

  virtual bool shouldExecute(const AISnapshot &snapshot,
                             const AIContext &context) const = 0;

  virtual BehaviorPriority getPriority() const = 0;

  virtual bool canRunConcurrently() const { return false; }
};

} // namespace Game::Systems::AI
