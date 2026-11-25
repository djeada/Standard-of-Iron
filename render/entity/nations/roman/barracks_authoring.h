#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>

#include "game/core/component.h"
#include "game/visuals/team_colors.h"
#include "render/entity/barracks_activity.h"
#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_archetype_library.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/building_state.h"
#include "render/entity/nations/roman/building_architecture.h"
#include "render/entity/nations/roman/building_palette.h"
#include "render/frame/render_archetype.h"
#include "render/geom/primitives.h"
#include "render/math/math_utils.h"

namespace Render::GL::Roman {

namespace barracks_detail {

using Render::Geom::clamp_vec_01;

struct RomanPalette {
  QVector3D plaster = BuildingPalette::k_plaster;
  QVector3D plaster_shade = BuildingPalette::k_plaster_shade;
  QVector3D limestone = BuildingPalette::k_limestone;
  QVector3D limestone_shade = BuildingPalette::k_limestone_shade;
  QVector3D limestone_dark = BuildingPalette::k_limestone_dark;
  QVector3D cedar = BuildingPalette::k_cedar;
  QVector3D cedar_dark = BuildingPalette::k_cedar_dark;
  QVector3D oak{0.46F, 0.39F, 0.30F};
  QVector3D terracotta = BuildingPalette::k_terracotta;
  QVector3D terracotta_dark = BuildingPalette::k_terracotta_dark;
  QVector3D terracotta_light = BuildingPalette::k_terracotta_light;
  QVector3D dado{0.53F, 0.10F, 0.07F};
  QVector3D blue_accent = BuildingPalette::k_blue_accent;
  QVector3D iron{0.30F, 0.30F, 0.32F};
  QVector3D bronze = BuildingPalette::k_bronze;
  QVector3D gold = BuildingPalette::k_gold;
  QVector3D shadow{0.12F, 0.09F, 0.07F};
  QVector3D water{0.30F, 0.42F, 0.50F};
  QVector3D turf{0.40F, 0.42F, 0.22F};
  QVector3D earth{0.46F, 0.36F, 0.24F};
  QVector3D earth_dark{0.27F, 0.21F, 0.14F};
  QVector3D leather{0.42F, 0.30F, 0.19F};
  QVector3D leather_dark{0.30F, 0.20F, 0.12F};
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

const QVector3D k_cookhouse_vent{0.86F, 2.17F, -0.64F};

const BarracksGrounds k_grounds{.parade_centre = QVector3D(-2.86F, 0.0F, 0.10F),
                                .parade_half_x = 0.92F,
                                .parade_half_z = 1.30F,
                                .smithy_centre = QVector3D(-0.30F, 0.0F, -2.24F)};

} // namespace barracks_detail

auto build_barracks_desc(BuildingState state) -> BuildingArchetypeDesc;
auto build_barracks_grounds_desc(BuildingState state) -> BuildingArchetypeDesc;

} // namespace Render::GL::Roman
