#include "home_renderer.h"

#include <QVector3D>

#include <algorithm>
#include <array>

#include "building_palette.h"
#include "game/core/component.h"
#include "math/math_utils.h"
#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_facade_depth.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/building_render_common.h"
#include "render/entity/building_state.h"
#include "render/entity/home_renderer_common.h"
#include "render/entity/registry.h"
#include "render/submitter.h"

namespace Render::GL::Carthage {
namespace {

using Render::Geom::clamp_vec_01;

constexpr std::uint8_t k_home_team_slot = 0;
constexpr std::uint8_t k_home_roof_slot = 1;

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

auto home_palette_slots(const QVector3D& team)
    -> std::array<QVector3D, k_home_palette_slots> {
  const auto palette = make_palette(team);
  return {palette.team, palette.tile_red};
}

auto build_home_desc_impl(BuildingState state) -> BuildingArchetypeDesc {
  CarthagePalette const c = make_palette(QVector3D(1.0F, 1.0F, 1.0F));
  float const wall_height = 0.9F;
  float height_multiplier = 1.0F;

  if (state == BuildingState::Damaged) {
    height_multiplier = 0.7F;
  } else if (state == BuildingState::Destroyed) {
    height_multiplier = 0.4F;
  }

  BuildingArchetypeDesc desc("carthage_home");

  constexpr float k_front_wall_face = 1.02F;

  desc.add_box(
      QVector3D(0.0F, 0.06F, 0.0F), QVector3D(1.14F, 0.06F, 1.14F), c.stone_dark);
  desc.add_box(
      QVector3D(0.0F, 0.14F, 0.0F), QVector3D(1.06F, 0.02F, 1.06F), c.stone_base);
  desc.add_box(
      QVector3D(0.0F, 0.18F, 0.0F), QVector3D(1.00F, 0.02F, 1.00F), c.stone_light);

  float const wall_cy = wall_height * 0.5F * height_multiplier + 0.20F;
  float const wall_hy = wall_height * 0.5F * height_multiplier;
  desc.add_box(
      QVector3D(0.0F, wall_cy, -0.92F), QVector3D(0.88F, wall_hy, 0.10F), c.plaster);
  desc.add_box(
      QVector3D(0.0F, wall_cy, 0.92F), QVector3D(0.88F, wall_hy, 0.10F), c.plaster);
  desc.add_box(
      QVector3D(-0.92F, wall_cy, 0.0F), QVector3D(0.10F, wall_hy, 0.82F), c.plaster);
  desc.add_box(
      QVector3D(0.92F, wall_cy, 0.0F), QVector3D(0.10F, wall_hy, 0.82F), c.plaster);

  for (const float side : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(0.0F, 0.28F, side * 0.92F),
                 QVector3D(0.90F, 0.06F, 0.12F),
                 c.stone_base,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(side * 0.92F, 0.28F, 0.0F),
                 QVector3D(0.12F, 0.06F, 0.86F),
                 c.stone_base,
                 k_building_state_mask_intact);
  }

  float const post_hy = wall_height * 0.5F * height_multiplier;
  float const post_cy = post_hy + 0.20F;
  for (float const px : {-0.96F, 0.96F}) {
    for (float const pz : {-0.96F, 0.96F}) {
      desc.add_box(QVector3D(px, post_cy, pz),
                   QVector3D(0.08F, post_hy + 0.04F, 0.08F),
                   c.stone_light,
                   k_building_state_mask_intact);

      desc.add_box(QVector3D(px, post_cy + post_hy + 0.06F, pz),
                   QVector3D(0.10F, 0.03F, 0.10F),
                   c.stone_dark,
                   k_building_state_mask_intact);
    }
  }

  float const roof_y = wall_height * height_multiplier + 0.24F;
  desc.add_palette_box(QVector3D(0.0F, roof_y, 0.0F),
                       QVector3D(1.02F, 0.05F, 1.02F),
                       k_home_roof_slot,
                       k_building_state_mask_intact);

