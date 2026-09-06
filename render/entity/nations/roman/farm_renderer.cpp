#include "farm_renderer.h"

#include <QVector3D>

#include <cmath>
#include <string>

#include "building_palette.h"
#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/building_render_common.h"
#include "render/entity/building_state.h"
#include "render/entity/farm_renderer_common.h"
#include "render/entity/registry.h"
#include "render/render_archetype.h"

namespace Render::GL::Roman {
namespace {

struct RomanFarmPalette {
  QVector3D limestone = BuildingPalette::k_limestone;
  QVector3D limestone_shade = BuildingPalette::k_limestone_shade;
  QVector3D limestone_dark = BuildingPalette::k_limestone_dark;
  QVector3D plaster = BuildingPalette::k_plaster;
  QVector3D plaster_shade = BuildingPalette::k_plaster_shade;
  QVector3D terracotta = BuildingPalette::k_terracotta;
  QVector3D terracotta_dark = BuildingPalette::k_terracotta_dark;
  QVector3D cedar = BuildingPalette::k_cedar;
  QVector3D cedar_light = BuildingPalette::k_cedar_light;
  QVector3D cedar_dark = BuildingPalette::k_cedar_dark;
  QVector3D iron{0.16F, 0.16F, 0.16F};
  QVector3D straw{0.76F, 0.62F, 0.31F};
  QVector3D cloth{0.46F, 0.11F, 0.08F};
  QVector3D clay{0.62F, 0.39F, 0.23F};
  QVector3D clay_band{0.36F, 0.16F, 0.10F};
  QVector3D grain{0.81F, 0.67F, 0.33F};
};

void add_stone_boundary(BuildingArchetypeDesc& desc, const RomanFarmPalette& c) {

  constexpr float k_wall_h = 0.055F;
  constexpr float k_wall_half = 0.030F;
  auto add_run = [&](const QVector3D& start,
                     const QVector3D& end,
                     int blocks,
                     int seed) {
    QVector3D const step = (end - start) / static_cast<float>(blocks);
    for (int i = 0; i < blocks; ++i) {
      QVector3D const centre = start + step * (static_cast<float>(i) + 0.5F);
      float const jitter = (decay_hash(seed + i) - 0.5F) * 0.012F;
      bool const horizontal = std::abs(step.x()) > std::abs(step.z());
      QVector3D const half(step.x() != 0.0F ? std::abs(step.x()) * 0.46F : k_wall_half,
                           k_wall_h * (0.85F + decay_hash(seed + i * 3) * 0.3F),
                           step.z() != 0.0F ? std::abs(step.z()) * 0.46F : k_wall_half);
      QVector3D const block_colour = (i % 3 == 0 ? c.limestone_shade : c.limestone) *
                                     (0.94F + decay_hash(seed + i * 7) * 0.10F);
      desc.add_rotated_box(
          centre + QVector3D(horizontal ? jitter * 0.35F : jitter,
                             half.y(),
                             horizontal ? jitter : jitter * 0.35F),
          half,
          QVector3D(0.0F, (decay_hash(seed + i * 13) - 0.5F) * 5.0F, 0.0F),
          block_colour,
          BuildingStateMask::All);
    }
  };

  add_run(QVector3D(-0.94F, 0.0F, -0.94F), QVector3D(0.94F, 0.0F, -0.94F), 14, 11);
  add_run(QVector3D(-0.94F, 0.0F, -0.94F), QVector3D(-0.94F, 0.0F, 0.94F), 14, 41);
  add_run(QVector3D(0.94F, 0.0F, -0.94F), QVector3D(0.94F, 0.0F, 0.20F), 9, 71);
  add_run(QVector3D(0.94F, 0.0F, 0.42F), QVector3D(0.94F, 0.0F, 0.94F), 4, 91);
  add_run(QVector3D(-0.94F, 0.0F, 0.94F), QVector3D(0.94F, 0.0F, 0.94F), 14, 121);

  for (float const z : {0.20F, 0.42F}) {
    desc.add_cylinder(QVector3D(0.94F, 0.0F, z),
                      QVector3D(0.94F, 0.16F, z),
                      0.026F,
                      c.cedar_dark,
                      BuildingStateMask::All);
  }
}

void add_granary_shed(BuildingArchetypeDesc& desc, const RomanFarmPalette& c) {
  QVector3D const centre(0.50F, 0.0F, 0.70F);
  constexpr float k_half_x = 0.30F;
  constexpr float k_half_z = 0.19F;
  constexpr float k_wall_h = 0.30F;

  desc.add_box(centre + QVector3D(0.0F, 0.035F, 0.0F),
               QVector3D(k_half_x + 0.06F, 0.035F, k_half_z + 0.06F),
               c.limestone_dark,
               BuildingStateMask::All);
  desc.add_box(centre + QVector3D(0.0F, 0.09F, 0.0F),
               QVector3D(k_half_x + 0.03F, 0.02F, k_half_z + 0.03F),
               c.limestone,
               BuildingStateMask::All);

  desc.add_box(centre + QVector3D(0.0F, 0.11F + k_wall_h * 0.5F, 0.0F),
               QVector3D(k_half_x, k_wall_h * 0.5F, k_half_z),
               c.plaster,
               k_building_state_mask_intact);
  desc.add_box(centre + QVector3D(0.0F, 0.11F + 0.05F, 0.0F),
               QVector3D(k_half_x + 0.004F, 0.05F, k_half_z + 0.004F),
               c.plaster_shade,
               k_building_state_mask_intact);
  desc.add_box(centre + QVector3D(0.0F, 0.11F + 0.09F, 0.0F),
               QVector3D(k_half_x * 0.9F, 0.09F, k_half_z * 0.9F),
               c.limestone_shade,
               BuildingStateMask::Destroyed);

  for (float const sx : {-1.0F, 1.0F}) {
    desc.add_box(centre +
                     QVector3D(sx * (k_half_x - 0.02F), 0.11F + k_wall_h * 0.5F, 0.0F),
                 QVector3D(0.02F, k_wall_h * 0.5F, k_half_z + 0.01F),
                 c.cedar,
                 k_building_state_mask_intact);
  }

  desc.add_box(centre + QVector3D(0.0F, 0.11F + 0.11F, -k_half_z - 0.004F),
               QVector3D(0.075F, 0.11F, 0.006F),
               c.cedar_dark,
               k_building_state_mask_intact);
  desc.add_box(centre + QVector3D(0.0F, 0.11F + 0.11F, -k_half_z - 0.010F),
               QVector3D(0.010F, 0.11F, 0.004F),
               c.cedar_light,
               k_building_state_mask_intact);

  float const eave_y = 0.11F + k_wall_h;
  desc.add_box(centre + QVector3D(0.0F, eave_y + 0.01F, 0.0F),
               QVector3D(k_half_x + 0.05F, 0.012F, k_half_z + 0.05F),
               c.cedar,
               k_building_state_mask_intact);
  add_gable_roof_x(
      [&](const QVector3D& pos,
          const QVector3D& scale,
          const QVector3D& euler,
          const QVector3D& color) {
        desc.add_rotated_box(pos, scale, euler, color, k_building_state_mask_intact);
      },
      centre.x(),
      centre.z(),
      eave_y + 0.02F,
      k_half_x + 0.05F,
      k_half_z + 0.05F,
      0.15F,
      0.016F,
      c.terracotta,
      0.02F);
  desc.add_box(centre + QVector3D(0.0F, eave_y + 0.02F + 0.15F, 0.0F),
               QVector3D(k_half_x + 0.07F, 0.014F, 0.022F),
               c.terracotta_dark,
               k_building_state_mask_intact);
  for (float const sx : {-1.0F, 1.0F}) {
    desc.add_box(centre + QVector3D(sx * (k_half_x + 0.02F), eave_y + 0.08F, 0.0F),
                 QVector3D(0.012F, 0.06F, k_half_z * 0.55F),
                 c.plaster_shade,
                 k_building_state_mask_intact);
  }

  for (int i = 0; i < 3; ++i) {
    float const x = centre.x() - k_half_x + 0.08F + static_cast<float>(i) * 0.11F;
    QVector3D const base(x, 0.11F, centre.z() + k_half_z + 0.10F);
    desc.add_cylinder(base,
                      base + QVector3D(0.0F, 0.03F, 0.0F),
                      0.03F,
                      c.clay_band,
                      k_building_state_mask_intact);
    desc.add_cylinder(base + QVector3D(0.0F, 0.03F, 0.0F),
                      base + QVector3D(0.0F, 0.12F, 0.0F),
                      0.055F,
                      c.clay,
                      k_building_state_mask_intact);
    desc.add_cone(base + QVector3D(0.0F, 0.11F, 0.0F),
                  base + QVector3D(0.0F, 0.17F, 0.0F),
                  0.056F,
                  c.clay,
                  k_building_state_mask_intact);
    desc.add_cylinder(base + QVector3D(0.0F, 0.16F, 0.0F),
                      base + QVector3D(0.0F, 0.20F, 0.0F),
                      0.022F,
                      c.clay_band,
                      k_building_state_mask_intact);
  }

  add_ruin_dressing(desc,
                    RuinDressing{.center = centre,
                                 .extent = QVector3D(k_half_x, 0.0F, k_half_z),
                                 .stone = c.limestone,
                                 .stone_dark = c.limestone_dark,
                                 .timber = c.cedar_dark,
                                 .ground_y = 0.11F,
                                 .scale = 0.7F,
                                 .seed = 211,
                                 .collapsed_roof = true});
}

void add_ox_cart(BuildingArchetypeDesc& desc, const RomanFarmPalette& c) {
  QVector3D const centre(-0.55F, 0.0F, 0.72F);
  constexpr float k_wheel_r = 0.085F;

  for (float const sz : {-1.0F, 1.0F}) {
    QVector3D const hub = centre + QVector3D(0.0F, k_wheel_r, sz * 0.15F);
    constexpr int k_spokes = 8;
    for (int spoke = 0; spoke < k_spokes; ++spoke) {
      float const angle =
          static_cast<float>(spoke) * 6.2831853F / static_cast<float>(k_spokes);
      float const next_angle =
          static_cast<float>(spoke + 1) * 6.2831853F / static_cast<float>(k_spokes);
      QVector3D const rim =
          hub +
          QVector3D(std::cos(angle) * k_wheel_r, std::sin(angle) * k_wheel_r, 0.0F);
      QVector3D const next_rim = hub + QVector3D(std::cos(next_angle) * k_wheel_r,
                                                 std::sin(next_angle) * k_wheel_r,
                                                 0.0F);
      desc.add_cylinder(hub, rim, 0.007F, c.cedar, k_building_state_mask_intact);
      desc.add_cylinder(
          rim, next_rim, 0.009F, c.cedar_dark, k_building_state_mask_intact);
    }
    desc.add_cylinder(hub + QVector3D(0.0F, 0.0F, -0.016F),
                      hub + QVector3D(0.0F, 0.0F, 0.016F),
                      k_wheel_r * 0.35F,
                      c.cedar_light,
                      k_building_state_mask_intact);
  }
  desc.add_cylinder(centre + QVector3D(0.0F, k_wheel_r, -0.17F),
                    centre + QVector3D(0.0F, k_wheel_r, 0.17F),
                    0.014F,
                    c.iron,
                    k_building_state_mask_intact);
  desc.add_box(centre + QVector3D(0.02F, k_wheel_r + 0.05F, 0.0F),
               QVector3D(0.17F, 0.028F, 0.13F),
               c.cedar,
               k_building_state_mask_intact);
  for (float const sz : {-1.0F, 1.0F}) {
    desc.add_box(centre + QVector3D(0.02F, k_wheel_r + 0.10F, sz * 0.125F),
                 QVector3D(0.17F, 0.03F, 0.008F),
                 c.cedar_light,
                 k_building_state_mask_intact);
  }
  for (int sheaf = 0; sheaf < 7; ++sheaf) {
    float const t = (static_cast<float>(sheaf) + 0.5F) / 7.0F;
    float const along = -0.12F + t * 0.24F;
    float const across = (decay_hash(601 + sheaf * 17) - 0.5F) * 0.14F;
    float const rise = 0.055F - std::abs(along) * 0.16F;
    QVector3D const butt(
        centre.x() + 0.02F + along, k_wheel_r + 0.075F, centre.z() + across);
    QVector3D const ear(butt.x() + (decay_hash(607 + sheaf * 13) - 0.5F) * 0.05F,
                        butt.y() + rise + decay_hash(613 + sheaf * 11) * 0.022F,
                        butt.z() + (decay_hash(619 + sheaf * 7) - 0.5F) * 0.09F);
    float const tint = 0.90F + decay_hash(631 + sheaf * 5) * 0.20F;
    desc.add_cylinder(
        butt, ear, 0.026F, c.grain * tint * 0.88F, k_building_state_mask_intact);
    desc.add_cone(ear,
                  ear + (ear - butt) * 0.55F,
                  0.022F,
                  c.grain * tint,
                  k_building_state_mask_intact);
  }
  desc.add_cylinder(centre + QVector3D(-0.16F, k_wheel_r + 0.04F, 0.0F),
                    centre + QVector3D(-0.44F, 0.05F, 0.0F),
                    0.014F,
                    c.cedar,
                    k_building_state_mask_intact);

  desc.add_box(centre + QVector3D(0.0F, 0.03F, 0.0F),
               QVector3D(0.16F, 0.03F, 0.12F),
               c.cedar_dark * 0.6F,
               BuildingStateMask::Destroyed);
}

void add_haystack(BuildingArchetypeDesc& desc, const RomanFarmPalette& c) {
  QVector3D const base(0.02F, 0.0F, 0.74F);
  desc.add_cylinder(base,
                    base + QVector3D(0.0F, 0.12F, 0.0F),
                    0.12F,
                    c.straw * 0.9F,
                    k_building_state_mask_intact);
  desc.add_cone(base + QVector3D(0.0F, 0.11F, 0.0F),
                base + QVector3D(0.0F, 0.30F, 0.0F),
                0.125F,
                c.straw,
                k_building_state_mask_intact);
  desc.add_cylinder(base + QVector3D(0.0F, 0.02F, 0.0F),
                    base + QVector3D(0.0F, 0.34F, 0.0F),
                    0.012F,
                    c.cedar_dark,
                    k_building_state_mask_intact);
  desc.add_cylinder(base,
                    base + QVector3D(0.0F, 0.04F, 0.0F),
                    0.10F,
                    QVector3D(0.14F, 0.12F, 0.10F),
                    BuildingStateMask::Destroyed);
}

auto build_farm_archetype(BuildingState state, int stage) -> RenderArchetype {
  RomanFarmPalette const c;
  BuildingArchetypeDesc desc("roman_farm_stage_" + std::to_string(stage));

  FarmFieldPalette field_palette;
  add_farm_field(desc,
                 FarmFieldSpec{.center = QVector3D(-0.08F, 0.0F, -0.22F),
                               .half_x = 0.80F,
                               .half_z = 0.62F,
                               .ground_y = 0.0F,
                               .rows = 18,
                               .stalks_per_row = 36,
                               .seed = 7,
                               .rows_along_x = true},
                 field_palette,
                 stage);

  add_stone_boundary(desc, c);
  add_granary_shed(desc, c);
  add_ox_cart(desc, c);
  add_haystack(desc, c);
  add_farm_scarecrow(
      desc, QVector3D(0.60F, 0.03F, -0.60F), c.cedar_dark, c.cloth, c.straw);

  add_scorch_patch(desc,
                   ScorchPatch{.center = QVector3D(-0.08F, 0.0F, -0.2F),
                               .radius = 0.55F,
                               .ground_y = 0.05F,
                               .count = 6,
                               .seed = 331});
  add_charred_beams(desc,
                    CharredBeams{.center = QVector3D(0.50F, 0.0F, 0.70F),
                                 .extent = QVector3D(0.30F, 0.0F, 0.19F),
                                 .timber = c.cedar_dark * 0.5F,
                                 .ground_y = 0.12F,
                                 .length = 0.5F,
                                 .radius = 0.02F,
                                 .count = 4,
                                 .seed = 353});

  return build_building_archetype(desc, state);
}

auto farm_archetype(BuildingState state, int stage) -> const RenderArchetype& {
  static const auto k_table = build_farm_archetype_table(build_farm_archetype);
  return farm_archetype_from_table(k_table, state, stage);
}

} // namespace

void register_farm_renderer(EntityRendererRegistry& registry) {
  register_farm_renderer_variant(
      registry,
      FarmRendererConfig{.nation_slug = "roman",
                         .archetype = &farm_archetype,
                         .selection = BuildingSelectionStyle{2.2F, 2.2F}});
}

} // namespace Render::GL::Roman
