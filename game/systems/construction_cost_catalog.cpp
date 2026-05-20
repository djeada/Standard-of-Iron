#include "construction_cost_catalog.h"

#include "wall_network_service.h"

namespace Game::Systems {

auto construction_cost_info(std::string_view item_type) -> ConstructionCostInfo {
  ConstructionCostInfo info;

  if (item_type == "catapult") {
    info.resource_costs.set(ResourceType::Wood, 90);
    info.resource_costs.set(ResourceType::Iron, 35);
    return info;
  }
  if (item_type == "ballista") {
    info.resource_costs.set(ResourceType::Wood, 75);
    info.resource_costs.set(ResourceType::Iron, 45);
    return info;
  }
  if (item_type == "defense_tower") {
    info.resource_costs.set(ResourceType::Wood, 90);
    info.resource_costs.set(ResourceType::Stone, 120);
    return info;
  }
  if (item_type == "home") {
    info.resource_costs.set(ResourceType::Wood, 80);
    info.resource_costs.set(ResourceType::Stone, 25);
    return info;
  }
  if (item_type == "barracks") {
    info.resource_costs.set(ResourceType::Wood, 140);
    info.resource_costs.set(ResourceType::Stone, 90);
    return info;
  }
  if (item_type == "wall_segment") {
    info.resource_costs.set(ResourceType::Wood,
                            WallNetworkService::k_wall_segment_wood_cost);
    return info;
  }

  return info;
}

} // namespace Game::Systems
