#pragma once

#include <QVector3D>
#include <optional>

namespace Render::GL::Humanoid {

auto saturate_color(const QVector3D &value) -> QVector3D;

auto blend_with_team(const QVector3D &base, const QVector3D &team,
                     float team_weight) -> QVector3D;

auto mix_palette_color(const QVector3D &base_color,
                       const std::optional<QVector3D> &override_color,
                       const QVector3D &team_tint, float team_weight,
                       float style_weight) -> QVector3D;

} 
