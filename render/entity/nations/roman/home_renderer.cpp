#include "home_renderer.h"

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "building_architecture.h"
#include "building_palette.h"
#include "game/core/component.h"
#include "math/math_utils.h"
#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_archetype_library.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/building_render_common.h"
#include "render/entity/building_state.h"
#include "render/entity/home_activity.h"
#include "render/entity/home_renderer_common.h"
#include "render/entity/registry.h"
#include "render/submitter.h"

namespace Render::GL::Roman {
namespace {

using Render::Geom::clamp_vec_01;

constexpr std::uint8_t k_home_team_slot = 0;

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

constexpr std::uint8_t k_home_wall_slot = 1;

constexpr std::array<QVector3D, 5> k_wall_washes{
    QVector3D(0.80F, 0.78F, 0.71F),
    QVector3D(0.84F, 0.77F, 0.64F),
    QVector3D(0.85F, 0.76F, 0.70F),
    QVector3D(0.86F, 0.85F, 0.80F),
    QVector3D(0.78F, 0.73F, 0.62F),
};

auto home_palette_slots(const QVector3D& team, std::uint32_t seed, BuildingState state)
    -> std::array<QVector3D, k_home_palette_slots> {
  const auto palette = make_palette(team);
  QVector3D wash = k_wall_washes[home_tint_pick(seed, k_wall_washes.size())] *
                   (1.0F + home_tint_jitter(seed) * 0.06F);
  wash = decayed_color(clamp_vec_01(wash), state, static_cast<int>(seed & 0xFFFFU));
  return {palette.team, wash};
}

struct RomanHomeTraits {
  enum class Roof : std::uint8_t {
    GableZ,
    GableX,
    Compluvium
  };
  Roof roof{Roof::GableZ};
  float pitch{0.40F};
  float wall_height{1.0F};
  bool porch_roof{false};
  bool back_lean_to{false};
  bool upper_windows{false};
};

auto roman_home_traits(int variant) -> RomanHomeTraits {
  using Roof = RomanHomeTraits::Roof;
  switch (variant) {
  case 1:
    return {.roof = Roof::GableX, .pitch = 0.28F};
  case 2:
    return {.roof = Roof::Compluvium, .pitch = 0.22F};
  case 3:
    return {.roof = Roof::GableX,
            .pitch = 0.24F,
            .wall_height = 1.12F,
            .porch_roof = true,
            .upper_windows = true};
  case 4:
    return {.roof = Roof::GableZ, .pitch = 0.34F, .back_lean_to = true};
  case 5:
    return {.roof = Roof::GableX,
            .pitch = 0.33F,
            .wall_height = 1.05F,
            .porch_roof = true,
            .back_lean_to = true};
  default:
    return {.roof = Roof::GableZ, .pitch = 0.40F};
  }
}

auto build_home_desc_impl(BuildingState state, int variant) -> BuildingArchetypeDesc {
  RomanPalette const c = make_palette(QVector3D(1.0F, 1.0F, 1.0F));
  RomanHomeTraits const traits = roman_home_traits(variant);
  float const wall_height = traits.wall_height;
  float height_multiplier = 1.0F;
  if (state == BuildingState::Damaged) {
    height_multiplier = 0.7F;
  } else if (state == BuildingState::Destroyed) {
    height_multiplier = 0.4F;
  }

  BuildingArchetypeDesc desc(
      variant == 0 ? "roman_home" : "roman_home_variant_" + std::to_string(variant));

  desc.add_box(
      QVector3D(0.0F, 0.04F, 0.0F), QVector3D(1.18F, 0.04F, 1.18F), c.limestone_dark);
  desc.add_box(
      QVector3D(0.0F, 0.10F, 0.0F), QVector3D(1.10F, 0.02F, 1.10F), c.limestone_shade);
  desc.add_box(
      QVector3D(0.0F, 0.14F, 0.0F), QVector3D(1.04F, 0.02F, 1.04F), c.limestone);

  float const wall_cy = wall_height * 0.5F * height_multiplier + 0.16F;
  float const wall_hy = wall_height * 0.5F * height_multiplier;
  desc.add_palette_box(QVector3D(0.0F, wall_cy, -0.88F),
                       QVector3D(0.82F, wall_hy, 0.08F),
                       k_home_wall_slot);
  desc.add_palette_box(QVector3D(0.0F, wall_cy, 0.88F),
                       QVector3D(0.82F, wall_hy, 0.08F),
                       k_home_wall_slot);
  desc.add_palette_box(QVector3D(-0.88F, wall_cy, 0.0F),
                       QVector3D(0.08F, wall_hy, 0.78F),
                       k_home_wall_slot);
  desc.add_palette_box(QVector3D(0.88F, wall_cy, 0.0F),
                       QVector3D(0.08F, wall_hy, 0.78F),
                       k_home_wall_slot);

  float const cornice_y = wall_height * height_multiplier + 0.18F;

  desc.add_box(QVector3D(0.0F, cornice_y, -0.88F),
               QVector3D(0.78F, 0.05F, 0.10F),
               c.limestone_shade,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.0F, cornice_y, 0.88F),
               QVector3D(0.78F, 0.05F, 0.10F),
               c.limestone_shade,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(-0.88F, cornice_y, 0.0F),
               QVector3D(0.10F, 0.05F, 0.98F),
               c.limestone_shade,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.88F, cornice_y, 0.0F),
               QVector3D(0.10F, 0.05F, 0.98F),
               c.limestone_shade,
               k_building_state_mask_intact);

