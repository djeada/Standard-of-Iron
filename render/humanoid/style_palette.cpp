#include "style_palette.h"

#include <algorithm>

namespace Render::GL::Humanoid {

namespace {

inline auto clamp01(float value) -> float {
  return std::clamp(value, 0.0F, 1.0F);
}

} // namespace

auto saturate_color(const QVector3D &value) -> QVector3D {
  return {clamp01(value.x()), clamp01(value.y()), clamp01(value.z())};
}

auto blend_with_team(const QVector3D &base, const QVector3D &team,
                     float team_weight) -> QVector3D {
  float const base_weight = 1.0F - clamp01(team_weight);
  float const team_contrib = clamp01(team_weight);
  auto mix_component = [&](float base_c, float team_c) -> float {
    return clamp01(base_c * base_weight + team_c * team_contrib);
  };

  return {mix_component(base.x(), team.x()), mix_component(base.y(), team.y()),
          mix_component(base.z(), team.z())};
}

auto mix_palette_color(const QVector3D &base_color,
                       const std::optional<QVector3D> &override_color,
                       const QVector3D &team_tint, float team_weight,
                       float style_weight) -> QVector3D {
  if (!override_color) {
    return base_color;
  }

  QVector3D styled =
      blend_with_team(*override_color, team_tint, clamp01(team_weight));
  styled = saturate_color(styled);

  float const t = clamp01(style_weight);
  QVector3D mixed = base_color * (1.0F - t) + styled * t;
  return saturate_color(mixed);
}

} // namespace Render::GL::Humanoid
