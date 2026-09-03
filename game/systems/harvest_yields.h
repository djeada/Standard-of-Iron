#pragma once

#include <optional>
#include <string_view>

#include "builder_product_types.h"
#include "resource_types.h"

namespace Game::Systems {

inline constexpr int k_cut_tree_wood_reward = 80;
inline constexpr int k_collect_stone_reward = 35;
inline constexpr int k_collect_iron_ore_reward = 30;
inline constexpr int k_harvest_grain_food_reward = 60;
inline constexpr int k_slaughter_sheep_food_reward = 35;

inline constexpr float k_farm_growth_cycle_seconds = 60.0F;

[[nodiscard]] constexpr auto is_gatherable_resource(ResourceType type) -> bool {
  return type == ResourceType::Wood || type == ResourceType::Stone ||
         type == ResourceType::Iron || type == ResourceType::Food;
}

[[nodiscard]] constexpr auto harvest_yield(ResourceType type) -> int {
  switch (type) {
  case ResourceType::Wood:
    return k_cut_tree_wood_reward;
  case ResourceType::Stone:
    return k_collect_stone_reward;
  case ResourceType::Iron:
    return k_collect_iron_ore_reward;
  case ResourceType::Food:
    return k_harvest_grain_food_reward;
  case ResourceType::Gold:
  case ResourceType::Count:
    break;
  }
  return 0;
}

[[nodiscard]] constexpr auto
harvest_product_for_resource(ResourceType type) -> std::string_view {
  switch (type) {
  case ResourceType::Wood:
    return k_builder_product_cut_tree;
  case ResourceType::Stone:
    return k_builder_product_collect_stone;
  case ResourceType::Iron:
    return k_builder_product_collect_iron_ore;
  case ResourceType::Food:
    return k_builder_product_harvest_grain;
  case ResourceType::Gold:
  case ResourceType::Count:
    break;
  }
  return {};
}

[[nodiscard]] inline auto resource_for_harvest_product(std::string_view product_type)
    -> std::optional<ResourceType> {
  if (product_type == k_builder_product_cut_tree) {
    return ResourceType::Wood;
  }
  if (product_type == k_builder_product_collect_stone) {
    return ResourceType::Stone;
  }
  if (product_type == k_builder_product_collect_iron_ore) {
    return ResourceType::Iron;
  }
  if (product_type == k_builder_product_harvest_grain ||
      product_type == k_builder_product_slaughter_sheep) {
    return ResourceType::Food;
  }
  return std::nullopt;
}

[[nodiscard]] constexpr auto
harvest_product_yield(std::string_view product_type) -> int {
  if (product_type == k_builder_product_cut_tree) {
    return k_cut_tree_wood_reward;
  }
  if (product_type == k_builder_product_collect_stone) {
    return k_collect_stone_reward;
  }
  if (product_type == k_builder_product_collect_iron_ore) {
    return k_collect_iron_ore_reward;
  }
  if (product_type == k_builder_product_harvest_grain) {
    return k_harvest_grain_food_reward;
  }
  if (product_type == k_builder_product_slaughter_sheep) {
    return k_slaughter_sheep_food_reward;
  }
  return 0;
}

} // namespace Game::Systems
