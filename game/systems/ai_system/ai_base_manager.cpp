#include "ai_base_manager.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <vector>

#include "../../core/ownership_constants.h"
#include "ai_utils.h"
#include "units/spawn_type.h"

namespace Game::Systems::AI {

namespace {

struct Cluster {
  float center_x = 0.0F;
  float center_z = 0.0F;
  std::vector<const EntitySnapshot*> members;
};

auto is_own_building(const EntitySnapshot& entity) -> bool {
  return entity.is_building && !Game::Core::is_neutral_owner(entity.owner_id);
}

auto base_score(const AIBase& base) -> int {
  return base.barracks_count * 3 + base.home_count + base.defense_tower_count;
}

void assign_cluster_centers(std::vector<Cluster>& clusters) {
  for (auto& cluster : clusters) {
    if (cluster.members.empty()) {
      continue;
    }
    float sum_x = 0.0F;
    float sum_z = 0.0F;
    for (const auto* member : cluster.members) {
      sum_x += member->pos_x;
      sum_z += member->pos_z;
    }
    const float scale = 1.0F / static_cast<float>(cluster.members.size());
    cluster.center_x = sum_x * scale;
    cluster.center_z = sum_z * scale;
  }
}

auto build_clusters(const AISnapshot& snapshot) -> std::vector<Cluster> {
  std::vector<const EntitySnapshot*> buildings;
  buildings.reserve(snapshot.friendly_units.size());
  for (const auto& entity : snapshot.friendly_units) {
    if (is_own_building(entity)) {
      buildings.push_back(&entity);
    }
  }

  std::sort(
      buildings.begin(),
      buildings.end(),
      [](const EntitySnapshot* a, const EntitySnapshot* b) { return a->id < b->id; });

  const float radius_sq =
      AIBaseManager::k_base_cluster_radius * AIBaseManager::k_base_cluster_radius;

  std::vector<Cluster> clusters;
  for (const auto* building : buildings) {
    int best_index = -1;
    float best_distance_sq = radius_sq;

    for (std::size_t index = 0; index < clusters.size(); ++index) {
      const float distance_sq = distance_squared(building->pos_x,
                                                 0.0F,
                                                 building->pos_z,
                                                 clusters[index].center_x,
                                                 0.0F,
                                                 clusters[index].center_z);
      if (distance_sq <= best_distance_sq) {
        best_distance_sq = distance_sq;
        best_index = static_cast<int>(index);
      }
    }

    if (best_index < 0) {
      Cluster cluster;
      cluster.center_x = building->pos_x;
      cluster.center_z = building->pos_z;
      cluster.members.push_back(building);
      clusters.push_back(std::move(cluster));
      continue;
    }

    auto& cluster = clusters[static_cast<std::size_t>(best_index)];
    cluster.members.push_back(building);
    const float scale = 1.0F / static_cast<float>(cluster.members.size());
    cluster.center_x += (building->pos_x - cluster.center_x) * scale;
    cluster.center_z += (building->pos_z - cluster.center_z) * scale;
  }

  assign_cluster_centers(clusters);

  std::sort(clusters.begin(), clusters.end(), [](const Cluster& a, const Cluster& b) {
    if (a.members.size() != b.members.size()) {
      return a.members.size() > b.members.size();
    }
    return a.members.front()->id < b.members.front()->id;
  });

  return clusters;
}

void fill_base_from_cluster(const Cluster& cluster,
                            Engine::Core::EntityID sticky_primary,
                            AIBase& base) {
  base.center_x = cluster.center_x;
  base.center_z = cluster.center_z;
  base.buildings.clear();
  base.production_buildings.clear();
  base.barracks_count = 0;
  base.home_count = 0;
  base.defense_tower_count = 0;
  base.queued_production = 0;
  base.production_capacity = 0;

  const EntitySnapshot* primary = nullptr;
  const EntitySnapshot* fallback = nullptr;

  for (const auto* member : cluster.members) {
    base.buildings.push_back(member->id);

    switch (member->spawn_type) {
    case Game::Units::SpawnType::Barracks:
      base.barracks_count++;
      break;
    case Game::Units::SpawnType::Home:
      base.home_count++;
      break;
    case Game::Units::SpawnType::DefenseTower:
      base.defense_tower_count++;
      break;
    default:
      break;
    }

    if (member->spawn_type != Game::Units::SpawnType::Barracks) {
      continue;
    }

    base.production_buildings.push_back(member->id);
    if (member->production.has_component) {
      base.queued_production +=
          (member->production.in_progress ? 1 : 0) + member->production.queue_size;
      const int remaining =
          member->production.max_units - member->production.produced_count;
      base.production_capacity += std::max(0, remaining);
    }

    if (member->id == sticky_primary) {
      primary = member;
    }
    if (fallback == nullptr || member->id < fallback->id) {
      fallback = member;
    }
  }

  const EntitySnapshot* anchor = (primary != nullptr) ? primary : fallback;
  if (anchor != nullptr) {
    base.primary_barracks = anchor->id;
    base.rally_x = anchor->pos_x - 5.0F;
    base.rally_z = anchor->pos_z;
  } else {
    base.primary_barracks = 0;
    base.rally_x = base.center_x - 5.0F;
    base.rally_z = base.center_z;
  }
}

void update_threat(const AISnapshot& snapshot, AIBase& base) {
  const float radius_sq =
      AIBaseManager::k_base_defend_radius * AIBaseManager::k_base_defend_radius;

  base.nearby_threat_count = 0;
  for (const auto& enemy : snapshot.visible_enemies) {
    if (!is_threatening_contact(enemy)) {
      continue;
    }
    const float distance_sq = distance_squared(
        enemy.pos_x, 0.0F, enemy.pos_z, base.center_x, 0.0F, base.center_z);
    if (distance_sq <= radius_sq) {
      base.nearby_threat_count++;
    }
  }

  base.under_threat = base.nearby_threat_count > 0;
  if (base.under_threat) {
    base.last_threat_time = snapshot.game_time;
  }
}

auto nearest_objective_distance_sq(const AISnapshot& snapshot,
                                   float x,
                                   float z) -> float {
  float best = std::numeric_limits<float>::infinity();
  for (const auto& objective : snapshot.strategic_objectives) {
    if (Game::Core::is_neutral_owner(objective.owner_id)) {
      continue;
    }
    best = std::min(
        best, distance_squared(objective.pos_x, 0.0F, objective.pos_z, x, 0.0F, z));
  }
  return best;
}

void assign_roles(const AISnapshot& snapshot, AIContext& ctx) {
  if (ctx.bases.empty()) {
    ctx.main_base_id = 0;
    ctx.forward_base_id = 0;
    return;
  }

  AIBase* incumbent = nullptr;
  AIBase* challenger = nullptr;
  int challenger_score = -1;

  for (auto& base : ctx.bases) {
    if (base.id == ctx.main_base_id) {
      incumbent = &base;
    }
    const int score = base_score(base);
    if (score > challenger_score) {
      challenger_score = score;
      challenger = &base;
    }
  }

  AIBase* main = incumbent;
  if (main == nullptr || main->barracks_count == 0) {
    main = challenger;
  } else if (challenger != nullptr && challenger != main &&
             challenger_score >=
                 base_score(*main) + AIBaseManager::k_migration_score_margin) {
    main = challenger;
  }

  if (main == nullptr) {
    main = &ctx.bases.front();
  }

  ctx.main_base_id = main->id;

  AIBase* forward = nullptr;
  float forward_distance_sq = std::numeric_limits<float>::infinity();
  const float min_separation_sq = AIBaseManager::k_forward_base_min_distance *
                                  AIBaseManager::k_forward_base_min_distance;

  for (auto& base : ctx.bases) {
    if (base.id == main->id) {
      continue;
    }
    const float separation_sq = distance_squared(
        base.center_x, 0.0F, base.center_z, main->center_x, 0.0F, main->center_z);
    if (separation_sq < min_separation_sq) {
      continue;
    }

    const float objective_distance_sq =
        nearest_objective_distance_sq(snapshot, base.center_x, base.center_z);
    if (objective_distance_sq < forward_distance_sq) {
      forward_distance_sq = objective_distance_sq;
      forward = &base;
    }
  }

  ctx.forward_base_id = (forward != nullptr) ? forward->id : 0;

  for (auto& base : ctx.bases) {
    if (base.id == ctx.main_base_id) {
      base.role = BaseRole::Main;
    } else if (base.id == ctx.forward_base_id) {
      base.role = BaseRole::Forward;
    } else if (base.barracks_count > 0) {
      base.role = BaseRole::Production;
    } else {
      base.role = BaseRole::Defensive;
    }
  }
}

void expire_abandoned_sites(AIContext& ctx, float game_time) {
  auto& sites = ctx.abandoned_expansion_sites;
  sites.erase(std::remove_if(sites.begin(),
                             sites.end(),
                             [game_time](const AbandonedSite& site) {
                               return (game_time - site.time) >
                                      AIBaseManager::k_abandoned_site_memory;
                             }),
              sites.end());
}

void update_forward_plan(const AISnapshot& snapshot, AIContext& ctx) {
  auto& plan = ctx.forward_plan;

  if (!plan.has_site) {
    plan.attempt_in_flight = false;
    plan.failed_attempts = 0;
    return;
  }

  const bool outpost_exists =
      ctx.outpost_barracks_count > 0 || ctx.outpost_home_count > 0;
  if (outpost_exists) {
    plan.attempt_in_flight = false;
    plan.failed_attempts = 0;
    return;
  }

  if (!plan.attempt_in_flight) {
    return;
  }

  if (ctx.expansion_construction_pending) {
    plan.attempt_deadline =
        snapshot.game_time + AIBaseManager::k_outpost_attempt_timeout;
    return;
  }

  if (snapshot.game_time < plan.attempt_deadline) {
    return;
  }

  plan.attempt_in_flight = false;
  plan.failed_attempts++;

  if (plan.failed_attempts < AIBaseManager::k_max_outpost_failures) {
    return;
  }

  ctx.abandoned_expansion_sites.push_back(
      {plan.site_x, plan.site_z, snapshot.game_time});
  plan.has_site = false;
  plan.failed_attempts = 0;
  plan.abandoned_count++;
  ctx.has_expansion_site = false;
  ctx.expansion_construction_pending = false;
}

void reassign_orphaned_units(AIContext& ctx) {
  ctx.reassigned_units_last_update = 0;

  std::unordered_set<int> live_bases;
  live_bases.reserve(ctx.bases.size());
  for (const auto& base : ctx.bases) {
    live_bases.insert(base.id);
  }

  for (auto it = ctx.assigned_units.begin(); it != ctx.assigned_units.end();) {
    if (it->second.base_id != 0 && !live_bases.contains(it->second.base_id)) {
      it = ctx.assigned_units.erase(it);
      ctx.reassigned_units_last_update++;
    } else {
      ++it;
    }
  }
}

} // namespace

void AIBaseManager::update(const AISnapshot& snapshot, AIContext& ctx) {
  expire_abandoned_sites(ctx, snapshot.game_time);

  const std::vector<AIBase> previous = ctx.bases;
  const auto clusters = build_clusters(snapshot);

  std::vector<AIBase> bases;
  bases.reserve(clusters.size());
  std::vector<bool> matched(previous.size(), false);

  for (const auto& cluster : clusters) {
    int best_previous = -1;
    float best_distance_sq =
        AIBaseManager::k_base_identity_radius * AIBaseManager::k_base_identity_radius;

    for (std::size_t index = 0; index < previous.size(); ++index) {
      if (matched[index]) {
        continue;
      }
      const float distance_sq = distance_squared(cluster.center_x,
                                                 0.0F,
                                                 cluster.center_z,
                                                 previous[index].center_x,
                                                 0.0F,
                                                 previous[index].center_z);
      if (distance_sq <= best_distance_sq) {
        best_distance_sq = distance_sq;
        best_previous = static_cast<int>(index);
      }
    }

    AIBase base;
    Engine::Core::EntityID sticky_primary = 0;
    if (best_previous >= 0) {
      const auto& carried = previous[static_cast<std::size_t>(best_previous)];
      matched[static_cast<std::size_t>(best_previous)] = true;
      base.id = carried.id;
      base.role = carried.role;
      base.last_threat_time = carried.last_threat_time;
      sticky_primary = carried.primary_barracks;
    } else {
      base.id = ctx.next_base_id++;
    }

    fill_base_from_cluster(cluster, sticky_primary, base);
    update_threat(snapshot, base);
    bases.push_back(std::move(base));
  }

  ctx.bases = std::move(bases);

  assign_roles(snapshot, ctx);
  update_forward_plan(snapshot, ctx);
  reassign_orphaned_units(ctx);

  ctx.any_base_under_threat = false;
  for (const auto& base : ctx.bases) {
    if (!base.under_threat) {
      continue;
    }
    ctx.any_base_under_threat = true;
    if (base.barracks_count > 0) {
      ctx.barracks_under_threat = true;
    }
  }

  const AIBase* main = main_base(ctx);
  if (main != nullptr) {
    ctx.primary_barracks = main->primary_barracks;
    ctx.rally_x = main->rally_x;
    ctx.rally_z = main->rally_z;
    ctx.base_pos_x = main->center_x;
    ctx.base_pos_y = 0.0F;
    ctx.base_pos_z = main->center_z;
    ctx.has_base_anchor = true;
    ctx.anchor_is_structural = true;

    for (const auto& entity : snapshot.friendly_units) {
      if (entity.id == main->primary_barracks) {
        ctx.base_pos_x = entity.pos_x;
        ctx.base_pos_y = entity.pos_y;
        ctx.base_pos_z = entity.pos_z;
        break;
      }
    }
  }
}

void AIBaseManager::note_expansion_order(AIContext& ctx,
                                         float game_time,
                                         float site_x,
                                         float site_z) {
  ctx.last_expansion_order_time = game_time;
  ctx.forward_plan.has_site = true;
  ctx.forward_plan.site_x = site_x;
  ctx.forward_plan.site_z = site_z;
  ctx.forward_plan.attempt_in_flight = true;
  ctx.forward_plan.attempt_deadline = game_time + k_outpost_attempt_timeout;
}

auto AIBaseManager::find_base(const AIContext& ctx, int base_id) -> const AIBase* {
  for (const auto& base : ctx.bases) {
    if (base.id == base_id) {
      return &base;
    }
  }
  return nullptr;
}

auto AIBaseManager::main_base(const AIContext& ctx) -> const AIBase* {
  return find_base(ctx, ctx.main_base_id);
}

auto AIBaseManager::base_for_position(const AIContext& ctx,
                                      float x,
                                      float z) -> const AIBase* {
  const AIBase* best = nullptr;
  float best_distance_sq = k_base_cluster_radius * k_base_cluster_radius;

  for (const auto& base : ctx.bases) {
    const float distance_sq =
        distance_squared(x, 0.0F, z, base.center_x, 0.0F, base.center_z);
    if (distance_sq <= best_distance_sq) {
      best_distance_sq = distance_sq;
      best = &base;
    }
  }

  return best;
}

auto AIBaseManager::site_is_abandoned(const AIContext& ctx,
                                      float x,
                                      float z,
                                      float game_time) -> bool {
  const float radius_sq = k_abandoned_site_radius * k_abandoned_site_radius;
  for (const auto& site : ctx.abandoned_expansion_sites) {
    if ((game_time - site.time) > k_abandoned_site_memory) {
      continue;
    }
    if (distance_squared(x, 0.0F, z, site.x, 0.0F, site.z) <= radius_sq) {
      return true;
    }
  }
  return false;
}

} // namespace Game::Systems::AI
