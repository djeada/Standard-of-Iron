#pragma once

#include <vector>

#include "../ai_behavior.h"

namespace Game::Systems::AI {

class BuilderBehavior : public AIBehavior {
public:
  void execute(const AISnapshot& snapshot,
               AIContext& context,
               float delta_time,
               std::vector<AICommand>& out_commands) override;

  [[nodiscard]] auto should_execute(const AISnapshot& snapshot,
                                    const AIContext& context) const -> bool override;

  [[nodiscard]] auto get_priority() const -> BehaviorPriority override {
    return BehaviorPriority::High;
  }

  [[nodiscard]] auto can_run_concurrently() const -> bool override { return true; }

private:
  void divide_work_parties(const AISnapshot& snapshot,
                           const AIContext& context,
                           std::vector<AICommand>& out_commands) const;

  void manage_gather_crew(const AISnapshot& snapshot,
                          const AIContext& context,
                          bool reclaim_one,
                          std::vector<Engine::Core::EntityID>& available_builders,
                          std::vector<AICommand>& out_commands);

  [[nodiscard]] auto is_deferred(const char* building_type,
                                 float game_time) const -> bool;

  void note_construction_order(const char* building_type,
                               int building_total,
                               float game_time,
                               int plan_slot = -1);

  float m_construction_timer = 0.0F;
  int m_construction_counter = 0;

  const char* m_last_order_type = nullptr;
  int m_last_order_repeats = 0;
  int m_last_building_total = -1;
  const char* m_deferred_type = nullptr;
  float m_deferred_until = -1000.0F;
  const char* m_gather_priority = nullptr;
  float m_gather_priority_time = -1000.0F;

  int m_last_plan_slot = -1;
  int m_last_plan_slot_repeats = 0;
  std::vector<int> m_blocked_plan_slots;
};

} // namespace Game::Systems::AI
