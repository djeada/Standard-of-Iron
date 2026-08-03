#pragma once

#include <string_view>

namespace Game::Systems {

// Builder "products" that are jobs rather than structures. They travel through
// BuilderProductionComponent::product_type, so the simulation, the order layer
// and the activity classifier have to agree on the spelling.
inline constexpr std::string_view k_builder_product_cut_tree = "cut_tree";
inline constexpr std::string_view k_builder_product_collect_stone = "collect_stone";
inline constexpr std::string_view k_builder_product_collect_iron_ore =
    "collect_iron_ore";
inline constexpr std::string_view k_builder_product_repair = "repair_structure";
inline constexpr std::string_view k_builder_product_dismantle = "dismantle_structure";

// One repair pass. Repair loops this tick until the structure is whole, so a
// player can interrupt it at any point rather than committing to a long job.
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
is_wall_builder_product(std::string_view product_type) -> bool {
  return product_type == k_builder_product_wall_segment ||
         product_type == k_builder_product_wall_gate;
}

} // namespace Game::Systems
