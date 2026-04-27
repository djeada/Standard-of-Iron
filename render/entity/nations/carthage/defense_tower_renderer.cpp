#include "defense_tower_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../geom/math_utils.h"
#include "../../../gl/primitives.h"
#include "../../../gl/resources.h"
#include "../../../render_archetype.h"
#include "../../../submitter.h"
#include "../../registry.h"

#include <QMatrix4x4>
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

void submit_box(ISubmitter &out, Texture *white, const QMatrix4x4 &model,
                const QVector3D &pos, const QVector3D &size,
                const QVector3D &color) {
  QMatrix4x4 m = model;
  m.translate(pos);
  m.scale(size);
  out.mesh(get_unit_cube(), m, color, white, 1.0F);
}

auto tower_archetype() -> const RenderArchetype & {
  static const RenderArchetype k_tower = [] {
    TowerPalette const c = make_palette(QVector3D(1.0F, 1.0F, 1.0F));
    RenderArchetypeBuilder builder("carthage_defense_tower");
    builder.set_max_distance(std::numeric_limits<float>::infinity());

    builder.add_box(QVector3D(0.0F, 0.15F, 0.0F), QVector3D(1.0F, 0.15F, 1.0F),
                    c.stone_base);
    for (float x = -0.9F; x <= 0.9F; x += 0.45F) {
      builder.add_box(QVector3D(x, 0.35F, -0.85F),
                      QVector3D(0.12F, 0.08F, 0.08F), c.brick_dark);
      builder.add_box(QVector3D(x, 0.35F, 0.85F),
                      QVector3D(0.12F, 0.08F, 0.08F), c.brick_dark);
    }
    for (float z = -0.8F; z <= 0.8F; z += 0.4F) {
      builder.add_box(QVector3D(-0.85F, 0.35F, z),
                      QVector3D(0.08F, 0.08F, 0.12F), c.brick_dark);
      builder.add_box(QVector3D(0.85F, 0.35F, z),
                      QVector3D(0.08F, 0.08F, 0.12F), c.brick_dark);
    }
    builder.add_box(QVector3D(0.0F, 0.5F, 0.0F), QVector3D(0.9F, 0.1F, 0.9F),
                    c.stone_light);

    builder.add_box(QVector3D(0.0F, 1.2F, 0.0F), QVector3D(0.75F, 0.7F, 0.75F),
                    c.stone_light);
    for (int i = 0; i < 4; ++i) {
      float const angle = static_cast<float>(i) * 1.57F;
      float const ox = sinf(angle) * 0.65F;
      float const oz = cosf(angle) * 0.65F;
      builder.add_cylinder(QVector3D(ox, 0.5F, oz), QVector3D(ox, 1.9F, oz),
                           0.14F, c.stone_dark);
    }

    for (int i = 0; i < 4; ++i) {
      float const angle = static_cast<float>(i) * 1.57F + 0.785F;
      float const ox = sinf(angle) * 0.62F;
      float const oz = cosf(angle) * 0.62F;
      builder.add_box(QVector3D(ox, 0.9F, oz), QVector3D(0.12F, 0.4F, 0.12F),
                      c.brick);
    }
    builder.add_box(QVector3D(0.0F, 1.65F, 0.0F),
                    QVector3D(0.82F, 0.08F, 0.82F), c.brick_dark);

    builder.add_box(QVector3D(0.0F, 1.95F, 0.0F),
                    QVector3D(0.95F, 0.05F, 0.95F), c.wood);
    for (int i = 0; i < 8; ++i) {
      float const angle = static_cast<float>(i) * 0.785F;
      float const ox = sinf(angle) * 0.82F;
      float const oz = cosf(angle) * 0.82F;
      builder.add_box(QVector3D(ox, 2.12F, oz), QVector3D(0.12F, 0.17F, 0.12F),
                      c.brick);
    }
    builder.add_box(QVector3D(0.0F, 2.32F, 0.0F), QVector3D(1.0F, 0.03F, 1.0F),
                    c.tile_red);

    builder.add_cylinder(QVector3D(0.0F, 2.05F, 0.0F),
                         QVector3D(0.0F, 2.9F, 0.0F), 0.08F, c.wood_dark);
    builder.add_palette_box(QVector3D(0.15F, 2.6F, 0.0F),
                            QVector3D(0.25F, 0.18F, 0.025F), kTowerTeamSlot);
    for (int i = 0; i < 3; ++i) {
      float const ring_y = 2.3F + static_cast<float>(i) * 0.25F;
      builder.add_cylinder(QVector3D(0.0F, ring_y, 0.0F),
                           QVector3D(0.0F, ring_y + 0.03F, 0.0F), 0.12F,
                           c.iron);
    }
    builder.add_box(QVector3D(0.0F, 2.95F, 0.0F), QVector3D(0.1F, 0.08F, 0.1F),
                    c.iron);
    return std::move(builder).build();
  }();
  return k_tower;
}

