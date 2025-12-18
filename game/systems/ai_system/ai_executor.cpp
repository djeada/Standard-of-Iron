#include "ai_executor.h"
#include "systems/ai_system/ai_behavior.h"
#include "systems/ai_system/ai_behavior_registry.h"
#include "systems/ai_system/ai_types.h"
#include <vector>

namespace Game::Systems::AI {

void AIExecutor::run(const AISnapshot &snapshot, AIContext &context,
                     float delta_time, AIBehaviorRegistry &registry,
                     std::vector<AICommand> &outCommands) {

  bool exclusive_behavior_executed = false;

  registry.forEach([&](AIBehavior &behavior) {
    if (exclusive_behavior_executed && !behavior.can_run_concurrently()) {
      return;
    }

    bool const should_exec = behavior.should_execute(snapshot, context);

    if (should_exec) {
      size_t commands_before = outCommands.size();
      behavior.execute(snapshot, context, delta_time, outCommands);
      size_t commands_after = outCommands.size();

      context.debug_info.total_commands_issued +=
          static_cast<int>(commands_after - commands_before);

      if (!behavior.can_run_concurrently()) {
        exclusive_behavior_executed = true;
      }
    }
  });
}

} // namespace Game::Systems::AI
