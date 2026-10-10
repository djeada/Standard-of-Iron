#include <cstdlib>
#include "builder_intent.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <utility>

#include "../../economy/construction_cost_catalog.h"
#include "../ai_base_manager.h"
#include "../ai_doctrine_catalog.h"
#include "builder_affordability.h"
#include "builder_catalog.h"
#include "builder_site_geometry.h"
#include "builder_site_planner.h"

namespace Game::Systems::AI {

namespace {

constexpr int k_treasury_worth_a_market = 120;
constexpr int k_roofs_before_the_frame = 2;
constexpr int k_fields_before_the_frame = 2;
constexpr int k_roofs_before_the_castle = 4;

struct BuildCandidate {
  const char* type = nullptr;
  int current = 0;
  int target = 0;
};

auto unmet_candidates(std::initializer_list<BuildCandidate> candidates)
    -> std::vector<const char*> {
  std::vector<std::pair<float, const char*>> wanted;
  for (const auto& candidate : candidates) {
    if (candidate.type == nullptr || candidate.target <= candidate.current) {
      continue;
    }

    const float completion = static_cast<float>(candidate.current) /
                             static_cast<float>(std::max(1, candidate.target));
    wanted.emplace_back(completion, candidate.type);
  }
  std::stable_sort(wanted.begin(), wanted.end(), [](const auto& a, const auto& b) {
    return a.first < b.first;
  });

  std::vector<const char*> order;
  order.reserve(wanted.size());
  for (const auto& entry : wanted) {
    order.push_back(entry.second);
  }
  return order;
}

auto needs_outpost_construction(const AIContext& context) -> bool {
  if (!context.has_expansion_site) {
    return false;
  }

  if (context.outpost_barracks_count <
      context.strategy_config.desired_outpost_barracks_count) {
    return true;
  }

  return context.outpost_barracks_count >=
             context.strategy_config.desired_outpost_barracks_count &&
         context.outpost_home_count < context.strategy_config.outpost_home_target;
}

auto recent_outpost_order(const AISnapshot& snapshot,
                          const AIContext& context) -> bool {
  return (snapshot.game_time - context.last_expansion_order_time) < 4.0F;
}

auto exposed_secondary_base(const AIContext& context) -> const AIBase* {
  for (const auto& base : context.bases) {
    if (base.role == BaseRole::Main || base.defense_tower_count > 0) {
      continue;
    }
    if (base.under_threat) {
      return &base;
    }
  }
  return nullptr;
}

struct PlanStepState {
  bool present = false;
  PlanStepChoice choice;
  bool silhouette = false;
  bool fortification = false;
};

void add_outpost_intent(const AISnapshot& snapshot,
                        const AIContext& context,
                        std::vector<ConstructionIntent>& intents) {
  if (!(context.state == AIState::Expanding && needs_outpost_construction(context) &&
        !context.expansion_construction_pending &&
        !recent_outpost_order(snapshot, context))) {
    return;
  }
  ConstructionIntent outpost;
  outpost.expansion = true;
  outpost.site_known = true;
  if (context.outpost_barracks_count <
      context.strategy_config.desired_outpost_barracks_count) {
    outpost.type = BUILDING_TYPE_BARRACKS;
    outpost.x = context.expansion_site_x;
    outpost.z = context.expansion_site_z;
  } else {
    outpost.type = BUILDING_TYPE_HOME;
    const float dx = context.expansion_site_x - context.base_pos_x;
    const float dz = context.expansion_site_z - context.base_pos_z;
    const float dist = std::sqrt(std::max(0.0F, dx * dx + dz * dz));
    const float offset_scale = (dist > 0.1F) ? (8.0F / dist) : 0.0F;
    outpost.x = context.expansion_site_x - dz * offset_scale;
    outpost.z = context.expansion_site_z + dx * offset_scale;
  }
  intents.push_back(outpost);
}

void add_relief_tower_intent(const AISnapshot& snapshot,
                             const AIContext& context,
                             int construction_counter,
                             std::vector<ConstructionIntent>& intents) {
  const AIBase* exposed = exposed_secondary_base(context);
  if (exposed == nullptr) {
    return;
  }
  constexpr int k_outpost_site_attempts = 12;
  for (int attempt = 0; attempt < k_outpost_site_attempts; ++attempt) {
    const QVector3D offset =
        expanding_ring_offset(context, construction_counter + attempt, 6, 9.0F, 4.0F);
    const float candidate_x = exposed->center_x + offset.x();
    const float candidate_z = exposed->center_z + offset.z();
    if (!site_is_free(
            snapshot, BUILDING_TYPE_DEFENSE_TOWER, candidate_x, candidate_z) ||
        plan_reserves_ground(
            context, snapshot, BUILDING_TYPE_DEFENSE_TOWER, candidate_x, candidate_z)) {
      continue;
    }
    ConstructionIntent relief;
    relief.type = BUILDING_TYPE_DEFENSE_TOWER;
    relief.x = candidate_x;
    relief.z = candidate_z;
    relief.site_known = true;
    intents.push_back(relief);
    break;
  }
}

void wish(std::vector<ConstructionIntent>& intents, const char* type) {
  if (type != nullptr) {
    ConstructionIntent intent;
    intent.type = type;
    intents.push_back(intent);
  }
}

void wish_for_basics(const AISnapshot& snapshot,
                     const SettlementAssessment& town,
                     bool field_before_plan,
                     std::vector<ConstructionIntent>& intents) {
  const auto& standing = town.standing;
  if (standing.barracks == 0) {
    wish(intents, BUILDING_TYPE_BARRACKS);
  }
  if (standing.homes < k_roofs_before_the_frame) {
    wish(intents, BUILDING_TYPE_HOME);
  }
  if (field_before_plan) {
    wish(intents, BUILDING_TYPE_FARM);
  }
  if (standing.markets < 1 && town.targets.markets > 0 &&
      snapshot.has_resource_snapshot &&
      snapshot.resources.get(ResourceType::Gold) >= k_treasury_worth_a_market) {
    wish(intents, BUILDING_TYPE_MARKETPLACE);
  }
}

void add_plan_step_intent(const AIContext& context,
                          const PlanStepState& plan,
                          std::vector<ConstructionIntent>& intents) {
  if (!plan.present) {
    return;
  }
  ConstructionIntent step;
  step.type = plan.choice.building;
  step.x = context.base_pos_x + plan.choice.offset.x();
  step.z = context.base_pos_z + plan.choice.offset.z();
  step.rotation_y = plan.choice.rotation_y;
  step.site_known = context.has_base_anchor;
  step.plan_slot = plan.choice.slot;
  intents.push_back(step);
}

void wish_for_shortfalls(const AIContext& context,
                         const SettlementAssessment& town,
                         const std::vector<int>& blocked_plan_slots,
                         const PlanStepState& plan,
                         std::vector<ConstructionIntent>& intents) {
  const auto& standing = town.standing;
  const auto& targets = town.targets;
  for (const char* candidate : unmet_candidates({
           {BUILDING_TYPE_FARM, standing.farms, targets.farms},
           {BUILDING_TYPE_BARRACKS, standing.barracks, targets.barracks},
           {BUILDING_TYPE_DEFENSE_TOWER, standing.towers, targets.towers},
           {BUILDING_TYPE_MARKETPLACE, standing.markets, targets.markets},
           {BUILDING_TYPE_WALL_SEGMENT, standing.walls, targets.walls},
           {BUILDING_TYPE_WALL_GATE, standing.gates, targets.gates},
           {BUILDING_TYPE_HOME, standing.homes, targets.homes},
           {town.siege_engine, town.siege_count, town.target_catapults},
       })) {
    const bool roof_the_plan_has_not_reached =
        candidate == BUILDING_TYPE_HOME && plan.choice.building != BUILDING_TYPE_HOME && std::getenv("SOI_TMP_OFF_HOMES") == nullptr;
    if (plan.present && !roof_the_plan_has_not_reached &&
        plan_still_sites_this_itself(context, town, blocked_plan_slots, candidate)) {
      continue;
    }
    wish(intents, candidate);
  }
}

} // namespace

auto gather_construction_intents(const AISnapshot& snapshot,
                                 const AIContext& context,
                                 const SettlementAssessment& town,
                                 const std::vector<int>& blocked_plan_slots,
                                 int construction_counter)
    -> std::vector<ConstructionIntent> {
  std::vector<ConstructionIntent> intents;
  const auto& standing = town.standing;
  const auto& macro = context.macro_targets;

  add_outpost_intent(snapshot, context, intents);
  add_relief_tower_intent(snapshot, context, construction_counter, intents);

  const bool needs_a_field = standing.farms < town.targets.farms;
  const bool starving = starved_of_food(snapshot);
  const char* preferred = nullptr;
  if (needs_a_field && starving) {
    preferred = BUILDING_TYPE_FARM;
  } else if (macro.raise_homes_first) {
    preferred = BUILDING_TYPE_HOME;
  }

  PlanStepState plan;
  plan.present =
      context.primary_barracks != 0 &&
      authored_plan_step(
          context, snapshot, town, preferred, blocked_plan_slots, plan.choice);

  const bool field_before_plan =
      needs_a_field && (starving || standing.farms < k_fields_before_the_frame);
  wish_for_basics(snapshot, town, field_before_plan, intents);

  plan.silhouette = plan.present && town.town_plan != nullptr &&
                    town.town_plan->is_silhouette_step(plan.choice.slot);
  plan.fortification =
      plan.present && is_fortification(plan.choice.building) && !plan.silhouette;
  if (macro.raise_homes_first && standing.homes < MAX_HOMES &&
      (!plan.present || standing.homes < k_roofs_before_the_castle ||
       plan.fortification)) {
    wish(intents, BUILDING_TYPE_HOME);
  }

  const bool town_can_spare_an_engine = standing.barracks > 0 &&
                                        standing.homes >= k_roofs_before_the_frame &&
                                        standing.farms > 0;
  if (town_can_spare_an_engine && town.siege_count < town.target_catapults) {
    wish(intents, town.siege_engine);
  }
  constexpr int k_homes_before_a_ram = 6;
  if (town_can_spare_an_engine && standing.homes >= k_homes_before_a_ram &&
      standing.farms >= 2 && town.siege_count >= 1 &&
      town.ram_count < town.target_rams) {
    wish(intents, BUILDING_TYPE_RAM);
  }

  add_plan_step_intent(context, plan, intents);

  if (context.barracks_under_threat && standing.towers < town.targets.towers &&
      !plan_still_sites_this_itself(
          context, town, blocked_plan_slots, BUILDING_TYPE_DEFENSE_TOWER)) {
    wish(intents, BUILDING_TYPE_DEFENSE_TOWER);
  }

  wish_for_shortfalls(context, town, blocked_plan_slots, plan, intents);
  return intents;
}

auto choose_construction_intent(const AISnapshot& snapshot,
                                AIContext& context,
                                const std::vector<ConstructionIntent>& intents,
                                const ConstructionLedger& ledger) -> IntentChoice {
  IntentChoice choice;
  ResourceType saving_for = ResourceType::Count;
  context.construction_need = ResourceAmounts{};
  bool need_noted = false;
  for (const auto& intent : intents) {
    if (ledger.is_deferred(intent.type, snapshot.game_time)) {
      continue;
    }
    if (saving_for != ResourceType::Count && snapshot.has_resource_snapshot &&
        construction_cost_info(intent.type).resource_costs.get(saving_for) > 0) {
      continue;
    }
    const auto verdict = affordability_of(snapshot, intent.type);
    if (verdict.blocked) {
      if (!need_noted && intent.type != nullptr) {
        context.construction_need = construction_cost_info(intent.type).resource_costs;
        need_noted = true;
      }
      continue;
    }
    if (verdict.missing != ResourceType::Count) {

      if (choice.missing_resource == ResourceType::Count) {
        choice.missing_resource = verdict.missing;
      }
      const bool worth_saving_for =
          (intent.plan_slot >= 0 && (intent.type == BUILDING_TYPE_WALL_GATE ||
                                     intent.type == BUILDING_TYPE_DEFENSE_TOWER)) ||
          is_siege_engine_building(intent.type);
      if (worth_saving_for && saving_for == ResourceType::Count) {
        saving_for = verdict.missing;
      }
      continue;
    }
    choice.chosen = &intent;
    break;
  }
  return choice;
}

} // namespace Game::Systems::AI
