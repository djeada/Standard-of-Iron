#pragma once

#include "ai_types.h"
#include <memory>
#include <vector>

namespace Game::Systems::AI {

class AIBehavior {
public:
  virtual ~AIBehavior() = default;

  virtual void execute(const AISnapshot &snapshot, AIContext &context,
                       float delta_time,
                       std::vector<AICommand> &outCommands) = 0;

  [[nodiscard]] virtual auto
  should_execute(const AISnapshot &snapshot,
                 const AIContext &context) const -> bool = 0;

  [[nodiscard]] virtual auto getPriority() const -> BehaviorPriority = 0;

  [[nodiscard]] virtual auto canRunConcurrently() const -> bool {
    return false;
  }
};

} // namespace Game::Systems::AI
