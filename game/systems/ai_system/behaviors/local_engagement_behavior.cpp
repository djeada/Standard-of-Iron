#include "local_engagement_behavior.h"

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../ai_tactical.h"
#include "../ai_utils.h"
#include "systems/ai_system/ai_types.h"

namespace Game::Systems::AI {

namespace {

struct ThreatCluster {
  float center_x = 0.0F;
  float center_z = 0.0F;
  std::vector<const ContactSnapshot*> threats;
};

auto cluster_threats(const AISnapshot& snapshot) -> std::vector<ThreatCluster> {
  const float radius_sq = LocalEngagementBehavior::k_threat_cluster_radius *
                          LocalEngagementBehavior::k_threat_cluster_radius;

  std::vector<ThreatCluster> clusters;
  for (const auto& enemy : snapshot.visible_enemies) {
    if (enemy.is_building || enemy.health <= 0) {
      continue;
    }

    ThreatCluster* best = nullptr;
    float best_distance_sq = radius_sq;
    for (auto& cluster : clusters) {
      const float distance_sq = distance_squared(
          enemy.pos_x, 0.0F, enemy.pos_z, cluster.center_x, 0.0F, cluster.center_z);
      if (distance_sq <= best_distance_sq) {
        best_distance_sq = distance_sq;
        best = &cluster;
      }
    }

    if (best == nullptr) {
      clusters.push_back({enemy.pos_x, enemy.pos_z, {&enemy}});
      continue;
    }

    best->threats.push_back(&enemy);
    const float count = static_cast<float>(best->threats.size());
    best->center_x += (enemy.pos_x - best->center_x) / count;
    best->center_z += (enemy.pos_z - best->center_z) / count;
  }

  std::sort(clusters.begin(),
            clusters.end(),
            [](const ThreatCluster& a, const ThreatCluster& b) {
              return a.threats.size() > b.threats.size();
            });
  return clusters;
}

auto is_local_engagement_assignment(const EntitySnapshot& entity,
                                    const AIContext& context) -> bool {
  const auto assignment = context.assigned_units.find(entity.id);
  return assignment != context.assigned_units.end() &&
         std::string_view(assignment->second.assigned_task) ==
             LocalEngagementBehavior::k_task_name;
}

auto is_local_responder(const EntitySnapshot& entity,
                        const AIContext& context) -> bool {
  if (!is_combat_role_unit(entity) || entity.is_assault ||
      is_harass_unit(entity.id, context)) {
    return false;
  }

  const auto assignment = context.assigned_units.find(entity.id);
  if (assignment == context.assigned_units.end()) {
    return true;
  }
  return assignment->second.owner_priority <= BehaviorPriority::Low ||
         is_local_engagement_assignment(entity, context);
}

} // namespace

auto LocalEngagementBehavior::fresh_responder_slots(int already_fighting,
                                                    const AIStrategyConfig& strategy,
                                                    int threat_count) -> int {

  constexpr int k_responders_per_threat = 3;
  int const budget = std::max(strategy.max_local_responders,
                              std::max(1, threat_count) * k_responders_per_threat);
  return std::max(0, budget - std::max(0, already_fighting));
}

void LocalEngagementBehavior::execute(const AISnapshot& snapshot,
                                      AIContext& context,
                                      float delta_time,
                                      std::vector<AICommand>& out_commands) {
  m_timer += delta_time;
  if (m_timer < k_update_interval) {
    return;
  }
  m_timer = 0.0F;

  const auto clusters = cluster_threats(snapshot);
  if (clusters.empty()) {
    return;
  }

  std::vector<const EntitySnapshot*> responders;
  responders.reserve(snapshot.friendly_units.size());
  for (const auto& entity : snapshot.friendly_units) {
    if (is_local_responder(entity, context)) {
      responders.push_back(&entity);
    }
  }
  if (responders.empty()) {
    return;
  }

  const float response_radius = context.strategy_config.local_response_radius;
  std::unordered_set<Engine::Core::EntityID> committed;
  std::vector<Engine::Core::EntityID> stood_down;

  for (const auto& cluster : clusters) {
    std::vector<const EntitySnapshot*> candidates;
    for (const auto* unit : responders) {
      if (committed.contains(unit->id)) {
        continue;
      }
      const bool within_reach = std::any_of(cluster.threats.begin(),
                                            cluster.threats.end(),
                                            [&](const ContactSnapshot* threat) {
                                              return distance_squared(unit->pos_x,
                                                                      0.0F,
                                                                      unit->pos_z,
                                                                      threat->pos_x,
                                                                      0.0F,
                                                                      threat->pos_z) <=
                                                     response_radius * response_radius;
                                            });
      if (within_reach) {
        candidates.push_back(unit);
      }
    }
    if (candidates.empty()) {
      continue;
    }

    std::sort(candidates.begin(),
              candidates.end(),
              [&cluster](const EntitySnapshot* a, const EntitySnapshot* b) {
                const float da = distance_squared(
                    a->pos_x, 0.0F, a->pos_z, cluster.center_x, 0.0F, cluster.center_z);
                const float db = distance_squared(
                    b->pos_x, 0.0F, b->pos_z, cluster.center_x, 0.0F, cluster.center_z);
                if (da != db) {
                  return da < db;
                }
                return a->id < b->id;
              });

    std::vector<const EntitySnapshot*> already_fighting;
    std::vector<const EntitySnapshot*> fresh;
    for (const auto* unit : candidates) {
      if (is_local_engagement_assignment(*unit, context) ||
          is_entity_engaged(*unit, snapshot.visible_enemies)) {
        already_fighting.push_back(unit);
      } else {
        fresh.push_back(unit);
      }
    }
    const auto fresh_slots = static_cast<std::size_t>(
        fresh_responder_slots(static_cast<int>(already_fighting.size()),
                              context.strategy_config,
                              static_cast<int>(cluster.threats.size())));
    fresh.resize(std::min(fresh.size(), fresh_slots));

    candidates = already_fighting;
    candidates.insert(candidates.end(), fresh.begin(), fresh.end());
    if (candidates.empty()) {
      continue;
    }

    if (already_fighting.empty() && !TacticalUtils::assess_engagement(
                                         candidates, cluster.threats, k_min_force_ratio)
                                         .should_engage) {
      continue;
    }

    const auto target = TacticalUtils::select_focus_fire_target(
        candidates, cluster.threats, cluster.center_x, 0.0F, cluster.center_z, context);
    if (target.target_id == 0) {
      continue;
    }

    std::vector<Engine::Core::EntityID> ids;
    ids.reserve(candidates.size());
    for (const auto* unit : candidates) {
      ids.push_back(unit->id);
    }
    auto claimed = claim_units(
        ids, get_priority(), k_task_name, context, snapshot.game_time, 2.0F);
    if (claimed.empty()) {
      continue;
    }
    committed.insert(claimed.begin(), claimed.end());

    AICommand attack;
    attack.type = AICommandType::AttackTarget;
    attack.units = std::move(claimed);
    attack.target_id = target.target_id;
    attack.should_chase = true;
    out_commands.push_back(std::move(attack));
  }

  for (const auto& [unit_id, assignment] : context.assigned_units) {
    if (std::string_view(assignment.assigned_task) == k_task_name &&
        !committed.contains(unit_id)) {
      stood_down.push_back(unit_id);
    }
  }
  release_units(stood_down, context);
}

auto LocalEngagementBehavior::should_execute(const AISnapshot& snapshot,
                                             const AIContext& context) const -> bool {
  if (context.strategy_config.local_response_radius <= 0.0F ||
      context.strategy_config.max_local_responders <= 0) {
    return false;
  }
  if (context.state == AIState::Retreating) {
    return false;
  }
  return std::any_of(snapshot.visible_enemies.begin(),
                     snapshot.visible_enemies.end(),
                     [](const ContactSnapshot& enemy) { return !enemy.is_building; });
}

} // namespace Game::Systems::AI
