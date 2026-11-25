#pragma once

#include <cstdint>

namespace Render::GL {

namespace MarketLot {

inline constexpr float k_half = 1.405F;
} // namespace MarketLot

inline constexpr std::uint8_t k_lot_team_slot = 0;
inline constexpr std::uint8_t k_lot_wall_slot = 1;
inline constexpr std::uint8_t k_lot_accent_slot = 2;
inline constexpr std::uint8_t k_lot_cloth_slot = 3;

inline constexpr int k_portico_walk_variants = 3;
inline constexpr int k_portico_basilica_variant = 3;
inline constexpr int k_portico_variant_count = 4;
inline constexpr int k_dock_variant_count = 3;

[[nodiscard]] auto lot_run_seed(float world_x,
                                float world_z,
                                float front_x,
                                float front_z) -> std::uint32_t;

[[nodiscard]] auto portico_run_variant(std::uint32_t run_seed) -> int;
[[nodiscard]] auto portico_visual_variant(float world_x, float world_z) -> int;

[[nodiscard]] auto dock_visual_variant(float world_x, float world_z) -> int;

} // namespace Render::GL
