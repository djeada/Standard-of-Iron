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
#include "render/entity/nations/carthage/building_palette.h"
#include "render/frame/render_archetype.h"
#include "render/geom/primitives.h"
#include "render/math/math_utils.h"

namespace Render::GL::Carthage {

namespace barracks_detail {

using Render::Geom::clamp_vec_01;

struct CarthagePalette {
  QVector3D sandstone = BuildingPalette::k_sandstone;
  QVector3D sandstone_light = BuildingPalette::k_sandstone_light;
  QVector3D sandstone_dark = BuildingPalette::k_sandstone_dark;
  QVector3D stone_dark = BuildingPalette::k_sandstone_dark;
  QVector3D plaster = BuildingPalette::k_plaster;
  QVector3D plaster_shade = BuildingPalette::k_plaster_shade;
  QVector3D brick_dark = BuildingPalette::k_brick_dark;
  QVector3D indigo = BuildingPalette::k_indigo;
  QVector3D oxblood = BuildingPalette::k_oxblood;
  QVector3D saffron = BuildingPalette::k_saffron;
  QVector3D wood = BuildingPalette::k_wood;
  QVector3D wood_dark = BuildingPalette::k_wood_dark;
  QVector3D iron{0.24F, 0.23F, 0.21F};
  QVector3D bronze = BuildingPalette::k_bronze;
  QVector3D ember{0.68F, 0.22F, 0.045F};
  QVector3D flame{0.95F, 0.55F, 0.12F};
  QVector3D shadow{0.10F, 0.08F, 0.06F};
  QVector3D sack{0.72F, 0.62F, 0.44F};
  QVector3D frond{0.30F, 0.46F, 0.20F};
  QVector3D frond_dark{0.20F, 0.34F, 0.14F};
  QVector3D palm_trunk{0.47F, 0.37F, 0.24F};
  QVector3D date{0.62F, 0.34F, 0.10F};
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

constexpr float k_platform_top = 0.22F;

constexpr float k_keep_top = 1.95F;

const QVector3D k_oven_vent{0.82F, k_keep_top + 0.30F, -1.02F};

const BarracksGrounds k_grounds{.parade_centre = QVector3D(-2.92F, 0.0F, 0.10F),
                                .parade_half_x = 0.92F,
                                .parade_half_z = 1.30F,
                                .smithy_centre = QVector3D(0.0F, 0.0F, -2.20F)};

} // namespace barracks_detail

auto build_barracks_desc(BuildingState state) -> BuildingArchetypeDesc;
auto build_barracks_grounds_desc(BuildingState state) -> BuildingArchetypeDesc;
auto build_barracks_palms_desc(BuildingState state) -> BuildingArchetypeDesc;
auto build_barracks_firs_desc(BuildingState state) -> BuildingArchetypeDesc;

} // namespace Render::GL::Carthage
