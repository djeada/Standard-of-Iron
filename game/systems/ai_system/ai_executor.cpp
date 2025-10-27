#include "ai_executor.h"
#include "systems/ai_system/ai_behavior.h"
#include "systems/ai_system/ai_behavior_registry.h"
#include "systems/ai_system/ai_types.h"
#include <vector>

namespace Game::Systems::AI {

void AIExecutor::run(const AISnapshot &snapshot, AIContext &context,
                     float deltaTime, AIBehaviorRegistry &registry,
                     std::vector<AICommand> &outCommands) {

  bool exclusive_behavior_executed = false;

  registry.forEach([&](AIBehavior &behavior) {
    if (exclusive_behavior_executed && !behavior.canRunConcurrently()) {
      return;
    }

    bool const should_exec = behavior.should_execute(snapshot, context);

    if (should_exec) {
      behavior.execute(snapshot, context, deltaTime, outCommands);

      if (!behavior.canRunConcurrently()) {
        exclusive_behavior_executed = true;
      }
    }
  });
}

} // namespace Game::Systems::AI
