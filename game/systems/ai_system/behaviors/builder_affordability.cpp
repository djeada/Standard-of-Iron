#include "builder_affordability.h"

#include <algorithm>

#include "../../../map/terrain_service.h"
#include "../../economy/construction_cost_catalog.h"
#include "../ai_doctrine_catalog.h"
#include "builder_catalog.h"

namespace Game::Systems::AI {

namespace {

constexpr int k_ai_food_reserve = 60;
constexpr int k_ai_granary_target = 900;
constexpr int k_ai_larder_target = 320;

auto stockpile_target(ResourceType type, int building_count) -> int {
  const int town = std::clamp(building_count, 1, 24);
  switch (type) {
  case ResourceType::Wood:
    return 240 + town * 20;
  case ResourceType::Stone:
    return 160 + town * 14;
  case ResourceType::Iron:
    return 120 + town * 10;
  default:
    return 0;
  }
}

auto any_node_left(const AISnapshot& snapshot, ResourceType resource) -> bool {
  return std::any_of(snapshot.resource_nodes.begin(),
                     snapshot.resource_nodes.end(),
                     [resource](const ResourceNodeSnapshot& node) {
                       return !node.reserved && node_matches_resource(node, resource);
                     });
}

} // namespace

auto harvest_type_for_resource(ResourceType resource) -> const char* {
  switch (resource) {
  case ResourceType::Wood:
    return HARVEST_TREE;
  case ResourceType::Stone:
    return HARVEST_STONE;
  case ResourceType::Iron:
    return HARVEST_IRON;
  default:
    return nullptr;
  }
}

auto starved_of_food(const AISnapshot& snapshot) -> bool {
  return snapshot.has_resource_snapshot &&
         snapshot.resources.get(ResourceType::Food) < k_ai_food_reserve;
}

auto granary_has_room(const AISnapshot& snapshot) -> bool {
  return snapshot.has_resource_snapshot &&
         snapshot.resources.get(ResourceType::Food) < k_ai_granary_target;
}

auto node_matches_resource(const ResourceNodeSnapshot& node,
                           ResourceType resource) -> bool {
  switch (resource) {
  case ResourceType::Wood:
    return Game::Map::is_tree_world_prop_type(node.type);
  case ResourceType::Stone:
    return Game::Map::is_boulder_world_prop_type(node.type);
  case ResourceType::Iron:
    return Game::Map::is_iron_ore_world_prop_type(node.type);
  default:
    return false;
  }
}

auto affordability_of(const AISnapshot& snapshot,
                      const char* building_type) -> AffordabilityVerdict {
  AffordabilityVerdict verdict;
  if (building_type == nullptr) {
    verdict.blocked = true;
    return verdict;
  }
  if (!snapshot.has_resource_snapshot) {
    return verdict;
  }

  const auto costs = construction_cost_info(building_type).resource_costs;
  int largest_deficit = 0;
  for (const ResourceType type : k_all_resource_types) {
    const int deficit = costs.get(type) - snapshot.resources.get(type);
    if (deficit <= 0) {
      continue;
    }
    if (harvest_type_for_resource(type) == nullptr) {

      verdict.blocked = true;
      return verdict;
    }
    if (!any_node_left(snapshot, type)) {

      verdict.blocked = true;
      return verdict;
    }
    if (deficit > largest_deficit) {
      largest_deficit = deficit;
      verdict.missing = type;
    }
  }
  return verdict;
}

auto planned_wall_wood(const AIContext& context) -> int {
  const auto* doctrine = context.strategy_config.doctrine;
  if (doctrine == nullptr || doctrine->town_plan == nullptr) {
    return 0;
  }
  const int pending =
      std::max(0,
               std::min(doctrine->town_plan->wall_step_count(), MAX_WALL_SEGMENTS) -
                   context.wall_segment_count);
  const auto wall_cost =
      construction_cost_info(BUILDING_TYPE_WALL_SEGMENT).resource_costs;
  constexpr int k_planned_wood_cap = 700;
  return std::min(k_planned_wood_cap, pending * wall_cost.get(ResourceType::Wood));
}

auto neediest_stockpile(const AISnapshot& snapshot,
                        int building_count,
                        int planned_wood) -> const char* {
  if (!snapshot.has_resource_snapshot) {
    return nullptr;
  }

  const bool a_field_is_ripe =
      std::any_of(snapshot.friendly_units.begin(),
                  snapshot.friendly_units.end(),
                  [](const EntitySnapshot& entity) { return entity.crop_is_ripe; });
  if (a_field_is_ripe &&
      snapshot.resources.get(ResourceType::Food) < k_ai_larder_target) {
    return HARVEST_GRAIN;
  }

  const char* neediest = nullptr;
  float worst_ratio = 1.0F;
  for (const auto type :
       {ResourceType::Wood, ResourceType::Stone, ResourceType::Iron}) {
    const int target = stockpile_target(type, building_count) +
                       (type == ResourceType::Wood ? planned_wood : 0);

    if (target <= 0 || !any_node_left(snapshot, type)) {
      continue;
    }
    const float ratio =
        static_cast<float>(snapshot.resources.get(type)) / static_cast<float>(target);
    if (ratio < worst_ratio) {
      worst_ratio = ratio;
      neediest = harvest_type_for_resource(type);
    }
  }
  if (neediest != nullptr) {
    return neediest;
  }

  return a_field_is_ripe ? HARVEST_GRAIN : nullptr;
}

} // namespace Game::Systems::AI
