#include "defense_tower_renderer.h"

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "../../../../game/core/component.h"
#include "../../../geom/math_utils.h"
#include "../../../gl/backend.h"
#include "../../../gl/primitives.h"
#include "../../../submitter.h"
#include "../../barracks_flag_renderer.h"
#include "../../building_archetype_desc.h"
#include "../../building_render_common.h"
#include "../../registry.h"

namespace Render::GL::Carthage {
namespace {

using Render::Geom::clamp_vec_01;

struct TowerPalette {
  QVector3D stone_light{0.62F, 0.60F, 0.58F};
  QVector3D stone_dark{0.50F, 0.48F, 0.46F};
  QVector3D stone_base{0.55F, 0.53F, 0.51F};
  QVector3D brick{0.75F, 0.52F, 0.42F};
  QVector3D brick_dark{0.62F, 0.42F, 0.32F};
  QVector3D tile_red{0.72F, 0.40F, 0.30F};
  QVector3D wood{0.42F, 0.28F, 0.16F};
  QVector3D wood_dark{0.32F, 0.20F, 0.10F};
  QVector3D iron{0.35F, 0.35F, 0.38F};
  QVector3D team{0.8F, 0.9F, 1.0F};
  QVector3D team_trim{0.48F, 0.54F, 0.60F};
};

auto make_palette(const QVector3D& team) -> TowerPalette {
  TowerPalette p;
  p.team = clamp_vec_01(team);
  p.team_trim =
      clamp_vec_01(QVector3D(team.x() * 0.6F, team.y() * 0.6F, team.z() * 0.6F));
  return p;
}

auto build_tower_archetype(BuildingState state) -> RenderArchetype {
  TowerPalette const c = make_palette(QVector3D(1.0F, 1.0F, 1.0F));
  BuildingArchetypeDesc desc("carthage_defense_tower");

  const bool damaged = state == BuildingState::Damaged;
  const bool destroyed = state == BuildingState::Destroyed;
  const float core_half_y = destroyed ? 0.46F : (damaged ? 0.58F : 0.70F);
  const float core_top = 0.5F + core_half_y * 2.0F;
  const float upper_drum_y = destroyed ? 0.0F : (damaged ? 1.48F : 1.65F);
  const float deck_y = destroyed ? 0.0F : (damaged ? 1.74F : 2.00F);
  const float roof_y = destroyed ? 0.0F : (damaged ? 2.02F : 2.40F);
  desc.add_box(
      QVector3D(0.0F, 0.05F, 0.0F), QVector3D(1.15F, 0.05F, 1.15F), c.stone_base);
  desc.add_box(
      QVector3D(0.0F, 0.15F, 0.0F), QVector3D(1.0F, 0.15F, 1.0F), c.stone_base);
  for (float x = -0.9F; x <= 0.9F; x += 0.45F) {
    desc.add_box(QVector3D(x, 0.35F, -0.85F),
                 QVector3D(0.12F, 0.08F, 0.08F),
                 c.brick_dark,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
    desc.add_box(QVector3D(x, 0.35F, 0.85F),
                 QVector3D(0.12F, 0.08F, 0.08F),
                 c.brick_dark,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
  }
  for (float z = -0.8F; z <= 0.8F; z += 0.4F) {
    desc.add_box(QVector3D(-0.85F, 0.35F, z),
                 QVector3D(0.08F, 0.08F, 0.12F),
                 c.brick_dark,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
    desc.add_box(QVector3D(0.85F, 0.35F, z),
                 QVector3D(0.08F, 0.08F, 0.12F),
                 c.brick_dark,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
  }
  desc.add_box(QVector3D(0.0F, 0.5F, 0.0F), QVector3D(0.9F, 0.1F, 0.9F), c.stone_light);
  desc.add_box(
      QVector3D(0.0F, 0.5F + core_half_y, 0.0F),
      QVector3D(destroyed ? 0.66F : 0.75F, core_half_y, destroyed ? 0.66F : 0.75F),
      c.stone_light);

  if (!destroyed) {
    desc.add_box(QVector3D(0.0F, damaged ? 1.02F : 1.18F, 0.0F),
                 QVector3D(damaged ? 0.72F : 0.78F, 0.04F, damaged ? 0.72F : 0.78F),
                 c.stone_dark,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
  }

  const std::array<int, 4> corner_indices =
      damaged ? std::array<int, 4>{0, 1, 3, -1} : std::array<int, 4>{0, 1, 2, 3};
  for (int const index : corner_indices) {
    if (index < 0) {
      continue;
    }
    const float angle = static_cast<float>(index) * 1.57F;
    const float ox = std::sin(angle) * 0.65F;
    const float oz = std::cos(angle) * 0.65F;
    desc.add_cylinder(QVector3D(ox, 0.4F, oz),
                      QVector3D(ox, damaged ? 1.82F : 2.05F, oz),
                      damaged ? 0.13F : 0.15F,
                      c.stone_dark);
    desc.add_box(QVector3D(ox, damaged ? 1.88F : 2.12F, oz),
                 QVector3D(damaged ? 0.17F : 0.20F, 0.07F, damaged ? 0.17F : 0.20F),
                 c.stone_light,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
  }

  if (!destroyed) {
    const int buttress_count = damaged ? 2 : 4;
    for (int i = 0; i < buttress_count; ++i) {
      const float angle =
          static_cast<float>(i) * (damaged ? std::numbers::pi_v<float> : 1.57F) +
          0.785F;
      const float ox = std::sin(angle) * 0.62F;
      const float oz = std::cos(angle) * 0.62F;
      desc.add_box(QVector3D(ox, damaged ? 0.78F : 0.9F, oz),
                   QVector3D(0.12F, damaged ? 0.28F : 0.4F, 0.12F),
                   c.brick,
                   BuildingStateMask::All,
                   BuildingLODMask::Full);
    }

    desc.add_box(QVector3D(0.0F, upper_drum_y, 0.0F),
                 QVector3D(damaged ? 0.74F : 0.82F, 0.08F, damaged ? 0.74F : 0.82F),
                 c.brick_dark);
    desc.add_box(QVector3D(0.0F, damaged ? 1.84F : 1.94F, 0.0F),
                 QVector3D(damaged ? 0.90F : 1.00F, 0.04F, damaged ? 0.90F : 1.00F),
                 c.stone_dark,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
    desc.add_box(QVector3D(0.0F, deck_y, 0.0F),
                 QVector3D(damaged ? 0.86F : 0.95F, 0.05F, damaged ? 0.86F : 0.95F),
                 c.wood);

    const int merlon_count = damaged ? 4 : 8;
    for (int i = 0; i < merlon_count; ++i) {
      const float angle =
          static_cast<float>(i) * (6.28318F / static_cast<float>(merlon_count));
      const float ox = std::sin(angle) * (damaged ? 0.74F : 0.82F);
      const float oz = std::cos(angle) * (damaged ? 0.74F : 0.82F);
      desc.add_box(QVector3D(ox, damaged ? 2.02F : 2.19F, oz),
                   QVector3D(0.13F, damaged ? 0.18F : 0.22F, 0.13F),
                   c.brick,
                   BuildingStateMask::All,
                   BuildingLODMask::Full);
    }

    desc.add_box(QVector3D(0.0F, roof_y, 0.0F),
                 QVector3D(damaged ? 0.88F : 1.0F, 0.03F, damaged ? 0.88F : 1.0F),
                 c.tile_red);
  }

  if (damaged) {
    desc.add_box(
        QVector3D(0.42F, 1.48F, 0.30F), QVector3D(0.20F, 0.10F, 0.16F), c.brick_dark);
    desc.add_box(
        QVector3D(-0.32F, 1.22F, -0.34F), QVector3D(0.18F, 0.12F, 0.18F), c.stone_dark);
  }

  if (destroyed) {
    desc.add_box(
        QVector3D(0.0F, 1.32F, 0.0F), QVector3D(0.72F, 0.08F, 0.50F), c.brick_dark);
    desc.add_box(
        QVector3D(0.38F, 0.98F, 0.34F), QVector3D(0.20F, 0.14F, 0.18F), c.brick);
    desc.add_box(
        QVector3D(-0.36F, 0.92F, -0.30F), QVector3D(0.22F, 0.12F, 0.16F), c.stone_dark);
  }

  return build_building_archetype(desc, state);
}

auto tower_archetype(BuildingState state) -> const RenderArchetype& {
  static const BuildingArchetypeSet k_set =
      build_stateful_building_archetype_set(build_tower_archetype);
  return k_set.for_state(state);
}

auto tower_health_bar_style(BuildingState state) -> BuildingHealthBarStyle {
  switch (state) {
  case BuildingState::Damaged:
    return BuildingHealthBarStyle{0.96F, 0.085F, 2.82F, true};
  case BuildingState::Destroyed:
    return BuildingHealthBarStyle{0.90F, 0.08F, 1.86F, true};
  case BuildingState::Normal:
  default:
    return BuildingHealthBarStyle{1.02F, 0.09F, 3.18F, true};
  }
}

auto tower_banner_style(BuildingState state)
    -> BarracksFlagRenderer::HangingBannerStyle {
  if (state == BuildingState::Damaged) {
    return {.pole_base = QVector3D(0.0F, 2.10F, 0.0F),
            .pole_height = 0.88F,
            .pole_radius = 0.042F,
            .banner_width = 0.56F,
            .banner_height = 0.34F,
            .beam_inset = 0.02F,
            .banner_depth = 0.02F,
            .banner_z_offset = 0.0F,
            .connector_drop_ratio = 0.30F,
            .capture_lowering_ratio = 0.45F,
            .pole_color = QVector3D(),
            .beam_color = QVector3D(),
            .connector_color = QVector3D(),
            .ornament_offset = QVector3D(0.0F, 0.90F, 0.0F),
            .ornament_size = QVector3D(0.12F, 0.10F, 0.12F),
            .ornament_color = QVector3D(),
            .ring_count = 1,
            .ring_y_start = 0.26F,
            .ring_spacing = 0.22F,
            .ring_height = 0.024F,
            .ring_radius_scale = 2.1F,
            .ring_color = QVector3D()};
  }

  return {.pole_base = QVector3D(0.0F, 2.48F, 0.0F),
          .pole_height = 1.16F,
          .pole_radius = 0.045F,
          .banner_width = 0.72F,
          .banner_height = 0.40F,
          .beam_inset = 0.02F,
          .banner_depth = 0.02F,
          .banner_z_offset = 0.0F,
          .connector_drop_ratio = 0.32F,
          .capture_lowering_ratio = 0.50F,
          .pole_color = QVector3D(),
          .beam_color = QVector3D(),
          .connector_color = QVector3D(),
          .ornament_offset = QVector3D(0.0F, 1.12F, 0.0F),
          .ornament_size = QVector3D(0.14F, 0.11F, 0.14F),
          .ornament_color = QVector3D(),
          .ring_count = 2,
          .ring_y_start = 0.44F,
          .ring_spacing = 0.24F,
          .ring_height = 0.026F,
          .ring_radius_scale = 2.2F,
          .ring_color = QVector3D()};
}

void draw_tower_banner(const DrawContext& p,
                       ISubmitter& out,
                       const TowerPalette& palette,
                       BuildingState state) {
  if (state == BuildingState::Destroyed || p.resources == nullptr) {
    return;
  }

  Mesh* unit = p.resources->unit();
  if (unit == nullptr) {
    unit = get_unit_cube();
  }
  Texture* white = p.resources->white();

  auto style = tower_banner_style(state);
  style.pole_color = palette.wood_dark;
  style.beam_color = palette.wood;
  style.connector_color = palette.stone_light;
  style.ornament_color = palette.iron;
  style.ring_color = palette.iron;

  BarracksFlagRenderer::ClothBannerResources cloth;
  if (p.backend != nullptr) {
    cloth.cloth_mesh = p.backend->banner_mesh();
    cloth.banner_shader = p.backend->banner_shader();
  }

  BarracksFlagRenderer::draw_hanging_banner(
      p, out, unit, white, palette.team, palette.team_trim, style, &cloth);
}

void draw_defense_tower(const DrawContext& p, ISubmitter& out) {
  if (p.entity == nullptr) {
    return;
  }

  auto* r = p.entity->get_component<Engine::Core::RenderableComponent>();
  if (r == nullptr) {
    return;
  }

  TowerPalette const palette =
      make_palette(QVector3D(r->color[0], r->color[1], r->color[2]));
  BuildingState const state = resolve_building_state(p);
  submit_building_instance(out, p, tower_archetype(state));
  draw_tower_banner(p, out, palette, state);
  draw_building_health_bar(out, p, tower_health_bar_style(state));
  draw_building_selection_overlay(out, p, BuildingSelectionStyle{1.6F, 1.6F});
}

} // namespace

void register_defense_tower_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_building_renderer(registry, "carthage", "defense_tower", draw_defense_tower);
}

} // namespace Render::GL::Carthage
