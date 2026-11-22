#pragma once

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "game/core/component.h"
#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_archetype_library.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/building_state.h"
#include "render/entity/nations/roman/building_architecture.h"
#include "render/entity/nations/roman/building_palette.h"
#include "render/geom/primitives.h"
#include "render/math/math_utils.h"

// The defense tower geometry, authored as BuildingArchetypeDescs and baked by
// the building archetype catalog. Nothing here draws or reads frame state.
namespace Render::GL::Roman {

// Palette and layout shared by the authoring and the defense tower renderer.
namespace defense_tower_detail {

using Render::Geom::clamp_vec_01;

struct TowerPalette {
  QVector3D limestone = BuildingPalette::k_limestone;
  QVector3D limestone_shade = BuildingPalette::k_limestone_shade;
  QVector3D limestone_dark = BuildingPalette::k_limestone_dark;
  QVector3D sandstone_light = BuildingPalette::k_limestone_shade;
  QVector3D sandstone_dark = BuildingPalette::k_limestone_dark;
  QVector3D sandstone_base = BuildingPalette::k_limestone;
  QVector3D marble = BuildingPalette::k_marble;
  QVector3D terracotta = BuildingPalette::k_terracotta;
  QVector3D terracotta_dark = BuildingPalette::k_terracotta_dark;
  QVector3D cedar = BuildingPalette::k_cedar;
  QVector3D cedar_dark = BuildingPalette::k_cedar_dark;
  QVector3D blue_accent = BuildingPalette::k_blue_accent;
  QVector3D blue_light = BuildingPalette::k_blue_light;
  QVector3D bronze = BuildingPalette::k_bronze;
  QVector3D gold = BuildingPalette::k_gold;
  QVector3D team{0.8F, 0.9F, 1.0F};
  QVector3D team_trim{0.48F, 0.54F, 0.60F};
};

inline auto make_palette(const QVector3D& team) -> TowerPalette {
  TowerPalette p;
  p.team = clamp_vec_01(team);
  p.team_trim =
      clamp_vec_01(QVector3D(team.x() * 0.6F, team.y() * 0.6F, team.z() * 0.6F));
  return p;
}

} // namespace defense_tower_detail

auto build_tower_desc(BuildingState state) -> BuildingArchetypeDesc;

} // namespace Render::GL::Roman
