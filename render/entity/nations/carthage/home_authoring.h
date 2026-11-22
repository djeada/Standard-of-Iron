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
#include "render/entity/building_facade_depth.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/building_state.h"
#include "render/entity/home_activity.h"
#include "render/entity/home_yard.h"
#include "render/entity/nations/carthage/building_palette.h"
#include "render/math/math_utils.h"

// The home geometry, authored as BuildingArchetypeDescs and baked by
// the building archetype catalog. Nothing here draws or reads frame state.
namespace Render::GL::Carthage {

// Palette and layout shared by the authoring and the home renderer.
namespace home_detail {

using Render::Geom::clamp_vec_01;

struct CarthagePalette {
  QVector3D stone_light = BuildingPalette::k_sandstone_light;
  QVector3D stone_dark = BuildingPalette::k_sandstone_dark;
  QVector3D stone_base = BuildingPalette::k_sandstone;
  QVector3D plaster = BuildingPalette::k_plaster;
  QVector3D indigo = BuildingPalette::k_indigo;
  QVector3D tile_red = BuildingPalette::k_tile_red;
  QVector3D wood = BuildingPalette::k_wood;
  QVector3D wood_dark = BuildingPalette::k_wood_dark;
  QVector3D bronze = BuildingPalette::k_bronze;
  QVector3D team{0.8F, 0.9F, 1.0F};
  QVector3D team_trim{0.48F, 0.54F, 0.60F};
};

inline auto make_palette(const QVector3D& team) -> CarthagePalette {
  CarthagePalette p;
  p.team = clamp_vec_01(team);
  p.team_trim =
      clamp_vec_01(QVector3D(team.x() * 0.6F, team.y() * 0.6F, team.z() * 0.6F));
  return p;
}

} // namespace home_detail

auto build_home_desc(BuildingState state, int variant = 0) -> BuildingArchetypeDesc;

} // namespace Render::GL::Carthage
