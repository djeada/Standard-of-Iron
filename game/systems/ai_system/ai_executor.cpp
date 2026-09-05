#include "ai_executor.h"

#include <vector>

#include "systems/ai_system/ai_behavior.h"
#include "systems/ai_system/ai_behavior_registry.h"
#include "systems/ai_system/ai_types.h"

namespace Game::Systems::AI {

void AIExecutor::run(const AISnapshot& snapshot,
                     AIContext& context,
                     float delta_time,
                     AIBehaviorRegistry& registry,
                     std::vector<AICommand>& out_commands) {

  bool exclusive_behavior_executed = false;

  registry.for_each([&](AIBehavior& behavior) {
    if (exclusive_behavior_executed && behavior.yields_to_exclusive(context)) {
      return;
    }

    bool const should_exec = behavior.should_execute(snapshot, context);

    if (should_exec) {
      behavior.execute(snapshot, context, delta_time, out_commands);

      if (!behavior.can_run_concurrently()) {
        exclusive_behavior_executed = true;
      }
    }
  });
}

} // namespace Game::Systems::AI
