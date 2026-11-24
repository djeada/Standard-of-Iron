#pragma once

#include <QVector3D>

#include <cstddef>
#include <cstdint>

#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_state.h"
#include "render/entity/tenement_layout.h"

// The Punic tall house: a whitewashed cube of two to four storeys on its whole
// lot, few and small openings on the street, a painted band and a Tanit sign at
// the door, and a flat roof that is lived on. Authored as BuildingArchetypeDescs
// and baked by the building archetype catalog; nothing here draws or reads
// frame state.
namespace Render::GL::Carthage {

inline constexpr std::uint8_t k_tenement_team_slot = 0;
inline constexpr std::uint8_t k_tenement_wall_slot = 1;
inline constexpr std::uint8_t k_tenement_accent_slot = 2;
inline constexpr std::uint8_t k_tenement_cloth_slot = 3;
inline constexpr std::size_t k_tenement_palette_slots = 4;

auto build_tenement_desc(BuildingState state, int variant) -> BuildingArchetypeDesc;

// Where the hearth smoke leaves the roof, in building-local units.
[[nodiscard]] auto tenement_vent(int variant) -> QVector3D;

} // namespace Render::GL::Carthage
