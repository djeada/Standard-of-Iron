#pragma once

#include "../core/event_manager.h"
#include "../core/system.h"
#include "ai_system/ai_behavior_registry.h"
#include "ai_system/ai_command_applier.h"
#include "ai_system/ai_command_filter.h"
#include "ai_system/ai_executor.h"
#include "ai_system/ai_reasoner.h"
#include "ai_system/ai_snapshot_builder.h"
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

  void update(Engine::Core::World *world, float deltaTime) override;

  void reinitialize();

private:
  struct AIInstance {
    AI::AIContext context;
    std::unique_ptr<AI::AIWorker> worker;
    float updateTimer = 0.0f;
  };

  std::vector<AIInstance> m_aiInstances;

  AI::AIBehaviorRegistry m_behaviorRegistry;
  AI::AISnapshotBuilder m_snapshotBuilder;
  AI::AIReasoner m_reasoner;
  AI::AIExecutor m_executor;
  AI::AICommandApplier m_applier;
  AI::AICommandFilter m_commandFilter;

  float m_totalGameTime = 0.0f;

  Engine::Core::ScopedEventSubscription<Engine::Core::BuildingAttackedEvent>
      m_buildingAttackedSubscription;

  void initializeAIPlayers();

  void processResults(Engine::Core::World &world);

  void onBuildingAttacked(const Engine::Core::BuildingAttackedEvent &event);
};

} // namespace Game::Systems
