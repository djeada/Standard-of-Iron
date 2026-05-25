#include "construction_cost_catalog.h"

#include "wall_network_service.h"

namespace Game::Systems {

auto construction_cost_info(std::string_view item_type) -> ConstructionCostInfo {
  ConstructionCostInfo info;

  if (item_type == "catapult") {
    info.resource_costs.set(ResourceType::Wood, 60);
    info.resource_costs.set(ResourceType::Iron, 25);
    return info;
  }
  if (item_type == "ballista") {
    info.resource_costs.set(ResourceType::Wood, 50);
    info.resource_costs.set(ResourceType::Iron, 30);
    return info;
  }
  if (item_type == "defense_tower") {
    info.resource_costs.set(ResourceType::Wood, 60);
    info.resource_costs.set(ResourceType::Stone, 80);
    return info;
  }
  if (item_type == "home") {
    info.resource_costs.set(ResourceType::Wood, 50);
    info.resource_costs.set(ResourceType::Stone, 15);
    return info;
  }
  if (item_type == "barracks") {
    info.resource_costs.set(ResourceType::Wood, 100);
    info.resource_costs.set(ResourceType::Stone, 60);
    return info;
  }
  if (item_type == "wall_segment") {
    info.resource_costs.set(ResourceType::Wood,
                            WallNetworkService::k_wall_segment_wood_cost);
    return info;
  }
  if (item_type == "marketplace") {
    info.resource_costs.set(ResourceType::Wood, 60);
    info.resource_costs.set(ResourceType::Stone, 40);
    info.resource_costs.set(ResourceType::Gold, 50);
    return info;
  }

  return info;
}

} // namespace Game::Systems
