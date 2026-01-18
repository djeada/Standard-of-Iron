#include "home_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/visuals/team_colors.h"
#include "../../../geom/math_utils.h"
#include "../../../geom/transforms.h"
#include "../../../gl/primitives.h"
#include "../../../gl/resources.h"
#include "../../../submitter.h"
#include "../../building_state.h"
#include "../../registry.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>

namespace Render::GL::Roman {
namespace {

using Render::Geom::clamp01;
using Render::Geom::clamp_vec_01;
using Render::Geom::cylinder_between;

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
  p.team_trim =
      clamp_vec_01(QVector3D(team.x() * 0.6F, team.y() * 0.6F, team.z() * 0.6F));
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

void draw_home_platform(const DrawContext &p, ISubmitter &out, Mesh *unit,
                        Texture *white, const RomanPalette &c) {
  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.06F, 0.0F),
           QVector3D(1.1F, 0.06F, 1.1F), c.limestone_dark);

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.14F, 0.0F),
           QVector3D(1.0F, 0.02F, 1.0F), c.limestone);
}

void draw_home_walls(const DrawContext &p, ISubmitter &out, Mesh *unit,
                     Texture *white, const RomanPalette &c,
                     BuildingState state) {
  float const wall_height = 0.9F;
  float height_multiplier = 1.0F;

  if (state == BuildingState::Damaged) {
    height_multiplier = 0.7F;
  } else if (state == BuildingState::Destroyed) {
    height_multiplier = 0.4F;
  }

  draw_box(
      out, unit, white, p.model,
      QVector3D(0.0F, wall_height * 0.5F * height_multiplier + 0.16F, -0.85F),
      QVector3D(0.8F, wall_height * 0.5F * height_multiplier, 0.08F),
      c.limestone);
  draw_box(
      out, unit, white, p.model,
      QVector3D(0.0F, wall_height * 0.5F * height_multiplier + 0.16F, 0.85F),
      QVector3D(0.8F, wall_height * 0.5F * height_multiplier, 0.08F),
      c.limestone);
  draw_box(
      out, unit, white, p.model,
      QVector3D(-0.85F, wall_height * 0.5F * height_multiplier + 0.16F, 0.0F),
      QVector3D(0.08F, wall_height * 0.5F * height_multiplier, 0.75F),
      c.limestone);
  draw_box(
      out, unit, white, p.model,
      QVector3D(0.85F, wall_height * 0.5F * height_multiplier + 0.16F, 0.0F),
      QVector3D(0.08F, wall_height * 0.5F * height_multiplier, 0.75F),
      c.limestone);
}

void draw_home_columns(const DrawContext &p, ISubmitter &out, Mesh *unit,
                       Texture *white, const RomanPalette &c,
                       BuildingState state) {
  float const col_height = 0.8F;
  float const col_radius = 0.06F;

  float height_multiplier = 1.0F;
  if (state == BuildingState::Damaged) {
    height_multiplier = 0.7F;
  } else if (state == BuildingState::Destroyed) {
    height_multiplier = 0.4F;
  }

  QVector3D columns[4] = {
      QVector3D(-0.7F, 0.0F, 0.88F), QVector3D(0.7F, 0.0F, 0.88F),
      QVector3D(-0.7F, 0.0F, -0.88F), QVector3D(0.7F, 0.0F, -0.88F)};

  for (int i = 0; i < 4; ++i) {
    draw_box(out, unit, white, p.model,
             QVector3D(columns[i].x(), 0.18F, columns[i].z()),
             QVector3D(col_radius * 1.2F, 0.04F, col_radius * 1.2F), c.marble);

    draw_cyl(out, p.model, QVector3D(columns[i].x(), 0.16F, columns[i].z()),
             QVector3D(columns[i].x(), 0.16F + col_height * height_multiplier,
                       columns[i].z()),
             col_radius, c.limestone_shade, white);

    if (state != BuildingState::Destroyed) {
      draw_box(out, unit, white, p.model,
               QVector3D(columns[i].x(),
                         0.16F + col_height * height_multiplier + 0.04F,
                         columns[i].z()),
               QVector3D(col_radius * 1.4F, 0.06F, col_radius * 1.4F),
               c.marble);
    }
  }
}

