#include "home_renderer.h"

#include <QVector3D>

#include <algorithm>
#include <array>

#include "../../../../game/core/component.h"
#include "../../../geom/math_utils.h"
#include "../../../submitter.h"
#include "../../building_archetype_desc.h"
#include "../../building_ornaments.h"
#include "../../building_render_common.h"
#include "../../building_state.h"
#include "../../registry.h"

namespace Render::GL::Carthage {
namespace {

using Render::Geom::clamp_vec_01;

constexpr std::uint8_t k_home_team_slot = 0;

struct CarthagePalette {
  QVector3D stone_light{0.62F, 0.60F, 0.58F};
  QVector3D stone_dark{0.50F, 0.48F, 0.46F};
  QVector3D stone_base{0.55F, 0.53F, 0.51F};
  QVector3D brick{0.75F, 0.52F, 0.42F};
  QVector3D brick_dark{0.62F, 0.42F, 0.32F};
  QVector3D tile_red{0.72F, 0.40F, 0.30F};
  QVector3D tile_dark{0.58F, 0.30F, 0.22F};
  QVector3D wood{0.42F, 0.28F, 0.16F};
  QVector3D wood_dark{0.32F, 0.20F, 0.10F};
  QVector3D royal_purple{0.46F, 0.22F, 0.44F};
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

auto home_palette_slots(const CarthagePalette& palette) -> std::array<QVector3D, 1> {
  return {palette.team};
}

auto build_home_archetype(BuildingState state) -> RenderArchetype {
  CarthagePalette const c = make_palette(QVector3D(1.0F, 1.0F, 1.0F));
  float const wall_height = 0.9F;
  float height_multiplier = 1.0F;

  if (state == BuildingState::Damaged) {
    height_multiplier = 0.7F;
  } else if (state == BuildingState::Destroyed) {
    height_multiplier = 0.4F;
  }

  BuildingArchetypeDesc desc("carthage_home");

  // == FOUNDATION: Heavy fortress-style stepped base ==
  desc.add_box(
      QVector3D(0.0F, 0.06F, 0.0F), QVector3D(1.14F, 0.06F, 1.14F), c.stone_dark);
  desc.add_box(
      QVector3D(0.0F, 0.14F, 0.0F), QVector3D(1.06F, 0.02F, 1.06F), c.stone_base);
  desc.add_box(
      QVector3D(0.0F, 0.18F, 0.0F), QVector3D(1.00F, 0.02F, 1.00F), c.stone_light);

  // == MAIN WALLS: Thick Punic brick with plaster ==
  float const wall_cy = wall_height * 0.5F * height_multiplier + 0.20F;
  float const wall_hy = wall_height * 0.5F * height_multiplier;
  desc.add_box(
      QVector3D(0.0F, wall_cy, -0.92F), QVector3D(0.88F, wall_hy, 0.10F), c.brick);
  desc.add_box(
      QVector3D(0.0F, wall_cy, 0.92F), QVector3D(0.88F, wall_hy, 0.10F), c.brick);
  desc.add_box(
      QVector3D(-0.92F, wall_cy, 0.0F), QVector3D(0.10F, wall_hy, 0.82F), c.brick);
  desc.add_box(
      QVector3D(0.92F, wall_cy, 0.0F), QVector3D(0.10F, wall_hy, 0.82F), c.brick);

  // == WALL BANDING: Horizontal stone courses (Punic masonry signature) ==
  float const lower_band_y = wall_height * 0.25F * height_multiplier + 0.20F;
  float const upper_band_y = wall_height * 0.65F * height_multiplier + 0.20F;
  for (float const band_y : {lower_band_y, upper_band_y}) {
    desc.add_box(QVector3D(0.0F, band_y, -0.92F),
                 QVector3D(0.90F, 0.03F, 0.12F),
                 c.stone_light,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(0.0F, band_y, 0.92F),
                 QVector3D(0.90F, 0.03F, 0.12F),
                 c.stone_light,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(-0.92F, band_y, 0.0F),
                 QVector3D(0.12F, 0.03F, 0.86F),
                 c.stone_light,
                 k_building_state_mask_intact);
    desc.add_box(QVector3D(0.92F, band_y, 0.0F),
                 QVector3D(0.12F, 0.03F, 0.86F),
                 c.stone_light,
                 k_building_state_mask_intact);
  }

  // == CORNER TOWERS: Rectangular pilasters (Punic fortress motif) ==
  float const post_hy = wall_height * 0.5F * height_multiplier;
  float const post_cy = post_hy + 0.20F;
  for (float const px : {-0.96F, 0.96F}) {
    for (float const pz : {-0.96F, 0.96F}) {
      desc.add_box(QVector3D(px, post_cy, pz),
                   QVector3D(0.08F, post_hy + 0.04F, 0.08F),
                   c.stone_light,
                   k_building_state_mask_intact);
      // Pilaster cap
      desc.add_box(QVector3D(px, post_cy + post_hy + 0.06F, pz),
                   QVector3D(0.10F, 0.03F, 0.10F),
                   c.stone_dark,
                   k_building_state_mask_intact,
                   BuildingLODMask::Full);
    }
  }

  // == ROOF: Flat terracotta with parapets ==
  float const roof_y = wall_height * height_multiplier + 0.24F;
  desc.add_box(QVector3D(0.0F, roof_y, 0.0F),
               QVector3D(1.02F, 0.05F, 1.02F),
               c.tile_red,
               k_building_state_mask_intact);
  add_tile_rows_z(
      [&](const QVector3D& center, const QVector3D& size, const QVector3D& color) {
        desc.add_box(
            center, size, color, k_building_state_mask_intact, BuildingLODMask::Full);
      },
      roof_y + 0.06F,
      -0.82F,
      0.82F,
      0.28F,
      QVector3D(0.98F, 0.02F, 0.05F),
      c.tile_dark);

  // == MERLONS: Stepped battlements on all four sides (key Punic identifier) ==
  float const parapet_y = roof_y + 0.08F;
  float const merlon_h = 0.13F;
  auto add_merlon =
      [&](const QVector3D& center, const QVector3D& size, const QVector3D& color) {
        desc.add_box(
            center, size, color, k_building_state_mask_intact, BuildingLODMask::Full);
      };
  add_merlon_strip_x(add_merlon,
                     parapet_y,
                     -0.94F,
                     -0.68F,
                     0.34F,
                     5,
                     QVector3D(0.12F, merlon_h, 0.08F),
                     c.brick);
  add_merlon_strip_x(add_merlon,
                     parapet_y,
                     0.94F,
                     -0.68F,
                     0.34F,
                     5,
                     QVector3D(0.12F, merlon_h, 0.08F),
                     c.brick);
  add_merlon_strip_z(add_merlon,
                     -0.94F,
                     parapet_y,
                     -0.68F,
                     0.34F,
                     5,
                     QVector3D(0.08F, merlon_h, 0.12F),
                     c.brick);
  add_merlon_strip_z(add_merlon,
                     0.94F,
                     parapet_y,
                     -0.68F,
                     0.34F,
                     5,
                     QVector3D(0.08F, merlon_h, 0.12F),
                     c.brick);

  // == ENTRANCE: Monumental arched doorway ==
  desc.add_box(
      QVector3D(0.0F, 0.44F, 0.97F), QVector3D(0.32F, 0.44F, 0.06F), c.wood_dark);
  desc.add_box(QVector3D(0.0F, 0.92F, 0.98F),
               QVector3D(0.36F, 0.05F, 0.08F),
               c.stone_light,
               BuildingStateMask::All,
               BuildingLODMask::Full);
  // Door frame pillars
  desc.add_box(QVector3D(-0.34F, 0.52F, 0.96F),
               QVector3D(0.04F, 0.32F, 0.04F),
               c.stone_light,
               k_building_state_mask_intact,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(0.34F, 0.52F, 0.96F),
               QVector3D(0.04F, 0.32F, 0.04F),
               c.stone_light,
               k_building_state_mask_intact,
               BuildingLODMask::Full);

  // == ENTRANCE STEPS ==
  desc.add_box(
      QVector3D(0.0F, 0.10F, 1.06F), QVector3D(0.40F, 0.04F, 0.14F), c.stone_light);
  desc.add_box(
      QVector3D(0.0F, 0.16F, 1.02F), QVector3D(0.36F, 0.02F, 0.10F), c.stone_base);

  // == WINDOWS: Narrow arrow-slit style (fortress theme) ==
  for (float const xw : {-0.95F, 0.95F}) {
    desc.add_box(QVector3D(xw, 0.58F, -0.32F),
                 QVector3D(0.015F, 0.22F, 0.06F),
                 c.wood_dark,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
    desc.add_box(QVector3D(xw, 0.58F, 0.32F),
                 QVector3D(0.015F, 0.22F, 0.06F),
                 c.wood_dark,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
    // Window lintels
    desc.add_box(QVector3D(xw, 0.82F, -0.32F),
                 QVector3D(0.02F, 0.03F, 0.09F),
                 c.stone_light,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
    desc.add_box(QVector3D(xw, 0.82F, 0.32F),
                 QVector3D(0.02F, 0.03F, 0.09F),
                 c.stone_light,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
  }

  // == INTERIOR COURTYARD: Visible cistern/well ==
  desc.add_box(QVector3D(0.0F, 0.21F, 0.0F),
               QVector3D(0.62F, 0.005F, 0.62F),
               c.stone_dark,
               k_building_state_mask_intact,
               BuildingLODMask::Full);
  desc.add_cylinder(QVector3D(0.0F, 0.22F, 0.0F),
                    QVector3D(0.0F, 0.38F, 0.0F),
                    0.18F,
                    c.stone_light,
                    k_building_state_mask_intact,
                    BuildingLODMask::Full);
  desc.add_box(QVector3D(0.0F, 0.39F, 0.0F),
               QVector3D(0.22F, 0.02F, 0.22F),
               c.stone_base,
               k_building_state_mask_intact,
               BuildingLODMask::Full);
  // Courtyard basin (blue water)
  desc.add_cylinder(QVector3D(0.0F, 0.24F, 0.0F),
                    QVector3D(0.0F, 0.25F, 0.0F),
                    0.14F,
                    QVector3D(0.25F, 0.42F, 0.55F),
                    k_building_state_mask_intact,
                    BuildingLODMask::Full);

  // == ROYAL PURPLE CLOTH: Draped over entrance (Carthaginian cultural marker) ==
  if (state != BuildingState::Destroyed) {
    desc.add_box(QVector3D(0.0F, 0.78F, 1.04F),
                 QVector3D(0.44F, 0.05F, 0.10F),
                 c.royal_purple,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
    // Banner support poles
    desc.add_box(QVector3D(-0.38F, 0.62F, 1.00F),
                 QVector3D(0.02F, 0.20F, 0.02F),
                 c.wood_dark,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
    desc.add_box(QVector3D(0.38F, 0.62F, 1.00F),
                 QVector3D(0.02F, 0.20F, 0.02F),
                 c.wood_dark,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);

    // == WATCHTOWER: Small elevated lookout on roof (fortress identity) ==
    desc.add_box(QVector3D(0.0F, roof_y + 0.22F, -0.10F),
                 QVector3D(0.26F, 0.12F, 0.26F),
                 c.stone_light,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
    desc.add_box(QVector3D(0.0F, roof_y + 0.38F, -0.10F),
                 QVector3D(0.22F, 0.04F, 0.22F),
                 c.tile_red,
                 k_building_state_mask_intact,
                 BuildingLODMask::Full);
    if (state == BuildingState::Normal) {
      // Lookout pinnacle
      desc.add_box(QVector3D(0.0F, roof_y + 0.46F, -0.10F),
                   QVector3D(0.08F, 0.06F, 0.08F),
                   c.royal_purple,
                   BuildingStateMask::Normal,
                   BuildingLODMask::Full);
    }
  }

  // == BACK WALL DETAIL: Recessed niche (Punic religious motif) ==
  desc.add_box(QVector3D(0.0F, 0.52F, -0.94F),
               QVector3D(0.22F, 0.28F, 0.02F),
               c.stone_dark,
               k_building_state_mask_intact,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(0.0F, 0.82F, -0.94F),
               QVector3D(0.26F, 0.04F, 0.03F),
               c.stone_light,
               k_building_state_mask_intact,
               BuildingLODMask::Full);

  // == TEAM COLOR BANNER ==
  desc.add_palette_box(QVector3D(0.0F, 0.80F, 1.01F),
                       QVector3D(0.30F, 0.12F, 0.02F),
                       k_home_team_slot,
                       BuildingStateMask::All,
                       BuildingLODMask::Full);

  return build_building_archetype(desc, state);
}

auto home_archetype(BuildingState state) -> const RenderArchetype& {
  static const BuildingArchetypeSet k_set =
      build_stateful_building_archetype_set(build_home_archetype);
  return k_set.for_state(state);
}

void draw_home(const DrawContext& p, ISubmitter& out) {
  if (p.entity == nullptr) {
    return;
  }

  auto* r = p.entity->get_component<Engine::Core::RenderableComponent>();
  if (r == nullptr) {
    return;
  }

  CarthagePalette const palette =
      make_palette(QVector3D(r->color[0], r->color[1], r->color[2]));
  const auto palette_slots = home_palette_slots(palette);
  submit_building_instance(
      out, p, home_archetype(resolve_building_state(p)), palette_slots);
  draw_building_health_bar(out, p, BuildingHealthBarStyle{1.0F, 0.08F, 1.5F});
  draw_building_selection_overlay(out, p, BuildingSelectionStyle{2.1F, 2.1F});
}

} // namespace

void register_home_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_building_renderer(registry, "carthage", "home", draw_home);
}

} // namespace Render::GL::Carthage
