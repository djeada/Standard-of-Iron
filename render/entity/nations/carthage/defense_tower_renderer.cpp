#include "defense_tower_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/visuals/team_colors.h"
#include "../../../geom/math_utils.h"
#include "../../../geom/transforms.h"
#include "../../../gl/primitives.h"
#include "../../../gl/resources.h"
#include "../../../submitter.h"
#include "../../registry.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>

namespace Render::GL::Carthage {
namespace {

using Render::Geom::clampVec01;
using Render::Geom::cylinder_between;

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

inline auto make_palette(const QVector3D &team) -> TowerPalette {
  TowerPalette p;
  p.team = clampVec01(team);
  return p;
}

inline void draw_box(ISubmitter &out, Mesh *unit, Texture *white,
                     const QMatrix4x4 &model, const QVector3D &pos,
                     const QVector3D &size, const QVector3D &color) {
  QMatrix4x4 m = model;
  m.translate(pos);
  m.scale(size);
  out.mesh(unit, m, color, white, 1.0F);
}

inline void draw_cyl(ISubmitter &out, const QMatrix4x4 &model,
                     const QVector3D &a, const QVector3D &b, float r,
                     const QVector3D &color, Texture *white) {
  out.mesh(get_unit_cylinder(), model * cylinder_between(a, b, r), color, white,
           1.0F);
}

void draw_tower_base(const DrawContext &p, ISubmitter &out, Mesh *unit,
                     Texture *white, const TowerPalette &c) {
  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.12F, 0.0F),
           QVector3D(1.1F, 0.12F, 1.1F), c.limestone_dark);

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.26F, 0.0F),
           QVector3D(1.0F, 0.02F, 1.0F), c.limestone);

  for (float x = -0.85F; x <= 0.85F; x += 0.425F) {
    for (float z = -0.85F; z <= 0.85F; z += 0.425F) {
      if (fabsf(x) > 0.3F || fabsf(z) > 0.3F) {
        draw_box(out, unit, white, p.model, QVector3D(x, 0.29F, z),
                 QVector3D(0.18F, 0.01F, 0.18F), c.terracotta);
      }
    }
  }

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.42F, 0.0F),
           QVector3D(0.9F, 0.12F, 0.9F), c.sandstone_light);
}

void draw_tower_body(const DrawContext &p, ISubmitter &out, Mesh *unit,
                     Texture *white, const TowerPalette &c) {
  draw_cyl(out, p.model, QVector3D(0.0F, 0.5F, 0.0F),
           QVector3D(0.0F, 2.2F, 0.0F), 0.55F, c.limestone, white);

  for (int i = 0; i < 4; ++i) {
    float const angle = static_cast<float>(i) * 1.57F + 0.785F;
    float const ox = sinf(angle) * 0.48F;
    float const oz = cosf(angle) * 0.48F;

    draw_cyl(out, p.model, QVector3D(ox, 0.5F, oz), QVector3D(ox, 1.9F, oz),
             0.08F, c.marble, white);

    draw_box(out, unit, white, p.model, QVector3D(ox, 0.58F, oz),
             QVector3D(0.12F, 0.08F, 0.12F), c.marble);

    draw_box(out, unit, white, p.model, QVector3D(ox, 1.95F, oz),
             QVector3D(0.13F, 0.08F, 0.13F), c.marble);

    draw_box(out, unit, white, p.model, QVector3D(ox, 2.05F, oz),
             QVector3D(0.10F, 0.04F, 0.10F), c.gold);
  }

  for (int i = 0; i < 8; ++i) {
    float const angle = static_cast<float>(i) * 0.785F;
    float const ox = sinf(angle) * 0.45F;
    float const oz = cosf(angle) * 0.45F;
    draw_box(out, unit, white, p.model, QVector3D(ox, 1.2F, oz),
             QVector3D(0.06F, 0.25F, 0.06F), c.sandstone_dark);
  }
}

