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

namespace Render::GL::Roman {
namespace {

using Render::Geom::clamp_vec_01;

constexpr std::uint8_t kTowerTeamSlot = 0;

struct TowerPalette {
  QVector3D limestone{0.96F, 0.94F, 0.88F};
  QVector3D limestone_shade{0.88F, 0.85F, 0.78F};
  QVector3D limestone_dark{0.80F, 0.76F, 0.70F};
  QVector3D sandstone_light{0.82F, 0.75F, 0.62F};
  QVector3D sandstone_dark{0.70F, 0.62F, 0.50F};
  QVector3D sandstone_base{0.75F, 0.68F, 0.56F};
  QVector3D marble{0.98F, 0.97F, 0.95F};
  QVector3D terracotta{0.80F, 0.55F, 0.38F};
  QVector3D terracotta_dark{0.68F, 0.48F, 0.32F};
  QVector3D cedar{0.52F, 0.38F, 0.26F};
  QVector3D cedar_dark{0.38F, 0.26F, 0.16F};
  QVector3D blue_accent{0.28F, 0.48F, 0.68F};
  QVector3D blue_light{0.40F, 0.60F, 0.80F};
  QVector3D bronze{0.60F, 0.45F, 0.25F};
  QVector3D gold{0.85F, 0.72F, 0.35F};
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
    BuildingArchetypeDesc desc("roman_defense_tower");

    desc.add_box(QVector3D(0.0F, 0.12F, 0.0F), QVector3D(1.1F, 0.12F, 1.1F),
                 c.limestone_dark);
    desc.add_box(QVector3D(0.0F, 0.26F, 0.0F), QVector3D(1.0F, 0.02F, 1.0F),
                 c.limestone);

    for (float x = -0.85F; x <= 0.85F; x += 0.425F) {
      for (float z = -0.85F; z <= 0.85F; z += 0.425F) {
        if (fabsf(x) > 0.3F || fabsf(z) > 0.3F) {
          desc.add_box(QVector3D(x, 0.29F, z), QVector3D(0.18F, 0.01F, 0.18F),
                       c.terracotta, BuildingStateMask::All,
                       BuildingLODMask::Full);
        }
      }
    }

    desc.add_box(QVector3D(0.0F, 0.42F, 0.0F), QVector3D(0.9F, 0.12F, 0.9F),
                 c.sandstone_light);
    desc.add_cylinder(QVector3D(0.0F, 0.5F, 0.0F), QVector3D(0.0F, 2.2F, 0.0F),
                      0.55F, c.limestone);

    for (int i = 0; i < 4; ++i) {
      float const angle = static_cast<float>(i) * 1.57F + 0.785F;
      float const ox = sinf(angle) * 0.48F;
      float const oz = cosf(angle) * 0.48F;

      desc.add_cylinder(QVector3D(ox, 0.5F, oz), QVector3D(ox, 1.9F, oz), 0.08F,
                        c.marble);
      desc.add_box(QVector3D(ox, 0.58F, oz), QVector3D(0.12F, 0.08F, 0.12F),
                   c.marble, BuildingStateMask::All, BuildingLODMask::Full);
      desc.add_box(QVector3D(ox, 1.95F, oz), QVector3D(0.13F, 0.08F, 0.13F),
                   c.marble, BuildingStateMask::All, BuildingLODMask::Full);
      desc.add_box(QVector3D(ox, 2.05F, oz), QVector3D(0.10F, 0.04F, 0.10F),
                   c.gold, BuildingStateMask::All, BuildingLODMask::Full);
    }

    for (int i = 0; i < 8; ++i) {
      float const angle = static_cast<float>(i) * 0.785F;
      float const ox = sinf(angle) * 0.45F;
      float const oz = cosf(angle) * 0.45F;
      desc.add_box(QVector3D(ox, 1.2F, oz), QVector3D(0.06F, 0.25F, 0.06F),
                   c.sandstone_dark, BuildingStateMask::All,
                   BuildingLODMask::Full);
    }

    desc.add_box(QVector3D(0.0F, 2.28F, 0.0F), QVector3D(0.8F, 0.05F, 0.8F),
                 c.cedar);
    for (int i = 0; i < 8; ++i) {
      float const angle = static_cast<float>(i) * 0.785F;
      float const ox = sinf(angle) * 0.7F;
      float const oz = cosf(angle) * 0.7F;
      desc.add_box(QVector3D(ox, 2.45F, oz), QVector3D(0.14F, 0.17F, 0.14F),
                   c.terracotta, BuildingStateMask::All, BuildingLODMask::Full);
    }

    desc.add_box(QVector3D(0.0F, 2.58F, 0.0F), QVector3D(0.85F, 0.04F, 0.85F),
                 c.limestone);
    for (float x : {-0.75F, 0.75F}) {
      for (float z : {-0.75F, 0.75F}) {
        desc.add_box(QVector3D(x, 2.64F, z), QVector3D(0.06F, 0.06F, 0.06F),
                     c.blue_accent, BuildingStateMask::All,
                     BuildingLODMask::Full);
      }
    }

    desc.add_cylinder(QVector3D(0.0F, 2.25F, 0.0F), QVector3D(0.0F, 3.1F, 0.0F),
                      0.07F, c.cedar_dark);
    desc.add_palette_box(QVector3D(0.12F, 2.75F, 0.0F),
                         QVector3D(0.22F, 0.15F, 0.025F), kTowerTeamSlot);
    for (int i = 0; i < 2; ++i) {
      float const ring_y = 2.65F + static_cast<float>(i) * 0.30F;
      desc.add_cylinder(QVector3D(0.0F, ring_y, 0.0F),
                        QVector3D(0.0F, ring_y + 0.025F, 0.0F), 0.11F, c.gold,
                        BuildingStateMask::All, BuildingLODMask::Full);
    }
    desc.add_box(QVector3D(0.0F, 3.15F, 0.0F), QVector3D(0.08F, 0.06F, 0.08F),
                 c.bronze, BuildingStateMask::All, BuildingLODMask::Full);
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
  draw_building_compact_health_bar(out, p, 3.2F);
  draw_building_selection_overlay(out, p, BuildingSelectionStyle{1.6F, 1.6F});
}

} // namespace

void register_defense_tower_renderer(
    Render::GL::EntityRendererRegistry &registry) {
  register_building_renderer(registry, "roman", "defense_tower",
                             draw_defense_tower);
}

} // namespace Render::GL::Roman
