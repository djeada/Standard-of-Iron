#include "ai_context_census.h"

#include <algorithm>

#include "../../core/ownership_constants.h"
#include "../../units/troop_config.h"
#include "../nation_registry.h"
#include "ai_stall_recovery.h"
#include "ai_utils.h"

namespace Game::Systems::AI {

namespace {

constexpr float k_attack_record_timeout = 10.0F;
constexpr float k_base_defend_radius = 30.0F;

void tally_building_type(const EntitySnapshot& entity, AIContext& ctx) {
  switch (entity.spawn_type) {
  case Game::Units::SpawnType::Home:
    ctx.home_count++;
    if (entity.production.has_component) {
      ctx.home_civilians_remaining += std::max(
          0,
          entity.production.max_units - entity.production.produced_count -
              entity.production.queue_size - (entity.production.in_progress ? 1 : 0));
    }
    break;
  case Game::Units::SpawnType::DefenseTower:
    ctx.defense_tower_count++;
    break;
  case Game::Units::SpawnType::WallSegment:
    ctx.wall_segment_count++;
    break;
  case Game::Units::SpawnType::Barracks:
    ctx.barracks_count++;
    break;
  case Game::Units::SpawnType::Marketplace:
    ctx.marketplace_count++;
    break;
  case Game::Units::SpawnType::Farm:
    ctx.farm_count++;
    break;
  default:
    break;
  }
}

void tally_building(const EntitySnapshot& entity,
                    AIContext& ctx,
                    Engine::Core::EntityID previous_primary_barracks,
                    const EntitySnapshot*& sticky_primary,
                    const EntitySnapshot*& fallback_primary) {
  ctx.buildings.push_back(entity.id);
  tally_building_type(entity, ctx);

  if (entity.spawn_type != Game::Units::SpawnType::Barracks) {
    return;
  }
  if (entity.production.has_component) {
    ctx.recruitment_manpower_available += entity.production.manpower_available;
  }
  if (entity.id == previous_primary_barracks) {
    sticky_primary = &entity;
  }
  if (fallback_primary == nullptr || entity.id < fallback_primary->id) {
    fallback_primary = &entity;
  }
}

void tally_combat_role(const EntitySnapshot& entity, AIContext& ctx) {
  if (ctx.nation == nullptr) {
    return;
  }
  auto troop_type_opt = Game::Units::spawn_typeToTroopType(entity.spawn_type);
  if (!troop_type_opt) {
    return;
  }
  auto troop_type = *troop_type_opt;
  if (troop_type == Game::Units::TroopType::Builder) {
    return;
  }
  if (Game::Units::is_cavalry(entity.spawn_type)) {
    ctx.cavalry_count++;
  } else if (entity.spawn_type == Game::Units::SpawnType::Catapult ||
             entity.spawn_type == Game::Units::SpawnType::Ballista) {
    ctx.siege_count++;
  } else if (ctx.nation->is_ranged_unit(troop_type)) {
    ctx.ranged_count++;
  } else if (ctx.nation->is_melee_unit(troop_type)) {
    ctx.melee_count++;
  }
}

void tally_unit(const EntitySnapshot& entity,
                const AISnapshot& snapshot,
                AIContext& ctx,
                float& total_health_ratio) {
  ctx.total_units++;
  ctx.population_used +=
      Game::Units::TroopConfig::instance().get_population_cost(entity.spawn_type);

  if (entity.is_commander) {
    ctx.commander_ids.push_back(entity.id);
  }

  if (entity.spawn_type == Game::Units::SpawnType::Builder) {
    ctx.builder_count++;
  }

  if (entity.spawn_type == Game::Units::SpawnType::Civilian) {
    ctx.civilian_count++;
  }

  tally_combat_role(entity, ctx);

  if (!entity.movement.has_component || !entity.movement.has_target ||
      is_going_nowhere(entity) || is_stood_down(entity.id, ctx, snapshot.game_time)) {
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

} // namespace

void reset_context_counters(const AISnapshot& snapshot, AIContext& ctx) {
  ctx.buildings.clear();
  ctx.commander_ids.clear();
  ctx.primary_barracks = 0;
  ctx.total_units = 0;
  ctx.idle_units = 0;
  ctx.combat_units = 0;
  ctx.melee_count = 0;
  ctx.ranged_count = 0;
  ctx.cavalry_count = 0;
  ctx.siege_count = 0;
  ctx.builder_count = 0;
  ctx.civilian_count = 0;
  ctx.damaged_units_count = 0;
  ctx.average_health = 1.0F;
  ctx.anchor_station.offered = false;
  ctx.anchor_station.x = 0.0F;
  ctx.anchor_station.z = 0.0F;
  ctx.barracks_under_threat = false;
  ctx.nearby_threat_count = 0;
  ctx.base_pos_x = 0.0F;
  ctx.base_pos_y = 0.0F;
  ctx.base_pos_z = 0.0F;
  ctx.has_base_anchor = false;
  ctx.anchor_is_structural = false;
  ctx.has_expansion_site = false;
  ctx.expansion_site_x = 0.0F;
  ctx.expansion_site_z = 0.0F;
  ctx.visible_enemy_count = 0;
  ctx.neutral_barracks_count = 0;
  ctx.home_count = 0;
  ctx.farm_count = 0;
  ctx.defense_tower_count = 0;
  ctx.wall_segment_count = 0;
  ctx.barracks_count = 0;
  ctx.marketplace_count = 0;
  ctx.assembled_unit_count = 0;
  ctx.recruitment_manpower_available = 0;
  ctx.home_civilians_remaining = 0;
  ctx.effective_reserve_units = 0;
  ctx.effective_harass_units = 0;
  ctx.assault_unit_count = 0;
  ctx.any_base_under_threat = false;
  ctx.outpost_barracks_count = 0;
  ctx.outpost_home_count = 0;
  ctx.expansion_construction_pending = false;
  ctx.population_used = 0;
  ctx.population_cap = snapshot.max_troops_per_player;
  if (snapshot.max_troops_per_player > 0) {
    ctx.max_troops_per_player = snapshot.max_troops_per_player;
  }
}

void expire_attack_records(
    const AISnapshot& snapshot,
    AIContext& ctx,
    const std::unordered_set<Engine::Core::EntityID>& alive_ids) {
  auto it = ctx.buildings_under_attack.begin();
  while (it != ctx.buildings_under_attack.end()) {
    if (alive_ids.find(it->first) == alive_ids.end() ||
        (snapshot.game_time - it->second) > k_attack_record_timeout) {
      it = ctx.buildings_under_attack.erase(it);
    } else {
      ++it;
    }
  }
}

auto tally_friendly_units(const AISnapshot& snapshot,
                          AIContext& ctx,
                          Engine::Core::EntityID previous_primary_barracks)
    -> FriendlyTally {
  FriendlyTally tally;
  const EntitySnapshot* sticky_primary = nullptr;
  const EntitySnapshot* fallback_primary = nullptr;

  for (const auto& entity : snapshot.friendly_units) {
    if (entity.is_building) {
      tally_building(
          entity, ctx, previous_primary_barracks, sticky_primary, fallback_primary);
    } else {
      tally_unit(entity, snapshot, ctx, tally.total_health_ratio);
    }
  }

  tally.primary_barracks =
      (sticky_primary != nullptr) ? sticky_primary : fallback_primary;
  return tally;
}

auto count_siege_engines(const AISnapshot& snapshot) -> int {
  int catapult_count = 0;
  for (const auto& entity : snapshot.friendly_units) {
    if (!entity.is_building && Game::Units::is_siege_engine_spawn(entity.spawn_type)) {
      catapult_count++;
    }
    if (entity.builder_production.raising_a_building &&
        Game::Units::is_siege_engine_spawn(
            entity.builder_production.building_under_way)) {
      catapult_count++;
    }
  }
  return catapult_count;
}

void count_neutral_barracks(const AISnapshot& snapshot, AIContext& ctx) {
  for (const auto& enemy : snapshot.visible_enemies) {
    if (enemy.is_building && enemy.spawn_type == Game::Units::SpawnType::Barracks &&
        Game::Core::is_neutral_owner(enemy.owner_id) &&
        !is_gold_vein_anchor(snapshot, enemy.id)) {
      ctx.neutral_barracks_count++;
    }
  }
}

void count_nearby_threats(const AISnapshot& snapshot, AIContext& ctx) {
  if (!ctx.has_base_anchor) {
    return;
  }

  const float defend_radius =
      k_base_defend_radius +
      10.0F * std::min(2.0F, ctx.strategy_config.defense_modifier);
  const float defend_radius_sq = defend_radius * defend_radius;

  for (const auto& enemy : snapshot.visible_enemies) {
    if (!is_threatening_contact(enemy)) {
      continue;
    }
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
    }
  }
}

} // namespace Game::Systems::AI
