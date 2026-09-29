#pragma once

#include <QVector3D>

#include <vector>

#include "../ai_types.h"

namespace Game::Systems::AI {

struct TownPlan;

struct SettlementCensus {
  int homes = 0;
  int barracks = 0;
  int towers = 0;
  int walls = 0;
  int gates = 0;
  int markets = 0;
  int farms = 0;
};

using SettlementTargets = SettlementCensus;

struct SettlementAssessment {
  SettlementCensus standing;
  SettlementTargets targets;
  int target_catapults = 0;
  int siege_count = 0;
  const char* siege_engine = nullptr;
  const TownPlan* town_plan = nullptr;
};

[[nodiscard]] auto assess_settlement(const AISnapshot& snapshot,
                                     const AIContext& context) -> SettlementAssessment;

struct PlanStepChoice {
  const char* building = nullptr;
  QVector3D offset;
  float rotation_y = 0.0F;
  int slot = -1;
};

[[nodiscard]] auto authored_plan_step(const AIContext& context,
                                      const AISnapshot& snapshot,
                                      const SettlementAssessment& town,
                                      const char* preferred,
                                      const std::vector<int>& blocked_slots,
                                      PlanStepChoice& out_choice) -> bool;

[[nodiscard]] auto plan_still_sites_this_itself(const AIContext& context,
                                                const SettlementAssessment& town,
                                                const std::vector<int>& blocked_slots,
                                                const char* building) -> bool;

[[nodiscard]] auto plan_reserves_ground(const AIContext& context,
                                        const AISnapshot& snapshot,
                                        const char* building_type,
                                        float world_x,
                                        float world_z,
                                        bool fortifications_only = false) -> bool;

[[nodiscard]] auto plan_keep_out(const AIContext& context, const AISnapshot& snapshot)
    -> std::vector<SiteKeepOut>;

[[nodiscard]] auto plan_footprint_radius(const AIContext& context) -> float;

} // namespace Game::Systems::AI
