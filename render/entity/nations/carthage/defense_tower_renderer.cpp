#include "defense_tower_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../geom/math_utils.h"
#include "../../../submitter.h"
#include "../../building_archetype_desc.h"
#include "../../building_render_common.h"
#include "../../registry.h"

#include <QVector3D>
#include <algorithm>
#include <array>

namespace Render::GL::Carthage {
namespace {

using Render::Geom::clamp_vec_01;

constexpr std::uint8_t kTowerTeamSlot = 0;

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
};

auto make_palette(const QVector3D &team) -> TowerPalette {
  TowerPalette p;
  p.team = clamp_vec_01(team);
  return p;
}

auto tower_archetype() -> const RenderArchetype & {
  static const RenderArchetype k_tower = [] {
    TowerPalette const c = make_palette(QVector3D(1.0F, 1.0F, 1.0F));
    BuildingArchetypeDesc desc("carthage_defense_tower");

    // Outer stepped foundation for visual weight
    desc.add_box(QVector3D(0.0F, 0.05F, 0.0F), QVector3D(1.15F, 0.05F, 1.15F),
                 c.stone_base);
    desc.add_box(QVector3D(0.0F, 0.15F, 0.0F), QVector3D(1.0F, 0.15F, 1.0F),
                 c.stone_base);
    for (float x = -0.9F; x <= 0.9F; x += 0.45F) {
      desc.add_box(QVector3D(x, 0.35F, -0.85F), QVector3D(0.12F, 0.08F, 0.08F),
                   c.brick_dark, BuildingStateMask::All, BuildingLODMask::Full);
      desc.add_box(QVector3D(x, 0.35F, 0.85F), QVector3D(0.12F, 0.08F, 0.08F),
                   c.brick_dark, BuildingStateMask::All, BuildingLODMask::Full);
    }
    for (float z = -0.8F; z <= 0.8F; z += 0.4F) {
      desc.add_box(QVector3D(-0.85F, 0.35F, z), QVector3D(0.08F, 0.08F, 0.12F),
                   c.brick_dark, BuildingStateMask::All, BuildingLODMask::Full);
      desc.add_box(QVector3D(0.85F, 0.35F, z), QVector3D(0.08F, 0.08F, 0.12F),
                   c.brick_dark, BuildingStateMask::All, BuildingLODMask::Full);
    }
    desc.add_box(QVector3D(0.0F, 0.5F, 0.0F), QVector3D(0.9F, 0.1F, 0.9F),
                 c.stone_light);

    desc.add_box(QVector3D(0.0F, 1.2F, 0.0F), QVector3D(0.75F, 0.7F, 0.75F),
                 c.stone_light);

    // Mid-body decorative band visible on tower faces
    desc.add_box(QVector3D(0.0F, 1.18F, 0.0F), QVector3D(0.78F, 0.04F, 0.78F),
                 c.stone_dark, BuildingStateMask::All, BuildingLODMask::Full);

    // Taller corner columns with decorative stone caps
    for (int i = 0; i < 4; ++i) {
      float const angle = static_cast<float>(i) * 1.57F;
      float const ox = sinf(angle) * 0.65F;
      float const oz = cosf(angle) * 0.65F;
      desc.add_cylinder(QVector3D(ox, 0.4F, oz), QVector3D(ox, 2.05F, oz), 0.15F,
                        c.stone_dark);
      desc.add_box(QVector3D(ox, 2.12F, oz), QVector3D(0.20F, 0.07F, 0.20F),
                   c.stone_light, BuildingStateMask::All, BuildingLODMask::Full);
    }

    for (int i = 0; i < 4; ++i) {
      float const angle = static_cast<float>(i) * 1.57F + 0.785F;
      float const ox = sinf(angle) * 0.62F;
      float const oz = cosf(angle) * 0.62F;
      desc.add_box(QVector3D(ox, 0.9F, oz), QVector3D(0.12F, 0.4F, 0.12F),
                   c.brick);
    }
    desc.add_box(QVector3D(0.0F, 1.65F, 0.0F), QVector3D(0.82F, 0.08F, 0.82F),
                 c.brick_dark);

    // Corbelled gallery projection below the battlements
    desc.add_box(QVector3D(0.0F, 1.94F, 0.0F), QVector3D(1.00F, 0.04F, 1.00F),
                 c.stone_dark);
    desc.add_box(QVector3D(0.0F, 2.00F, 0.0F), QVector3D(0.95F, 0.05F, 0.95F),
                 c.wood);

    // Taller, more prominent merlons
    for (int i = 0; i < 8; ++i) {
      float const angle = static_cast<float>(i) * 0.785F;
      float const ox = sinf(angle) * 0.82F;
      float const oz = cosf(angle) * 0.82F;
      desc.add_box(QVector3D(ox, 2.19F, oz), QVector3D(0.13F, 0.22F, 0.13F),
                   c.brick, BuildingStateMask::All, BuildingLODMask::Full);
    }
    desc.add_box(QVector3D(0.0F, 2.40F, 0.0F), QVector3D(1.0F, 0.03F, 1.0F),
                 c.tile_red);

    desc.add_cylinder(QVector3D(0.0F, 2.10F, 0.0F), QVector3D(0.0F, 3.00F, 0.0F),
                      0.08F, c.wood_dark);
    desc.add_palette_box(QVector3D(0.16F, 2.70F, 0.0F),
                         QVector3D(0.26F, 0.19F, 0.025F), kTowerTeamSlot);
    for (int i = 0; i < 3; ++i) {
      float const ring_y = 2.38F + static_cast<float>(i) * 0.26F;
      desc.add_cylinder(QVector3D(0.0F, ring_y, 0.0F),
                        QVector3D(0.0F, ring_y + 0.032F, 0.0F), 0.13F, c.iron,
                        BuildingStateMask::All, BuildingLODMask::Full);
    }
    desc.add_box(QVector3D(0.0F, 3.07F, 0.0F), QVector3D(0.11F, 0.09F, 0.11F),
                 c.iron, BuildingStateMask::All, BuildingLODMask::Full);
    return build_building_archetype(desc, BuildingState::Normal);
  }();
  return k_tower;
}

auto tower_palette_slots(const TowerPalette &palette)
    -> std::array<QVector3D, 1> {
  return {palette.team};
}

void draw_defense_tower(const DrawContext &p, ISubmitter &out) {
  if (p.entity == nullptr) {
    return;
  }

  auto *r = p.entity->get_component<Engine::Core::RenderableComponent>();
  if (r == nullptr) {
    return;
  }

  TowerPalette const palette =
      make_palette(QVector3D(r->color[0], r->color[1], r->color[2]));
  const auto palette_slots = tower_palette_slots(palette);
  submit_building_instance(out, p, tower_archetype(), palette_slots);
  draw_building_compact_health_bar(out, p, 3.4F);
  draw_building_selection_overlay(out, p, BuildingSelectionStyle{1.6F, 1.6F});
}

} // namespace

void register_defense_tower_renderer(
    Render::GL::EntityRendererRegistry &registry) {
  register_building_renderer(registry, "carthage", "defense_tower",
                             draw_defense_tower);
}

} // namespace Render::GL::Carthage
