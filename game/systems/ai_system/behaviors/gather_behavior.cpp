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

  const QVector3D rally_point(context.rally_x, 0.0F, context.rally_z);
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
                          get_priority(),
                          muster_intent,
                          spacing});
    }
  } else {
    stations.push_back(
        {rally_point, {}, "gathering", get_priority(), muster_intent, spacing});
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
         get_priority(),
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

  for (auto& station : stations) {
    if (station.units.empty()) {
      continue;
    }
    order_ranks(station.units);

    AIFormationRequest formation_request;
    formation_request.player_id = context.player_id;
    formation_request.nation = context.nation;
    formation_request.anchor = station.center;
    formation_request.spacing = station.spacing;
    formation_request.intent = station.intent;
    const auto formation_targets = plan_ai_formation(formation_request, station.units);

    std::vector<Engine::Core::EntityID> units_to_move;
    units_to_move.reserve(station.units.size());
    for (const auto* unit : station.units) {
      units_to_move.push_back(unit->id);
    }
    auto claimed_units = claim_units(units_to_move,
                                     station.priority,
                                     station.task,
                                     context,
                                     snapshot.game_time,
                                     2.0F);
    if (claimed_units.empty()) {
      continue;
    }

    const std::unordered_set<Engine::Core::EntityID> claimed_set(claimed_units.begin(),
                                                                 claimed_units.end());
    AICommand command;
    command.type = AICommandType::MoveUnits;
    for (std::size_t i = 0; i < units_to_move.size(); ++i) {
      if (!claimed_set.contains(units_to_move[i])) {
        continue;
      }
      command.units.push_back(units_to_move[i]);
      command.move_target_x.push_back(formation_targets[i].x());
      command.move_target_y.push_back(formation_targets[i].y());
      command.move_target_z.push_back(formation_targets[i].z());
    }
    out_commands.push_back(std::move(command));
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