void draw_home_roof(const DrawContext &p, ISubmitter &out, Mesh *unit,
                    Texture *white, const RomanPalette &c,
                    BuildingState state) {
  if (state == BuildingState::Destroyed) {
    return;
  }

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 1.25F, 0.0F),
           QVector3D(1.05F, 0.06F, 1.05F), c.terracotta);

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 1.3F, 0.0F),
           QVector3D(1.0F, 0.04F, 1.0F), c.terracotta_dark);
}

void draw_home_door(const DrawContext &p, ISubmitter &out, Mesh *unit,
                    Texture *white, const RomanPalette &c) {
  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.45F, 0.9F),
           QVector3D(0.3F, 0.4F, 0.05F), c.cedar_dark);

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.62F, 0.92F),
           QVector3D(0.32F, 0.04F, 0.02F), c.blue_accent);
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

  auto *capture = p.entity->get_component<Engine::Core::CaptureComponent>();
  bool under_attack = (capture != nullptr && capture->is_being_captured);

  if (!under_attack && u->health >= u->max_health) {
    return;
  }

  float const bar_width = 1.0F;
  float const bar_height = 0.08F;
  float const bar_y = 1.6F;
  float const border_thickness = 0.012F;

  if (under_attack) {
    float pulse = HEALTHBAR_PULSE_MIN +
                  HEALTHBAR_PULSE_AMPLITUDE *
                      sinf(p.animation_time * HEALTHBAR_PULSE_SPEED);
    draw_box(out, unit, white, p.model, QVector3D(0.0F, bar_y, 0.0F),
             QVector3D(bar_width * 0.5F + border_thickness * 3.0F,
                       bar_height * 0.5F + border_thickness * 3.0F, 0.095F),
             HealthBarColors::GLOW_ATTACK * pulse * 0.6F);
  }

  draw_box(out, unit, white, p.model, QVector3D(0.0F, bar_y, 0.0F),
           QVector3D(bar_width * 0.5F + border_thickness,
                     bar_height * 0.5F + border_thickness, 0.09F),
           HealthBarColors::BORDER);

  draw_box(out, unit, white, p.model, QVector3D(0.0F, bar_y, 0.0F),
           QVector3D(bar_width * 0.5F + border_thickness * 0.5F,
                     bar_height * 0.5F + border_thickness * 0.5F, 0.088F),
           HealthBarColors::INNER_BORDER);

  draw_box(out, unit, white, p.model, QVector3D(0.0F, bar_y + 0.003F, 0.0F),
           QVector3D(bar_width * 0.5F, bar_height * 0.5F, 0.085F),
           HealthBarColors::BACKGROUND);

  QVector3D fg_color;
  QVector3D fg_dark;

  if (ratio >= HEALTH_THRESHOLD_NORMAL) {
    fg_color = HealthBarColors::NORMAL_BRIGHT;
    fg_dark = HealthBarColors::NORMAL_DARK;
  } else if (ratio >= HEALTH_THRESHOLD_DAMAGED) {
    float t = (ratio - HEALTH_THRESHOLD_DAMAGED) /
              (HEALTH_THRESHOLD_NORMAL - HEALTH_THRESHOLD_DAMAGED);
    fg_color = HealthBarColors::NORMAL_BRIGHT * t +
               HealthBarColors::DAMAGED_BRIGHT * (1.0F - t);
    fg_dark = HealthBarColors::NORMAL_DARK * t +
              HealthBarColors::DAMAGED_DARK * (1.0F - t);
  } else {
    float t = ratio / HEALTH_THRESHOLD_DAMAGED;
    fg_color = HealthBarColors::DAMAGED_BRIGHT * t +
               HealthBarColors::CRITICAL_BRIGHT * (1.0F - t);
    fg_dark = HealthBarColors::DAMAGED_DARK * t +
              HealthBarColors::CRITICAL_DARK * (1.0F - t);
  }

  draw_box(
      out, unit, white, p.model,
      QVector3D(-(bar_width * (1.0F - ratio)) * 0.5F, bar_y + 0.005F, 0.0F),
      QVector3D(bar_width * ratio * 0.5F, bar_height * 0.48F, 0.08F), fg_dark);

  draw_box(
      out, unit, white, p.model,
      QVector3D(-(bar_width * (1.0F - ratio)) * 0.5F, bar_y + 0.008F, 0.0F),
      QVector3D(bar_width * ratio * 0.5F, bar_height * 0.40F, 0.078F),
      fg_color);

  QVector3D const highlight = fg_color * 1.6F;
  draw_box(out, unit, white, p.model,
           QVector3D(-(bar_width * (1.0F - ratio)) * 0.5F,
                     bar_y + bar_height * 0.35F, 0.0F),
           QVector3D(bar_width * ratio * 0.5F, bar_height * 0.20F, 0.075F),
           clamp_vec_01(highlight));

  draw_box(out, unit, white, p.model,
           QVector3D(-(bar_width * (1.0F - ratio)) * 0.5F,
                     bar_y + bar_height * 0.48F, 0.0F),
           QVector3D(bar_width * ratio * 0.5F, bar_height * 0.08F, 0.073F),
           HealthBarColors::SHINE * 0.8F);

  float marker_70_x = bar_width * 0.5F * (HEALTH_THRESHOLD_NORMAL - 0.5F);
  draw_box(out, unit, white, p.model, QVector3D(marker_70_x, bar_y, 0.0F),
           QVector3D(0.015F, bar_height * 0.55F, 0.09F),
           HealthBarColors::SEGMENT);

  float marker_30_x = bar_width * 0.5F * (HEALTH_THRESHOLD_DAMAGED - 0.5F);
  draw_box(out, unit, white, p.model, QVector3D(marker_30_x, bar_y, 0.0F),
           QVector3D(0.015F, bar_height * 0.55F, 0.09F),
           HealthBarColors::SEGMENT);
}

