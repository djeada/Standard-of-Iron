#pragma once

#include <cstdint>

// The lot a marketplace stands on, in building-local units, and the variants
// its lot-filling styles draw. A portico or a dock fills this lot edge to edge
// the way a tenement fills a home's, so a row of them is one colonnade or one
// quay. Nothing here draws.
namespace Render::GL {

namespace MarketLot {
// Half the marketplace's navigation footprint, less a hair so two neighbours
// do not share a plane.
inline constexpr float k_half = 1.405F;
} // namespace MarketLot

inline constexpr std::uint8_t k_lot_team_slot = 0;
inline constexpr std::uint8_t k_lot_wall_slot = 1;
inline constexpr std::uint8_t k_lot_accent_slot = 2;
inline constexpr std::uint8_t k_lot_cloth_slot = 3;

// Variants a portico picks from by where it stands, and after them the one the
// basilica style always draws: a hall with a clerestory behind a two-storey
// colonnade.
inline constexpr int k_portico_walk_variants = 3;
inline constexpr int k_portico_basilica_variant = 3;
inline constexpr int k_portico_variant_count = 4;
inline constexpr int k_dock_variant_count = 3;

// A colonnade is one building however many lots it runs. Every lot of a
// straight run stands on one line and looks the same way, so that line names
// the run: `front_x, front_z` is the way the lot faces. Its colours, its
// weathering and its variant all come from this seed.
[[nodiscard]] auto lot_run_seed(float world_x,
                                float world_z,
                                float front_x,
                                float front_z) -> std::uint32_t;

// The variant a portico run draws; one facing +z when no facing is given.
[[nodiscard]] auto portico_run_variant(std::uint32_t run_seed) -> int;
[[nodiscard]] auto portico_visual_variant(float world_x, float world_z) -> int;

// Ships are longer than a lot, so neighbouring docks may not both moor one.
[[nodiscard]] auto dock_visual_variant(float world_x, float world_z) -> int;

} // namespace Render::GL