auto tower_palette_slots(const TowerPalette &palette)
    -> std::array<QVector3D, 1> {
  return {palette.team};
}

void draw_health_bar(const DrawContext &p, ISubmitter &out, Texture *white) {
  if (p.entity == nullptr) {
    return;
  }
  auto *u = p.entity->get_component<Engine::Core::UnitComponent>();
  if (u == nullptr) {
    return;
  }

  float const ratio =
      std::clamp(u->health / float(std::max(1, u->max_health)), 0.0F, 1.0F);
  if (ratio <= 0.0F) {
    return;
  }

  QVector3D const bg(0.06F, 0.06F, 0.06F);
  submit_box(out, white, p.model, QVector3D(0.0F, 3.2F, 0.0F),
             QVector3D(0.6F, 0.03F, 0.05F), bg);

  QVector3D const fg = QVector3D(0.22F, 0.78F, 0.22F) * ratio +
                       QVector3D(0.85F, 0.15F, 0.15F) * (1.0F - ratio);
  submit_box(out, white, p.model,
             QVector3D(-0.3F * (1.0F - ratio), 3.21F, 0.0F),
             QVector3D(0.3F * ratio, 0.025F, 0.045F), fg);
}

void draw_selection(const DrawContext &p, ISubmitter &out) {
  QMatrix4x4 m;
  QVector3D const pos = p.model.column(3).toVector3D();
  m.translate(pos.x(), 0.0F, pos.z());
  m.scale(1.6F, 1.0F, 1.6F);
  if (p.selected) {
    out.selection_smoke(m, QVector3D(0.2F, 0.85F, 0.2F), 0.35F);
  } else if (p.hovered) {
    out.selection_smoke(m, QVector3D(0.95F, 0.92F, 0.25F), 0.22F);
  }
}

void draw_defense_tower(const DrawContext &p, ISubmitter &out) {
  if (p.entity == nullptr) {
    return;
  }

  auto *r = p.entity->get_component<Engine::Core::RenderableComponent>();
  if (r == nullptr) {
    return;
  }

  Texture *white = (p.resources != nullptr) ? p.resources->white() : nullptr;
  TowerPalette const palette =
      make_palette(QVector3D(r->color[0], r->color[1], r->color[2]));
  const auto palette_slots = tower_palette_slots(palette);
  const RenderArchetype &archetype = tower_archetype();

  RenderInstance instance;
  instance.archetype = &archetype;
  instance.world = p.model;
  instance.palette = palette_slots;
  instance.default_texture = white;
  instance.lod = RenderArchetypeLod::Full;
  submit_render_instance(out, instance);

  draw_health_bar(p, out, white);
  draw_selection(p, out);
}

} // namespace

void register_defense_tower_renderer(
    Render::GL::EntityRendererRegistry &registry) {
  registry.register_renderer("troops/carthage/defense_tower",
                             draw_defense_tower);
}

} // namespace Render::GL::Carthage
