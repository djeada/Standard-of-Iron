#include "ai_macro_targets.h"

#include <algorithm>
#include <limits>

#include "../../units/spawn_type.h"
#include "../../units/troop_config.h"
#include "../../units/troop_type.h"
#include "../economy/production_service.h"
#include "../nation_registry.h"
#include "ai_doctrine_catalog.h"
#include "ai_doctrine_rules.h"
#include "ai_utils.h"

namespace Game::Systems::AI {

namespace {

constexpr int k_food_reserve = 60;
constexpr float k_worker_pop_share = 0.28F;

struct GrowthDemand {
  int army_the_doctrine_wants = 0;
  int extra_barracks = 0;
  int extra_catapults = 0;
};

auto cheapest_recruit_cost(const AIContext& ctx) -> int {
  if (ctx.nation == nullptr) {
    return 1;
  }
  int cheapest = std::numeric_limits<int>::max();
  for (const auto& troop : ctx.nation->available_troops) {
    if (!is_foot_line_recruit(troop.unit_type)) {
      continue;
    }
    if (recruiting_building_for(troop.unit_type) != Game::Units::SpawnType::Barracks) {
      continue;
    }
    cheapest = std::min(cheapest, troop.cost);
  }
  return cheapest == std::numeric_limits<int>::max() ? 1 : cheapest;
}

auto doctrine_engine_target(const AIDoctrine& doctrine,
                            int army_the_doctrine_wants) -> int {
  const int planned =
      doctrine.town_plan != nullptr ? doctrine.town_plan->engine_step_count() : 0;
  const float share = std::clamp(doctrine.recruitment.siege_share, 0.0F, 1.0F);
  if (share <= 0.0F && planned == 0) {
    return 0;
  }
  const int by_share =
      static_cast<int>((share * static_cast<float>(army_the_doctrine_wants)) + 0.999F);
  return std::max(planned, by_share);
}

auto macro_targets_without_economy(const AIContext& ctx) -> AIContext::MacroTargets {
  AIContext::MacroTargets targets;
  targets.builder_count = 0;
  targets.home_count = 0;
  targets.barracks_count = 0;
  targets.marketplace_count = 0;
  targets.defense_tower_count = 0;
  targets.wall_segment_count = 0;
  targets.catapult_count = 0;
  targets.assembly_size = std::max(2, ctx.strategy_config.reactive_attack_size);
  targets.assembly_radius = ctx.strategy_config.assembly_radius;
  targets.gather_spacing = ctx.strategy_config.gather_spacing;
  return targets;
}

void set_builder_target(AIContext::MacroTargets& targets, const AIContext& ctx) {
  targets.builder_count = ctx.strategy_config.target_builder_count;
  if (const auto* doctrine = ctx.strategy_config.doctrine;
      doctrine != nullptr && doctrine->town_plan != nullptr) {
    constexpr int k_links_per_extra_builder = 24;
    targets.builder_count +=
        doctrine->town_plan->wall_step_count() / k_links_per_extra_builder;
  }

  if (ctx.population_cap > 0) {
    const int builder_population =
        std::max(1,
                 Game::Units::TroopConfig::instance().get_population_cost(
                     Game::Units::TroopType::Builder));
    const int worker_budget =
        static_cast<int>(static_cast<float>(ctx.population_cap) * k_worker_pop_share);
    targets.builder_count = std::clamp(
        targets.builder_count, 1, std::max(1, worker_budget / builder_population));
  }
}

void set_strategy_targets(AIContext::MacroTargets& targets, const AIContext& ctx) {
  targets.barracks_count = ctx.strategy_config.desired_barracks_count;
  targets.marketplace_count = 1;

  targets.farm_count = std::clamp(1 + (ctx.home_count / 2), 1, 8);
  targets.defense_tower_count = ctx.strategy_config.desired_defense_tower_count;
  targets.wall_segment_count = ctx.strategy_config.desired_wall_segment_count;
  targets.catapult_count = ctx.strategy_config.desired_catapult_count;
  targets.assembly_size = std::max(ctx.strategy_config.desired_assembly_size,
                                   proactive_attack_size(ctx.strategy_config));
  targets.assembly_radius = ctx.strategy_config.assembly_radius;
  targets.gather_spacing = ctx.strategy_config.gather_spacing;
}

auto grow_homes_with_army(AIContext::MacroTargets& targets,
                          const AIContext& ctx) -> GrowthDemand {
  GrowthDemand demand;
  const int troop_pressure = std::max(0, ctx.total_units - targets.builder_count);
  const int home_growth = troop_pressure / 6;
  demand.extra_barracks = troop_pressure / 10;
  demand.extra_catapults = std::max(0, ctx.barracks_count - 1) / 2;

  targets.home_count = std::max(ctx.strategy_config.base_home_target,
                                2 + home_growth + std::min(2, demand.extra_barracks));

  const auto* doctrine = ctx.strategy_config.doctrine;
  demand.army_the_doctrine_wants =
      doctrine != nullptr ? doctrine->wave.size + doctrine->garrison.minimum_units
                          : proactive_attack_size(ctx.strategy_config) +
                                std::max(0, ctx.strategy_config.reserve_units);
  targets.home_count = std::max(targets.home_count, 2 + demand.army_the_doctrine_wants);
  return demand;
}

void raise_to_town_plan(AIContext::MacroTargets& targets, const AIContext& ctx) {
  const auto* doctrine = ctx.strategy_config.doctrine;
  const auto* plan = doctrine != nullptr ? doctrine->town_plan : nullptr;
  if (plan == nullptr) {
    return;
  }

  targets.home_count = std::max(targets.home_count, plan->step_count("home"));
  targets.barracks_count =
      std::max(targets.barracks_count, plan->step_count("barracks"));
  targets.defense_tower_count =
      std::max(targets.defense_tower_count, plan->step_count("defense_tower"));
  targets.wall_segment_count =
      std::max(targets.wall_segment_count, plan->wall_step_count());
  targets.marketplace_count =
      std::max(targets.marketplace_count, plan->step_count("marketplace"));
  targets.farm_count = std::max(targets.farm_count, plan->step_count("farm"));
  targets.catapult_count = std::max(targets.catapult_count, plan->engine_step_count());
}

void react_to_shortfalls(AIContext::MacroTargets& targets,
                         const AISnapshot& snapshot,
                         const AIContext& ctx) {
  if (snapshot.has_resource_snapshot &&
      snapshot.resources.get(Game::Systems::ResourceType::Food) < k_food_reserve) {
    targets.farm_count = std::max(targets.farm_count, ctx.farm_count + 1);
  }

  if (ctx.home_civilians_remaining == 0) {
    targets.home_count = std::max(targets.home_count, ctx.home_count + 2);

    if (ctx.recruitment_manpower_available < cheapest_recruit_cost(ctx)) {
      targets.raise_homes_first = true;
    }
  }
}

void finish_defence_and_engines(AIContext::MacroTargets& targets,
                                const AIContext& ctx,
                                const GrowthDemand& demand,
                                int catapult_count) {
  targets.barracks_count = std::max(targets.barracks_count, 1 + demand.extra_barracks);
  targets.defense_tower_count =
      std::max(targets.defense_tower_count,
               (ctx.home_count >= 4 || ctx.barracks_under_threat ||
                !ctx.buildings_under_attack.empty())
                   ? 2
                   : targets.defense_tower_count);
  if (const auto* doctrine = ctx.strategy_config.doctrine; doctrine != nullptr) {
    targets.catapult_count =
        doctrine_engine_target(*doctrine, demand.army_the_doctrine_wants);
  } else {
    targets.catapult_count =
        std::max(targets.catapult_count, std::min(3, demand.extra_catapults));
  }

  if (ctx.primary_barracks == 0 && ctx.home_count < 2) {
    targets.barracks_count = std::max(targets.barracks_count, 1);
  }
  if (catapult_count >= targets.catapult_count) {
    targets.catapult_count = catapult_count;
  }
}

} // namespace

auto compute_macro_targets(const AISnapshot& snapshot,
                           const AIContext& ctx,
                           int catapult_count) -> AIContext::MacroTargets {
  if (is_no_economy_nation(ctx)) {
    return macro_targets_without_economy(ctx);
  }

  AIContext::MacroTargets targets;
  set_builder_target(targets, ctx);
  set_strategy_targets(targets, ctx);
  const GrowthDemand demand = grow_homes_with_army(targets, ctx);
  raise_to_town_plan(targets, ctx);
  react_to_shortfalls(targets, snapshot, ctx);
  finish_defence_and_engines(targets, ctx, demand, catapult_count);
  return targets;
}

} // namespace Game::Systems::AI
