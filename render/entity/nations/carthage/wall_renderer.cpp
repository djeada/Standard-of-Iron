#include "wall_renderer.h"

#include <QVector3D>

#include <array>

#include "../../../../game/core/component.h"
#include "../../../geom/math_utils.h"
#include "../../../submitter.h"
#include "../../building_archetype_desc.h"
#include "../../building_render_common.h"
#include "../../registry.h"

namespace Render::GL::Carthage {
namespace {

using Render::Geom::clamp_vec_01;

constexpr std::uint8_t k_wall_team_slot = 0;

struct WallPalette {
  QVector3D wood_light{0.68F, 0.50F, 0.30F};
  QVector3D wood_mid{0.55F, 0.40F, 0.22F};
  QVector3D wood_dark{0.40F, 0.28F, 0.14F};
  QVector3D plank_highlight{0.76F, 0.58F, 0.36F};
  QVector3D rope{0.58F, 0.48F, 0.30F};
  QVector3D team{0.8F, 0.9F, 1.0F};
};

auto make_palette(const QVector3D& team) -> WallPalette {
  WallPalette p;
  p.team = clamp_vec_01(team);
  return p;
}

auto wall_archetype() -> const RenderArchetype& {
  static const RenderArchetype k_wall = [] {
    WallPalette const c = make_palette(QVector3D(1.0F, 1.0F, 1.0F));
    BuildingArchetypeDesc desc("carthage_wall_segment");

    // Base posts (slightly wider for Carthage style)
    desc.add_box(
        QVector3D(-0.82F, 0.12F, 0.0F), QVector3D(0.12F, 0.12F, 0.22F), c.wood_dark);
    desc.add_box(
        QVector3D(0.82F, 0.12F, 0.0F), QVector3D(0.12F, 0.12F, 0.22F), c.wood_dark);

    // Diagonal cross-bracing (Carthage style)
    desc.add_box(
        QVector3D(-0.40F, 0.55F, 0.0F), QVector3D(0.06F, 0.44F, 0.12F), c.wood_dark,
        BuildingStateMask::All, BuildingLODMask::Full);
    desc.add_box(
        QVector3D(0.40F, 0.55F, 0.0F), QVector3D(0.06F, 0.44F, 0.12F), c.wood_dark,
        BuildingStateMask::All, BuildingLODMask::Full);
    desc.add_box(
        QVector3D(-0.82F, 0.55F, 0.0F), QVector3D(0.10F, 0.44F, 0.18F), c.wood_dark,
        BuildingStateMask::All, BuildingLODMask::Full);
    desc.add_box(
        QVector3D(0.82F, 0.55F, 0.0F), QVector3D(0.10F, 0.44F, 0.18F), c.wood_dark,
        BuildingStateMask::All, BuildingLODMask::Full);

    // Horizontal planks
    desc.add_box(
        QVector3D(0.0F, 0.24F, 0.0F), QVector3D(0.90F, 0.045F, 0.18F), c.wood_light);
    desc.add_box(
        QVector3D(0.0F, 0.35F, 0.0F), QVector3D(0.90F, 0.045F, 0.18F), c.wood_mid);
    desc.add_box(
        QVector3D(0.0F, 0.46F, 0.0F), QVector3D(0.90F, 0.045F, 0.18F), c.wood_light);
    desc.add_box(
        QVector3D(0.0F, 0.57F, 0.0F), QVector3D(0.90F, 0.045F, 0.18F), c.wood_mid);
    desc.add_box(
        QVector3D(0.0F, 0.68F, 0.0F), QVector3D(0.90F, 0.045F, 0.18F), c.wood_light);
    desc.add_box(
        QVector3D(0.0F, 0.79F, 0.0F), QVector3D(0.90F, 0.045F, 0.18F), c.wood_mid);
    desc.add_box(
        QVector3D(0.0F, 0.90F, 0.0F), QVector3D(0.90F, 0.045F, 0.18F), c.wood_light);

    // Rope lashings (Carthage style detail)
    desc.add_box(
        QVector3D(-0.40F, 0.36F, 0.0F), QVector3D(0.04F, 0.035F, 0.19F), c.rope,
        BuildingStateMask::All, BuildingLODMask::Full);
    desc.add_box(
        QVector3D(0.40F, 0.36F, 0.0F), QVector3D(0.04F, 0.035F, 0.19F), c.rope,
        BuildingStateMask::All, BuildingLODMask::Full);
    desc.add_box(
        QVector3D(-0.40F, 0.76F, 0.0F), QVector3D(0.04F, 0.035F, 0.19F), c.rope,
        BuildingStateMask::All, BuildingLODMask::Full);
    desc.add_box(
        QVector3D(0.40F, 0.76F, 0.0F), QVector3D(0.04F, 0.035F, 0.19F), c.rope,
        BuildingStateMask::All, BuildingLODMask::Full);

    // Top sharpened stakes
    for (int i = -3; i <= 3; ++i) {
      desc.add_box(QVector3D(static_cast<float>(i) * 0.25F, 1.02F, 0.0F),
                   QVector3D(0.06F, 0.12F, 0.06F),
                   c.plank_highlight,
                   BuildingStateMask::All,
                   BuildingLODMask::Full);
    }

    // Team banner post
    desc.add_box(QVector3D(0.0F, 1.02F, 0.0F),
                 QVector3D(0.022F, 0.16F, 0.022F),
                 c.wood_dark,
                 BuildingStateMask::All,
                 BuildingLODMask::Full);
    desc.add_palette_box(QVector3D(0.06F, 1.10F, 0.0F),
                         QVector3D(0.11F, 0.07F, 0.010F),
                         k_wall_team_slot,
                         BuildingStateMask::All,
                         BuildingLODMask::Full);

    return build_building_archetype(desc, BuildingState::Normal);
  }();
  return k_wall;
}

auto wall_palette_slots(const WallPalette& palette) -> std::array<QVector3D, 1> {
  return {palette.team};
}

void draw_wall_segment(const DrawContext& p, ISubmitter& out) {
  if (p.entity == nullptr) {
    return;
  }

  auto* r = p.entity->get_component<Engine::Core::RenderableComponent>();
  if (r == nullptr) {
    return;
  }

  WallPalette const palette =
      make_palette(QVector3D(r->color[0], r->color[1], r->color[2]));
  const auto palette_slots = wall_palette_slots(palette);
  submit_building_instance(out, p, wall_archetype(), palette_slots);
  draw_building_compact_health_bar(out, p, 1.3F);
  draw_building_selection_overlay(out, p, BuildingSelectionStyle{2.2F, 0.6F});
}

} // namespace

void register_wall_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_building_renderer(registry, "carthage", "wall_segment", draw_wall_segment);
}

} // namespace Render::GL::Carthage
