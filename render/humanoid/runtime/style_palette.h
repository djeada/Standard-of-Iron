#pragma once

#include <QVector3D>

#include <optional>
#include <string_view>

#include "render/palette.h"

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

} // namespace Render::GL::Humanoid
