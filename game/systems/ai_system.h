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

#include <cstddef>
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
  [[nodiscard]] auto ai_player_count() const -> std::size_t {
    return m_ai_instances.size();
  }
  [[nodiscard]] auto ai_update_timer(std::size_t index) const -> float {
    return (index < m_ai_instances.size()) ? m_ai_instances[index].update_timer
                                           : 0.0F;
  }

  void set_ai_strategy(int player_id, AI::AIStrategy strategy,
                       float aggression = 0.5F, float defense = 0.5F,
                       float harassment = 0.5F,
                       const QString &difficulty = {});
  void set_commander_recruitment_enabled(bool enabled);

private:
  struct AIInstance {
    AI::AIContext context;
    // Heap-allocated so its address is stable across vector moves; AIWorker
    // holds a reference to this registry.
    std::unique_ptr<AI::AIBehaviorRegistry> behavior_registry;
    std::unique_ptr<AI::AIWorker> worker;
    float update_timer = 0.0F;
  };

  std::vector<AIInstance> m_ai_instances;

  AI::AISnapshotBuilder m_snapshot_builder;
  AI::AIReasoner m_reasoner;
  AI::AIExecutor m_executor;
  AI::AICommandApplier m_applier;
  AI::AICommandFilter m_command_filter;

  float m_total_game_time = 0.0F;
  float m_update_interval = 0.3F;
  bool m_allow_commander_recruitment = false;

  Engine::Core::ScopedEventSubscription<Engine::Core::BuildingAttackedEvent>
      m_building_attacked_subscription;

  void initialize_ai_players();

  static void populate_behavior_registry(AI::AIBehaviorRegistry &registry);

  void process_results(Engine::Core::World &world);

  void on_building_attacked(const Engine::Core::BuildingAttackedEvent &event);
};

} // namespace Game::Systems
