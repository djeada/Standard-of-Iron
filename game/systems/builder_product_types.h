#pragma once

#include <string_view>

namespace Game::Systems {

inline constexpr std::string_view k_builder_product_cut_tree = "cut_tree";
inline constexpr std::string_view k_builder_product_collect_stone = "collect_stone";
inline constexpr std::string_view k_builder_product_collect_iron_ore =
    "collect_iron_ore";
inline constexpr std::string_view k_builder_product_harvest_grain = "harvest_grain";
inline constexpr std::string_view k_builder_product_slaughter_sheep = "slaughter_sheep";
inline constexpr std::string_view k_builder_product_repair = "repair_structure";
inline constexpr std::string_view k_builder_product_dismantle = "dismantle_structure";

inline constexpr float k_builder_repair_tick_seconds = 1.2F;

inline constexpr std::string_view k_builder_product_wall_segment = "wall_segment";
inline constexpr std::string_view k_builder_product_wall_gate = "wall_gate";

[[nodiscard]] inline auto
is_harvest_builder_product(std::string_view product_type) -> bool {
  return product_type == k_builder_product_cut_tree ||
         product_type == k_builder_product_collect_stone ||
         product_type == k_builder_product_collect_iron_ore;
}

[[nodiscard]] inline auto
is_food_builder_product(std::string_view product_type) -> bool {
  return product_type == k_builder_product_harvest_grain ||
         product_type == k_builder_product_slaughter_sheep;
}

[[nodiscard]] inline auto
is_gather_builder_product(std::string_view product_type) -> bool {
  return is_harvest_builder_product(product_type) ||
         is_food_builder_product(product_type);
}

[[nodiscard]] inline auto
is_wall_builder_product(std::string_view product_type) -> bool {
  return product_type == k_builder_product_wall_segment ||
         product_type == k_builder_product_wall_gate;
}

} // namespace Game::Systems
