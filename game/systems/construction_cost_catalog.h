#pragma once

#include <QString>

#include <string_view>

#include "resource_types.h"

namespace Game::Systems {

struct ConstructionCostInfo {
  ResourceAmounts resource_costs{};
};

inline constexpr float k_default_dismantle_refund_fraction = 0.5F;

inline constexpr float k_dismantle_speed_multiplier = 0.35F;

struct DismantleInfo {
  bool allowed = true;
  float refund_fraction = k_default_dismantle_refund_fraction;
};

[[nodiscard]] auto
construction_cost_info(std::string_view item_type) -> ConstructionCostInfo;

[[nodiscard]] auto construction_build_time(std::string_view item_type) -> float;

[[nodiscard]] auto dismantle_info(std::string_view item_type) -> DismantleInfo;

[[nodiscard]] auto dismantle_refund(std::string_view item_type) -> ResourceAmounts;

[[nodiscard]] auto dismantle_duration(std::string_view item_type) -> float;

auto load_construction_catalog(const QString& path) -> bool;
auto load_default_construction_catalog() -> bool;

void reset_construction_catalog();

} // namespace Game::Systems
