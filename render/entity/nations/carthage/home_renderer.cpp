#include "home_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../geom/math_utils.h"
#include "../../../submitter.h"
#include "../../building_archetype_desc.h"
#include "../../building_render_common.h"
#include "../../building_state.h"
#include "../../registry.h"

#include <QVector3D>
#include <algorithm>
#include <array>

namespace Render::GL::Carthage {
namespace {

using Render::Geom::clamp_vec_01;

constexpr std::uint8_t kHomeTeamSlot = 0;

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
  QVector3D team{0.8F, 0.9F, 1.0F};
  QVector3D team_trim{0.48F, 0.54F, 0.60F};
};

inline auto make_palette(const QVector3D &team) -> CarthagePalette {
  CarthagePalette p;
  p.team = clamp_vec_01(team);
  p.team_trim = clamp_vec_01(
      QVector3D(team.x() * 0.6F, team.y() * 0.6F, team.z() * 0.6F));
  return p;
}

auto home_palette_slots(const CarthagePalette &palette)
    -> std::array<QVector3D, 1> {
  return {palette.team};
}

auto build_home_archetype(BuildingState state) -> RenderArchetype {
  CarthagePalette const c = make_palette(QVector3D(1.0F, 1.0F, 1.0F));
  float const wall_height = 0.8F;
  float height_multiplier = 1.0F;

  if (state == BuildingState::Damaged) {
    height_multiplier = 0.7F;
  } else if (state == BuildingState::Destroyed) {
    height_multiplier = 0.4F;
  }

  BuildingArchetypeDesc desc("carthage_home");

  // Foundation — broad stone base
  desc.add_box(QVector3D(0.0F, 0.10F, 0.0F), QVector3D(1.02F, 0.10F, 1.02F),
               c.stone_base);
  desc.add_box(QVector3D(0.0F, 0.22F, 0.0F), QVector3D(0.96F, 0.02F, 0.96F),
               c.stone_light);

  // Walls (four sides)
  float const wall_cy = wall_height * 0.5F * height_multiplier + 0.24F;
  float const wall_hy = wall_height * 0.5F * height_multiplier;
  desc.add_box(QVector3D(0.0F, wall_cy, -0.90F),
               QVector3D(0.85F, wall_hy, 0.08F), c.brick);
  desc.add_box(QVector3D(0.0F, wall_cy, 0.90F),
               QVector3D(0.85F, wall_hy, 0.08F), c.brick);
  desc.add_box(QVector3D(-0.90F, wall_cy, 0.0F),
               QVector3D(0.08F, wall_hy, 0.80F), c.brick);
  desc.add_box(QVector3D(0.90F, wall_cy, 0.0F),
               QVector3D(0.08F, wall_hy, 0.80F), c.brick);

  // Decorative stone banding at mid-height (intact only)
  float const band_y = wall_height * 0.35F * height_multiplier + 0.24F;
  desc.add_box(QVector3D(0.0F, band_y, -0.90F),
               QVector3D(0.88F, 0.04F, 0.10F), c.stone_light,
               kBuildingStateMaskIntact);
  desc.add_box(QVector3D(0.0F, band_y, 0.90F),
               QVector3D(0.88F, 0.04F, 0.10F), c.stone_light,
               kBuildingStateMaskIntact);
  desc.add_box(QVector3D(-0.90F, band_y, 0.0F),
               QVector3D(0.10F, 0.04F, 0.84F), c.stone_light,
               kBuildingStateMaskIntact);
  desc.add_box(QVector3D(0.90F, band_y, 0.0F),
               QVector3D(0.10F, 0.04F, 0.84F), c.stone_light,
               kBuildingStateMaskIntact);

  // Corner accent posts (intact only)
  float const post_hy = wall_height * 0.5F * height_multiplier;
  float const post_cy = post_hy + 0.24F;
  for (float px : {-0.92F, 0.92F}) {
    for (float pz : {-0.92F, 0.92F}) {
      desc.add_box(QVector3D(px, post_cy, pz),
                   QVector3D(0.06F, post_hy, 0.06F), c.stone_light,
                   kBuildingStateMaskIntact);
    }
  }

  // Flat roof with tile-strip texture (intact only)
  desc.add_box(QVector3D(0.0F, 1.08F, 0.0F), QVector3D(1.00F, 0.05F, 1.00F),
               c.tile_red, kBuildingStateMaskIntact);
  for (float z = -0.80F; z <= 0.80F; z += 0.32F) {
    desc.add_box(QVector3D(0.0F, 1.13F, z), QVector3D(0.96F, 0.02F, 0.06F),
                 c.tile_dark, kBuildingStateMaskIntact, BuildingLODMask::Full);
  }

  // Stepped parapet / crenellations on the flat roof edge (intact, Full LOD)
  float const parapet_y = 1.14F;
  float const merlon_h = 0.12F;
  // Back parapet (z = -0.90)
  for (int i = 0; i < 4; ++i) {
    float const mx = -0.54F + float(i) * 0.36F;
    desc.add_box(QVector3D(mx, parapet_y, -0.92F),
                 QVector3D(0.14F, merlon_h, 0.10F), c.brick,
                 kBuildingStateMaskIntact, BuildingLODMask::Full);
  }
  // Front parapet with gap for entrance (z = 0.90)
  for (int i = 0; i < 2; ++i) {
    desc.add_box(
        QVector3D(-0.76F + float(i) * 0.34F, parapet_y, 0.92F),
        QVector3D(0.14F, merlon_h, 0.10F), c.brick, kBuildingStateMaskIntact,
        BuildingLODMask::Full);
    desc.add_box(
        QVector3D(0.42F + float(i) * 0.34F, parapet_y, 0.92F),
        QVector3D(0.14F, merlon_h, 0.10F), c.brick, kBuildingStateMaskIntact,
        BuildingLODMask::Full);
  }
  // Side parapets (x = ±0.90)
  for (float zp : {-0.55F, 0.0F, 0.55F}) {
    desc.add_box(QVector3D(-0.92F, parapet_y, zp),
                 QVector3D(0.10F, merlon_h, 0.14F), c.brick,
                 kBuildingStateMaskIntact, BuildingLODMask::Full);
    desc.add_box(QVector3D(0.92F, parapet_y, zp),
                 QVector3D(0.10F, merlon_h, 0.14F), c.brick,
                 kBuildingStateMaskIntact, BuildingLODMask::Full);
  }

  // Door (wood plank with iron bar accents)
  desc.add_box(QVector3D(0.0F, 0.46F, 0.95F), QVector3D(0.30F, 0.46F, 0.05F),
               c.wood_dark);
  // Door arch / lintel
  desc.add_box(QVector3D(0.0F, 0.93F, 0.96F),
               QVector3D(0.34F, 0.04F, 0.06F), c.stone_light,
               BuildingStateMask::All, BuildingLODMask::Full);

  // Entrance step in front of the door
  desc.add_box(QVector3D(0.0F, 0.13F, 1.04F), QVector3D(0.36F, 0.04F, 0.14F),
               c.stone_light);

  // Window slits on side walls (narrow vertical openings, Full LOD, intact)
  for (float xw : {-0.93F, 0.93F}) {
    desc.add_box(QVector3D(xw, 0.60F, -0.30F),
                 QVector3D(0.015F, 0.20F, 0.07F), c.wood_dark,
                 kBuildingStateMaskIntact, BuildingLODMask::Full);
    desc.add_box(QVector3D(xw, 0.60F, 0.30F),
                 QVector3D(0.015F, 0.20F, 0.07F), c.wood_dark,
                 kBuildingStateMaskIntact, BuildingLODMask::Full);
  }

  // Interior courtyard pavement (visible only when intact and close)
  desc.add_box(QVector3D(0.0F, 0.25F, 0.0F), QVector3D(0.58F, 0.005F, 0.58F),
               c.stone_dark, kBuildingStateMaskIntact, BuildingLODMask::Full);

  // Courtyard fountain — circular basin with rim
  desc.add_cylinder(QVector3D(0.0F, 0.24F, 0.0F),
                    QVector3D(0.0F, 0.34F, 0.0F), 0.15F, c.stone_light,
                    kBuildingStateMaskIntact, BuildingLODMask::Full);
  desc.add_box(QVector3D(0.0F, 0.34F, 0.0F), QVector3D(0.34F, 0.025F, 0.34F),
               c.stone_base, kBuildingStateMaskIntact, BuildingLODMask::Full);

  // Team-colored hanging cloth above the entrance
  desc.add_palette_box(QVector3D(0.0F, 0.78F, 0.99F),
                       QVector3D(0.28F, 0.10F, 0.02F), kHomeTeamSlot,
                       BuildingStateMask::All, BuildingLODMask::Full);

  return build_building_archetype(desc, state);
}

auto home_archetype(BuildingState state) -> const RenderArchetype & {
  static const BuildingArchetypeSet k_set =
      build_stateful_building_archetype_set(build_home_archetype);
  return k_set.for_state(state);
}

void draw_home(const DrawContext &p, ISubmitter &out) {
  if (p.entity == nullptr) {
    return;
  }

  auto *r = p.entity->get_component<Engine::Core::RenderableComponent>();
  if (r == nullptr) {
    return;
  }

  CarthagePalette const palette =
      make_palette(QVector3D(r->color[0], r->color[1], r->color[2]));
  const auto palette_slots = home_palette_slots(palette);
  submit_building_instance(out, p, home_archetype(resolve_building_state(p)),
                           palette_slots);
  draw_building_health_bar(out, p, BuildingHealthBarStyle{1.0F, 0.08F, 1.5F});
  draw_building_selection_overlay(out, p, BuildingSelectionStyle{1.4F, 1.4F});
}

} // namespace

void register_home_renderer(Render::GL::EntityRendererRegistry &registry) {
  register_building_renderer(registry, "carthage", "home", draw_home);
}

} // namespace Render::GL::Carthage
