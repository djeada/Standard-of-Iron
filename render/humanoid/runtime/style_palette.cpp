#include "render/humanoid/runtime/style_palette.h"

#include <algorithm>
#include <optional>

namespace Render::GL::Humanoid {

namespace {

inline auto clamp01(float value) -> float {
  return std::clamp(value, 0.0F, 1.0F);
}

} // namespace

void apply_commander_palette(std::string_view renderer_key,
                             const QVector3D& team_tint,
                             HumanoidPalette& palette) {
  struct Dress {
    std::string_view key;
    QVector3D cloth;
    QVector3D metal;
    std::optional<QVector3D> skin{};
  };
  const Dress dresses[] = {
      {"troops/roman/commanders/fabius_maximus",
       {0.30F, 0.065F, 0.055F},
       {0.46F, 0.43F, 0.35F}},
      {"troops/roman/commanders/scipio_africanus",
       {0.46F, 0.045F, 0.065F},
       {0.66F, 0.51F, 0.28F}},
      {"troops/roman/commanders/marcellus",
       {0.27F, 0.045F, 0.05F},
       {0.35F, 0.36F, 0.37F}},
      {"troops/carthage/commanders/hanno_the_great",
       {0.25F, 0.105F, 0.25F},
       {0.59F, 0.46F, 0.27F}},
      {"troops/carthage/commanders/hasdrubal_barca",
       {0.055F, 0.19F, 0.18F},
       {0.39F, 0.38F, 0.29F}},
      {"troops/carthage/commanders/hannibal_barca",
       {0.11F, 0.085F, 0.13F},
       {0.44F, 0.36F, 0.24F}},

      {"troops/roman/commanders/sempronius_longus",
       {0.56F, 0.12F, 0.05F},
       {0.55F, 0.45F, 0.28F}},
      {"troops/roman/commanders/gaius_flaminius",
       {0.50F, 0.36F, 0.12F},
       {0.42F, 0.43F, 0.45F}},
      {"troops/roman/commanders/terentius_varro",
       {0.30F, 0.07F, 0.33F},
       {0.72F, 0.56F, 0.26F}},
      {"troops/roman/commanders/aemilius_paullus",
       {0.70F, 0.68F, 0.62F},
       {0.62F, 0.63F, 0.66F}},
      {"troops/roman/commanders/scipio_consul_218",
       {0.17F, 0.25F, 0.40F},
       {0.50F, 0.41F, 0.25F}},
      {"troops/carthage/commanders/mago_barca",
       {0.07F, 0.24F, 0.11F},
       {0.52F, 0.41F, 0.23F}},
      {"troops/carthage/commanders/maharbal",
       {0.05F, 0.28F, 0.29F},
       {0.56F, 0.47F, 0.28F}},
      {"troops/carthage/commanders/hanno_bomilcar",
       {0.11F, 0.11F, 0.34F},
       {0.46F, 0.41F, 0.31F}},
      {"troops/carthage/commanders/hasdrubal_cavalry",
       {0.10F, 0.09F, 0.10F},
       {0.30F, 0.30F, 0.32F}},
      {"troops/numidian/commanders/masinissa",
       {0.86F, 0.83F, 0.74F},
       {0.80F, 0.64F, 0.30F},
       QVector3D(0.36F, 0.24F, 0.16F)},
  };
  for (const auto& dress : dresses) {
    if (renderer_key != dress.key) {
      continue;
    }
    palette.cloth = blend_with_team(dress.cloth, team_tint, 0.08F);
    palette.metal = dress.metal;
    palette.leather = {0.23F, 0.14F, 0.095F};
    palette.leather_dark = {0.105F, 0.07F, 0.055F};
    if (dress.skin.has_value()) {
      palette.skin = *dress.skin;
    }
    return;
  }
}

auto saturate_color(const QVector3D& value) -> QVector3D {
  return {clamp01(value.x()), clamp01(value.y()), clamp01(value.z())};
}

auto blend_with_team(const QVector3D& base,
                     const QVector3D& team,
                     float team_weight) -> QVector3D {
  float const base_weight = 1.0F - clamp01(team_weight);
  float const team_contrib = clamp01(team_weight);
  auto mix_component = [&](float base_c, float team_c) -> float {
    return clamp01(base_c * base_weight + team_c * team_contrib);
  };

  return {mix_component(base.x(), team.x()),
          mix_component(base.y(), team.y()),
          mix_component(base.z(), team.z())};
}

auto mix_palette_color(const QVector3D& base_color,
                       const std::optional<QVector3D>& override_color,
                       const QVector3D& team_tint,
                       float team_weight,
                       float style_weight) -> QVector3D {
  if (!override_color) {
    return base_color;
  }

  QVector3D styled = blend_with_team(*override_color, team_tint, clamp01(team_weight));
  styled = saturate_color(styled);

  float const t = clamp01(style_weight);
  QVector3D mixed = base_color * (1.0F - t) + styled * t;
  return saturate_color(mixed);
}

} // namespace Render::GL::Humanoid
