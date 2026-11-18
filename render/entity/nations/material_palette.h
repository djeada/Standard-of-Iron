#pragma once

#include <QVector3D>

#include "render/entity/nations/sepulcher/palette.h"

namespace Render::GL::Nation {

// Material albedos before team identification, weathering and scene lighting.
// Roles can choose a secondary dye (for example Punic spearmen wear saffron).
struct MaterialPalette {
  QVector3D cloth;
  QVector3D accent;
  QVector3D leather;
  QVector3D dark_leather;
  QVector3D metal;
  QVector3D timber;
};

inline constexpr MaterialPalette k_roman_materials{
    .cloth = {0.72F, 0.16F, 0.18F},
    .accent = {0.73F, 0.61F, 0.34F},
    .leather = {0.36F, 0.21F, 0.12F},
    .dark_leather = {0.24F, 0.15F, 0.09F},
    .metal = {0.74F, 0.72F, 0.68F},
    .timber = {0.46F, 0.32F, 0.20F}};
inline constexpr MaterialPalette k_carthage_materials{
    .cloth = {0.16F, 0.29F, 0.65F},
    .accent = {0.70F, 0.49F, 0.21F},
    .leather = {0.39F, 0.25F, 0.14F},
    .dark_leather = {0.27F, 0.17F, 0.10F},
    .metal = {0.72F, 0.58F, 0.35F},
    .timber = {0.41F, 0.33F, 0.25F}};
inline constexpr MaterialPalette k_gallic_materials{
    .cloth = {0.66F, 0.47F, 0.25F},
    .accent = {0.18F, 0.38F, 0.36F},
    .leather = {0.38F, 0.25F, 0.13F},
    .dark_leather = {0.24F, 0.16F, 0.10F},
    .metal = {0.60F, 0.61F, 0.60F},
    .timber = {0.42F, 0.31F, 0.20F}};
inline constexpr MaterialPalette k_iberian_materials{
    .cloth = {0.93F, 0.91F, 0.86F},
    .accent = {0.59F, 0.22F, 0.14F},
    .leather = {0.34F, 0.22F, 0.12F},
    .dark_leather = {0.22F, 0.14F, 0.09F},
    .metal = {0.66F, 0.66F, 0.66F},
    .timber = {0.40F, 0.26F, 0.15F}};
inline constexpr MaterialPalette k_sepulcher_materials{
    .cloth = Sepulcher::k_grave_cloth,
    .accent = Sepulcher::k_bone,
    .leather = Sepulcher::k_grave_leather,
    .dark_leather = Sepulcher::k_grave_leather_dark,
    .metal = Sepulcher::k_grave_iron,
    .timber = Sepulcher::k_grave_wood};

template <typename Style>
constexpr void apply_materials(Style& style, const MaterialPalette& materials) {
  style.cloth_color = materials.cloth;
  style.leather_color = materials.leather;
  style.leather_dark_color = materials.dark_leather;
  style.metal_color = materials.metal;
}

} // namespace Render::GL::Nation