  desc.add_box(QVector3D(0.0F, cornice_y - 0.06F, -0.88F),
               QVector3D(0.86F, 0.02F, 0.09F),
               c.marble,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.0F, cornice_y - 0.06F, 0.88F),
               QVector3D(0.86F, 0.02F, 0.09F),
               c.marble,
               k_building_state_mask_intact);

  for (const float x : {-0.68F, -0.34F, 0.0F, 0.34F, 0.68F}) {
    for (const float side : {-1.0F, 1.0F}) {
      desc.add_box(QVector3D(x, cornice_y - 0.12F, side * 0.98F),
                   QVector3D(0.045F, 0.055F, 0.035F),
                   c.limestone_shade,
                   k_building_state_mask_intact);
    }
  }

  for (float const xw : {-0.968F, 0.968F}) {
    desc.add_box(QVector3D(xw, 0.58F, -0.30F),
                 QVector3D(0.015F, 0.20F, 0.16F),
                 c.cedar_dark,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(xw, 0.58F, 0.30F),
                 QVector3D(0.015F, 0.20F, 0.16F),
                 c.cedar_dark,
                 k_building_state_mask_intact);

    desc.add_box(QVector3D(xw, 0.38F, -0.30F),
                 QVector3D(0.022F, 0.020F, 0.18F),
                 c.limestone_shade,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(xw, 0.38F, 0.30F),
                 QVector3D(0.022F, 0.020F, 0.18F),
                 c.limestone_shade,
                 k_building_state_mask_intact);

    desc.add_box(QVector3D(xw, 0.80F, -0.30F),
                 QVector3D(0.024F, 0.025F, 0.19F),
                 c.marble,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(xw, 0.80F, 0.30F),
                 QVector3D(0.024F, 0.025F, 0.19F),
                 c.marble,
                 k_building_state_mask_intact);
  }

  float const col_height = 0.88F;
  float const col_radius = 0.055F;
  QVector3D const front_cols[4] = {QVector3D(-0.72F, 0.0F, 0.92F),
                                   QVector3D(-0.38F, 0.0F, 0.92F),
                                   QVector3D(0.38F, 0.0F, 0.92F),
                                   QVector3D(0.72F, 0.0F, 0.92F)};

  for (const QVector3D& col : front_cols) {

    desc.add_box(QVector3D(col.x(), 0.18F, col.z()),
                 QVector3D(col_radius * 1.3F, 0.04F, col_radius * 1.3F),
                 c.marble,
                 BuildingStateMask::All);

    desc.add_cylinder(
        QVector3D(col.x(), 0.16F, col.z()),
        QVector3D(col.x(), 0.16F + col_height * height_multiplier, col.z()),
        col_radius,
        c.limestone_shade);

    desc.add_box(
        QVector3D(col.x(), 0.16F + col_height * height_multiplier + 0.04F, col.z()),
        QVector3D(col_radius * 1.40F, 0.055F, col_radius * 1.40F),
        c.marble,
        k_building_state_mask_intact);
  }

  float const entab_y = 0.16F + col_height * height_multiplier + 0.12F;
  desc.add_box(QVector3D(0.0F, entab_y, 0.96F),
               QVector3D(0.826F, 0.04F, 0.06F),
               c.limestone,
               k_building_state_mask_intact);

  desc.add_box(QVector3D(0.0F, entab_y + 0.06F, 0.92F),
               QVector3D(0.80F, 0.03F, 0.054F),
               c.blue_accent,
               k_building_state_mask_intact);

  desc.add_box(
      QVector3D(0.0F, 0.48F, 0.92F), QVector3D(0.28F, 0.42F, 0.05F), c.cedar_dark);

  desc.add_box(QVector3D(0.0F, 0.72F, 0.968F),
               QVector3D(0.32F, 0.04F, 0.02F),
               c.blue_accent,
               BuildingStateMask::All);

  desc.add_box(
      QVector3D(0.0F, 0.06F, 1.08F), QVector3D(0.60F, 0.04F, 0.16F), c.limestone_dark);
  desc.add_box(QVector3D(0.0F, 0.12F, 1.04F),
               QVector3D(0.52F, 0.04F, 0.12F),
               c.limestone_shade,
               k_building_state_mask_intact);
  if (state != BuildingState::Destroyed) {
    desc.add_box(QVector3D(0.0F, 0.18F, 1.00F),
                 QVector3D(0.44F, 0.03F, 0.08F),
                 c.marble,
                 k_building_state_mask_intact);
  }

  desc.add_box(QVector3D(0.0F, 0.1625F, 0.0F),
               QVector3D(0.54F, 0.005F, 0.54F),
               c.marble,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.0F, 0.158F, 0.34F),
               QVector3D(0.48F, 0.02F, 0.03F),
               c.limestone_dark,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.0F, 0.158F, -0.34F),
               QVector3D(0.48F, 0.02F, 0.03F),
               c.limestone_dark,
               k_building_state_mask_intact);

  desc.add_box(QVector3D(0.34F, 0.158F, 0.0F),
               QVector3D(0.03F, 0.02F, 0.31F),
               c.limestone_dark,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(-0.34F, 0.158F, 0.0F),
               QVector3D(0.03F, 0.02F, 0.31F),
               c.limestone_dark,
               k_building_state_mask_intact);

  desc.add_box(QVector3D(0.0F, 0.153F, 0.0F),
               QVector3D(0.28F, 0.005F, 0.28F),
               c.blue_light,
               k_building_state_mask_intact);

  float const eave_y = cornice_y + 0.02F;

  if (traits.upper_windows) {
    BuildingPartLabel const label(desc, "upper_windows");
    float const sill_y = 0.16F + (wall_height - 0.28F) * height_multiplier;
    auto const window = [&](const QVector3D& at, bool along_x) {
      QVector3D const pane =
          along_x ? QVector3D(0.10F, 0.10F, 0.015F) : QVector3D(0.015F, 0.10F, 0.10F);
      QVector3D const sill =
          along_x ? QVector3D(0.13F, 0.018F, 0.022F) : QVector3D(0.022F, 0.018F, 0.13F);
      desc.add_box(at + QVector3D(0.0F, 0.12F, 0.0F),
                   pane,
                   c.cedar_dark,
                   k_building_state_mask_intact);
      desc.add_box(at, sill, c.marble, k_building_state_mask_intact);
    };
    for (float const xw : {-0.50F, 0.50F}) {
      window(QVector3D(xw, sill_y, -0.968F), true);
    }
    for (float const xw : {-0.968F, 0.968F}) {
      window(QVector3D(xw, sill_y, 0.0F), false);
    }
  }

  if (traits.porch_roof) {
    BuildingPartLabel const label(desc, "porch_roof");
    float const in_z = 1.00F;
    float const out_z = 1.15F;
    float const high = eave_y + 0.004F;
    float const low = eave_y - 0.085F * height_multiplier;
    float const run = out_z - in_z;
    float const drop = high - low;
    float const angle = std::atan2(drop, run) * 180.0F / 3.14159265F;
    {
      BuildingPartMaterial const tiles(desc, k_building_material_ceramic);
      desc.add_rotated_box(
          QVector3D(0.0F, (low + high) * 0.5F + 0.014F, (in_z + out_z) * 0.5F),
          QVector3D(0.88F, 0.012F, std::sqrt(run * run + drop * drop) * 0.5F),
          QVector3D(angle, 0.0F, 0.0F),
          c.terracotta,
          k_building_state_mask_intact);
      for (int roll = 0; roll < 9; ++roll) {
        float const x = -0.80F + 0.20F * static_cast<float>(roll);
        desc.add_cylinder(
            QVector3D(x, low + 0.030F, out_z - 0.012F),
            QVector3D(x, high + 0.030F, in_z + 0.012F),
            0.012F,
            weathered(BuildingPalette::k_terracotta_light, roll + 71, 0.06F),
            k_building_state_mask_intact);
      }
    }
    BuildingPartMaterial const timber(desc, k_building_material_wood);
    desc.add_box(QVector3D(0.0F, low - 0.012F, out_z - 0.035F),
                 QVector3D(0.874F, 0.020F, 0.024F),
                 c.cedar_dark,
                 k_building_state_mask_intact);
    for (float const side : {-1.0F, 1.0F}) {
      desc.add_cylinder(QVector3D(side * 0.82F, 0.08F, out_z - 0.035F),
                        QVector3D(side * 0.82F, low - 0.03F, out_z - 0.035F),
                        0.020F,
                        c.cedar_dark,
                        k_building_state_mask_intact);
    }
  }

  if (traits.back_lean_to) {
    BuildingPartLabel const label(desc, "back_lean_to");
    float const x0 = -0.80F;
    float const x1 = 0.24F;
    float const cx = (x0 + x1) * 0.5F;
    float const half_x = (x1 - x0) * 0.5F;
    float const in_z = -0.968F;
    float const out_z = -1.14F;
    float const high = 0.80F * height_multiplier + 0.16F;
    float const low = 0.66F * height_multiplier + 0.16F;
    float const run = in_z - out_z;
    float const drop = high - low;
    float const angle = std::atan2(drop, run) * 180.0F / 3.14159265F;
    {
      BuildingPartMaterial const timber(desc, k_building_material_wood);
      for (float const x : {x0 + 0.04F, cx, x1 - 0.04F}) {
        desc.add_cylinder(QVector3D(x, 0.08F, out_z + 0.03F),
                          QVector3D(x, low, out_z + 0.03F),
                          0.022F,
                          c.cedar_dark,
                          k_building_state_mask_intact);
      }
      desc.add_box(QVector3D(cx, low + 0.010F, out_z + 0.03F),
                   QVector3D(half_x + 0.022F, 0.018F, 0.024F),
                   c.cedar_dark,
                   k_building_state_mask_intact);
      for (int board = 0; board < 4; ++board) {
        float const x =
            x0 + 0.16F + (half_x * 2.0F - 0.32F) * static_cast<float>(board) / 3.0F;
        float const board_half = (low - 0.08F) * 0.30F;
        desc.add_box(QVector3D(x, 0.08F + board_half, out_z + 0.011F),
                     QVector3D(0.09F, board_half, 0.007F),
                     c.cedar_dark * (1.10F + 0.06F * static_cast<float>(board % 2)),
                     k_building_state_mask_intact);
      }
    }
    {
      BuildingPartMaterial const tiles(desc, k_building_material_ceramic);
      desc.add_rotated_box(
          QVector3D(cx, (low + high) * 0.5F + 0.020F, (in_z + out_z) * 0.5F - 0.006F),
          QVector3D(half_x + 0.05F,
                    0.013F,
                    std::sqrt(run * run + drop * drop) * 0.5F + 0.012F),
          QVector3D(-angle, 0.0F, 0.0F),
          c.terracotta_dark * 1.06F,
          k_building_state_mask_intact);
    }
    for (int jar = 0; jar < 3; ++jar) {
      QVector3D const at(x0 + 0.20F + 0.24F * static_cast<float>(jar), 0.12F, -1.07F);
      desc.add_cylinder(at,
                        at + QVector3D(0.0F, 0.19F, 0.0F),
                        0.052F,
                        c.terracotta * (0.94F + 0.04F * static_cast<float>(jar)),
                        k_building_state_mask_intact);
    }
  }

  if (traits.roof == RomanHomeTraits::Roof::Compluvium) {
    BuildingPartLabel const label(desc, "courtyard_roof");

    for (float const side : {-1.0F, 1.0F}) {
      add_tiled_roof(desc,
                     QVector3D(side * 0.73F, eave_y, 0.0F),
                     1.05F,
                     0.31F,
                     0.22F * height_multiplier,
                     true,
                     k_building_state_mask_intact,
                     true);
      desc.add_box(QVector3D(side * 0.41F, wall_cy, -0.05F),
                   QVector3D(0.035F, wall_hy, 0.80F),
                   c.limestone_shade,
                   k_building_state_mask_intact);
    }
    add_tiled_roof(desc,
                   QVector3D(0.0F, eave_y, -0.73F),
                   0.41F,
                   0.31F,
                   0.22F * height_multiplier,
                   false,
                   k_building_state_mask_intact,
                   true);
    {
      BuildingPartMaterial const timber(desc, k_building_material_wood);
      for (float const side : {-1.0F, 1.0F}) {
        desc.add_box(QVector3D(side * 0.39F, eave_y + 0.042F, 0.70F),
                     QVector3D(0.025F, 0.028F, 0.34F),
                     c.cedar_dark,
                     k_building_state_mask_intact);
      }
      for (int slat = 0; slat < 7; ++slat) {
        desc.add_box(QVector3D(0.0F, eave_y + 0.085F, 0.42F + slat * 0.095F),
                     QVector3D(0.43F, 0.018F, 0.024F),
                     c.cedar_dark * 1.22F,
                     k_building_state_mask_intact);
      }
    }
  } else {
    BuildingPartLabel const label(desc, "gable_roof");
    add_tiled_roof(desc,
                   QVector3D(0.0F, eave_y, 0.0F),
                   1.05F,
                   1.02F,
                   traits.pitch * height_multiplier,
                   traits.roof == RomanHomeTraits::Roof::GableZ,
                   k_building_state_mask_intact,
                   true);
  }

  desc.add_palette_box(QVector3D(0.0F, 0.76F, 0.97F),
                       QVector3D(0.28F, 0.12F, 0.02F),
                       k_home_team_slot,
                       BuildingStateMask::All);

  add_home_yard(desc,
                HomeYardStyle{.plinth_half = 1.18F,
                              .wall_half = 0.96F,
                              .door_half_width = 0.60F,
                              .vent = QVector3D(0.0F, 1.66F, -0.73F),
                              .chimney_base_y = eave_y + 0.02F,
                              .clay_oven_chimney = false,
                              .stone = c.limestone_shade,
                              .clay = c.terracotta,
                              .clay_dark = c.terracotta_dark,
                              .chimney = c.limestone_shade,
                              .seed = 3 + variant * 17,
                              .variant = variant,
                              .wall_foot_y = 0.16F,
                              .wall_face = 0.96F,
                              .wall_foot_door_gap = 0.30F,
                              .wall_foot = c.limestone_dark * 0.92F});

  add_ruin_dressing(desc,
                    RuinDressing{.extent = QVector3D(0.98F, 0.0F, 0.98F),
                                 .stone = c.limestone_shade,
                                 .stone_dark = c.limestone_dark,
                                 .timber = c.terracotta_dark * 0.45F,
                                 .ground_y = 0.16F,
                                 .scale = 1.0F,
                                 .seed = 101});

  return desc;
}

