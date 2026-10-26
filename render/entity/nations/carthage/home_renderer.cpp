#include "home_renderer.h"

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "building_palette.h"
#include "game/core/component.h"
#include "math/math_utils.h"
#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_archetype_library.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_facade_depth.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/building_render_common.h"
#include "render/entity/building_state.h"
#include "render/entity/home_activity.h"
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

constexpr std::array<QVector3D, 5> k_roof_finishes{
    BuildingPalette::k_tile_red,
    QVector3D(0.80F, 0.76F, 0.66F),
    QVector3D(0.70F, 0.52F, 0.33F),
    QVector3D(0.47F, 0.29F, 0.22F),
    QVector3D(0.73F, 0.52F, 0.43F),
};

auto home_palette_slots(const QVector3D& team, std::uint32_t seed, BuildingState state)
    -> std::array<QVector3D, k_home_palette_slots> {
  const auto palette = make_palette(team);
  QVector3D roof = k_roof_finishes[home_tint_pick(seed, k_roof_finishes.size())] *
                   (1.0F + home_tint_jitter(seed) * 0.08F);
  roof = decayed_color(clamp_vec_01(roof), state, static_cast<int>(seed & 0xFFFFU));
  return {palette.team, roof};
}

struct CarthageHomeTraits {
  float wall_height{0.9F};
  bool stepped_parapet{false};
  bool light_well{false};
  bool roof_planters{false};
  bool cistern{false};
  QVector3D awning{BuildingPalette::k_indigo};
  float awning_center_z{-0.08F};
  float awning_half_z{0.43F};
};

auto carthage_home_traits(int variant) -> CarthageHomeTraits {
  switch (variant) {
  case 3:
    return {.wall_height = 1.0F,
            .stepped_parapet = true,
            .cistern = true,
            .awning = BuildingPalette::k_oxblood,
            .awning_center_z = 0.04F,
            .awning_half_z = 0.31F};
  case 4:
    return {.light_well = true,
            .awning = BuildingPalette::k_saffron,
            .awning_center_z = 0.21F,
            .awning_half_z = 0.21F};
  case 5:
    return {.wall_height = 0.98F,
            .roof_planters = true,
            .awning_center_z = 0.02F,
            .awning_half_z = 0.33F};
  default:
    return {};
  }
}

