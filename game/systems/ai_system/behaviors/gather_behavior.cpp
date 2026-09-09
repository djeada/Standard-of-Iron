#include "gather_behavior.h"

#include <QVector3D>
#include <qvectornd.h>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../../nation_registry.h"
#include "../ai_formation.h"
#include "../ai_stall_recovery.h"
#include "../ai_utils.h"
#include "systems/ai_system/ai_types.h"

namespace Game::Systems::AI {

namespace {

struct Station {
  QVector3D center;
  std::vector<const EntitySnapshot*> units;
  const char* task = "gathering";
  BehaviorPriority priority = BehaviorPriority::Low;
  Game::Formation::ArmyFormationIntent intent =
      Game::Formation::ArmyFormationIntent::FactionDefault;
  float spacing = 1.4F;
  bool on_the_resolved_station = false;
};

void order_ranks(std::vector<const EntitySnapshot*>& units) {
  std::sort(
      units.begin(), units.end(), [](const EntitySnapshot* a, const EntitySnapshot* b) {
        return a->id < b->id;
      });
}

auto claimed_by_someone_else(const AIContext& context,
                             Engine::Core::EntityID unit_id,
                             const char* task) -> bool {
  const auto it = context.assigned_units.find(unit_id);
  return it != context.assigned_units.end() &&
         std::string_view(it->second.assigned_task) != task;
}

enum class StationHold {
  Settled,
  Holding,
  NeedsOrder
};

constexpr float k_slot_reissue_metres = 0.5F;

constexpr float k_unambiguous_slot_ratio = 2.0F;

auto nearest_rival_slot_sq(const std::vector<AIFormationSlot>& slot_list,
                           std::size_t self,
                           const EntitySnapshot& unit) -> float {
  float nearest = std::numeric_limits<float>::infinity();
  for (std::size_t i = 0; i < slot_list.size(); ++i) {
    if (i == self || !slot_list[i].usable) {
      continue;
    }
    nearest = std::min(nearest,
                       distance_squared(unit.pos_x,
                                        0.0F,
                                        unit.pos_z,
                                        slot_list[i].position.x(),
                                        0.0F,
                                        slot_list[i].position.z()));
  }
  return nearest;
}

auto hold_state(const EntitySnapshot& unit,
                const AIContext& context,
                float current_time,
                const std::vector<AIFormationSlot>& slot_list,
                std::size_t self,
                float spacing) -> StationHold {
  const QVector3D& slot = slot_list[self].position;

  switch (soldier_motion(unit, context, current_time)) {
  case SoldierMotion::Blocked:
    return StationHold::NeedsOrder;
  case SoldierMotion::UnderWay: {
    if (!unit.movement.has_objective) {
      return StationHold::NeedsOrder;
    }
    const float drift = distance_squared(unit.movement.objective_x,
                                         0.0F,
                                         unit.movement.objective_z,
                                         slot.x(),
                                         0.0F,
                                         slot.z());
    return drift <= k_slot_reissue_metres * k_slot_reissue_metres
               ? StationHold::Holding
               : StationHold::NeedsOrder;
  }
  case SoldierMotion::Standing:
    break;
  }

  const float hold = std::max(1.0F, spacing);
  const float error =
      distance_squared(unit.pos_x, 0.0F, unit.pos_z, slot.x(), 0.0F, slot.z());
  if (error > hold * hold) {
    return StationHold::NeedsOrder;
  }
  if (error * k_unambiguous_slot_ratio > nearest_rival_slot_sq(slot_list, self, unit)) {
    return StationHold::NeedsOrder;
  }
  return StationHold::Settled;
}

auto muster_stations(const AISnapshot& snapshot,
                     const AIContext& context,
                     BehaviorPriority priority) -> std::vector<Station> {
  const QVector3D rally_point(context.station.x, 0.0F, context.station.z);
  const bool garrison_posture = context.strategy_config.posture == AIPosture::Garrison;
  const bool garrison_per_base = garrison_posture && context.bases.size() > 1U;
  const float spacing = context.macro_targets.gather_spacing;

  const auto muster_intent =
      select_ai_intent(snapshot, context, garrison_posture, false);
  std::vector<Station> stations;
  if (garrison_per_base) {
    stations.reserve(context.bases.size());
    for (const auto& base : context.bases) {
      stations.push_back({QVector3D(base.rally_x, 0.0F, base.rally_z),
                          {},
                          "gathering",
                          priority,
                          muster_intent,
                          spacing});
    }
  } else {
    stations.push_back(
        {rally_point, {}, "gathering", priority, muster_intent, spacing, true});
  }
  const std::size_t rally_stations = stations.size();

  const bool garrison_holds_the_ward =
      context.has_settlement_stations && context.musters_outside;
  std::size_t garrison_station = std::numeric_limits<std::size_t>::max();
  if (garrison_holds_the_ward) {
    garrison_station = stations.size();
    stations.push_back(
        {QVector3D(context.muster_inside_x, 0.0F, context.muster_inside_z),
         {},
         "garrison",
         priority,
         Game::Formation::ArmyFormationIntent::Defensive,
         spacing});
  }
  const std::unordered_set<Engine::Core::EntityID> garrison(
      context.garrison_unit_ids.begin(), context.garrison_unit_ids.end());

  for (const auto* entity : collect_attack_force_units(snapshot, context)) {
    if (marches_with_the_wave(entity->id, context)) {
      continue;
    }
    if (garrison_holds_the_ward && garrison.contains(entity->id)) {
      stations[garrison_station].units.push_back(entity);
      continue;
    }
    std::size_t nearest = 0;
    if (garrison_per_base) {
      float best_distance_sq = std::numeric_limits<float>::infinity();
      for (std::size_t index = 0; index < rally_stations; ++index) {
        const auto& candidate = stations[index];
        const float dist_sq = distance_squared(entity->pos_x,
                                               0.0F,
                                               entity->pos_z,
                                               candidate.center.x(),
                                               0.0F,
                                               candidate.center.z());
        if (dist_sq < best_distance_sq) {
          best_distance_sq = dist_sq;
          nearest = index;
        }
      }
    }
    stations[nearest].units.push_back(entity);
  }

  if (context.anchor_is_structural && context.effective_reserve_units > 0) {
    Station reserve{
        QVector3D(context.base_pos_x, context.base_pos_y, context.base_pos_z),
        {},
        "holding-reserve",
        BehaviorPriority::VeryLow,
        Game::Formation::ArmyFormationIntent::Defensive,
        std::max(1.2F, spacing * 0.8F)};

    const float hold_radius = context.strategy_config.reserve_hold_radius;
    for (const auto* entity : collect_reserve_force_units(snapshot, context)) {
      if (distance_squared(entity->pos_x,
                           0.0F,
                           entity->pos_z,
                           reserve.center.x(),
                           0.0F,
                           reserve.center.z()) > hold_radius * hold_radius) {
        reserve.units.push_back(entity);
      }
    }
    stations.push_back(std::move(reserve));
  }

  if (context.has_settlement_stations) {
    Station detachment{QVector3D(context.detachment_x, 0.0F, context.detachment_z),
                       {},
                       "detachment",
                       BehaviorPriority::VeryLow,
                       Game::Formation::ArmyFormationIntent::FactionDefault,
                       spacing};
    for (const auto* entity : collect_harass_force_units(snapshot, context)) {
      if (claimed_by_someone_else(context, entity->id, detachment.task)) {
        continue;
      }
      detachment.units.push_back(entity);
    }
    stations.push_back(std::move(detachment));
  }

  return stations;
}

void dispatch_station(Station& station,
                      const AISnapshot& snapshot,
                      AIContext& context,
                      std::vector<AICommand>& out_commands) {
  if (station.units.empty()) {
    return;
  }
  if (station.on_the_resolved_station && !context.station.fits) {
    ++context.gather_report.refused_stations;
    return;
  }
  order_ranks(station.units);
  ++context.gather_report.stations;
  context.gather_report.members += static_cast<int>(station.units.size());

  std::vector<Engine::Core::EntityID> units_to_move;
  units_to_move.reserve(station.units.size());
  for (const auto* unit : station.units) {
    units_to_move.push_back(unit->id);
  }
  auto claimed_units = claim_units(
      units_to_move, station.priority, station.task, context, snapshot.game_time, 2.0F);
  if (claimed_units.empty()) {
    return;
  }

  const std::unordered_set<Engine::Core::EntityID> claimed_set(claimed_units.begin(),
                                                               claimed_units.end());

  AIFormationRequest formation_request;
  formation_request.player_id = context.player_id;
  formation_request.nation = context.nation;
  formation_request.anchor = station.center;
  formation_request.spacing = station.spacing;
  formation_request.intent = station.intent;
  formation_request.facing = context.station.facing_deg;
  formation_request.preserve_member_order = true;
  const auto formation_targets = plan_ai_formation(formation_request, station.units);

  AICommand command;
  command.type = AICommandType::MoveUnits;
  command.owner = station.priority;
  for (std::size_t i = 0; i < units_to_move.size(); ++i) {
    if (!claimed_set.contains(units_to_move[i])) {
      continue;
    }
    if (!formation_targets[i].usable) {
      ++context.gather_report.unplaceable;
      continue;
    }
    if (is_under_recovery(units_to_move[i], context, snapshot.game_time)) {
      ++context.gather_report.holding;
      continue;
    }
    switch (hold_state(*station.units[i],
                       context,
                       snapshot.game_time,
                       formation_targets,
                       i,
                       station.spacing)) {
    case StationHold::Settled:
      ++context.gather_report.settled;
      continue;
    case StationHold::Holding:
      ++context.gather_report.holding;
      continue;
    case StationHold::NeedsOrder:
      break;
    }
    ++context.gather_report.ordered;
    command.units.push_back(units_to_move[i]);
    command.move_target_x.push_back(formation_targets[i].position.x());
    command.move_target_y.push_back(formation_targets[i].position.y());
    command.move_target_z.push_back(formation_targets[i].position.z());
  }
  if (command.units.empty()) {
    return;
  }
  out_commands.push_back(std::move(command));
}

} // namespace

void GatherBehavior::execute(const AISnapshot& snapshot,
                             AIContext& context,
                             float delta_time,
                             std::vector<AICommand>& out_commands) {
  m_gather_timer += delta_time;
  if (m_gather_timer < 1.0F) {
    return;
  }
  m_gather_timer = 0.0F;

  if (!context.has_base_anchor) {
    return;
  }

  context.gather_report = AIContext::GatherReport{};
  auto stations = muster_stations(snapshot, context, get_priority());
  for (auto& station : stations) {
    dispatch_station(station, snapshot, context, out_commands);
  }
}

auto GatherBehavior::should_execute(const AISnapshot&,
                                    const AIContext& context) const -> bool {
  if (!context.has_base_anchor) {
    return false;
  }

  return context.state != AIState::Retreating;
}

} // namespace Game::Systems::AI
