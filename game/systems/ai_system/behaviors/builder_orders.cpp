#include "builder_orders.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "../ai_utils.h"
#include "builder_affordability.h"
#include "builder_catalog.h"
#include "builder_site_planner.h"
#include "builder_town_plan.h"

namespace Game::Systems::AI {

namespace {

constexpr int k_repair_health_fraction_numerator = 9;
constexpr int k_repair_health_fraction_denominator = 10;
constexpr int k_max_repair_crews = 2;

auto wants_repair(const EntitySnapshot& entity) -> bool {
  return entity.is_building && entity.health > 0 && entity.max_health > 0 &&
         entity.health * k_repair_health_fraction_denominator <
             entity.max_health * k_repair_health_fraction_numerator;
}

auto field_is_worked(const AISnapshot& snapshot,
                     Engine::Core::EntityID field_id) -> bool {
  for (const auto& entity : snapshot.friendly_units) {
    if (entity.builder_production.has_component &&
        entity.builder_production.task_target_id == field_id) {
      return true;
    }
  }
  return false;
}

auto entity_is_worked(const AISnapshot& snapshot,
                      Engine::Core::EntityID target_id) -> bool {
  for (const auto& entity : snapshot.friendly_units) {
    if (entity.builder_production.has_component &&
        entity.builder_production.task_target_id == target_id) {
      return true;
    }
  }
  return false;
}

} // namespace

void order_harvest(const AISnapshot& snapshot,
                   const AIContext& context,
                   ResourceType resource,
                   const SourNodes& sour,
                   BuilderPool& pool,
                   std::vector<AICommand>& out_commands) {
  const char* harvest_type = harvest_type_for_resource(resource);
  if (harvest_type == nullptr) {
    return;
  }
  const ResourceNodeSnapshot* closest = nullptr;
  float closest_distance_sq = std::numeric_limits<float>::infinity();
  for (const auto& node : snapshot.resource_nodes) {
    if (node.reserved || !node_matches_resource(node, resource) ||
        node_is_sour(sour, node.id, snapshot.game_time)) {
      continue;
    }
    const float dx = node.pos_x - context.base_pos_x;
    const float dz = node.pos_z - context.base_pos_z;
    const float distance_sq = dx * dx + dz * dz;
    if (distance_sq < closest_distance_sq) {
      closest = &node;
      closest_distance_sq = distance_sq;
    }
  }
  if (closest == nullptr) {
    return;
  }
  const auto builder = pool.take_nearest(snapshot, closest->pos_x, closest->pos_z);
  if (builder == 0) {
    return;
  }

  AICommand command;
  command.type = AICommandType::StartBuilderHarvest;
  command.units.push_back(builder);
  command.construction_type = harvest_type;
  command.construction_site_x = closest->pos_x;
  command.construction_site_z = closest->pos_z;
  command.resource_target_id = closest->id;
  out_commands.push_back(std::move(command));
}

void order_repairs(const AISnapshot& snapshot,
                   BuilderPool& pool,
                   int reserve,
                   std::vector<AICommand>& out_commands) {
  int crews = 0;
  for (const auto& entity : snapshot.friendly_units) {
    if (crews >= k_max_repair_crews || pool.size() <= reserve) {
      return;
    }
    if (!wants_repair(entity) || entity_is_worked(snapshot, entity.id)) {
      continue;
    }
    const auto builder = pool.take_nearest(snapshot, entity.pos_x, entity.pos_z);
    if (builder == 0) {
      return;
    }
    AICommand command;
    command.type = AICommandType::StartBuilderRepair;
    command.units.push_back(builder);
    command.target_id = entity.id;
    command.construction_site_x = entity.pos_x;
    command.construction_site_z = entity.pos_z;
    out_commands.push_back(std::move(command));
    ++crews;
  }
}

void order_field_work(const AISnapshot& snapshot,
                      const AIContext& context,
                      BuilderPool& pool,
                      int reserve,
                      std::vector<AICommand>& out_commands) {
  if (!granary_has_room(snapshot)) {
    return;
  }
  std::vector<const EntitySnapshot*> ripe;
  for (const auto& entity : snapshot.friendly_units) {
    if (!entity.crop_is_ripe || field_is_worked(snapshot, entity.id)) {
      continue;
    }
    ripe.push_back(&entity);
  }
  std::sort(ripe.begin(),
            ripe.end(),
            [&context](const EntitySnapshot* lhs, const EntitySnapshot* rhs) {
              return distance_squared(lhs->pos_x,
                                      0.0F,
                                      lhs->pos_z,
                                      context.base_pos_x,
                                      0.0F,
                                      context.base_pos_z) <
                     distance_squared(rhs->pos_x,
                                      0.0F,
                                      rhs->pos_z,
                                      context.base_pos_x,
                                      0.0F,
                                      context.base_pos_z);
            });
  for (const auto* field : ripe) {
    if (pool.size() <= reserve) {
      return;
    }
    const auto builder = pool.take_nearest(snapshot, field->pos_x, field->pos_z);
    if (builder == 0) {
      return;
    }
    AICommand command;
    command.type = AICommandType::StartBuilderHarvest;
    command.units.push_back(builder);
    command.construction_type = HARVEST_GRAIN;
    command.construction_site_x = field->pos_x;
    command.construction_site_z = field->pos_z;
    command.resource_target_id = field->id;
    out_commands.push_back(std::move(command));
  }
}

auto clear_node_in_the_way(const AISnapshot& snapshot,
                           const SourNodes& sour,
                           BuilderPool& pool,
                           const char* building_type,
                           float site_x,
                           float site_z,
                           std::vector<AICommand>& out_commands) -> ClearingOutcome {
  const auto in_the_way =
      node_on_the_ground(snapshot, sour, building_type, site_x, site_z);
  if (in_the_way.node == nullptr) {
    return ClearingOutcome::Unobstructed;
  }
  const auto builder =
      pool.take_nearest(snapshot, in_the_way.node->pos_x, in_the_way.node->pos_z);
  if (builder == 0) {
    return ClearingOutcome::NoBuilder;
  }
  AICommand clearing;
  clearing.type = AICommandType::StartBuilderHarvest;
  clearing.units.push_back(builder);
  clearing.construction_type = harvest_type_for_resource(in_the_way.resource);
  clearing.construction_site_x = in_the_way.node->pos_x;
  clearing.construction_site_z = in_the_way.node->pos_z;
  clearing.resource_target_id = in_the_way.node->id;
  out_commands.push_back(std::move(clearing));
  return ClearingOutcome::Clearing;
}

auto order_construction(const AISnapshot& snapshot,
                        const AIContext& context,
                        BuilderPool& pool,
                        const char* building_type,
                        float site_x,
                        float site_z,
                        float rotation_y,
                        std::vector<AICommand>& out_commands) -> bool {
  const auto builder = pool.take_strongest(snapshot, site_x, site_z);
  if (builder == 0) {
    return false;
  }
  AICommand command;
  command.type = AICommandType::StartBuilderConstruction;
  command.units.push_back(builder);
  command.construction_type = building_type;
  command.construction_site_x = site_x;
  command.construction_site_z = site_z;
  command.construction_rotation_y = rotation_y;
  if (!is_fortification_or_tower(building_type)) {
    command.construction_keep_out = plan_keep_out(context, snapshot);
  }
  out_commands.push_back(std::move(command));
  return true;
}

} // namespace Game::Systems::AI
