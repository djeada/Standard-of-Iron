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
#include "render/entity/nations/carthage/building_palette.h"
#include "render/geom/primitives.h"
#include "render/math/math_utils.h"

// The defense tower geometry, authored as BuildingArchetypeDescs and baked by
// the building archetype catalog. Nothing here draws or reads frame state.
namespace Render::GL::Carthage {

// Palette and layout shared by the authoring and the defense tower renderer.
namespace defense_tower_detail {

using Render::Geom::clamp_vec_01;

struct TowerPalette {
  QVector3D stone_light = BuildingPalette::k_sandstone_light;
  QVector3D stone_dark = BuildingPalette::k_sandstone_dark;
  QVector3D stone_base = BuildingPalette::k_sandstone;
  QVector3D brick = BuildingPalette::k_brick;
  QVector3D brick_dark = BuildingPalette::k_brick_dark;
  QVector3D tile_red = BuildingPalette::k_tile_red;
  QVector3D wood = BuildingPalette::k_wood;
  QVector3D wood_dark = BuildingPalette::k_wood_dark;
  QVector3D iron{0.24F, 0.23F, 0.21F};
  QVector3D bronze = BuildingPalette::k_bronze;
  QVector3D ember{0.68F, 0.22F, 0.045F};
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

} // namespace Render::GL::Carthage
