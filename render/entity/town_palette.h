#pragma once

#include <QVector3D>

#include <array>
#include <cstddef>
#include <cstdint>

#include "render/entity/building_state.h"

namespace Render::GL {

inline constexpr std::size_t k_town_palette_slots = 4;
using TownPalette = std::array<QVector3D, k_town_palette_slots>;
using TownPaletteResolver = TownPalette (*)(const QVector3D& team,
                                            std::uint32_t seed,
                                            BuildingState state);

[[nodiscard]] auto roman_town_palette(const QVector3D& team,
                                      std::uint32_t seed,
                                      BuildingState state) -> TownPalette;

[[nodiscard]] auto carthage_town_palette(const QVector3D& team,
                                         std::uint32_t seed,
                                         BuildingState state) -> TownPalette;

} // namespace Render::GL
