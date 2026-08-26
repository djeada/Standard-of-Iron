#pragma once

#include <queue>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include "../core/event_manager.h"
#include "../core/system.h"
#include "ai_system/ai_behavior_registry.h"
#include "ai_system/ai_command_filter.h"
#include "ai_system/ai_types.h"
#include "ai_system/ai_worker.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

class NationRegistry;
class OwnerRegistry;

class AISystem : public Engine::Core::System {
public:
  struct Services {
    OwnerRegistry& owners;
    NationRegistry& nations;
  };

  explicit AISystem(Services services);
  ~AISystem() override;

  void update(Engine::Core::World* world, float delta_time) override;

  void reinitialize();
  void shutdown_workers();

  void set_update_interval(float interval) { m_update_interval = interval; }
  [[nodiscard]] auto ai_player_count() const -> std::size_t {
    return m_ai_instances.size();
  }
  [[nodiscard]] auto ai_update_timer(std::size_t index) const -> float {
    return (index < m_ai_instances.size()) ? m_ai_instances[index].update_timer : 0.0F;
  }
  [[nodiscard]] auto completed_decision_count() const -> std::uint64_t {
    return m_completed_decision_count;
  }
  [[nodiscard]] auto applied_command_count() const -> std::uint64_t {
    return m_applied_command_count;
  }

  // Commands the world refused outright. A plan made of these is a computer
  // that looks busy and does nothing.
  [[nodiscard]] auto refused_command_count() const -> std::uint64_t {
    return m_refused_command_count;
  }

  void set_ai_profile(int player_id, const AI::AIPlayerProfile& profile);

  // The plan a player is currently running. A computer stuck in Defending looks
  // from outside exactly like one that is winning, so a harness that can only
  // count units cannot tell the difference; this is how it can.
  [[nodiscard]] auto plan_for(int player_id) const -> const AI::AIContext*;

private:
  struct AIInstance {
    AI::AIContext context;

    std::unique_ptr<AI::AIBehaviorRegistry> behavior_registry;
    std::unique_ptr<AI::AIWorker> worker;
    float update_timer = 0.0F;

    bool job_pending = false;
    std::uint64_t job_due_update = 0;

    std::unordered_map<Engine::Core::EntityID, float> unmerged_building_attacks;
  };

  static constexpr std::uint64_t k_decision_latency_updates = 6;
  std::uint64_t m_update_count = 0;

  std::vector<AIInstance> m_ai_instances;

  AI::AICommandFilter m_command_filter;

  float m_total_game_time = 0.0F;
  float m_update_interval = 0.3F;
  float m_next_trace_time = 0.0F;
  std::uint64_t m_completed_decision_count{0};
  std::uint64_t m_applied_command_count{0};
  std::uint64_t m_refused_command_count{0};

  Engine::Core::ScopedEventSubscription<Engine::Core::BuildingAttackedEvent>
      m_building_attacked_subscription;

  Services m_services;

  void initialize_ai_players();
  void trace_progress(const Engine::Core::World& world);

  static void populate_behavior_registry(AI::AIBehaviorRegistry& registry);

  void process_results(Engine::Core::World& world);

  static void merge_building_attacks(const AIInstance& ai, AI::AIContext& context);

  void on_building_attacked(const Engine::Core::BuildingAttackedEvent& event);
};

} // namespace Game::Systems
