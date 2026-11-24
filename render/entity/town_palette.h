#pragma once

#include <QVector3D>

#include <array>
#include <cstddef>
#include <cstdint>

#include "render/entity/building_state.h"

// The colours a town building takes from where it stands: team, wall wash,
// painted accent and dyed cloth. One table per nation, shared by every
// lot-filling style so a street of houses, porticoes and docks is one town.
namespace Render::GL {

inline constexpr std::size_t k_town_palette_slots = 4;
using TownPalette = std::array<QVector3D, k_town_palette_slots>;
using TownPaletteResolver = TownPalette (*)(const QVector3D& team,
                                            std::uint32_t seed,
                                            BuildingState state);

// Limewash in ochres, creams and rose; a Pompeian red or umber dado.
[[nodiscard]] auto roman_town_palette(const QVector3D& team,
                                      std::uint32_t seed,
                                      BuildingState state) -> TownPalette;

// Whitewash; Egyptian blue, oxblood or sea green paint; purple cloth.
[[nodiscard]] auto carthage_town_palette(const QVector3D& team,
                                         std::uint32_t seed,
                                         BuildingState state) -> TownPalette;

} // namespace Render::GL