  const float parapet_y = roof_y + 0.13F;
  for (const float side : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(0.0F, parapet_y, side * 0.96F),
                 QVector3D(1.00F, 0.10F, 0.055F),
                 c.plaster,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(side * 0.96F, parapet_y, 0.0F),
                 QVector3D(0.055F, 0.10F, 0.90F),
                 c.plaster,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(0.0F, parapet_y + 0.11F, side * 0.96F),
                 QVector3D(1.02F, 0.025F, 0.065F),
                 c.stone_light,
                 k_building_state_mask_intact);

    desc.add_box(QVector3D(side * 0.96F, parapet_y + 0.11F, 0.0F),
                 QVector3D(0.065F, 0.025F, 0.895F),
                 c.stone_light,
                 k_building_state_mask_intact);
  }

  {
    BuildingPartLabel const label(desc, "front_door");
    add_facade_box(desc,
                   FacadeBox{.normal = FacadeNormal::PlusZ,
                             .surface = k_front_wall_face,
                             .layer = BuildingDepth::k_inlay,
                             .thickness = 0.10F,
                             .center = QVector3D(0.0F, 0.44F, 0.0F),
                             .half_size = QVector3D(0.32F, 0.44F, 0.0F)},
                   c.wood_dark);
  }
  {
    BuildingPartLabel const label(desc, "front_door_lintel");
    add_facade_box(desc,
                   FacadeBox{.normal = FacadeNormal::PlusZ,
                             .surface = k_front_wall_face,
                             .layer = BuildingDepth::k_trim,
                             .thickness = 0.16F,
                             .center = QVector3D(0.0F, 0.92F, 0.0F),
                             .half_size = QVector3D(0.36F, 0.05F, 0.0F)},
                   c.stone_light,
                   BuildingStateMask::All);
  }

  for (const float side : {-1.0F, 1.0F}) {
    BuildingPartLabel const label(desc, "front_door_jamb");
    add_facade_box(desc,
                   FacadeBox{.normal = FacadeNormal::PlusZ,
                             .surface = k_front_wall_face,
                             .layer = BuildingDepth::k_trim,
                             .thickness = 0.14F,
                             .center = QVector3D(side * 0.34F, 0.535F, 0.0F),
                             .half_size = QVector3D(0.04F, 0.335F, 0.0F)},
                   c.stone_light,
                   k_building_state_mask_intact);
  }

  desc.add_box(
      QVector3D(0.0F, 0.10F, 1.06F), QVector3D(0.40F, 0.04F, 0.14F), c.stone_light);
  desc.add_box(
      QVector3D(0.0F, 0.16F, 1.02F), QVector3D(0.36F, 0.02F, 0.10F), c.stone_base);

  for (float const xw : {-1.025F, 1.025F}) {
    desc.add_box(QVector3D(xw, 0.58F, -0.32F),
                 QVector3D(0.015F, 0.22F, 0.06F),
                 c.wood_dark,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(xw, 0.58F, 0.32F),
                 QVector3D(0.015F, 0.22F, 0.06F),
                 c.wood_dark,
                 k_building_state_mask_intact);

    desc.add_box(QVector3D(xw, 0.82F, -0.32F),
                 QVector3D(0.02F, 0.03F, 0.09F),
                 c.stone_light,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(xw, 0.82F, 0.32F),
                 QVector3D(0.02F, 0.03F, 0.09F),
                 c.stone_light,
                 k_building_state_mask_intact);
  }

  desc.add_box(QVector3D(0.0F, 0.21F, 0.0F),
               QVector3D(0.62F, 0.005F, 0.62F),
               c.stone_dark,
               k_building_state_mask_intact);
  desc.add_cylinder(QVector3D(0.0F, 0.22F, 0.0F),
                    QVector3D(0.0F, 0.38F, 0.0F),
                    0.18F,
                    c.stone_light,
                    k_building_state_mask_intact);
  desc.add_box(QVector3D(0.0F, 0.39F, 0.0F),
               QVector3D(0.22F, 0.02F, 0.22F),
               c.stone_base,
               k_building_state_mask_intact);

