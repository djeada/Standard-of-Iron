#include "builder_behavior.h"

#include <QVector2D>

#include <utility>

#include "../../nation_registry.h"
#include "../ai_base_manager.h"
#include "../ai_settlement_frame.h"
#include "builder_affordability.h"
#include "builder_catalog.h"
#include "builder_orders.h"
#include "builder_site_geometry.h"
#include "builder_site_planner.h"
#include "builder_town_plan.h"
#include "units/spawn_type.h"

namespace Game::Systems::AI {

namespace {

constexpr int k_smallest_useful_work_party = 4;
constexpr float k_unsited_retry_seconds = 60.0F;

auto desired_work_parties(const AIContext& context) -> int {
  (void)context;

  return 4;
}

} // namespace

void BuilderBehavior::divide_work_parties(const AISnapshot& snapshot,
                                          const AIContext& context,
                                          std::vector<AICommand>& out_commands) const {
  const int wanted = desired_work_parties(context);
  if (context.builder_count >= wanted) {
    return;
  }

  const EntitySnapshot* biggest = nullptr;
  for (const auto& entity : snapshot.friendly_units) {
    if (entity.spawn_type != Game::Units::SpawnType::Builder) {
      continue;
    }

    if (entity.squad_strength < k_smallest_useful_work_party * 2) {
      continue;
    }
    if (biggest == nullptr || entity.squad_strength > biggest->squad_strength) {
      biggest = &entity;
    }
  }
  if (biggest == nullptr) {
    return;
  }

  AICommand command;
  command.type = AICommandType::DivideSquads;
  command.units.push_back(biggest->id);
  out_commands.push_back(std::move(command));
}

auto BuilderBehavior::resolve_site(const AISnapshot& snapshot,
                                   const AIContext& context,
                                   const ConstructionIntent* chosen) -> PendingSite {
  PendingSite site;
  site.building = chosen != nullptr ? chosen->type : nullptr;
  site.x = context.base_pos_x;
  site.z = context.base_pos_z;
  if (chosen == nullptr) {
    return site;
  }
  site.rotation_y = chosen->rotation_y;
  site.plan_slot = chosen->plan_slot;
  site.expansion = chosen->expansion;

  if (chosen->site_known) {
    site.x = chosen->x;
    site.z = chosen->z;
    site.resolved = true;
    return site;
  }
  if (!context.has_base_anchor) {
    site.resolved = true;
    return site;
  }

  const ResolvedSite found =
      find_free_site(snapshot, context, site.building, m_construction_counter);
  if (found.resolved) {
    site.x = found.x;
    site.z = found.z;
    site.resolved = true;
    return site;
  }

  // Nowhere to put it: step aside for a while so the next wish gets a turn. A
  // second barracks with no ground left in a walled estate otherwise stood
  // first in line for the rest of the match, and no farm was ever laid.
  m_ledger.defer(site.building, snapshot.game_time + k_unsited_retry_seconds);
  site.building = nullptr;
  return site;
}

auto BuilderBehavior::issue_construction(const AISnapshot& snapshot,
                                         AIContext& context,
                                         BuilderPool& pool,
                                         const PendingSite& site,
                                         std::vector<AICommand>& out_commands) -> bool {
  float construction_x = site.x;
  float construction_z = site.z;
  clamp_to_map_bounds(snapshot, construction_x, construction_z);

  if (!order_construction(snapshot,
                          context,
                          pool,
                          site.building,
                          construction_x,
                          construction_z,
                          site.rotation_y,
                          out_commands)) {
    return false;
  }

  if (site.expansion) {
    AIBaseManager::note_expansion_order(
        context, snapshot.game_time, construction_x, construction_z);
  }
  if (site.plan_slot >= 0 && !context.settlement_facing_locked) {
    const QVector2D facing = settlement_facing(context, snapshot);
    context.settlement_facing_x = facing.x();
    context.settlement_facing_z = facing.y();
    context.settlement_facing_locked = true;
  }
  m_ledger.note_order(site.building,
                      static_cast<int>(context.buildings.size()),
                      snapshot.game_time,
                      site.plan_slot);
  m_construction_counter++;
  return true;
}

auto BuilderBehavior::run_construction_cycle(const AISnapshot& snapshot,
                                             AIContext& context,
                                             BuilderPool& pool,
                                             std::vector<AICommand>& out_commands)
    -> bool {
  const SettlementAssessment town = assess_settlement(snapshot, context);
  const auto intents = gather_construction_intents(
      snapshot, context, town, m_ledger.blocked_plan_slots(), m_construction_counter);
  IntentChoice choice =
      choose_construction_intent(snapshot, context, intents, m_ledger);

  if (choice.missing_resource != ResourceType::Count) {
    order_harvest(snapshot,
                  context,
                  choice.missing_resource,
                  m_stalls.sour_nodes(),
                  pool,
                  out_commands);
  }

  PendingSite site = resolve_site(snapshot, context, choice.chosen);
  // A wish with nowhere to stand is deferred by resolve_site; the next one in
  // line gets this round rather than waiting a whole cycle behind it.
  constexpr int k_unsited_fallbacks = 4;
  for (int fallback = 0; fallback < k_unsited_fallbacks && choice.chosen != nullptr &&
                         site.building == nullptr;
       ++fallback) {
    choice = choose_construction_intent(snapshot, context, intents, m_ledger);
    site = resolve_site(snapshot, context, choice.chosen);
  }

  order_field_work(
      snapshot, context, pool, starved_of_food(snapshot) ? 0 : 1, out_commands);
  order_repairs(snapshot, pool, 1, out_commands);

  bool wanted_a_builder = false;
  if (site.building != nullptr && site.resolved && site.plan_slot >= 0 &&
      is_fortification_or_tower(site.building)) {
    const ClearingOutcome outcome = clear_node_in_the_way(snapshot,
                                                          m_stalls.sour_nodes(),
                                                          pool,
                                                          site.building,
                                                          site.x,
                                                          site.z,
                                                          out_commands);
    if (outcome != ClearingOutcome::Unobstructed) {
      wanted_a_builder = outcome == ClearingOutcome::NoBuilder;
      site.building = nullptr;
    }
  }

  if (site.building != nullptr && site.resolved) {
    wanted_a_builder = !issue_construction(snapshot, context, pool, site, out_commands);
  }
  return wanted_a_builder;
}

void BuilderBehavior::execute(const AISnapshot& snapshot,
                              AIContext& context,
                              float delta_time,
                              std::vector<AICommand>& out_commands) {
  if (context.nation != nullptr && !context.nation->has_economy) {
    return;
  }

  m_construction_timer += delta_time;
  if (m_construction_timer < 3.0F) {
    return;
  }
  m_construction_timer = 0.0F;

  m_stalls.review(snapshot, snapshot.game_time);
  BuilderPool pool = BuilderPool::gather(snapshot, m_stalls);

  const bool wanted_a_builder =
      run_construction_cycle(snapshot, context, pool, out_commands);

  divide_work_parties(snapshot, context, out_commands);
  m_gather.manage(snapshot, context, wanted_a_builder, pool, out_commands);
}

auto BuilderBehavior::should_execute(const AISnapshot& snapshot,
                                     const AIContext& context) const -> bool {
  (void)snapshot;
  if (context.nation != nullptr && !context.nation->has_economy) {
    return false;
  }

  return context.builder_count > 0;
}

} // namespace Game::Systems::AI