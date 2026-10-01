#include "builder_town_plan.h"

#include <QVector2D>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

#include "../ai_doctrine_catalog.h"
#include "../ai_settlement_frame.h"
#include "../ai_utils.h"
#include "builder_catalog.h"
#include "builder_site_geometry.h"
#include "units/spawn_type.h"

namespace Game::Systems::AI {

namespace {

auto plan_step_is_already_met(const SettlementCensus& standing,
                              const SettlementTargets& targets,
                              const char* building) -> bool {
  if (building == BUILDING_TYPE_HOME) {
    return standing.homes >= targets.homes;
  }
  if (building == BUILDING_TYPE_BARRACKS) {
    return standing.barracks >= targets.barracks;
  }
  if (building == BUILDING_TYPE_DEFENSE_TOWER) {
    return standing.towers >= targets.towers;
  }
  if (building == BUILDING_TYPE_WALL_SEGMENT) {
    return standing.walls >= targets.walls;
  }
  if (building == BUILDING_TYPE_WALL_GATE) {
    return standing.gates >= targets.gates;
  }
  if (building == BUILDING_TYPE_MARKETPLACE) {
    return standing.markets >= targets.markets;
  }
  if (building == BUILDING_TYPE_FARM) {
    return standing.farms >= targets.farms;
  }
  return false;
}

auto standing_count(const SettlementCensus& standing, const char* building) -> int {
  if (building == BUILDING_TYPE_HOME) {
    return standing.homes;
  }
  if (building == BUILDING_TYPE_BARRACKS) {
    return standing.barracks;
  }
  if (building == BUILDING_TYPE_DEFENSE_TOWER) {
    return standing.towers;
  }
  if (building == BUILDING_TYPE_WALL_SEGMENT) {
    return standing.walls;
  }
  if (building == BUILDING_TYPE_WALL_GATE) {
    return standing.gates;
  }
  if (building == BUILDING_TYPE_MARKETPLACE) {
    return standing.markets;
  }
  if (building == BUILDING_TYPE_FARM) {
    return standing.farms;
  }
  return -1;
}

struct SlotOccupant {
  bool occupied = false;
  bool wall_itself = false;
};

auto occupant_of_slot(const AISnapshot& snapshot,
                      const char* building,
                      float world_x,
                      float world_z) -> SlotOccupant {
  for (const auto& entity : snapshot.friendly_units) {
    if (entity.is_building) {
      const float clearance = slot_clearance(building, entity.spawn_type);
      if (distance_squared(entity.pos_x, 0.0F, entity.pos_z, world_x, 0.0F, world_z) <=
          clearance * clearance) {
        return {true, Game::Units::is_wall_network_spawn(entity.spawn_type)};
      }
      continue;
    }
    const auto& raising = entity.builder_production;
    if (!raising.raising_a_building || !raising.has_construction_site) {
      continue;
    }

    const float clearance = slot_clearance(building, raising.building_under_way);
    if (distance_squared(raising.construction_site_x,
                         0.0F,
                         raising.construction_site_z,
                         world_x,
                         0.0F,
                         world_z) <= clearance * clearance) {
      return {true, Game::Units::is_wall_network_spawn(raising.building_under_way)};
    }
  }
  return {};
}

auto free_slot_position(const AIContext& context,
                        const AISnapshot& snapshot,
                        const TownPlanStep& step,
                        const char* resolved,
                        const QVector2D& facing)
    -> std::optional<std::pair<float, float>> {
  constexpr float k_anchor_clearance = TownPlan::k_anchor_clearance;
  constexpr float k_anchor_clearance_sq = k_anchor_clearance * k_anchor_clearance;
  constexpr float k_deg_to_rad = 3.14159265358979323846F / 180.0F;

  std::array<float, 5> const gate_slide{0.0F, 2.4F, -2.4F, 4.8F, -4.8F};
  std::size_t const placements =
      resolved == BUILDING_TYPE_WALL_GATE ? gate_slide.size() : 1U;

  float world_x = 0.0F;
  float world_z = 0.0F;
  bool occupied = true;
  bool too_close_to_the_anchor = false;
  bool occupied_by_the_wall_itself = false;

  for (std::size_t placement = 0;
       placement < placements && occupied && !occupied_by_the_wall_itself;
       ++placement) {
    float const along = gate_slide.at(placement);
    float const run = step.rotation * k_deg_to_rad;
    const QVector3D offset = plan_offset_to_world(
        facing, step.x + (along * std::cos(run)), step.z + (along * std::sin(run)));
    world_x = context.base_pos_x + offset.x();
    world_z = context.base_pos_z + offset.z();

    if ((offset.x() * offset.x() + offset.z() * offset.z()) < k_anchor_clearance_sq) {
      too_close_to_the_anchor = true;
      continue;
    }
    too_close_to_the_anchor = false;

    const SlotOccupant occupant =
        occupant_of_slot(snapshot, resolved, world_x, world_z);
    occupied = occupant.occupied;
    occupied_by_the_wall_itself = occupant.wall_itself;
  }

  if (too_close_to_the_anchor || occupied) {
    return std::nullopt;
  }
  return std::make_pair(world_x, world_z);
}

} // namespace

auto assess_settlement(const AISnapshot& snapshot,
                       const AIContext& context) -> SettlementAssessment {
  SettlementAssessment town;

  for (const auto& entity : snapshot.friendly_units) {
    const auto is_artillery = [](Game::Units::SpawnType type) {
      return Game::Units::is_siege_engine_spawn(type) &&
             type != Game::Units::SpawnType::Ram &&
             type != Game::Units::SpawnType::SiegeTower;
    };
    if (is_artillery(entity.spawn_type) ||
        (entity.builder_production.raising_a_building &&
         is_artillery(entity.builder_production.building_under_way))) {
      town.siege_count++;
    }
    if (entity.spawn_type == Game::Units::SpawnType::Ram ||
        (entity.builder_production.raising_a_building &&
         entity.builder_production.building_under_way == Game::Units::SpawnType::Ram)) {
      town.ram_count++;
    }
  }
  town.siege_engine = preferred_siege_engine(context);

  const auto raising = [&snapshot](Game::Units::SpawnType type) {
    int count = 0;
    for (const auto& entity : snapshot.friendly_units) {
      if (entity.builder_production.raising_a_building &&
          entity.builder_production.building_under_way == type) {
        ++count;
      }
    }
    return count;
  };

  int standing_gates = 0;
  for (const auto& entity : snapshot.friendly_units) {
    if (entity.is_building && entity.health > 0 &&
        entity.spawn_type == Game::Units::SpawnType::WallGate) {
      ++standing_gates;
    }
  }
  town.standing = SettlementCensus{
      .homes = context.home_count + raising(Game::Units::SpawnType::Home),
      .barracks = context.barracks_count + raising(Game::Units::SpawnType::Barracks),
      .towers =
          context.defense_tower_count + raising(Game::Units::SpawnType::DefenseTower),
      .walls =
          context.wall_segment_count + raising(Game::Units::SpawnType::WallSegment),
      .gates = standing_gates + raising(Game::Units::SpawnType::WallGate),
      .markets =
          context.marketplace_count + raising(Game::Units::SpawnType::Marketplace),
      .farms = context.farm_count + raising(Game::Units::SpawnType::Farm)};

  const auto& macro = context.macro_targets;
  town.town_plan = context.strategy_config.doctrine != nullptr
                       ? context.strategy_config.doctrine->town_plan
                       : nullptr;
  const int planned_walls = town.town_plan != nullptr
                                ? town.town_plan->step_count(BUILDING_TYPE_WALL_SEGMENT)
                                : 0;
  town.targets = SettlementTargets{
      .homes = std::clamp(macro.home_count, 2, MAX_HOMES),
      .barracks = std::clamp(macro.barracks_count, 1, MAX_BARRACKS),
      .towers = std::clamp(macro.defense_tower_count, 0, MAX_DEFENSE_TOWERS),
      .walls = std::clamp(
          std::max(macro.wall_segment_count, planned_walls), 0, MAX_WALL_SEGMENTS),
      .gates = std::clamp(town.town_plan != nullptr
                              ? town.town_plan->step_count(BUILDING_TYPE_WALL_GATE)
                              : 0,
                          0,
                          MAX_WALL_GATES),
      .markets = std::clamp(macro.marketplace_count, 0, MAX_MARKETPLACES),
      .farms = std::clamp(macro.farm_count, 0, MAX_FARMS)};
  town.target_catapults = std::clamp(macro.catapult_count, 0, MAX_CATAPULTS);
  town.target_rams =
      town.target_catapults >= 4 ? 2 : (town.target_catapults > 0 ? 1 : 0);
  return town;
}

auto authored_plan_step(const AIContext& context,
                        const AISnapshot& snapshot,
                        const SettlementAssessment& town,
                        const char* preferred,
                        const std::vector<int>& blocked_slots,
                        PlanStepChoice& out_choice) -> bool {
  const auto* doctrine = context.strategy_config.doctrine;
  if (doctrine == nullptr || doctrine->town_plan == nullptr) {
    return false;
  }

  const QVector2D facing = locked_settlement_facing(context, snapshot);
  const bool fields_come_before_walls =
      town.standing.farms < 1 || town.standing.homes < 2;

  PlanStepChoice fallback;

  int slot = -1;
  for (const auto& step : doctrine->town_plan->steps) {
    ++slot;
    const char* resolved = building_type_name(step.building);
    if (resolved == nullptr) {
      continue;
    }
    if (std::find(blocked_slots.begin(), blocked_slots.end(), slot) !=
        blocked_slots.end()) {
      continue;
    }
    if (is_siege_engine_building(resolved)) {
      continue;
    }
    if (plan_step_is_already_met(town.standing, town.targets, resolved)) {
      continue;
    }
    if (resolved == BUILDING_TYPE_WALL_SEGMENT && fields_come_before_walls) {
      continue;
    }

    const auto position = free_slot_position(context, snapshot, step, resolved, facing);
    if (!position.has_value()) {
      continue;
    }

    const QVector3D offset(position->first - context.base_pos_x,
                           0.0F,
                           position->second - context.base_pos_z);

    const float rotation_y = plan_rotation_to_world(facing, step.rotation);
    if (preferred != nullptr && resolved != preferred) {
      if (fallback.building == nullptr) {
        fallback = {resolved, offset, rotation_y, slot};
      }
      continue;
    }

    out_choice = {resolved, offset, rotation_y, slot};
    return true;
  }

  if (fallback.building == nullptr) {
    return false;
  }

  out_choice = fallback;
  return true;
}

auto plan_still_sites_this_itself(const AIContext& context,
                                  const SettlementAssessment& town,
                                  const std::vector<int>& blocked_slots,
                                  const char* building) -> bool {
  const auto* doctrine = context.strategy_config.doctrine;
  if (doctrine == nullptr || doctrine->town_plan == nullptr || building == nullptr) {
    return false;
  }
  if (is_siege_engine_building(building)) {
    return false;
  }
  if (plan_step_is_already_met(town.standing, town.targets, building)) {
    return false;
  }

  int slot = -1;
  int plan_slots = 0;
  bool open_slot = false;
  for (const auto& step : doctrine->town_plan->steps) {
    ++slot;
    if (building_type_name(step.building) != building) {
      continue;
    }
    ++plan_slots;
    if (std::find(blocked_slots.begin(), blocked_slots.end(), slot) ==
        blocked_slots.end()) {
      open_slot = true;
    }
  }
  const int standing_of_type = standing_count(town.standing, building);
  if (standing_of_type >= 0 && standing_of_type >= plan_slots) {
    return false;
  }
  return open_slot;
}

auto plan_reserves_ground(const AIContext& context,
                          const AISnapshot& snapshot,
                          const char* building_type,
                          float world_x,
                          float world_z,
                          bool fortifications_only) -> bool {
  const auto* doctrine = context.strategy_config.doctrine;
  if (doctrine == nullptr || doctrine->town_plan == nullptr ||
      building_type == nullptr) {
    return false;
  }
  const float own_half = footprint_half_extent(std::string(building_type));
  const QVector2D facing = locked_settlement_facing(context, snapshot);
  for (const auto& step : doctrine->town_plan->steps) {
    const char* resolved = building_type_name(step.building);
    if (resolved == nullptr || is_siege_engine_building(resolved)) {
      continue;
    }
    if (fortifications_only && !is_fortification_or_tower(step.building)) {
      continue;
    }
    const QVector3D offset = plan_offset_to_world(facing, step.x, step.z);
    const float clearance =
        own_half + footprint_half_extent(step.building) + k_slot_gap;
    if (distance_squared(context.base_pos_x + offset.x(),
                         0.0F,
                         context.base_pos_z + offset.z(),
                         world_x,
                         0.0F,
                         world_z) <= clearance * clearance) {
      return true;
    }
  }
  return false;
}

auto plan_keep_out(const AIContext& context,
                   const AISnapshot& snapshot) -> std::vector<SiteKeepOut> {
  std::vector<SiteKeepOut> keep_out;
  const auto* doctrine = context.strategy_config.doctrine;
  if (doctrine == nullptr || doctrine->town_plan == nullptr ||
      !context.has_base_anchor) {
    return keep_out;
  }
  const QVector2D facing = locked_settlement_facing(context, snapshot);
  for (const auto& step : doctrine->town_plan->steps) {
    if (!is_fortification_or_tower(step.building)) {
      continue;
    }
    const QVector3D offset = plan_offset_to_world(facing, step.x, step.z);
    const float x = context.base_pos_x + offset.x();
    const float z = context.base_pos_z + offset.z();
    const float half = footprint_half_extent(step.building);
    keep_out.push_back({x, z, half + k_slot_gap});
  }
  return keep_out;
}

auto plan_footprint_radius(const AIContext& context) -> float {
  const auto* doctrine = context.strategy_config.doctrine;
  const auto* plan = doctrine != nullptr ? doctrine->town_plan : nullptr;
  if (plan == nullptr) {
    return 0.0F;
  }
  float reach = 0.0F;
  for (const auto& step : plan->steps) {
    reach = std::max(reach, std::hypot(step.x, step.z));
  }
  return reach;
}

} // namespace Game::Systems::AI
