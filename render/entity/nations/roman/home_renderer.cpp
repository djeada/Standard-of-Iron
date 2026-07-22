#include "home_renderer.h"

#include <QVector3D>

#include <algorithm>
#include <array>

#include "../../../../game/core/component.h"
#include "../../../submitter.h"
#include "../../building_archetype_desc.h"
#include "../../building_ornaments.h"
#include "../../building_render_common.h"
#include "../../building_state.h"
#include "../../home_renderer_common.h"
#include "../../registry.h"
#include "math/math_utils.h"

namespace Render::GL::Roman {
namespace {

using Render::Geom::clamp_vec_01;

constexpr std::uint8_t k_home_team_slot = 0;

struct RomanPalette {
  QVector3D limestone{0.96F, 0.94F, 0.88F};
  QVector3D limestone_shade{0.88F, 0.85F, 0.78F};
  QVector3D limestone_dark{0.80F, 0.76F, 0.70F};
  QVector3D marble{0.98F, 0.97F, 0.95F};
  QVector3D cedar{0.52F, 0.38F, 0.26F};
  QVector3D cedar_dark{0.38F, 0.26F, 0.16F};
  QVector3D terracotta{0.76F, 0.32F, 0.18F};
  QVector3D terracotta_dark{0.46F, 0.12F, 0.07F};
  QVector3D blue_accent{0.28F, 0.48F, 0.68F};
  QVector3D blue_light{0.40F, 0.60F, 0.80F};
  QVector3D gold{0.85F, 0.72F, 0.35F};
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

auto home_palette_slots(const QVector3D& team) -> std::array<QVector3D, 1> {
  return {make_palette(team).team};
}

auto build_home_archetype(BuildingState state) -> RenderArchetype {
  RomanPalette const c = make_palette(QVector3D(1.0F, 1.0F, 1.0F));
  float const wall_height = 1.0F;
  float height_multiplier = 1.0F;
  if (state == BuildingState::Damaged) {
    height_multiplier = 0.7F;
  } else if (state == BuildingState::Destroyed) {
    height_multiplier = 0.4F;
  }

  BuildingArchetypeDesc desc("roman_home");

  desc.add_box(
      QVector3D(0.0F, 0.04F, 0.0F), QVector3D(1.18F, 0.04F, 1.18F), c.limestone_dark);
  desc.add_box(
      QVector3D(0.0F, 0.10F, 0.0F), QVector3D(1.10F, 0.02F, 1.10F), c.limestone_shade);
  desc.add_box(
      QVector3D(0.0F, 0.14F, 0.0F), QVector3D(1.04F, 0.02F, 1.04F), c.limestone);

  float const wall_cy = wall_height * 0.5F * height_multiplier + 0.16F;
  float const wall_hy = wall_height * 0.5F * height_multiplier;
  desc.add_box(
      QVector3D(0.0F, wall_cy, -0.88F), QVector3D(0.82F, wall_hy, 0.08F), c.limestone);
  desc.add_box(
      QVector3D(0.0F, wall_cy, 0.88F), QVector3D(0.82F, wall_hy, 0.08F), c.limestone);
  desc.add_box(
      QVector3D(-0.88F, wall_cy, 0.0F), QVector3D(0.08F, wall_hy, 0.78F), c.limestone);
  desc.add_box(
      QVector3D(0.88F, wall_cy, 0.0F), QVector3D(0.08F, wall_hy, 0.78F), c.limestone);

  float const cornice_y = wall_height * height_multiplier + 0.18F;
  desc.add_box(QVector3D(0.0F, cornice_y, -0.88F),
               QVector3D(0.90F, 0.05F, 0.10F),
               c.limestone_shade,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.0F, cornice_y, 0.88F),
               QVector3D(0.90F, 0.05F, 0.10F),
               c.limestone_shade,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(-0.88F, cornice_y, 0.0F),
               QVector3D(0.10F, 0.05F, 0.84F),
               c.limestone_shade,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.88F, cornice_y, 0.0F),
               QVector3D(0.10F, 0.05F, 0.84F),
               c.limestone_shade,
               k_building_state_mask_intact);

  desc.add_box(QVector3D(0.0F, cornice_y - 0.06F, -0.88F),
               QVector3D(0.86F, 0.02F, 0.09F),
               c.marble,
               k_building_state_mask_intact,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(0.0F, cornice_y - 0.06F, 0.88F),
               QVector3D(0.86F, 0.02F, 0.09F),
               c.marble,
               k_building_state_mask_intact,
               BuildingLODMask::Full);

