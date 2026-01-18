#pragma once

#include "../core/event_manager.h"
#include "../core/system.h"
#include "ai_system/ai_behavior_registry.h"
#include "ai_system/ai_command_applier.h"
#include "ai_system/ai_command_filter.h"
#include "ai_system/ai_executor.h"
#include "ai_system/ai_reasoner.h"
#include "ai_system/ai_snapshot_builder.h"
#include "ai_system/ai_strategy.h"
#include "ai_system/ai_types.h"
#include "ai_system/ai_worker.h"

#include <memory>
#include <queue>

namespace Engine::Core {
class World;
}

namespace Game::Systems {

class AISystem : public Engine::Core::System {
public:
  AISystem();
  ~AISystem() override;

  void update(Engine::Core::World *world, float delta_time) override;

  void reinitialize();

  void set_update_interval(float interval) { m_update_interval = interval; }
  float get_update_interval() const { return m_update_interval; }

  void set_ai_strategy(int player_id, AI::AIStrategy strategy,
                       float aggression = 0.5F, float defense = 0.5F,
                       float harassment = 0.5F);

private:
  struct AIInstance {
    AI::AIContext context;
    std::unique_ptr<AI::AIWorker> worker;
    float update_timer = 0.0F;
  };

  std::vector<AIInstance> m_ai_instances;

  AI::AIBehaviorRegistry m_behavior_registry;
  AI::AISnapshotBuilder m_snapshot_builder;
  AI::AIReasoner m_reasoner;
  AI::AIExecutor m_executor;
  AI::AICommandApplier m_applier;
  AI::AICommandFilter m_command_filter;

  float m_total_game_time = 0.0F;
  float m_update_interval = 0.3F;

  Engine::Core::ScopedEventSubscription<Engine::Core::BuildingAttackedEvent>
      m_building_attacked_subscription;

  void initialize_ai_players();

  void process_results(Engine::Core::World &world);

  void on_building_attacked(const Engine::Core::BuildingAttackedEvent &event);
};

} // namespace Game::Systems
