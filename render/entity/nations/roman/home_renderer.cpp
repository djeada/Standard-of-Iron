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

namespace Render::GL::Roman {
namespace {

using Render::Geom::clamp_vec_01;

constexpr std::uint8_t kHomeTeamSlot = 0;

struct RomanPalette {
  QVector3D limestone{0.96F, 0.94F, 0.88F};
  QVector3D limestone_shade{0.88F, 0.85F, 0.78F};
  QVector3D limestone_dark{0.80F, 0.76F, 0.70F};
  QVector3D marble{0.98F, 0.97F, 0.95F};
  QVector3D cedar{0.52F, 0.38F, 0.26F};
  QVector3D cedar_dark{0.38F, 0.26F, 0.16F};
  QVector3D terracotta{0.82F, 0.62F, 0.45F};
  QVector3D terracotta_dark{0.68F, 0.48F, 0.32F};
  QVector3D blue_accent{0.28F, 0.48F, 0.68F};
  QVector3D blue_light{0.40F, 0.60F, 0.80F};
  QVector3D team{0.8F, 0.9F, 1.0F};
  QVector3D team_trim{0.48F, 0.54F, 0.60F};
};

inline auto make_palette(const QVector3D &team) -> RomanPalette {
  RomanPalette p;
  p.team = clamp_vec_01(team);
  p.team_trim = clamp_vec_01(
      QVector3D(team.x() * 0.6F, team.y() * 0.6F, team.z() * 0.6F));
  return p;
}

auto home_palette_slots(const RomanPalette &palette)
    -> std::array<QVector3D, 1> {
  return {palette.team};
}

auto build_home_archetype(BuildingState state) -> RenderArchetype {
  RomanPalette const c = make_palette(QVector3D(1.0F, 1.0F, 1.0F));
  float const wall_height = 0.9F;
  float height_multiplier = 1.0F;
  if (state == BuildingState::Damaged) {
    height_multiplier = 0.7F;
  } else if (state == BuildingState::Destroyed) {
    height_multiplier = 0.4F;
  }

  BuildingArchetypeDesc desc("roman_home");

  desc.add_box(QVector3D(0.0F, 0.06F, 0.0F), QVector3D(1.12F, 0.06F, 1.12F),
               c.limestone_dark);
  desc.add_box(QVector3D(0.0F, 0.14F, 0.0F), QVector3D(1.0F, 0.02F, 1.0F),
               c.limestone);

  float const wall_cy = wall_height * 0.5F * height_multiplier + 0.16F;
  float const wall_hy = wall_height * 0.5F * height_multiplier;
  desc.add_box(QVector3D(0.0F, wall_cy, -0.86F),
               QVector3D(0.80F, wall_hy, 0.08F), c.limestone);
  desc.add_box(QVector3D(0.0F, wall_cy, 0.86F),
               QVector3D(0.80F, wall_hy, 0.08F), c.limestone);
  desc.add_box(QVector3D(-0.86F, wall_cy, 0.0F),
               QVector3D(0.08F, wall_hy, 0.74F), c.limestone);
  desc.add_box(QVector3D(0.86F, wall_cy, 0.0F),
               QVector3D(0.08F, wall_hy, 0.74F), c.limestone);

  float const cornice_y = wall_height * height_multiplier + 0.18F;
  desc.add_box(QVector3D(0.0F, cornice_y, -0.86F),
               QVector3D(0.88F, 0.04F, 0.10F), c.limestone_shade,
               kBuildingStateMaskIntact);
  desc.add_box(QVector3D(0.0F, cornice_y, 0.86F),
               QVector3D(0.88F, 0.04F, 0.10F), c.limestone_shade,
               kBuildingStateMaskIntact);
  desc.add_box(QVector3D(-0.86F, cornice_y, 0.0F),
               QVector3D(0.10F, 0.04F, 0.80F), c.limestone_shade,
               kBuildingStateMaskIntact);
  desc.add_box(QVector3D(0.86F, cornice_y, 0.0F),
               QVector3D(0.10F, 0.04F, 0.80F), c.limestone_shade,
               kBuildingStateMaskIntact);

  for (float xw : {-0.90F, 0.90F}) {
    desc.add_box(QVector3D(xw, 0.56F, -0.28F), QVector3D(0.015F, 0.18F, 0.14F),
                 c.cedar_dark, kBuildingStateMaskIntact, BuildingLODMask::Full);
    desc.add_box(QVector3D(xw, 0.56F, 0.28F), QVector3D(0.015F, 0.18F, 0.14F),
                 c.cedar_dark, kBuildingStateMaskIntact, BuildingLODMask::Full);

    desc.add_box(QVector3D(xw, 0.46F, -0.28F), QVector3D(0.015F, 0.03F, 0.17F),
                 c.limestone_shade, kBuildingStateMaskIntact,
                 BuildingLODMask::Full);
    desc.add_box(QVector3D(xw, 0.46F, 0.28F), QVector3D(0.015F, 0.03F, 0.17F),
                 c.limestone_shade, kBuildingStateMaskIntact,
                 BuildingLODMask::Full);
  }

  float const col_height = 0.8F;
  float const col_radius = 0.06F;
  QVector3D const front_cols[4] = {
      QVector3D(-0.62F, 0.0F, 0.90F), QVector3D(-0.21F, 0.0F, 0.90F),
      QVector3D(0.21F, 0.0F, 0.90F), QVector3D(0.62F, 0.0F, 0.90F)};

  for (const QVector3D &col : front_cols) {
    desc.add_box(QVector3D(col.x(), 0.18F, col.z()),
                 QVector3D(col_radius * 1.2F, 0.04F, col_radius * 1.2F),
                 c.marble, BuildingStateMask::All, BuildingLODMask::Full);
    desc.add_cylinder(
        QVector3D(col.x(), 0.16F, col.z()),
        QVector3D(col.x(), 0.16F + col_height * height_multiplier, col.z()),
        col_radius, c.limestone_shade);
    desc.add_box(QVector3D(col.x(),
                           0.16F + col_height * height_multiplier + 0.04F,
                           col.z()),
                 QVector3D(col_radius * 1.4F, 0.06F, col_radius * 1.4F),
                 c.marble, kBuildingStateMaskIntact, BuildingLODMask::Full);
  }

  float const entab_y = 0.16F + col_height * height_multiplier + 0.12F;
  desc.add_box(QVector3D(0.0F, entab_y, 0.90F), QVector3D(0.72F, 0.05F, 0.06F),
               c.limestone, kBuildingStateMaskIntact);

  desc.add_box(QVector3D(0.0F, 1.25F, 0.0F), QVector3D(1.06F, 0.06F, 1.06F),
               c.terracotta, kBuildingStateMaskIntact);

  for (float z = -0.84F; z <= 0.84F; z += 0.42F) {
    desc.add_box(QVector3D(0.0F, 1.32F, z), QVector3D(1.02F, 0.03F, 0.06F),
                 c.terracotta_dark, kBuildingStateMaskIntact,
                 BuildingLODMask::Full);
  }

  desc.add_box(QVector3D(0.0F, 0.45F, 0.90F), QVector3D(0.30F, 0.40F, 0.05F),
               c.cedar_dark);

  desc.add_box(QVector3D(0.0F, 0.66F, 0.92F), QVector3D(0.32F, 0.04F, 0.02F),
               c.blue_accent, BuildingStateMask::All, BuildingLODMask::Full);

  desc.add_box(QVector3D(0.0F, 0.10F, 1.04F), QVector3D(0.38F, 0.04F, 0.14F),
               c.limestone_shade);

  desc.add_box(QVector3D(0.0F, 0.155F, 0.0F), QVector3D(0.50F, 0.005F, 0.50F),
               c.marble, kBuildingStateMaskIntact, BuildingLODMask::Full);

  desc.add_box(QVector3D(0.0F, 0.168F, 0.32F), QVector3D(0.44F, 0.02F, 0.03F),
               c.limestone_dark, kBuildingStateMaskIntact,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(0.0F, 0.168F, -0.32F), QVector3D(0.44F, 0.02F, 0.03F),
               c.limestone_dark, kBuildingStateMaskIntact,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(0.32F, 0.168F, 0.0F), QVector3D(0.03F, 0.02F, 0.38F),
               c.limestone_dark, kBuildingStateMaskIntact,
               BuildingLODMask::Full);
  desc.add_box(QVector3D(-0.32F, 0.168F, 0.0F), QVector3D(0.03F, 0.02F, 0.38F),
               c.limestone_dark, kBuildingStateMaskIntact,
               BuildingLODMask::Full);

  desc.add_box(QVector3D(0.0F, 0.158F, 0.0F), QVector3D(0.30F, 0.005F, 0.30F),
               c.blue_light, kBuildingStateMaskIntact, BuildingLODMask::Full);

  desc.add_palette_box(QVector3D(0.0F, 0.74F, 0.95F),
                       QVector3D(0.26F, 0.10F, 0.02F), kHomeTeamSlot,
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

  RomanPalette const palette =
      make_palette(QVector3D(r->color[0], r->color[1], r->color[2]));
  const auto palette_slots = home_palette_slots(palette);
  submit_building_instance(out, p, home_archetype(resolve_building_state(p)),
                           palette_slots);
  draw_building_health_bar(out, p, BuildingHealthBarStyle{1.0F, 0.08F, 1.6F});
  draw_building_selection_overlay(out, p, BuildingSelectionStyle{1.5F, 1.5F});
}

} // namespace

void register_home_renderer(Render::GL::EntityRendererRegistry &registry) {
  register_building_renderer(registry, "roman", "home", draw_home);
}

} // namespace Render::GL::Roman
