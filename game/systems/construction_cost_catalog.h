#pragma once

#include <string_view>

#include "resource_types.h"

namespace Game::Systems {

struct ConstructionCostInfo {
  ResourceAmounts resource_costs{};
};

[[nodiscard]] auto
construction_cost_info(std::string_view item_type) -> ConstructionCostInfo;

} // namespace Game::Systems