auto home_archetype(BuildingState state, int variant) -> const RenderArchetype& {
  static const std::array<const BuildingArchetypeSet*, k_home_visual_variant_count>
      k_sets{&building_archetype_set("roman_home"),
             &building_archetype_set("roman_home_variant_1"),
             &building_archetype_set("roman_home_variant_2"),
             &building_archetype_set("roman_home_variant_3"),
             &building_archetype_set("roman_home_variant_4"),
             &building_archetype_set("roman_home_variant_5")};
  return k_sets[static_cast<std::size_t>(variant)]->for_state(state);
}

const std::array<TorchMount, 2> k_torches{{
    TorchMount{.at = QVector3D(-0.38F, 0.6F, 0.975F),
               .outward = QVector3D(0.0F, 0.0F, 1.0F)},
    TorchMount{.at = QVector3D(0.38F, 0.6F, 0.975F),
               .outward = QVector3D(0.0F, 0.0F, 1.0F)},
}};

} // namespace

auto build_home_desc(BuildingState state, int variant) -> BuildingArchetypeDesc {
  return build_home_desc_impl(state, variant);
}

void register_home_renderer(Render::GL::EntityRendererRegistry& registry) {

  register_home_smoke_anchor(
      false,
      HomeSmokeAnchor{.vent = QVector3D(0.0F, 1.66F, -0.73F),
                      .smoke_tint = QVector3D(0.56F, 0.54F, 0.51F),
                      .plume_radius = 0.68F,
                      .doorstep = QVector3D(0.0F, 0.16F, 1.24F),
                      .outward = QVector3D(0.0F, 0.0F, 1.0F),

                      .cloth_top = QVector3D(0.0F, 0.74F, 0.99F),
                      .cloth_span = 0.30F,
                      .cloth_indigo = false,
                      .cloth_at_door = true,
                      .lamp = QVector3D(0.0F, 0.165F, 1.22F),

                      .window = QVector3D(0.95F, 0.58F, 0.30F),
                      .shutter_width = 0.16F,
                      .shutter_height = 0.40F,

                      .line_a = QVector3D(-0.72F, 0.99F, 0.95F),
                      .line_b = QVector3D(-0.38F, 0.99F, 0.95F),
                      .line_pole = false,
                      .max_laundry = 2});
  register_home_renderer_variant(
      registry,
      HomeRendererConfig{.nation_slug = "roman",
                         .archetype = &home_archetype,
                         .palette_slots = &home_palette_slots,
                         .selection = BuildingSelectionStyle{2.25F, 2.25F},
                         .torches = k_torches});
}

} // namespace Render::GL::Roman