auto build_home_desc_impl(BuildingState state, int variant) -> BuildingArchetypeDesc {
  CarthagePalette const c = make_palette(QVector3D(1.0F, 1.0F, 1.0F));
  CarthageHomeTraits const traits = carthage_home_traits(variant);
  float const wall_height = traits.wall_height;
  float height_multiplier = 1.0F;

  if (state == BuildingState::Damaged) {
    height_multiplier = 0.7F;
  } else if (state == BuildingState::Destroyed) {
    height_multiplier = 0.4F;
  }

  BuildingArchetypeDesc desc(variant == 0
                                 ? "carthage_home"
                                 : "carthage_home_variant_" + std::to_string(variant));

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

  const float stringcourse_y = 0.20F + wall_height * height_multiplier - 0.12F;
  for (const float side : {-1.0F, 1.0F}) {
    desc.add_box(QVector3D(0.0F, stringcourse_y, side * 1.03F),
                 QVector3D(0.90F, 0.025F, 0.025F),
                 c.stone_base,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(side * 1.03F, stringcourse_y, 0.0F),
                 QVector3D(0.025F, 0.025F, 0.90F),
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
  if (traits.light_well) {
    BuildingPartLabel const label(desc, "light_well_roof");
    constexpr float k_well_x0 = -0.62F;
    constexpr float k_well_x1 = 0.10F;
    constexpr float k_well_z0 = -0.62F;
    constexpr float k_well_z1 = -0.08F;
    desc.add_palette_box(QVector3D(0.0F, roof_y, (-1.02F + k_well_z0) * 0.5F),
                         QVector3D(1.02F, 0.05F, (k_well_z0 + 1.02F) * 0.5F),
                         k_home_roof_slot,
                         k_building_state_mask_intact);
    desc.add_palette_box(QVector3D(0.0F, roof_y + 0.0004F, (k_well_z1 + 1.02F) * 0.5F),
                         QVector3D(1.02F, 0.05F, (1.02F - k_well_z1) * 0.5F),
                         k_home_roof_slot,
                         k_building_state_mask_intact);
    float const mid_z = (k_well_z0 + k_well_z1) * 0.5F;
    float const half_z = (k_well_z1 - k_well_z0) * 0.5F;
    desc.add_palette_box(
        QVector3D((-1.02F + k_well_x0) * 0.5F, roof_y + 0.0008F, mid_z),
        QVector3D((k_well_x0 + 1.02F) * 0.5F, 0.05F, half_z),
        k_home_roof_slot,
        k_building_state_mask_intact);
    desc.add_palette_box(QVector3D((k_well_x1 + 1.02F) * 0.5F, roof_y + 0.0012F, mid_z),
                         QVector3D((1.02F - k_well_x1) * 0.5F, 0.05F, half_z),
                         k_home_roof_slot,
                         k_building_state_mask_intact);
    float const lip_y = roof_y + 0.0695F;
    float const mid_x = (k_well_x0 + k_well_x1) * 0.5F;
    float const half_x = (k_well_x1 - k_well_x0) * 0.5F;
    for (float const side : {-1.0F, 1.0F}) {
      desc.add_box(QVector3D(mid_x, lip_y, mid_z + side * (half_z + 0.026F)),
                   QVector3D(half_x + 0.052F, 0.018F, 0.026F),
                   c.stone_light,
                   k_building_state_mask_intact);
      desc.add_box(QVector3D(mid_x + side * (half_x + 0.026F), lip_y - 0.001F, mid_z),
                   QVector3D(0.025F, 0.017F, half_z - 0.001F),
                   c.stone_light,
                   k_building_state_mask_intact);
    }
    BuildingPartMaterial const plant(desc, k_building_material_plant);
    QVector3D const tree(mid_x + 0.02F, 0.215F, mid_z - 0.02F);
    desc.add_cylinder(tree,
                      tree + QVector3D(0.0F, roof_y - 0.10F, 0.0F),
                      0.030F,
                      c.wood_dark,
                      k_building_state_mask_intact);
    desc.add_cone(tree + QVector3D(0.0F, roof_y - 0.42F, 0.0F),
                  tree + QVector3D(0.0F, roof_y + 0.12F, 0.0F),
                  0.22F,
                  QVector3D(0.27F, 0.42F, 0.17F),
                  k_building_state_mask_intact);
    desc.add_cone(tree + QVector3D(0.0F, roof_y - 0.05F, 0.0F),
                  tree + QVector3D(0.0F, roof_y + 0.36F, 0.0F),
                  0.16F,
                  QVector3D(0.33F, 0.50F, 0.20F),
                  k_building_state_mask_intact);
  } else {
    desc.add_palette_box(QVector3D(0.0F, roof_y, 0.0F),
                         QVector3D(1.02F, 0.05F, 1.02F),
                         k_home_roof_slot,
                         k_building_state_mask_intact);
  }

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
                 QVector3D(1.02F, 0.016F, 0.065F),
                 c.stone_light,
                 k_building_state_mask_intact);

    desc.add_box(QVector3D(side * 0.96F, parapet_y + 0.11F, 0.0F),
                 QVector3D(0.065F, 0.016F, 0.895F),
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
                             .thickness = 0.10F,
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
                             .thickness = 0.09F,
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
      float const post_z = traits.light_well ? (z < 0.0F ? 0.02F : 0.40F) : z;
      desc.add_cylinder(QVector3D(x, roof_y + 0.05F, post_z),
                        QVector3D(x, roof_y + 0.48F, post_z),
                        0.025F,
                        c.wood,
                        BuildingStateMask::Normal);
    }
  }
  if (traits.stepped_parapet) {
    BuildingPartLabel const label(desc, "stepped_parapet");
    float const cap_top = roof_y + 0.13F + 0.11F + 0.016F;
    for (int i = 0; i < 7; ++i) {
      float const along = -0.84F + 0.28F * static_cast<float>(i);
      float const tall = (i % 2 == 0) ? 0.070F : 0.045F;
      for (float const side : {-1.0F, 1.0F}) {
        desc.add_box(QVector3D(along, cap_top + 0.001F + tall, side * 0.96F),
                     QVector3D(0.062F, tall, 0.048F),
                     weathered(c.plaster, i * 2 + (side < 0.0F ? 0 : 1)),
                     k_building_state_mask_intact);
        if (std::abs(along) < 0.80F) {
          desc.add_box(QVector3D(side * 0.96F, cap_top + 0.0015F + tall, along),
                       QVector3D(0.047F, tall - 0.0005F, 0.061F),
                       weathered(c.plaster, i * 2 + 31 + (side < 0.0F ? 0 : 1)),
                       k_building_state_mask_intact);
        }
      }
    }
  }
  if (traits.cistern) {
    BuildingPartLabel const label(desc, "roof_cistern");
    QVector3D const base(0.66F, roof_y + 0.05F, 0.10F);
    desc.add_cylinder(base,
                      base + QVector3D(0.0F, 0.20F, 0.0F),
                      0.15F,
                      c.stone_base,
                      k_building_state_mask_intact);
    desc.add_cylinder(base + QVector3D(0.0F, 0.20F, 0.0F),
                      base + QVector3D(0.0F, 0.225F, 0.0F),
                      0.165F,
                      c.stone_light,
                      k_building_state_mask_intact);
  }
  if (traits.roof_planters) {
    BuildingPartLabel const label(desc, "roof_planters");
    for (int pot = 0; pot < 4; ++pot) {
      QVector3D const base(
          -0.75F + 0.5F * static_cast<float>(pot), roof_y + 0.05F, 0.78F);
      desc.add_cylinder(base,
                        base + QVector3D(0.0F, 0.11F, 0.0F),
                        0.065F,
                        QVector3D(0.66F, 0.42F, 0.27F),
                        k_building_state_mask_intact);
      BuildingPartMaterial const plant(desc, k_building_material_plant);
      desc.add_cone(
          base + QVector3D(0.0F, 0.10F, 0.0F),
          base + QVector3D(0.0F, 0.26F + 0.04F * static_cast<float>(pot % 2), 0.0F),
          0.085F,
          QVector3D(0.26F + 0.03F * static_cast<float>(pot % 3), 0.43F, 0.18F),
          k_building_state_mask_intact);
    }
  }
  if (variant == 1) {
    BuildingPartLabel const label(desc, "terrace_pergola");
    BuildingPartMaterial const timber(desc, k_building_material_wood);
    for (float const x : {-0.72F, 0.02F}) {
      desc.add_box(QVector3D(x, roof_y + 0.47F, -0.08F),
                   QVector3D(0.030F, 0.027F, 0.46F),
                   c.wood_dark,
                   BuildingStateMask::Normal);
    }
    for (int slat = 0; slat < 10; ++slat) {
      desc.add_box(QVector3D(-0.35F, roof_y + 0.515F, -0.49F + slat * 0.09F),
                   QVector3D(0.43F, 0.020F, 0.025F),
                   c.wood,
                   BuildingStateMask::Normal);
    }
  } else if (variant == 2) {
    BuildingPartLabel const label(desc, "terrace_upper_room");

    desc.add_box(QVector3D(-0.40F, roof_y + 0.32F, -0.48F),
                 QVector3D(0.34F, 0.27F, 0.29F),
                 c.plaster,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(-0.40F, roof_y + 0.62F, -0.48F),
                 QVector3D(0.38F, 0.035F, 0.33F),
                 c.stone_light,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(-0.40F, roof_y + 0.32F, -0.184F),
                 QVector3D(0.12F, 0.21F, 0.012F),
                 c.wood_dark,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(-0.35F, roof_y + 0.49F, 0.17F),
                 QVector3D(0.40F, 0.012F, 0.18F),
                 c.indigo * 0.85F,
                 BuildingStateMask::Normal);
  } else {
    BuildingPartMaterial const cloth(desc, k_building_material_cloth);
    desc.add_box(QVector3D(-0.35F, roof_y + 0.49F, traits.awning_center_z),
                 QVector3D(0.40F, 0.012F, traits.awning_half_z),
                 traits.awning,
                 BuildingStateMask::Normal);
  }

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
  add_home_yard(desc,
                HomeYardStyle{.plinth_half = 1.14F,
                              .wall_half = 1.04F,
                              .door_half_width = 0.40F,
                              .vent = QVector3D(0.48F, 1.70F, -0.46F),
                              .chimney_base_y = roof_y + 0.04F,
                              .clay_oven_chimney = true,
                              .stone = c.stone_base,
                              .clay = QVector3D(0.68F, 0.43F, 0.26F),
                              .clay_dark = c.indigo,
                              .chimney = c.stone_light,
                              .seed = 7 + variant * 19,
                              .variant = variant,
                              .wall_foot_y = 0.20F,
                              .wall_face = 1.04F,
                              .wall_foot_door_gap = 0.36F,
                              .wall_foot = c.stone_dark * 0.95F});

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

auto home_archetype(BuildingState state, int variant) -> const RenderArchetype& {
  static const std::array<const BuildingArchetypeSet*, k_home_visual_variant_count>
      k_sets{&building_archetype_set("carthage_home"),
             &building_archetype_set("carthage_home_variant_1"),
             &building_archetype_set("carthage_home_variant_2"),
             &building_archetype_set("carthage_home_variant_3"),
             &building_archetype_set("carthage_home_variant_4"),
             &building_archetype_set("carthage_home_variant_5")};
  return k_sets[static_cast<std::size_t>(variant)]->for_state(state);
}

const std::array<TorchMount, 2> k_torches{{
    TorchMount{.at = QVector3D(-0.5F, 0.62F, 1.02F),
               .outward = QVector3D(0.0F, 0.0F, 1.0F)},
    TorchMount{.at = QVector3D(0.5F, 0.62F, 1.02F),
               .outward = QVector3D(0.0F, 0.0F, 1.0F)},
}};

} // namespace