  for (float const xw : {-0.92F, 0.92F}) {
    desc.add_box(QVector3D(xw, 0.58F, -0.30F),
                 QVector3D(0.015F, 0.20F, 0.16F),
                 c.cedar_dark,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
    desc.add_box(QVector3D(xw, 0.58F, 0.30F),
                 QVector3D(0.015F, 0.20F, 0.16F),
                 c.cedar_dark,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);

    desc.add_box(QVector3D(xw, 0.44F, -0.30F),
                 QVector3D(0.015F, 0.03F, 0.19F),
                 c.limestone_shade,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
    desc.add_box(QVector3D(xw, 0.44F, 0.30F),
                 QVector3D(0.015F, 0.03F, 0.19F),
                 c.limestone_shade,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);

    desc.add_box(QVector3D(xw, 0.80F, -0.30F),
                 QVector3D(0.015F, 0.04F, 0.20F),
                 c.marble,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
    desc.add_box(QVector3D(xw, 0.80F, 0.30F),
                 QVector3D(0.015F, 0.04F, 0.20F),
                 c.marble,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }

  float const col_height = 0.88F;
  float const col_radius = 0.055F;
  QVector3D const front_cols[6] = {QVector3D(-0.72F, 0.0F, 0.92F),
                                   QVector3D(-0.43F, 0.0F, 0.92F),
                                   QVector3D(-0.14F, 0.0F, 0.92F),
                                   QVector3D(0.14F, 0.0F, 0.92F),
                                   QVector3D(0.43F, 0.0F, 0.92F),
                                   QVector3D(0.72F, 0.0F, 0.92F)};

  for (const QVector3D& col : front_cols) {

    desc.add_box(QVector3D(col.x(), 0.18F, col.z()),
                 QVector3D(col_radius * 1.3F, 0.04F, col_radius * 1.3F),
                 c.marble,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);

    desc.add_cylinder(
        QVector3D(col.x(), 0.16F, col.z()),
        QVector3D(col.x(), 0.16F + col_height * height_multiplier, col.z()),
        col_radius,
        c.limestone_shade);

    desc.add_box(
        QVector3D(col.x(), 0.16F + col_height * height_multiplier + 0.04F, col.z()),
        QVector3D(col_radius * 1.6F, 0.07F, col_radius * 1.6F),
        c.marble,
        k_building_state_mask_intact,
        BuildingLODMask::Full);
  }

  float const entab_y = 0.16F + col_height * height_multiplier + 0.12F;
  desc.add_box(QVector3D(0.0F, entab_y, 0.92F),
               QVector3D(0.82F, 0.04F, 0.06F),
               c.limestone,
               k_building_state_mask_intact);

  desc.add_box(QVector3D(0.0F, entab_y + 0.06F, 0.92F),
               QVector3D(0.80F, 0.03F, 0.05F),
               c.blue_accent,
               k_building_state_mask_intact,
               BuildingLODMask::Full);

  desc.add_box(
      QVector3D(0.0F, 0.48F, 0.92F), QVector3D(0.28F, 0.42F, 0.05F), c.cedar_dark);
  desc.add_box(QVector3D(0.0F, 0.72F, 0.94F),
               QVector3D(0.32F, 0.04F, 0.02F),
               c.blue_accent,
               BuildingStateMask::All,
               BuildingLODMask::Full);

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

  desc.add_box(QVector3D(0.0F, 0.155F, 0.0F),
               QVector3D(0.54F, 0.005F, 0.54F),
               c.marble,
               k_building_state_mask_intact,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(0.0F, 0.158F, 0.34F),
               QVector3D(0.48F, 0.02F, 0.03F),
               c.limestone_dark,
               k_building_state_mask_intact,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(0.0F, 0.158F, -0.34F),
               QVector3D(0.48F, 0.02F, 0.03F),
               c.limestone_dark,
               k_building_state_mask_intact,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(0.34F, 0.158F, 0.0F),
               QVector3D(0.03F, 0.02F, 0.40F),
               c.limestone_dark,
               k_building_state_mask_intact,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(-0.34F, 0.158F, 0.0F),
               QVector3D(0.03F, 0.02F, 0.40F),
               c.limestone_dark,
               k_building_state_mask_intact,
               BuildingLODMask::Full);

  desc.add_box(QVector3D(0.0F, 0.153F, 0.0F),
               QVector3D(0.28F, 0.005F, 0.28F),
               c.blue_light,
               k_building_state_mask_intact,
               BuildingLODMask::Full);

  auto add_rot = [&](const QVector3D& center,
                     const QVector3D& scale,
                     const QVector3D& euler,
                     const QVector3D& color) {
    desc.add_rotated_box(center, scale, euler, color, k_building_state_mask_intact);
  };
  float const eave_y = cornice_y + 0.02F;
  float const roof_rise = 0.55F * height_multiplier;
  add_gable_roof_z(
      add_rot, 0.0F, 0.0F, eave_y, 0.98F, 0.98F, roof_rise, 0.05F, c.terracotta, 0.07F);

  desc.add_box(QVector3D(0.0F, eave_y + roof_rise + 0.01F, 0.0F),
               QVector3D(0.05F, 0.03F, 1.04F),
               c.terracotta_dark,
               k_building_state_mask_intact);

  desc.add_box(QVector3D(0.99F, eave_y, 0.0F),
               QVector3D(0.04F, 0.04F, 1.05F),
               c.terracotta_dark,
               k_building_state_mask_intact,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(-0.99F, eave_y, 0.0F),
               QVector3D(0.04F, 0.04F, 1.05F),
               c.terracotta_dark,
               k_building_state_mask_intact,
               BuildingLODMask::Full);

  desc.add_box(QVector3D(0.0F, eave_y + 0.14F, -0.97F),
               QVector3D(0.68F, 0.14F, 0.05F),
               c.limestone_shade,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.0F, eave_y + 0.36F, -0.97F),
               QVector3D(0.42F, 0.10F, 0.05F),
               c.limestone,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.0F, eave_y + 0.50F, -0.97F),
               QVector3D(0.16F, 0.06F, 0.05F),
               c.limestone_shade,
               k_building_state_mask_intact);

  desc.add_box(QVector3D(0.0F, eave_y + 0.18F, 0.93F),
               QVector3D(0.60F, 0.16F, 0.05F),
               c.limestone,
               k_building_state_mask_intact);
  desc.add_box(QVector3D(0.0F, eave_y + 0.42F, 0.93F),
               QVector3D(0.30F, 0.10F, 0.05F),
               c.limestone_shade,
               k_building_state_mask_intact);

  if (state != BuildingState::Destroyed) {
    desc.add_box(QVector3D(0.0F, cornice_y + 0.20F, 0.96F),
                 QVector3D(0.66F, 0.06F, 0.06F),
                 c.terracotta_dark,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(0.0F, cornice_y + 0.28F, 0.96F),
                 QVector3D(0.46F, 0.05F, 0.05F),
                 c.terracotta,
                 k_building_state_mask_intact);
    if (state == BuildingState::Normal) {

      desc.add_box(QVector3D(0.0F, cornice_y + 0.36F, 0.96F),
                   QVector3D(0.22F, 0.05F, 0.04F),
                   c.blue_accent,
                   BuildingStateMask::Normal,
                   BuildingLODMask::Full);

      desc.add_box(QVector3D(-0.62F, cornice_y + 0.24F, 0.96F),
                   QVector3D(0.05F, 0.07F, 0.05F),
                   c.gold,
                   BuildingStateMask::Normal,
                   BuildingLODMask::Full);
      desc.add_box(QVector3D(0.62F, cornice_y + 0.24F, 0.96F),
                   QVector3D(0.05F, 0.07F, 0.05F),
                   c.gold,
                   BuildingStateMask::Normal,
                   BuildingLODMask::Full);
    }
  }

  desc.add_palette_box(QVector3D(0.0F, 0.76F, 0.97F),
                       QVector3D(0.28F, 0.12F, 0.02F),
                       k_home_team_slot,
                       BuildingStateMask::All,
                       BuildingLODMask::Full);

  add_roman_aquila_relief(desc,
                          QVector3D(0.0F, cornice_y + 0.34F, 0.985F),
                          BuildingFacadePlane::XY,
                          0.62F,
                          c.gold,
                          c.blue_accent);
  add_roman_roof_standard(desc,
                          QVector3D(0.0F, eave_y + roof_rise + 0.04F, 0.0F),
                          0.62F,
                          c.gold,
                          c.terracotta_dark);

  return build_building_archetype(desc, state);
}

auto home_archetype(BuildingState state) -> const RenderArchetype& {
  static const BuildingArchetypeSet k_set =
      build_stateful_building_archetype_set(build_home_archetype);
  return k_set.for_state(state);
}

} // namespace

void register_home_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_home_renderer_variant(
      registry,
      HomeRendererConfig{.nation_slug = "roman",
                         .archetype = &home_archetype,
                         .palette_slots = &home_palette_slots,
                         .health_bar = BuildingHealthBarStyle{1.0F, 0.08F, 1.6F},
                         .selection = BuildingSelectionStyle{2.25F, 2.25F}});
}

} // namespace Render::GL::Roman