  desc.add_cylinder(QVector3D(0.0F, 0.24F, 0.0F),
                    QVector3D(0.0F, 0.25F, 0.0F),
                    0.14F,
                    QVector3D(0.25F, 0.42F, 0.55F),
                    k_building_state_mask_intact);

  desc.add_box(QVector3D(0.48F, roof_y + 0.22F, -0.46F),
               QVector3D(0.24F, 0.17F, 0.24F),
               c.plaster,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.48F, roof_y + 0.41F, -0.46F),
               QVector3D(0.27F, 0.025F, 0.27F),
               c.stone_light,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.48F, roof_y + 0.19F, -0.214F),
               QVector3D(0.075F, 0.14F, 0.008F),
               c.wood_dark,
               k_building_state_mask_intact);
  for (const float x : {-0.70F, 0.0F}) {
    for (const float z : {-0.46F, 0.30F}) {
      desc.add_cylinder(QVector3D(x, roof_y + 0.05F, z),
                        QVector3D(x, roof_y + 0.48F, z),
                        0.025F,
                        c.wood,
                        BuildingStateMask::Normal);
    }
  }
  desc.add_box(QVector3D(-0.35F, roof_y + 0.49F, -0.08F),
               QVector3D(0.40F, 0.012F, 0.43F),
               c.indigo,
               BuildingStateMask::Normal);

  desc.add_box(QVector3D(0.0F, 0.52F, -0.94F),
               QVector3D(0.22F, 0.28F, 0.02F),
               c.stone_dark,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.0F, 0.82F, -0.94F),
               QVector3D(0.26F, 0.04F, 0.03F),
               c.stone_light,
               k_building_state_mask_intact);

  {

    BuildingPartLabel const label(desc, "front_door_team_panel");
    add_facade_palette_box(desc,
                           FacadeBox{.normal = FacadeNormal::PlusZ,
                                     .surface = k_front_wall_face,
                                     .layer = BuildingDepth::k_panel,
                                     .thickness = 0.05F,
                                     .center = QVector3D(0.0F, 0.76F, 0.0F),
                                     .half_size = QVector3D(0.30F, 0.10F, 0.0F)},
                           k_home_team_slot,
                           BuildingStateMask::All);
  }

  add_punic_tanit_relief(desc,
                         QVector3D(1.025F, 0.80F, 0.0F),
                         BuildingFacadePlane::ZY,
                         0.28F,
                         c.bronze,
                         c.stone_dark);
  add_ruin_dressing(desc,
                    RuinDressing{.extent = QVector3D(0.94F, 0.0F, 0.94F),
                                 .stone = c.stone_base,
                                 .stone_dark = c.stone_dark,
                                 .timber = c.stone_dark * 0.5F,
                                 .ground_y = 0.2F,
                                 .scale = 1.0F,
                                 .seed = 137});

  return desc;
}

auto build_home_archetype(BuildingState state) -> RenderArchetype {
  return build_building_archetype(build_home_desc_impl(state), state);
}

auto home_archetype(BuildingState state) -> const RenderArchetype& {
  static const BuildingArchetypeSet k_set =
      build_stateful_building_archetype_set(build_home_archetype);
  return k_set.for_state(state);
}

} // namespace

auto build_home_desc(BuildingState state) -> BuildingArchetypeDesc {
  return build_home_desc_impl(state);
}

void register_home_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_home_renderer_variant(
      registry,
      HomeRendererConfig{.nation_slug = "carthage",
                         .archetype = &home_archetype,
                         .palette_slots = &home_palette_slots,
                         .selection = BuildingSelectionStyle{2.1F, 2.1F}});
}

} // namespace Render::GL::Carthage
