#include "ai_stall_recovery.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <utility>

#include "ai_utils.h"

namespace Game::Systems::AI {

namespace {

void drop_from(std::vector<Engine::Core::EntityID>& roster,
               Engine::Core::EntityID unit_id) {
  roster.erase(std::remove(roster.begin(), roster.end(), unit_id), roster.end());
}

[[nodiscard]] auto alternate_approach(const EntitySnapshot& entity,
                                      const AISnapshot& snapshot,
                                      int attempt) -> std::pair<float, float> {
  float const to_x = entity.movement.objective_x - entity.pos_x;
  float const to_z = entity.movement.objective_z - entity.pos_z;
  float const length = std::sqrt((to_x * to_x) + (to_z * to_z));
  if (length < 0.001F) {
    return {entity.movement.objective_x, entity.movement.objective_z};
  }

  float const side = (attempt % 2 == 0) ? 1.0F : -1.0F;
  float const reach = std::min(length * 0.5F, k_stall_detour_metres);

  float const step_x = (-to_z / length) * k_stall_detour_metres * side;
  float const step_z = (to_x / length) * k_stall_detour_metres * side;

  float waypoint_x = entity.pos_x + step_x + ((to_x / length) * reach);
  float waypoint_z = entity.pos_z + step_z + ((to_z / length) * reach);

  if (snapshot.has_map_bounds) {
    constexpr float k_edge_margin = 2.0F;
    waypoint_x = std::clamp(waypoint_x,
                            snapshot.map_min_x + k_edge_margin,
                            snapshot.map_max_x - k_edge_margin);
    waypoint_z = std::clamp(waypoint_z,
                            snapshot.map_min_z + k_edge_margin,
                            snapshot.map_max_z - k_edge_margin);
  }
  return {waypoint_x, waypoint_z};
}

void stand_down(Engine::Core::EntityID unit_id,
                AIContext& context,
                AIContext::StallRecord& record,
                float current_time) {
  record.stood_down_until = current_time + k_stall_stand_down_seconds;
  record.nudges = 0;
  context.assigned_units.erase(unit_id);
  drop_from(context.wave.members, unit_id);
  drop_from(context.harass_unit_ids, unit_id);
  drop_from(context.assault_unit_ids, unit_id);
  drop_from(context.reserve_unit_ids, unit_id);
  drop_from(context.garrison_unit_ids, unit_id);
}

} // namespace

auto is_stood_down(Engine::Core::EntityID unit_id,
                   const AIContext& context,
                   float current_time) -> bool {
  const auto found = context.stalled_units.find(unit_id);
  return found != context.stalled_units.end() &&
         current_time < found->second.stood_down_until;
}

auto is_going_nowhere(const EntitySnapshot& entity) -> bool {

  return is_combat_role_unit(entity) && entity.movement.has_component &&
         (entity.movement.objective_abandoned || entity.movement.stalled);
}

void update_stall_recovery(const AISnapshot& snapshot,
                           AIContext& context,
                           std::vector<AICommand>& out_commands) {
  std::unordered_set<Engine::Core::EntityID> seen;
  seen.reserve(snapshot.friendly_units.size());

  for (const auto& entity : snapshot.friendly_units) {
    if (entity.is_building || entity.health <= 0) {
      continue;
    }
    seen.insert(entity.id);

    if (!is_going_nowhere(entity)) {

      const auto found = context.stalled_units.find(entity.id);
      if (found != context.stalled_units.end() &&
          snapshot.game_time >= found->second.stood_down_until) {
        context.stalled_units.erase(found);
      }
      continue;
    }

    auto& record = context.stalled_units[entity.id];
    if (record.first_seen <= 0.0F) {
      record.first_seen = snapshot.game_time;
    }
    if (snapshot.game_time < record.stood_down_until) {
      continue;
    }

    bool const out_of_patience =
        record.nudges >= k_max_stall_nudges ||
        entity.movement.abandon_count >= k_stall_abandon_patience;
    if (out_of_patience) {
      stand_down(entity.id, context, record, snapshot.game_time);
      continue;
    }

    if (snapshot.game_time - record.last_nudge < k_stall_nudge_interval_seconds) {
      continue;
    }
    if (!entity.movement.has_objective) {

      stand_down(entity.id, context, record, snapshot.game_time);
      continue;
    }

    const auto [waypoint_x, waypoint_z] =
        alternate_approach(entity, snapshot, record.nudges);

    AICommand command;
    command.type = AICommandType::MoveUnits;
    command.units = {entity.id};
    command.move_target_x = {waypoint_x};
    command.move_target_y = {0.0F};
    command.move_target_z = {waypoint_z};
    out_commands.push_back(std::move(command));

    ++record.nudges;
    record.last_nudge = snapshot.game_time;
  }

  std::erase_if(context.stalled_units,
                [&](const auto& entry) { return !seen.contains(entry.first); });
}

} // namespace Game::Systems::AI
