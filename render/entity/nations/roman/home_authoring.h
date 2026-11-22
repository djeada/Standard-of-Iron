#pragma once

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "game/core/component.h"
#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_archetype_library.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/building_state.h"
#include "render/entity/home_activity.h"
#include "render/entity/home_yard.h"
#include "render/entity/nations/roman/building_architecture.h"
#include "render/entity/nations/roman/building_palette.h"
#include "render/math/math_utils.h"

// The home geometry, authored as BuildingArchetypeDescs and baked by
// the building archetype catalog. Nothing here draws or reads frame state.
namespace Render::GL::Roman {

// Palette and layout shared by the authoring and the home renderer.
namespace home_detail {

using Render::Geom::clamp_vec_01;

struct RomanPalette {
  QVector3D limestone = BuildingPalette::k_limestone;
  QVector3D limestone_shade = BuildingPalette::k_limestone_shade;
  QVector3D limestone_dark = BuildingPalette::k_limestone_dark;
  QVector3D marble = BuildingPalette::k_marble;
  QVector3D cedar_dark = BuildingPalette::k_cedar_dark;
  QVector3D terracotta = BuildingPalette::k_terracotta;
  QVector3D terracotta_dark = BuildingPalette::k_terracotta_dark;
  QVector3D blue_accent = BuildingPalette::k_blue_accent;
  QVector3D blue_light = BuildingPalette::k_blue_light;
  QVector3D team{0.8F, 0.9F, 1.0F};
  QVector3D team_trim{0.48F, 0.54F, 0.60F};
};

inline auto make_palette(const QVector3D& team) -> RomanPalette {
  RomanPalette p;
  p.team = clamp_vec_01(team);
  p.team_trim =
      clamp_vec_01(QVector3D(team.x() * 0.6F, team.y() * 0.6F, team.z() * 0.6F));
  return p;
}

} // namespace home_detail

auto build_home_desc(BuildingState state, int variant = 0) -> BuildingArchetypeDesc;

} // namespace Render::GL::Roman
