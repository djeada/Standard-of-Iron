#include "ai_executor.h"

namespace Game::Systems::AI {

void AIExecutor::run(const AISnapshot &snapshot, AIContext &context,
                     float deltaTime, AIBehaviorRegistry &registry,
                     std::vector<AICommand> &outCommands) {

  bool exclusiveBehaviorExecuted = false;

  registry.forEach([&](AIBehavior &behavior) {
    if (exclusiveBehaviorExecuted && !behavior.canRunConcurrently()) {
      return;
    }

    bool shouldExec = behavior.shouldExecute(snapshot, context);

    if (shouldExec) {
      behavior.execute(snapshot, context, deltaTime, outCommands);

      if (!behavior.canRunConcurrently()) {
        exclusiveBehaviorExecuted = true;
      }
    }
  });
}

} // namespace Game::Systems::AI
