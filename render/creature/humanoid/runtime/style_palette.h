#pragma once

#include <QVector3D>

#include <optional>
#include <string_view>

#include "render/frame/palette.h"

namespace Render::GL::Humanoid {

void apply_commander_palette(std::string_view renderer_key,
                             const QVector3D& team_tint,
                             HumanoidPalette& palette);

auto saturate_color(const QVector3D& value) -> QVector3D;

auto blend_with_team(const QVector3D& base,
                     const QVector3D& team,
                     float team_weight) -> QVector3D;

auto mix_palette_color(const QVector3D& base_color,
                       const std::optional<QVector3D>& override_color,
                       const QVector3D& team_tint,
                       float team_weight,
                       float style_weight) -> QVector3D;

struct ClothBlend {
  float team = 0.12F;
  float style = 0.90F;
};

template <typename Style>
void apply_uniform_palette(HumanoidPalette& palette,
                           const Style& style,
                           const QVector3D& team,
                           ClothBlend cloth = {}) {
  palette.cloth = mix_palette_color(
      palette.cloth, style.cloth_color, team, cloth.team, cloth.style);
  palette.leather =
      mix_palette_color(palette.leather, style.leather_color, team, 0.04F, 0.70F);
  palette.leather_dark = mix_palette_color(
      palette.leather_dark, style.leather_dark_color, team, 0.04F, 0.70F);
  palette.metal =
      mix_palette_color(palette.metal, style.metal_color, team, 0.0F, 0.75F);
  if constexpr (requires { style.wood_color; }) {
    palette.wood = mix_palette_color(palette.wood, style.wood_color, team, 0.0F, 0.75F);
  }
}

} // namespace Render::GL::Humanoid