void draw_selection(const DrawContext &p, ISubmitter &out) {
  QMatrix4x4 m;
  QVector3D const pos = p.model.column(3).toVector3D();
  m.translate(pos.x(), 0.0F, pos.z());
  m.scale(1.5F, 1.0F, 1.5F);
  if (p.selected) {
    out.selection_smoke(m, QVector3D(0.2F, 0.85F, 0.2F), 0.35F);
  } else if (p.hovered) {
    out.selection_smoke(m, QVector3D(0.95F, 0.92F, 0.25F), 0.22F);
  }
}

void draw_home(const DrawContext &p, ISubmitter &out) {
  if (!p.resources || !p.entity) {
    return;
  }

  auto *t = p.entity->get_component<Engine::Core::TransformComponent>();
  auto *r = p.entity->get_component<Engine::Core::RenderableComponent>();
  auto *u = p.entity->get_component<Engine::Core::UnitComponent>();
  if (!t || !r) {
    return;
  }

  BuildingState state = BuildingState::Normal;
  if (u != nullptr) {
    float const health_ratio =
        std::clamp(u->health / float(std::max(1, u->max_health)), 0.0F, 1.0F);
    state = get_building_state(health_ratio);
  }

  Mesh *unit = p.resources->unit();
  Texture *white = p.resources->white();
  QVector3D const team(r->color[0], r->color[1], r->color[2]);
  RomanPalette const c = make_palette(team);

  draw_home_platform(p, out, unit, white, c);
  draw_home_walls(p, out, unit, white, c, state);
  draw_home_columns(p, out, unit, white, c, state);
  draw_home_roof(p, out, unit, white, c, state);
  draw_home_door(p, out, unit, white, c);
  draw_health_bar(p, out, unit, white);
  draw_selection(p, out);
}

} // namespace

void register_home_renderer(Render::GL::EntityRendererRegistry &registry) {
  registry.register_renderer("troops/roman/home", draw_home);
}

} // namespace Render::GL::Roman
