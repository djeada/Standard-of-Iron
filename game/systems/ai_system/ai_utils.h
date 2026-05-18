#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ai_types.h"

namespace Game::Systems::AI {

inline constexpr float k_assignment_stale_timeout = 4.5F;

inline void replicate_last_target_if_needed(const std::vector<float>& from_x,
                                            const std::vector<float>& from_y,
                                            const std::vector<float>& from_z,
                                            size_t wanted,
                                            std::vector<float>& out_x,
                                            std::vector<float>& out_y,
                                            std::vector<float>& out_z) {

  out_x.clear();
  out_y.clear();
  out_z.clear();

  if (from_x.empty() || from_y.empty() || from_z.empty()) {
    return;
  }

  size_t const src_size = std::min({from_x.size(), from_y.size(), from_z.size()});

  out_x.reserve(wanted);
  out_y.reserve(wanted);
  out_z.reserve(wanted);

  for (size_t i = 0; i < wanted; ++i) {
    size_t const idx = std::min(i, src_size - 1);
    out_x.push_back(from_x[idx]);
    out_y.push_back(from_y[idx]);
    out_z.push_back(from_z[idx]);
  }
}

inline auto is_entity_engaged(const EntitySnapshot& entity,
                              const std::vector<ContactSnapshot>& enemies) -> bool {
  constexpr float ENGAGED_RADIUS = 7.5F;
  const float engaged_sq = ENGAGED_RADIUS * ENGAGED_RADIUS;

  for (const auto& enemy : enemies) {
    float const dx = enemy.pos_x - entity.pos_x;
    float const dy = enemy.pos_y - entity.pos_y;
    float const dz = enemy.pos_z - entity.pos_z;
    float const dist_sq = dx * dx + dy * dy + dz * dz;

    if (dist_sq <= engaged_sq) {
      return true;
    }
  }

  return false;
}

inline auto is_combat_role_unit(const EntitySnapshot& entity) -> bool {
  return !entity.is_building && entity.spawn_type != Game::Units::SpawnType::Builder;
}

inline auto is_reserved_unit(Engine::Core::EntityID unit_id, const AIContext& context)
    -> bool {
  return std::find(
             context.reserve_unit_ids.begin(), context.reserve_unit_ids.end(), unit_id) !=
         context.reserve_unit_ids.end();
}

inline auto is_harass_unit(Engine::Core::EntityID unit_id, const AIContext& context) -> bool {
  return std::find(
             context.harass_unit_ids.begin(), context.harass_unit_ids.end(), unit_id) !=
         context.harass_unit_ids.end();
}

inline auto collect_attack_force_units(const AISnapshot& snapshot,
                                       const AIContext& context,
                                       bool exclude_engaged = true)
    -> std::vector<const EntitySnapshot*> {
  std::vector<const EntitySnapshot*> result;
  result.reserve(snapshot.friendly_units.size());

  for (const auto& entity : snapshot.friendly_units) {
    if (!is_combat_role_unit(entity) || is_reserved_unit(entity.id, context) ||
        is_harass_unit(entity.id, context)) {
      continue;
    }
    if (exclude_engaged && is_entity_engaged(entity, snapshot.visible_enemies)) {
      continue;
    }
    result.push_back(&entity);
  }

  return result;
}

inline auto collect_harass_force_units(const AISnapshot& snapshot,
                                       const AIContext& context,
                                       bool exclude_engaged = true)
    -> std::vector<const EntitySnapshot*> {
  std::unordered_map<Engine::Core::EntityID, const EntitySnapshot*> by_id;
  by_id.reserve(snapshot.friendly_units.size());
  for (const auto& entity : snapshot.friendly_units) {
    if (!is_combat_role_unit(entity) || is_reserved_unit(entity.id, context)) {
      continue;
    }
    if (exclude_engaged && is_entity_engaged(entity, snapshot.visible_enemies)) {
      continue;
    }
    by_id.emplace(entity.id, &entity);
  }

  std::vector<const EntitySnapshot*> result;
  result.reserve(context.harass_unit_ids.size());
  for (auto unit_id : context.harass_unit_ids) {
    auto it = by_id.find(unit_id);
    if (it != by_id.end()) {
      result.push_back(it->second);
    }
  }

  return result;
}

inline auto collect_reserve_force_units(const AISnapshot& snapshot,
                                        const AIContext& context,
                                        bool exclude_engaged = true)
    -> std::vector<const EntitySnapshot*> {
  std::unordered_map<Engine::Core::EntityID, const EntitySnapshot*> by_id;
  by_id.reserve(snapshot.friendly_units.size());
  for (const auto& entity : snapshot.friendly_units) {
    if (!is_combat_role_unit(entity)) {
      continue;
    }
    if (exclude_engaged && is_entity_engaged(entity, snapshot.visible_enemies)) {
      continue;
    }
    by_id.emplace(entity.id, &entity);
  }

  std::vector<const EntitySnapshot*> result;
  result.reserve(context.reserve_unit_ids.size());
  for (auto unit_id : context.reserve_unit_ids) {
    auto it = by_id.find(unit_id);
    if (it != by_id.end()) {
      result.push_back(it->second);
    }
  }

  return result;
}

inline auto
distance_squared(float x1, float y1, float z1, float x2, float y2, float z2) -> float {
  float const dx = x2 - x1;
  float const dy = y2 - y1;
  float const dz = z2 - z1;
  return dx * dx + dy * dy + dz * dz;
}

inline auto
distance(float x1, float y1, float z1, float x2, float y2, float z2) -> float {
  return std::sqrt(distance_squared(x1, y1, z1, x2, y2, z2));
}

inline auto
claim_units(const std::vector<Engine::Core::EntityID>& requested_units,
            BehaviorPriority priority,
            const char* task_name,
            AIContext& context,
            float current_time,
            float min_lock_duration = 2.0F) -> std::vector<Engine::Core::EntityID> {

  std::vector<Engine::Core::EntityID> claimed;
  claimed.reserve(requested_units.size());

  for (Engine::Core::EntityID const unit_id : requested_units) {
    auto it = context.assigned_units.find(unit_id);

    if (it == context.assigned_units.end()) {

      AIContext::UnitAssignment assignment;
      assignment.owner_priority = priority;
      assignment.assignment_time = current_time;
      assignment.assigned_task = task_name;
      context.assigned_units[unit_id] = assignment;
      claimed.push_back(unit_id);

    } else {
      auto& existing = it->second;
      float const assignment_age = current_time - existing.assignment_time;
      bool const same_task = (existing.owner_priority == priority) &&
                             (std::string_view(existing.assigned_task) == task_name);

      if (same_task) {
        existing.assignment_time = current_time;
        claimed.push_back(unit_id);
        continue;
      }

      bool const can_steal =
          (priority > existing.owner_priority) && (assignment_age >= min_lock_duration);

      if (can_steal) {

        AIContext::UnitAssignment assignment;
        assignment.owner_priority = priority;
        assignment.assignment_time = current_time;
        assignment.assigned_task = task_name;
        context.assigned_units[unit_id] = assignment;
        claimed.push_back(unit_id);
      }
    }
  }

  return claimed;
}

inline void release_units(const std::vector<Engine::Core::EntityID>& units,
                          AIContext& context) {
  for (Engine::Core::EntityID const unit_id : units) {
    context.assigned_units.erase(unit_id);
  }
}

inline auto cleanup_dead_units(const AISnapshot& snapshot,
                               AIContext& context,
                               float current_time = -1.0F)
    -> std::unordered_set<Engine::Core::EntityID> {

  std::unordered_set<Engine::Core::EntityID> alive;
  alive.reserve(snapshot.friendly_units.size());
  for (const auto& entity : snapshot.friendly_units) {
    alive.insert(entity.id);
  }

  for (auto it = context.assigned_units.begin(); it != context.assigned_units.end();) {
    bool const unit_missing = (alive.find(it->first) == alive.end());
    bool const assignment_stale =
        (current_time >= 0.0F) &&
        ((current_time - it->second.assignment_time) > k_assignment_stale_timeout);

    if (unit_missing || assignment_stale) {
      it = context.assigned_units.erase(it);
    } else {
      ++it;
    }
  }

  return alive;
}

} // namespace Game::Systems::AI
