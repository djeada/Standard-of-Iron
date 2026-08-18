#include "gather_behavior.h"

#include <QVector3D>
#include <qvectornd.h>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../../nation_registry.h"
#include "../ai_formation.h"
#include "../ai_utils.h"
#include "systems/ai_system/ai_types.h"

namespace Game::Systems::AI {

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

  QVector3D const rally_point(context.rally_x, 0.0F, context.rally_z);
  const float gather_tolerance =
      std::max(2.0F, context.macro_targets.assembly_radius * 0.35F);
  const float gather_tolerance_sq = gather_tolerance * gather_tolerance;

  const bool garrison_per_base =
      context.strategy_config.posture == AIPosture::Garrison &&
      context.bases.size() > 1U;

  struct GatherGroup {
    QVector3D center;
    std::vector<const EntitySnapshot*> units;
  };
  std::vector<GatherGroup> gather_groups;
  if (garrison_per_base) {
    gather_groups.reserve(context.bases.size());
    for (const auto& base : context.bases) {
      gather_groups.push_back({QVector3D(base.rally_x, 0.0F, base.rally_z), {}});
    }
  } else {
    gather_groups.push_back({rally_point, {}});
  }

  for (const auto* entity : collect_attack_force_units(snapshot, context)) {
    GatherGroup* group = &gather_groups.front();
    if (garrison_per_base) {
      float best_distance_sq = std::numeric_limits<float>::infinity();
      for (auto& candidate : gather_groups) {
        const float dist_sq = distance_squared(entity->pos_x,
                                               0.0F,
                                               entity->pos_z,
                                               candidate.center.x(),
                                               0.0F,
                                               candidate.center.z());
        if (dist_sq < best_distance_sq) {
          best_distance_sq = dist_sq;
          group = &candidate;
        }
      }
    }

    const float dx = entity->pos_x - group->center.x();
    const float dz = entity->pos_z - group->center.z();
    if (dx * dx + dz * dz > gather_tolerance_sq) {
      group->units.push_back(entity);
    }
  }

  auto emit_move_command = [&](const std::vector<const EntitySnapshot*>& units,
                               const QVector3D& center,
                               float spacing,
                               const char* task_name,
                               BehaviorPriority priority) {
    if (units.empty()) {
      return;
    }

    AIFormationRequest formation_request;
    formation_request.player_id = context.player_id;
    formation_request.nation = context.nation;
    formation_request.anchor = center;
    formation_request.spacing = spacing;
    formation_request.intent = Game::Formation::ArmyFormationIntent::FactionDefault;
    auto formation_targets = plan_ai_formation(formation_request, units);

    std::vector<Engine::Core::EntityID> units_to_move;
    std::vector<float> target_x;
    std::vector<float> target_y;
    std::vector<float> target_z;
    units_to_move.reserve(units.size());
    target_x.reserve(units.size());
    target_y.reserve(units.size());
    target_z.reserve(units.size());

    for (size_t i = 0; i < units.size(); ++i) {
      units_to_move.push_back(units[i]->id);
      target_x.push_back(formation_targets[i].x());
      target_y.push_back(formation_targets[i].y());
      target_z.push_back(formation_targets[i].z());
    }

    auto claimed_units = claim_units(
        units_to_move, priority, task_name, context, snapshot.game_time, 2.0F);
    if (claimed_units.empty()) {
      return;
    }

    std::vector<float> filtered_x;
    std::vector<float> filtered_y;
    std::vector<float> filtered_z;
    const std::unordered_set<Engine::Core::EntityID> claimed_set(claimed_units.begin(),
                                                                 claimed_units.end());
    for (size_t i = 0; i < units_to_move.size(); ++i) {
      if (claimed_set.count(units_to_move[i])) {
        filtered_x.push_back(target_x[i]);
        filtered_y.push_back(target_y[i]);
        filtered_z.push_back(target_z[i]);
      }
    }

    AICommand command;
    command.type = AICommandType::MoveUnits;
    command.units = std::move(claimed_units);
    command.move_target_x = std::move(filtered_x);
    command.move_target_y = std::move(filtered_y);
    command.move_target_z = std::move(filtered_z);
    out_commands.push_back(std::move(command));
  };

  for (const auto& group : gather_groups) {
    emit_move_command(group.units,
                      group.center,
                      context.macro_targets.gather_spacing,
                      "gathering",
                      get_priority());
  }

  if (context.anchor_is_structural && context.effective_reserve_units > 0) {
    QVector3D const reserve_center(
        context.base_pos_x, context.base_pos_y, context.base_pos_z);
    const float reserve_hold_radius_sq = context.strategy_config.reserve_hold_radius *
                                         context.strategy_config.reserve_hold_radius;
    std::vector<const EntitySnapshot*> reserve_units_to_hold;
    for (const auto* entity : collect_reserve_force_units(snapshot, context)) {
      const float dx = entity->pos_x - reserve_center.x();
      const float dz = entity->pos_z - reserve_center.z();
      const float dist_sq = dx * dx + dz * dz;
      if (dist_sq > reserve_hold_radius_sq) {
        reserve_units_to_hold.push_back(entity);
      }
    }

    emit_move_command(reserve_units_to_hold,
                      reserve_center,
                      std::max(1.2F, context.macro_targets.gather_spacing * 0.8F),
                      "holding-reserve",
                      BehaviorPriority::VeryLow);
  }
}

auto GatherBehavior::should_execute(const AISnapshot& snapshot,
                                    const AIContext& context) const -> bool {
  if (!context.has_base_anchor) {
    return false;
  }

  if (context.state == AIState::Retreating) {
    return false;
  }

  if (context.state == AIState::Attacking) {
    return false;
  }

  if (context.state == AIState::Defending) {

    QVector3D const rally_point(context.rally_x, 0.0F, context.rally_z);
    const float defend_gather_radius =
        std::max(10.0F, context.macro_targets.assembly_radius);
    const float defend_gather_radius_sq = defend_gather_radius * defend_gather_radius;
    for (const auto& entity : snapshot.friendly_units) {
      if (entity.is_building) {
        continue;
      }

      const float dx = entity.pos_x - rally_point.x();
      const float dz = entity.pos_z - rally_point.z();
      const float dist_sq = dx * dx + dz * dz;

      if (dist_sq > defend_gather_radius_sq) {
        return true;
      }
    }
    return false;
  }

  if (context.state == AIState::Gathering || context.state == AIState::Idle) {
    return true;
  }

  return false;
}

} // namespace Game::Systems::AI
