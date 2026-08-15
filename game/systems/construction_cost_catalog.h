#pragma once

#include <QString>

#include <string_view>

#include "resource_types.h"

namespace Game::Systems {

struct ConstructionCostInfo {
  ResourceAmounts resource_costs{};
};

[[nodiscard]] auto
construction_cost_info(std::string_view item_type) -> ConstructionCostInfo;

[[nodiscard]] auto construction_build_time(std::string_view item_type) -> float;

auto load_construction_catalog(const QString& path) -> bool;
auto load_default_construction_catalog() -> bool;

void reset_construction_catalog();

} // namespace Game::Systems
