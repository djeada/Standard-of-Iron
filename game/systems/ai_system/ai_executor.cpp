#include "ai_executor.h"
#include <cstring>
#include <iostream>
#include <typeinfo>
#include <unordered_map>

namespace Game::Systems::AI {

void AIExecutor::run(const AISnapshot &snapshot, AIContext &context,
                     float deltaTime, AIBehaviorRegistry &registry,
                     std::vector<AICommand> &outCommands) {

  bool exclusiveBehaviorExecuted = false;

  static std::unordered_map<int, int> debugCounters;
  bool shouldDebug = (++debugCounters[context.playerId] % 3 == 0);

  if (shouldDebug) {
    std::cout << "  [AI " << context.playerId << " Behaviors]" << std::endl;
  }

  registry.forEach([&](AIBehavior &behavior) {
    if (exclusiveBehaviorExecuted && !behavior.canRunConcurrently()) {
      if (shouldDebug) {
        std::cout << "    SKIPPED (exclusive already ran)" << std::endl;
      }
      return;
    }

    bool shouldExec = behavior.shouldExecute(snapshot, context);

    if (shouldDebug) {
      const char *behaviorName = typeid(behavior).name();

      const char *className = behaviorName;
      if (const char *lastColon = strrchr(behaviorName, 'E')) {
        if (lastColon > behaviorName) {
          className = lastColon - 20;
        }
      }

      std::cout << "    " << behaviorName
                << " -> shouldExecute: " << (shouldExec ? "YES" : "NO");
      if (exclusiveBehaviorExecuted && !behavior.canRunConcurrently()) {
        std::cout << " (BLOCKED)";
      }
      std::cout << std::endl;
    }

    if (shouldExec) {
      size_t cmdsBefore = outCommands.size();

      behavior.execute(snapshot, context, deltaTime, outCommands);

      if (shouldDebug) {
        std::cout << "      ✓ EXECUTED! Generated "
                  << (outCommands.size() - cmdsBefore) << " commands"
                  << std::endl;
      }

      if (!behavior.canRunConcurrently()) {
        exclusiveBehaviorExecuted = true;
      }
    }
  });
}

} // namespace Game::Systems::AI
