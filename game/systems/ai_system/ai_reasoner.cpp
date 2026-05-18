#include "ai_reasoner.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include "../../core/ownership_constants.h"
#include "../../game_config.h"
#include "../nation_registry.h"
#include "ai_utils.h"
#include "systems/ai_system/ai_types.h"
#include "units/spawn_type.h"

namespace {

struct AnchorCandidate {
  float x = 0.0F;
  float z = 0.0F;
};

constexpr float k_anchor_cluster_radius = 12.0F;
constexpr float k_attack_initiation_aggression_threshold = 0.70F;
constexpr float k_local_threat_memory_duration = 6.0F;
constexpr float k_outpost_site_min_objective_distance = 36.0F;
constexpr float k_outpost_structure_radius = 16.0F;
constexpr float k_outpost_pending_radius = 10.0F;

auto densest_anchor_cluster(const std::vector<AnchorCandidate>& candidates)
    -> std::optional<AnchorCandidate> {
  if (candidates.empty()) {
    return std::nullopt;
  }

  const float cluster_radius_sq = k_anchor_cluster_radius * k_anchor_cluster_radius;
  int best_count = -1;
  float best_distance_sum = std::numeric_limits<float>::infinity();
  AnchorCandidate best_center{};

  for (const auto& candidate : candidates) {
    int cluster_count = 0;
    float sum_x = 0.0F;
    float sum_z = 0.0F;
    float distance_sum = 0.0F;

    for (const auto& other : candidates) {
      const float dx = other.x - candidate.x;
      const float dz = other.z - candidate.z;
      const float distance_sq = dx * dx + dz * dz;
      if (distance_sq > cluster_radius_sq) {
        continue;
      }

      cluster_count++;
      sum_x += other.x;
      sum_z += other.z;
      distance_sum += distance_sq;
    }

    if (cluster_count <= 0) {
      continue;
    }

    if (cluster_count > best_count ||
        (cluster_count == best_count && distance_sum < best_distance_sum)) {
      best_count = cluster_count;
      best_distance_sum = distance_sum;
      const float scale = 1.0F / static_cast<float>(cluster_count);
      best_center = {sum_x * scale, sum_z * scale};
    }
  }

  if (best_count <= 0) {
    return std::nullopt;
  }
  return best_center;
}

auto can_initiate_attack(const Game::Systems::AI::AIStrategyConfig& strategy) -> bool {
  return strategy.aggression_modifier >= k_attack_initiation_aggression_threshold;
}

auto reactive_attack_size(const Game::Systems::AI::AIStrategyConfig& strategy) -> int {
  return std::max(1, strategy.reactive_attack_size);
}

auto proactive_attack_size(const Game::Systems::AI::AIStrategyConfig& strategy) -> int {
  return std::max(strategy.reactive_attack_size, strategy.proactive_attack_size);
}

auto ready_attack_force(const Game::Systems::AI::AIContext& ctx) -> int;
auto committable_attack_force(const Game::Systems::AI::AIContext& ctx) -> int;

auto desired_outpost_barracks_count(const Game::Systems::AI::AIStrategyConfig& strategy)
    -> int {
  return std::max(0, strategy.desired_outpost_barracks_count);
}

auto expansion_force_threshold(const Game::Systems::AI::AIContext& ctx) -> int {
  const int priority_threshold = static_cast<int>(
      std::ceil(4.0F / std::max(0.25F, ctx.strategy_config.expansion_priority)));
  return std::max({2, reactive_attack_size(ctx.strategy_config), priority_threshold});
}

auto needs_outpost_construction(const Game::Systems::AI::AIContext& ctx) -> bool {
  if (!ctx.has_expansion_site) {
    return false;
  }

  if (ctx.outpost_barracks_count < desired_outpost_barracks_count(ctx.strategy_config)) {
    return true;
  }

  return ctx.outpost_barracks_count >= desired_outpost_barracks_count(ctx.strategy_config) &&
         ctx.outpost_home_count < ctx.strategy_config.outpost_home_target;
}

auto select_enemy_expansion_objective(const Game::Systems::AI::AISnapshot& snapshot,
                                      const Game::Systems::AI::AIContext& ctx)
    -> const Game::Systems::AI::ContactSnapshot* {
  const Game::Systems::AI::ContactSnapshot* best = nullptr;
  float best_distance_sq = std::numeric_limits<float>::infinity();

  for (const auto& objective : snapshot.strategic_objectives) {
    if (Game::Core::is_neutral_owner(objective.owner_id)) {
      continue;
    }

    const float distance_sq = Game::Systems::AI::distance_squared(
        objective.pos_x,
        objective.pos_y,
        objective.pos_z,
        ctx.base_pos_x,
        ctx.base_pos_y,
        ctx.base_pos_z);
    if (distance_sq < best_distance_sq) {
      best_distance_sq = distance_sq;
      best = &objective;
    }
  }

  return best;
}

void update_expansion_site(const Game::Systems::AI::AISnapshot& snapshot,
                           Game::Systems::AI::AIContext& ctx,
                           bool had_previous_site,
                           float previous_site_x,
                           float previous_site_z) {
  ctx.has_expansion_site = false;
  ctx.outpost_barracks_count = 0;
  ctx.outpost_home_count = 0;
  ctx.expansion_construction_pending = false;

  if (!ctx.anchor_is_structural || desired_outpost_barracks_count(ctx.strategy_config) <= 0) {
    return;
  }

  if (had_previous_site) {
    ctx.has_expansion_site = true;
    ctx.expansion_site_x = previous_site_x;
    ctx.expansion_site_z = previous_site_z;
  } else {
    const auto* objective = select_enemy_expansion_objective(snapshot, ctx);
    if (objective == nullptr) {
      return;
    }

    const float dx = objective->pos_x - ctx.base_pos_x;
    const float dz = objective->pos_z - ctx.base_pos_z;
    const float objective_distance = std::sqrt(std::max(0.0F, dx * dx + dz * dz));
    if (objective_distance < k_outpost_site_min_objective_distance) {
      return;
    }

    const float site_distance = std::min(ctx.strategy_config.expansion_site_distance,
                                         objective_distance * 0.5F);
    ctx.expansion_site_x = ctx.base_pos_x + (dx / objective_distance) * site_distance;
    ctx.expansion_site_z = ctx.base_pos_z + (dz / objective_distance) * site_distance;
    ctx.has_expansion_site = true;
  }

  const float outpost_radius_sq = k_outpost_structure_radius * k_outpost_structure_radius;
  const float pending_radius_sq = k_outpost_pending_radius * k_outpost_pending_radius;

  for (const auto& entity : snapshot.friendly_units) {
    const float site_distance_sq = Game::Systems::AI::distance_squared(entity.pos_x,
                                                                       entity.pos_y,
                                                                       entity.pos_z,
                                                                       ctx.expansion_site_x,
                                                                       0.0F,
                                                                       ctx.expansion_site_z);

    if (entity.is_building && site_distance_sq <= outpost_radius_sq) {
      if (entity.spawn_type == Game::Units::SpawnType::Barracks) {
        ctx.outpost_barracks_count++;
      } else if (entity.spawn_type == Game::Units::SpawnType::Home) {
        ctx.outpost_home_count++;
      }
    }

    if (entity.spawn_type == Game::Units::SpawnType::Builder &&
        entity.builder_production.has_component &&
        entity.builder_production.has_construction_site) {
      const float pending_distance_sq = Game::Systems::AI::distance_squared(
          entity.builder_production.construction_site_x,
          0.0F,
          entity.builder_production.construction_site_z,
          ctx.expansion_site_x,
          0.0F,
          ctx.expansion_site_z);
      if (pending_distance_sq <= pending_radius_sq) {
        ctx.expansion_construction_pending = true;
      }
    }
  }
}

auto can_capture_neutral_expansion(const Game::Systems::AI::AIContext& ctx) -> bool {
  return ctx.strategy_config.expansion_priority > 0.8F && ctx.neutral_barracks_count > 0 &&
         committable_attack_force(ctx) >= expansion_force_threshold(ctx);
}

auto can_build_outpost_expansion(const Game::Systems::AI::AIContext& ctx) -> bool {
  if (ctx.strategy_config.expansion_priority <= 0.8F || !ctx.has_expansion_site ||
      ctx.home_count < ctx.strategy_config.base_home_target || ctx.barracks_count == 0 ||
      ctx.builder_count == 0) {
    return false;
  }

  if (!(needs_outpost_construction(ctx) || ctx.expansion_construction_pending)) {
    return false;
  }

  return committable_attack_force(ctx) >= expansion_force_threshold(ctx);
}

auto wants_expansion(const Game::Systems::AI::AIContext& ctx) -> bool {
  return can_capture_neutral_expansion(ctx) || can_build_outpost_expansion(ctx);
}

auto compute_macro_targets(const Game::Systems::AI::AIContext& ctx,
                           int catapult_count) -> Game::Systems::AI::AIContext::MacroTargets {
  Game::Systems::AI::AIContext::MacroTargets targets;
  targets.builder_count = ctx.strategy_config.target_builder_count;
  targets.barracks_count = ctx.strategy_config.desired_barracks_count;
  targets.defense_tower_count = ctx.strategy_config.desired_defense_tower_count;
  targets.catapult_count = ctx.strategy_config.desired_catapult_count;
  targets.assembly_size = std::max(ctx.strategy_config.desired_assembly_size,
                                   proactive_attack_size(ctx.strategy_config));
  targets.assembly_radius = ctx.strategy_config.assembly_radius;
  targets.gather_spacing = ctx.strategy_config.gather_spacing;

  const int troop_pressure = std::max(0, ctx.total_units - targets.builder_count);
  const int home_growth = troop_pressure / 6;
  const int extra_barracks = troop_pressure / 10;
  const int extra_catapults = std::max(0, ctx.barracks_count - 1) / 2;

  targets.home_count =
      std::max(ctx.strategy_config.base_home_target,
               2 + home_growth + std::min(2, extra_barracks));
  targets.barracks_count =
      std::max(targets.barracks_count, 1 + extra_barracks);
  targets.defense_tower_count =
      std::max(targets.defense_tower_count,
               (ctx.home_count >= 4 || ctx.barracks_under_threat ||
                !ctx.buildings_under_attack.empty())
                   ? 2
                   : targets.defense_tower_count);
  targets.catapult_count =
      std::max(targets.catapult_count, std::min(3, extra_catapults));

  if (ctx.primary_barracks == 0 && ctx.home_count < 2) {
    targets.barracks_count = std::max(targets.barracks_count, 1);
  }
  if (catapult_count >= targets.catapult_count) {
    targets.catapult_count = catapult_count;
  }

  return targets;
}

auto available_combat_role_count(const Game::Systems::AI::AISnapshot& snapshot) -> int {
  int count = 0;
  for (const auto& entity : snapshot.friendly_units) {
    if (Game::Systems::AI::is_combat_role_unit(entity)) {
      count++;
    }
  }
  return count;
}

auto compute_effective_reserve_units(const Game::Systems::AI::AISnapshot& snapshot,
                                     const Game::Systems::AI::AIContext& ctx) -> int {
  if (!ctx.anchor_is_structural) {
    return 0;
  }

  const int combat_units = available_combat_role_count(snapshot);
  const int max_reserve =
      std::max(0, combat_units - reactive_attack_size(ctx.strategy_config));
  return std::min(ctx.strategy_config.reserve_units, max_reserve);
}

auto compute_effective_harass_units(const Game::Systems::AI::AISnapshot& snapshot,
                                    const Game::Systems::AI::AIContext& ctx) -> int {
  if (ctx.strategy_config.harass_units <= 0 || ctx.strategy_config.harassment_range <= 0.0F) {
    return 0;
  }

  const int combat_units = available_combat_role_count(snapshot);
  const int max_harass = std::max(0,
                                  combat_units - ctx.effective_reserve_units -
                                      reactive_attack_size(ctx.strategy_config));
  return std::min(ctx.strategy_config.harass_units, max_harass);
}

void update_reserve_unit_ids(const Game::Systems::AI::AISnapshot& snapshot,
                             Game::Systems::AI::AIContext& ctx) {
  ctx.effective_reserve_units = compute_effective_reserve_units(snapshot, ctx);
  if (ctx.effective_reserve_units <= 0) {
    ctx.reserve_unit_ids.clear();
    return;
  }

  std::vector<Engine::Core::EntityID> surviving_reserve;
  surviving_reserve.reserve(static_cast<std::size_t>(ctx.effective_reserve_units));
  for (auto unit_id : ctx.reserve_unit_ids) {
    auto it = std::find_if(snapshot.friendly_units.begin(),
                           snapshot.friendly_units.end(),
                           [unit_id](const Game::Systems::AI::EntitySnapshot& entity) {
                             return entity.id == unit_id &&
                                    Game::Systems::AI::is_combat_role_unit(entity);
                           });
    if (it != snapshot.friendly_units.end()) {
      surviving_reserve.push_back(unit_id);
    }
    if (static_cast<int>(surviving_reserve.size()) >= ctx.effective_reserve_units) {
      break;
    }
  }

  std::vector<const Game::Systems::AI::EntitySnapshot*> candidates;
  candidates.reserve(snapshot.friendly_units.size());
  for (const auto& entity : snapshot.friendly_units) {
    if (!Game::Systems::AI::is_combat_role_unit(entity)) {
      continue;
    }
    if (std::find(surviving_reserve.begin(), surviving_reserve.end(), entity.id) !=
        surviving_reserve.end()) {
      continue;
    }
    candidates.push_back(&entity);
  }

  std::sort(candidates.begin(),
            candidates.end(),
            [&ctx](const Game::Systems::AI::EntitySnapshot* a,
                   const Game::Systems::AI::EntitySnapshot* b) {
              const float distance_a = Game::Systems::AI::distance_squared(
                  a->pos_x, a->pos_y, a->pos_z, ctx.base_pos_x, ctx.base_pos_y, ctx.base_pos_z);
              const float distance_b = Game::Systems::AI::distance_squared(
                  b->pos_x, b->pos_y, b->pos_z, ctx.base_pos_x, ctx.base_pos_y, ctx.base_pos_z);
              if (distance_a != distance_b) {
                return distance_a < distance_b;
              }
              return a->id < b->id;
            });

  for (const auto* entity : candidates) {
    if (static_cast<int>(surviving_reserve.size()) >= ctx.effective_reserve_units) {
      break;
    }
    surviving_reserve.push_back(entity->id);
  }

  ctx.reserve_unit_ids = std::move(surviving_reserve);
}

void update_harass_unit_ids(const Game::Systems::AI::AISnapshot& snapshot,
                            Game::Systems::AI::AIContext& ctx) {
  ctx.effective_harass_units = compute_effective_harass_units(snapshot, ctx);
  if (ctx.effective_harass_units <= 0) {
    ctx.harass_unit_ids.clear();
    return;
  }

  std::vector<Engine::Core::EntityID> surviving_harass;
  surviving_harass.reserve(static_cast<std::size_t>(ctx.effective_harass_units));
  for (auto unit_id : ctx.harass_unit_ids) {
    auto it = std::find_if(snapshot.friendly_units.begin(),
                           snapshot.friendly_units.end(),
                           [&](const Game::Systems::AI::EntitySnapshot& entity) {
                             return entity.id == unit_id &&
                                    Game::Systems::AI::is_combat_role_unit(entity) &&
                                    !Game::Systems::AI::is_reserved_unit(entity.id, ctx);
                           });
    if (it != snapshot.friendly_units.end()) {
      surviving_harass.push_back(unit_id);
    }
    if (static_cast<int>(surviving_harass.size()) >= ctx.effective_harass_units) {
      break;
    }
  }

  std::vector<const Game::Systems::AI::EntitySnapshot*> candidates;
  candidates.reserve(snapshot.friendly_units.size());
  for (const auto& entity : snapshot.friendly_units) {
    if (!Game::Systems::AI::is_combat_role_unit(entity) ||
        Game::Systems::AI::is_reserved_unit(entity.id, ctx)) {
      continue;
    }
    if (std::find(surviving_harass.begin(), surviving_harass.end(), entity.id) !=
        surviving_harass.end()) {
      continue;
    }
    candidates.push_back(&entity);
  }

  const float assembly_radius_sq =
      ctx.macro_targets.assembly_radius * ctx.macro_targets.assembly_radius;
  std::sort(candidates.begin(),
            candidates.end(),
            [&ctx, assembly_radius_sq](const Game::Systems::AI::EntitySnapshot* a,
                                       const Game::Systems::AI::EntitySnapshot* b) {
              const float rally_distance_a = Game::Systems::AI::distance_squared(
                  a->pos_x, a->pos_y, a->pos_z, ctx.rally_x, 0.0F, ctx.rally_z);
              const float rally_distance_b = Game::Systems::AI::distance_squared(
                  b->pos_x, b->pos_y, b->pos_z, ctx.rally_x, 0.0F, ctx.rally_z);
              const bool a_outside_assembly = rally_distance_a > assembly_radius_sq;
              const bool b_outside_assembly = rally_distance_b > assembly_radius_sq;
              if (a_outside_assembly != b_outside_assembly) {
                return a_outside_assembly > b_outside_assembly;
              }

              const float distance_a = Game::Systems::AI::distance_squared(
                  a->pos_x, a->pos_y, a->pos_z, ctx.base_pos_x, ctx.base_pos_y, ctx.base_pos_z);
              const float distance_b = Game::Systems::AI::distance_squared(
                  b->pos_x, b->pos_y, b->pos_z, ctx.base_pos_x, ctx.base_pos_y, ctx.base_pos_z);
              if (distance_a != distance_b) {
                return distance_a > distance_b;
              }
              return a->id < b->id;
            });

  for (const auto* entity : candidates) {
    if (static_cast<int>(surviving_harass.size()) >= ctx.effective_harass_units) {
      break;
    }
    surviving_harass.push_back(entity->id);
  }

  ctx.harass_unit_ids = std::move(surviving_harass);
}

auto committed_army_count(const Game::Systems::AI::AISnapshot& snapshot,
                          const Game::Systems::AI::AIContext& ctx) -> int {
  if (!ctx.has_base_anchor) {
    return 0;
  }

  if (!ctx.anchor_is_structural) {
    return std::max(0, ctx.total_units - ctx.builder_count);
  }

  const float assembly_radius_sq =
      ctx.macro_targets.assembly_radius * ctx.macro_targets.assembly_radius;
  int committed_units = 0;
  for (const auto& entity : snapshot.friendly_units) {
    if (entity.is_building || entity.spawn_type == Game::Units::SpawnType::Builder) {
      continue;
    }
    if (Game::Systems::AI::is_harass_unit(entity.id, ctx)) {
      continue;
    }

    const float dx = entity.pos_x - ctx.rally_x;
    const float dz = entity.pos_z - ctx.rally_z;
    const float dist_sq = dx * dx + dz * dz;
    if (dist_sq <= assembly_radius_sq ||
        Game::Systems::AI::is_entity_engaged(entity, snapshot.visible_enemies)) {
      committed_units++;
    }
  }

  return committed_units;
}

auto ready_attack_force(const Game::Systems::AI::AIContext& ctx) -> int {
  return ctx.anchor_is_structural ? ctx.assembled_unit_count
                                  : std::max(0,
                                             ctx.total_units - ctx.builder_count -
                                                 ctx.effective_harass_units);
}

auto committable_attack_force(const Game::Systems::AI::AIContext& ctx) -> int {
  return std::max(0, ready_attack_force(ctx) - ctx.effective_reserve_units);
}

auto resume_attack_health_threshold(const Game::Systems::AI::AIStrategyConfig& strategy)
    -> float {
  return std::clamp(0.65F + 0.10F * (strategy.defense_modifier - 1.0F), 0.60F, 0.90F);
}

auto return_to_idle_health_threshold(
    const Game::Systems::AI::AIStrategyConfig& strategy) -> float {
  return std::clamp(0.80F + 0.05F * (strategy.defense_modifier - 1.0F), 0.75F, 0.95F);
}

auto has_active_local_threat(const Game::Systems::AI::AIContext& ctx) -> bool {
  return ctx.barracks_under_threat || !ctx.buildings_under_attack.empty() ||
         (ctx.nearby_threat_count > 0);
}

auto has_recent_local_threat(const Game::Systems::AI::AIContext& ctx, float game_time)
    -> bool {
  return has_active_local_threat(ctx) ||
         ((ctx.last_local_threat_time > 0.0F) &&
          ((game_time - ctx.last_local_threat_time) <= k_local_threat_memory_duration));
}

} // namespace