void draw_tower_platform(const DrawContext &p, ISubmitter &out, Mesh *unit,
                         Texture *white, const TowerPalette &c) {
  draw_box(out, unit, white, p.model, QVector3D(0.0F, 2.28F, 0.0F),
           QVector3D(0.8F, 0.05F, 0.8F), c.cedar);

  for (int i = 0; i < 8; ++i) {
    float const angle = static_cast<float>(i) * 0.785F;
    float const ox = sinf(angle) * 0.7F;
    float const oz = cosf(angle) * 0.7F;
    draw_box(out, unit, white, p.model, QVector3D(ox, 2.45F, oz),
             QVector3D(0.14F, 0.17F, 0.14F), c.terracotta);
  }

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 2.58F, 0.0F),
           QVector3D(0.85F, 0.04F, 0.85F), c.limestone);

  for (float x : {-0.75F, 0.75F}) {
    for (float z : {-0.75F, 0.75F}) {
      draw_box(out, unit, white, p.model, QVector3D(x, 2.64F, z),
               QVector3D(0.06F, 0.06F, 0.06F), c.blue_accent);
    }
  }
}

void draw_tower_top(const DrawContext &p, ISubmitter &out, Mesh *unit,
                    Texture *white, const TowerPalette &c) {
  draw_cyl(out, p.model, QVector3D(0.0F, 2.25F, 0.0F),
           QVector3D(0.0F, 3.1F, 0.0F), 0.07F, c.cedar_dark, white);

  draw_box(out, unit, white, p.model, QVector3D(0.12F, 2.75F, 0.0F),
           QVector3D(0.22F, 0.15F, 0.025F), c.team);

  for (int i = 0; i < 4; ++i) {
    float ring_y = 2.45F + static_cast<float>(i) * 0.25F;
    out.mesh(get_unit_cylinder(),
             p.model * Render::Geom::cylinder_between(
                           QVector3D(0.0F, ring_y, 0.0F),
                           QVector3D(0.0F, ring_y + 0.025F, 0.0F), 0.11F),
             c.gold, white, 1.0F);
  }

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 3.15F, 0.0F),
           QVector3D(0.08F, 0.06F, 0.08F), c.bronze);

  draw_box(out, unit, white, p.model, QVector3D(0.18F, 3.08F, 0.0F),
           QVector3D(0.3F, 0.025F, 0.015F), c.gold);
}

void draw_health_bar(const DrawContext &p, ISubmitter &out, Mesh *unit,
                     Texture *white) {
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
  draw_box(out, unit, white, p.model, QVector3D(0.0F, 3.2F, 0.0F),
           QVector3D(0.6F, 0.03F, 0.05F), bg);

  QVector3D const fg = QVector3D(0.22F, 0.78F, 0.22F) * ratio +
                       QVector3D(0.85F, 0.15F, 0.15F) * (1.0F - ratio);
  draw_box(out, unit, white, p.model,
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
  if (!p.resources || !p.entity) {
    return;
  }

  auto *t = p.entity->get_component<Engine::Core::TransformComponent>();
  auto *r = p.entity->get_component<Engine::Core::RenderableComponent>();
  if (!t || !r) {
    return;
  }

  Mesh *unit = p.resources->unit();
  Texture *white = p.resources->white();
  QVector3D const team(r->color[0], r->color[1], r->color[2]);
  TowerPalette const c = make_palette(team);

  draw_tower_base(p, out, unit, white, c);
  draw_tower_body(p, out, unit, white, c);
  draw_tower_platform(p, out, unit, white, c);
  draw_tower_top(p, out, unit, white, c);
  draw_health_bar(p, out, unit, white);
  draw_selection(p, out);
}

} // namespace

void register_defense_tower_renderer(
    Render::GL::EntityRendererRegistry &registry) {
  registry.register_renderer("troops/carthage/defense_tower",
                             draw_defense_tower);
}

} // namespace Render::GL::Carthage
