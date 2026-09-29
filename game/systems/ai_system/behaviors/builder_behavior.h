#pragma once

#include <vector>

#include "../ai_behavior.h"
#include "builder_gather_crew.h"
#include "builder_intent.h"
#include "builder_ledger.h"
#include "builder_pool.h"
#include "builder_stall_watch.h"

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
  struct PendingSite {
    const char* building = nullptr;
    float x = 0.0F;
    float z = 0.0F;
    float rotation_y = 0.0F;
    int plan_slot = -1;
    bool expansion = false;
    bool resolved = false;
  };

  auto run_construction_cycle(const AISnapshot& snapshot,
                              AIContext& context,
                              BuilderPool& pool,
                              std::vector<AICommand>& out_commands) -> bool;

  auto resolve_site(const AISnapshot& snapshot,
                    const AIContext& context,
                    const ConstructionIntent* chosen) -> PendingSite;

  auto issue_construction(const AISnapshot& snapshot,
                          AIContext& context,
                          BuilderPool& pool,
                          const PendingSite& site,
                          std::vector<AICommand>& out_commands) -> bool;

  void divide_work_parties(const AISnapshot& snapshot,
                           const AIContext& context,
                           std::vector<AICommand>& out_commands) const;

  float m_construction_timer = 0.0F;
  int m_construction_counter = 0;

  ConstructionLedger m_ledger;
  WorkerStallWatch m_stalls;
  GatherCrew m_gather;
};

} // namespace Game::Systems::AI