auto build_home_desc(BuildingState state, int variant) -> BuildingArchetypeDesc {
  return build_home_desc_impl(state, variant);
}

void register_home_renderer(Render::GL::EntityRendererRegistry& registry) {

  register_home_smoke_anchor(
      true,
      HomeSmokeAnchor{.vent = QVector3D(0.48F, 1.70F, -0.46F),
                      .smoke_tint = QVector3D(0.54F, 0.52F, 0.49F),
                      .plume_radius = 0.60F,
                      .doorstep = QVector3D(0.0F, 0.16F, 1.22F),
                      .outward = QVector3D(0.0F, 0.0F, 1.0F),

                      .cloth_top = QVector3D(-0.35F, 1.72F, 0.34F),
                      .cloth_span = 0.74F,
                      .cloth_indigo = true,
                      .cloth_at_door = false,
                      .lamp = QVector3D(0.0F, 0.165F, 1.20F),

                      .window = QVector3D(1.045F, 0.58F, 0.32F),
                      .shutter_width = 0.07F,
                      .shutter_height = 0.44F,

                      .line_a = QVector3D(0.0F, 1.64F, 0.30F),
                      .line_b = QVector3D(0.82F, 1.64F, 0.62F),
                      .line_pole = true,
                      .max_laundry = 3});
  register_home_renderer_variant(
      registry,
      HomeRendererConfig{.nation_slug = "carthage",
                         .archetype = &home_archetype,
                         .palette_slots = &home_palette_slots,
                         .selection = BuildingSelectionStyle{2.1F, 2.1F},
                         .torches = k_torches});
}

} // namespace Render::GL::Carthage