namespace Game::Systems::AI {

void AIReasoner::update_context(const AISnapshot& snapshot, AIContext& ctx) {

  if (ctx.nation == nullptr) {
    ctx.nation =
        Game::Systems::NationRegistry::instance().get_nation_for_player(ctx.player_id);
  }

  const auto alive_ids = cleanup_dead_units(snapshot, ctx, snapshot.game_time);

  int previous_unit_count = ctx.total_units;
  const Engine::Core::EntityID previous_primary_barracks = ctx.primary_barracks;
  const bool had_previous_site = ctx.has_expansion_site;
  const float previous_site_x = ctx.expansion_site_x;
  const float previous_site_z = ctx.expansion_site_z;

  ctx.military_units.clear();
  ctx.buildings.clear();
  ctx.commander_ids.clear();
  ctx.primary_barracks = 0;
  ctx.total_units = 0;
  ctx.idle_units = 0;
  ctx.combat_units = 0;
  ctx.melee_count = 0;
  ctx.ranged_count = 0;
  ctx.builder_count = 0;
  ctx.damaged_units_count = 0;
  ctx.average_health = 1.0F;
  ctx.rally_x = 0.0F;
  ctx.rally_z = 0.0F;
  ctx.barracks_under_threat = false;
  ctx.nearby_threat_count = 0;
  ctx.closest_threat_distance = std::numeric_limits<float>::infinity();
  ctx.base_pos_x = 0.0F;
  ctx.base_pos_y = 0.0F;
  ctx.base_pos_z = 0.0F;
  ctx.has_base_anchor = false;
  ctx.anchor_is_structural = false;
  ctx.has_expansion_site = false;
  ctx.expansion_site_x = 0.0F;
  ctx.expansion_site_z = 0.0F;
  ctx.visible_enemy_count = 0;
  ctx.enemy_buildings_count = 0;
  ctx.neutral_barracks_count = 0;
  ctx.average_enemy_distance = 0.0F;
  ctx.home_count = 0;
  ctx.defense_tower_count = 0;
  ctx.barracks_count = 0;
  ctx.assembled_unit_count = 0;
  ctx.effective_reserve_units = 0;
  ctx.effective_harass_units = 0;
  ctx.outpost_barracks_count = 0;
  ctx.outpost_home_count = 0;
  ctx.expansion_construction_pending = false;
  ctx.max_troops_per_player = Game::GameConfig::instance().get_max_troops_per_player();

  constexpr float attack_record_timeout = 10.0F;
  auto it = ctx.buildings_under_attack.begin();
  while (it != ctx.buildings_under_attack.end()) {
    if (alive_ids.find(it->first) == alive_ids.end() ||
        (snapshot.game_time - it->second) > attack_record_timeout) {
      it = ctx.buildings_under_attack.erase(it);
    } else {
      ++it;
    }
  }

  float total_health_ratio = 0.0F;
  const EntitySnapshot* sticky_primary_barracks = nullptr;
  const EntitySnapshot* fallback_primary_barracks = nullptr;

  for (const auto& entity : snapshot.friendly_units) {
    if (entity.is_building) {
      ctx.buildings.push_back(entity.id);

      if (entity.spawn_type == Game::Units::SpawnType::Home) {
        ctx.home_count++;
      } else if (entity.spawn_type == Game::Units::SpawnType::DefenseTower) {
        ctx.defense_tower_count++;
      } else if (entity.spawn_type == Game::Units::SpawnType::Barracks) {
        ctx.barracks_count++;
      }

      if (entity.spawn_type == Game::Units::SpawnType::Barracks) {
        if (entity.id == previous_primary_barracks) {
          sticky_primary_barracks = &entity;
        }
        if (fallback_primary_barracks == nullptr ||
            entity.id < fallback_primary_barracks->id) {
          fallback_primary_barracks = &entity;
        }
      }
      continue;
    }

    ctx.military_units.push_back(entity.id);
    ctx.total_units++;

    if (entity.is_commander) {
      ctx.commander_ids.push_back(entity.id);
    }

    if (entity.spawn_type == Game::Units::SpawnType::Builder) {
      ctx.builder_count++;
    }

    if (ctx.nation != nullptr) {
      auto troop_type_opt = Game::Units::spawn_typeToTroopType(entity.spawn_type);
      if (troop_type_opt) {
        auto troop_type = *troop_type_opt;
        if (troop_type == Game::Units::TroopType::Builder) {
        } else if (ctx.nation->is_ranged_unit(troop_type)) {
          ctx.ranged_count++;
        } else if (ctx.nation->is_melee_unit(troop_type)) {
          ctx.melee_count++;
        }
      }
    }

    if (!entity.movement.has_component || !entity.movement.has_target) {
      ctx.idle_units++;
    } else {
      ctx.combat_units++;
    }

    if (entity.max_health > 0) {
      float const health_ratio =
          static_cast<float>(entity.health) / static_cast<float>(entity.max_health);
      total_health_ratio += health_ratio;

      if (health_ratio < 0.5F) {
        ctx.damaged_units_count++;
      }
    }
  }

  const EntitySnapshot* primary_barracks_snapshot =
      (sticky_primary_barracks != nullptr) ? sticky_primary_barracks
                                           : fallback_primary_barracks;
  if (primary_barracks_snapshot != nullptr) {
    ctx.primary_barracks = primary_barracks_snapshot->id;
    ctx.rally_x = primary_barracks_snapshot->pos_x - 5.0F;
    ctx.rally_z = primary_barracks_snapshot->pos_z;
    ctx.base_pos_x = primary_barracks_snapshot->pos_x;
    ctx.base_pos_y = primary_barracks_snapshot->pos_y;
    ctx.base_pos_z = primary_barracks_snapshot->pos_z;
    ctx.has_base_anchor = true;
    ctx.anchor_is_structural = true;
  }

  if (!ctx.has_base_anchor) {
    std::vector<AnchorCandidate> unit_positions;
    unit_positions.reserve(snapshot.friendly_units.size());
    for (const auto& entity : snapshot.friendly_units) {
      if (entity.is_building || entity.spawn_type == Game::Units::SpawnType::Builder) {
        continue;
      }
      unit_positions.push_back({entity.pos_x, entity.pos_z});
    }

    if (const auto anchor = densest_anchor_cluster(unit_positions);
        anchor.has_value()) {
      ctx.base_pos_x = anchor->x;
      ctx.base_pos_y = 0.0F;
      ctx.base_pos_z = anchor->z;
      ctx.rally_x = anchor->x - 5.0F;
      ctx.rally_z = anchor->z;
      ctx.has_base_anchor = true;
    }
  }

  update_expansion_site(snapshot, ctx, had_previous_site, previous_site_x, previous_site_z);

  int catapult_count = 0;
  for (const auto& entity : snapshot.friendly_units) {
    if (!entity.is_building && entity.spawn_type == Game::Units::SpawnType::Catapult) {
      catapult_count++;
    }
  }

  ctx.macro_targets = compute_macro_targets(ctx, catapult_count);
  update_reserve_unit_ids(snapshot, ctx);
  update_harass_unit_ids(snapshot, ctx);
  ctx.assembled_unit_count = committed_army_count(snapshot, ctx);

  ctx.average_health = (ctx.total_units > 0)
                           ? (total_health_ratio / static_cast<float>(ctx.total_units))
                           : 1.0F;

  ctx.visible_enemy_count = static_cast<int>(snapshot.visible_enemies.size());
  float total_enemy_dist = 0.0F;

  for (const auto& enemy : snapshot.visible_enemies) {
    if (enemy.is_building) {
      ctx.enemy_buildings_count++;

      if (enemy.spawn_type == Game::Units::SpawnType::Barracks &&
          Game::Core::is_neutral_owner(enemy.owner_id)) {
        ctx.neutral_barracks_count++;
      }
    }

    if (ctx.has_base_anchor) {
      float const dist = distance(enemy.pos_x,
                                  enemy.pos_y,
                                  enemy.pos_z,
                                  ctx.base_pos_x,
                                  ctx.base_pos_y,
                                  ctx.base_pos_z);
      total_enemy_dist += dist;
    }
  }

  ctx.average_enemy_distance =
      (ctx.visible_enemy_count > 0)
          ? (total_enemy_dist / static_cast<float>(ctx.visible_enemy_count))
          : 1000.0F;

  if (ctx.has_base_anchor) {

    constexpr float k_base_defend_radius = 30.0F;
    const float defend_radius =
        k_base_defend_radius +
        10.0F * std::min(2.0F, ctx.strategy_config.defense_modifier);
    const float defend_radius_sq = defend_radius * defend_radius;

    for (const auto& enemy : snapshot.visible_enemies) {
      float const dist_sq = distance_squared(enemy.pos_x,
                                             enemy.pos_y,
                                             enemy.pos_z,
                                             ctx.base_pos_x,
                                             ctx.base_pos_y,
                                             ctx.base_pos_z);

      if (dist_sq <= defend_radius_sq) {
        ctx.nearby_threat_count++;
        if (ctx.primary_barracks != 0) {
          ctx.barracks_under_threat = true;
        }

        float const dist = std::sqrt(std::max(dist_sq, 0.0F));
        ctx.closest_threat_distance = std::min(ctx.closest_threat_distance, dist);
      }
    }

    if (!ctx.barracks_under_threat) {
      ctx.closest_threat_distance = std::numeric_limits<float>::infinity();
    }
  }

  if (has_active_local_threat(ctx)) {
    ctx.last_local_threat_time = snapshot.game_time;
  }

  if (ctx.total_units != previous_unit_count || ctx.combat_units > 0) {

    ctx.consecutive_no_progress_cycles = 0;
    ctx.last_meaningful_action_time = snapshot.game_time;
  } else if (ctx.idle_units > 0 || ctx.visible_enemy_count > 0) {

    ctx.consecutive_no_progress_cycles++;
  }

  if (ctx.last_meaningful_action_time == 0.0F) {
    ctx.last_meaningful_action_time = snapshot.game_time;
  }

  ctx.last_total_units = ctx.total_units;
}

void AIReasoner::update_state_machine(const AISnapshot& snapshot,
                                      AIContext& ctx,
                                      float delta_time) {
  ctx.state_timer += delta_time;
  ctx.decision_timer += delta_time;

  constexpr float min_state_duration = 3.0F;
  constexpr float max_no_progress_duration = 3.0F;

  bool deadlock_detected = false;

  if (ctx.state_timer > ctx.max_state_duration) {
    deadlock_detected = true;
  }

  float time_since_progress = snapshot.game_time - ctx.last_meaningful_action_time;
  if (time_since_progress >= max_no_progress_duration && ctx.idle_units > 0) {
    deadlock_detected = true;
  }

  AIState previous_state = ctx.state;

  if (has_active_local_threat(ctx) && ctx.state != AIState::Defending) {

    ctx.state = AIState::Defending;
  }

  else if ((ctx.nearby_threat_count > 0) &&
           (ctx.state == AIState::Gathering || ctx.state == AIState::Idle)) {
    ctx.state = AIState::Defending;
  }

  if (deadlock_detected && ctx.state != AIState::Defending) {

     if (ctx.state == AIState::Idle && ctx.total_units > 0) {
       ctx.state = AIState::Gathering;
     } else if (ctx.state == AIState::Gathering) {
       if (ctx.visible_enemy_count > 0 && can_initiate_attack(ctx.strategy_config) &&
           committable_attack_force(ctx) >= reactive_attack_size(ctx.strategy_config)) {
         ctx.state = AIState::Attacking;
       } else if (ctx.visible_enemy_count == 0) {
         ctx.state = AIState::Idle;
      }
    } else if (ctx.state == AIState::Attacking) {

      ctx.assigned_units.clear();
      if (ctx.average_health < 0.5F) {
        ctx.state = AIState::Defending;
      } else {
        ctx.state = AIState::Idle;
      }
    }
    ctx.consecutive_no_progress_cycles = 0;
    ctx.debug_info.deadlock_recoveries++;
  }

  if (ctx.decision_timer < 2.0F) {
    if (ctx.state != previous_state) {
      ctx.state_timer = 0.0F;
      ctx.debug_info.state_transitions++;
    }
    return;
  }

  ctx.decision_timer = 0.0F;
  ctx.debug_info.total_decisions_made++;
  previous_state = ctx.state;

  if (ctx.state_timer < min_state_duration &&
      ((!has_active_local_threat(ctx)) ||
        ctx.state == AIState::Defending)) {
    return;
  }

  switch (ctx.state) {
  case AIState::Idle:
    if (ctx.idle_units >= 1) {

      ctx.state = AIState::Gathering;
    } else if (ctx.average_health < (0.40F * ctx.strategy_config.defense_modifier) &&
               ctx.total_units > 0) {

      ctx.state = AIState::Defending;
    } else if (wants_expansion(ctx)) {

      ctx.state = AIState::Expanding;
     } else if (ctx.total_units >= 1 && ctx.visible_enemy_count > 0) {

       if (can_initiate_attack(ctx.strategy_config) &&
           committable_attack_force(ctx) >= reactive_attack_size(ctx.strategy_config)) {
         ctx.state = AIState::Attacking;
       }
     }
    break;

  case AIState::Gathering: {

     const auto& strategy = ctx.strategy_config;

     const int MIN_UNITS_FOR_REACTIVE_ATTACK = reactive_attack_size(strategy);
     const int MIN_UNITS_FOR_PROACTIVE_ATTACK = proactive_attack_size(strategy);
    if (ctx.total_units < 1) {
      ctx.state = AIState::Idle;
    } else if (ctx.average_health < (0.40F * strategy.defense_modifier)) {

      ctx.state = AIState::Defending;
    } else if (wants_expansion(ctx)) {

      ctx.state = AIState::Expanding;
      } else if (ctx.visible_enemy_count > 0 && can_initiate_attack(strategy) &&
                committable_attack_force(ctx) >= MIN_UNITS_FOR_REACTIVE_ATTACK) {

       ctx.state = AIState::Attacking;
     } else if (committable_attack_force(ctx) >=
                    std::max(MIN_UNITS_FOR_PROACTIVE_ATTACK,
                             ctx.macro_targets.assembly_size) &&
                can_initiate_attack(strategy)) {

       ctx.state = AIState::Attacking;
    }
  } break;

  case AIState::Attacking:
    if (ctx.average_health < ctx.strategy_config.retreat_threshold) {

      ctx.state = AIState::Retreating;
    } else if (ctx.total_units == 0) {

      ctx.state = AIState::Idle;
    } else if (ctx.visible_enemy_count == 0 && ctx.state_timer > 15.0F) {

      ctx.state = AIState::Idle;
    } else if (ctx.average_health < (0.50F * ctx.strategy_config.defense_modifier) &&
               ctx.damaged_units_count * 2 > ctx.total_units) {

      if (!ctx.barracks_under_threat) {
        ctx.state = AIState::Defending;
      }
    }

    break;

  case AIState::Defending:

     if (has_recent_local_threat(ctx, snapshot.game_time)) {

     } else if (can_initiate_attack(ctx.strategy_config) &&
                committable_attack_force(ctx) >=
                    std::max(proactive_attack_size(ctx.strategy_config),
                             ctx.macro_targets.assembly_size) &&
                ctx.average_health >
                     resume_attack_health_threshold(ctx.strategy_config)) {

      ctx.state = AIState::Attacking;
    } else if (ctx.total_units < 2) {

      ctx.state = AIState::Idle;
    } else if (ctx.visible_enemy_count > 0) {
      ctx.state = AIState::Gathering;
    } else if (ctx.average_health >
               return_to_idle_health_threshold(ctx.strategy_config)) {
      ctx.state = AIState::Idle;
    } else {
      ctx.state = AIState::Gathering;
    }
    break;

  case AIState::Retreating:

    if (ctx.state_timer > 6.0F && ctx.average_health > 0.55F) {

      ctx.state = AIState::Defending;
    } else if (ctx.state_timer > 12.0F) {

      ctx.state = AIState::Idle;
      ctx.assigned_units.clear();
    } else if (ctx.average_health > 0.70F && ctx.state_timer > 3.0F) {

      ctx.state = AIState::Defending;
    }
    break;

  case AIState::Expanding:

    if (!wants_expansion(ctx)) {

      if (ctx.visible_enemy_count > 0) {
        ctx.state = AIState::Attacking;
      } else {
        ctx.state = AIState::Gathering;
      }
    } else if (ctx.total_units < 2) {

      ctx.state = AIState::Gathering;
    } else if (ctx.barracks_under_threat || !ctx.buildings_under_attack.empty()) {

      ctx.state = AIState::Defending;
    } else if (ctx.average_health < 0.40F) {

      ctx.state = AIState::Defending;
    }
    break;
  }

  if (ctx.state != previous_state) {
    ctx.state_timer = 0.0F;
    if (previous_state == AIState::Defending && ctx.state != AIState::Defending) {
      ctx.assigned_units.clear();
    }
    if (ctx.state == AIState::Defending || ctx.state == AIState::Retreating) {
      release_units(ctx.harass_unit_ids, ctx);
    }
    ctx.consecutive_no_progress_cycles = 0;
    ctx.debug_info.state_transitions++;
  }
}

void AIReasoner::validate_state(AIContext& ctx) {

  constexpr size_t MAX_ASSIGNMENT_MULTIPLIER = 2;
  constexpr int MAX_NO_PROGRESS_CYCLES = 50;
  constexpr float MAX_STATE_TIMER = 1000.0F;
  constexpr float MAX_DECISION_TIMER = 100.0F;

  if (ctx.total_units == 0 && ctx.state != AIState::Idle) {
    ctx.state = AIState::Idle;
    ctx.state_timer = 0.0F;
    ctx.consecutive_no_progress_cycles = 0;
  }

  if (ctx.primary_barracks == 0 && ctx.buildings.empty()) {
    if (ctx.state == AIState::Defending && !ctx.barracks_under_threat) {
      ctx.state = AIState::Idle;
      ctx.state_timer = 0.0F;
    }
  }

  if (ctx.state_timer > MAX_STATE_TIMER) {
    ctx.state_timer = ctx.max_state_duration;
  }
  if (ctx.decision_timer > MAX_DECISION_TIMER) {
    ctx.decision_timer = 0.0F;
  }

  size_t max_expected_assignments =
      static_cast<size_t>(ctx.total_units) * MAX_ASSIGNMENT_MULTIPLIER;
  if (ctx.assigned_units.size() > max_expected_assignments) {

    ctx.assigned_units.clear();
  }

  if (ctx.consecutive_no_progress_cycles > MAX_NO_PROGRESS_CYCLES) {
    ctx.consecutive_no_progress_cycles = 0;
    ctx.state = AIState::Idle;
    ctx.assigned_units.clear();
  }
}

} // namespace Game::Systems::AI
