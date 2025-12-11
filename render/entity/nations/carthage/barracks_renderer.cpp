#include "barracks_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/visuals/team_colors.h"
#include "../../../geom/flag.h"
#include "../../../geom/math_utils.h"
#include "../../../geom/transforms.h"
#include "../../../gl/primitives.h"
#include "../../../gl/resources.h"
#include "../../../submitter.h"
#include "../../barracks_flag_renderer.h"
#include "../../registry.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <cmath>

namespace Render::GL::Carthage {
namespace {

using Render::Geom::clamp01;
using Render::Geom::clampVec01;
using Render::Geom::cylinderBetween;

struct CarthagePalette {
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
  QVector3D gold{0.85F, 0.72F, 0.35F};
  QVector3D team{0.8F, 0.9F, 1.0F};
  QVector3D team_trim{0.48F, 0.54F, 0.60F};
};

inline auto make_palette(const QVector3D &team) -> CarthagePalette {
  CarthagePalette p;
  p.team = clampVec01(team);
  p.team_trim =
      clampVec01(QVector3D(team.x() * 0.6F, team.y() * 0.6F, team.z() * 0.6F));
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
  out.mesh(getUnitCylinder(), model * cylinderBetween(a, b, r), color, white,
           1.0F);
}

void draw_platform(const DrawContext &p, ISubmitter &out, Mesh *unit,
                   Texture *white, const CarthagePalette &c) {

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.08F, 0.0F),
           QVector3D(2.0F, 0.08F, 1.8F), c.limestone_dark);

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.18F, 0.0F),
           QVector3D(1.8F, 0.02F, 1.6F), c.limestone);

  for (float x = -1.5F; x <= 1.5F; x += 0.35F) {
    for (float z = -1.3F; z <= 1.3F; z += 0.35F) {
      if (fabsf(x) > 0.6F || fabsf(z) > 0.5F) {
        draw_box(out, unit, white, p.model, QVector3D(x, 0.21F, z),
                 QVector3D(0.15F, 0.01F, 0.15F), c.terracotta);
      }
    }
  }
}

void draw_colonnade(const DrawContext &p, ISubmitter &out, Mesh *unit,
                    Texture *white, const CarthagePalette &c) {
  float const col_height = 1.6F;
  float const col_radius = 0.10F;

  for (int i = 0; i < 6; ++i) {
    float const x = -1.25F + float(i) * 0.5F;
    float const z = 1.4F;

    draw_box(out, unit, white, p.model, QVector3D(x, 0.25F, z),
             QVector3D(col_radius * 1.2F, 0.05F, col_radius * 1.2F), c.marble);

    draw_cyl(out, p.model, QVector3D(x, 0.2F, z),
             QVector3D(x, 0.2F + col_height, z), col_radius, c.limestone,
             white);

    draw_box(out, unit, white, p.model,
             QVector3D(x, 0.2F + col_height + 0.05F, z),
             QVector3D(col_radius * 1.5F, 0.08F, col_radius * 1.5F), c.marble);

    draw_box(out, unit, white, p.model,
             QVector3D(x, 0.2F + col_height + 0.12F, z),
             QVector3D(col_radius * 1.3F, 0.04F, col_radius * 1.3F), c.gold);
  }

  for (int i = 0; i < 3; ++i) {
    float const z = -1.0F + float(i) * 1.0F;

    float const x_left = -1.6F;
    draw_box(out, unit, white, p.model, QVector3D(x_left, 0.25F, z),
             QVector3D(col_radius * 1.2F, 0.05F, col_radius * 1.2F), c.marble);
    draw_cyl(out, p.model, QVector3D(x_left, 0.2F, z),
             QVector3D(x_left, 0.2F + col_height, z), col_radius, c.limestone,
             white);
    draw_box(out, unit, white, p.model,
             QVector3D(x_left, 0.2F + col_height + 0.05F, z),
             QVector3D(col_radius * 1.5F, 0.08F, col_radius * 1.5F), c.marble);

    float const x_right = 1.6F;
    draw_box(out, unit, white, p.model, QVector3D(x_right, 0.25F, z),
             QVector3D(col_radius * 1.2F, 0.05F, col_radius * 1.2F), c.marble);
    draw_cyl(out, p.model, QVector3D(x_right, 0.2F, z),
             QVector3D(x_right, 0.2F + col_height, z), col_radius, c.limestone,
             white);
    draw_box(out, unit, white, p.model,
             QVector3D(x_right, 0.2F + col_height + 0.05F, z),
             QVector3D(col_radius * 1.5F, 0.08F, col_radius * 1.5F), c.marble);
  }
}

void draw_central_courtyard(const DrawContext &p, ISubmitter &out, Mesh *unit,
                            Texture *white, const CarthagePalette &c) {

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.22F, 0.0F),
           QVector3D(1.3F, 0.01F, 1.1F), c.limestone_shade);

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.24F, 0.0F),
           QVector3D(0.7F, 0.02F, 0.5F), c.blue_light);

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.25F, -0.52F),
           QVector3D(0.72F, 0.02F, 0.02F), c.blue_accent);
  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.25F, 0.52F),
           QVector3D(0.72F, 0.02F, 0.02F), c.blue_accent);

  draw_cyl(out, p.model, QVector3D(0.0F, 0.25F, 0.0F),
           QVector3D(0.0F, 0.55F, 0.0F), 0.06F, c.marble, white);
  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.58F, 0.0F),
           QVector3D(0.08F, 0.03F, 0.08F), c.blue_accent);
}

void draw_chamber(const DrawContext &p, ISubmitter &out, Mesh *unit,
                  Texture *white, const CarthagePalette &c) {
  float const wall_h = 1.4F;

  draw_box(out, unit, white, p.model,
           QVector3D(0.0F, wall_h * 0.5F + 0.2F, -1.2F),
           QVector3D(1.4F, wall_h * 0.5F, 0.1F), c.limestone);

  draw_box(out, unit, white, p.model,
           QVector3D(-1.5F, wall_h * 0.5F + 0.2F, -0.5F),
           QVector3D(0.1F, wall_h * 0.5F, 0.6F), c.limestone);
  draw_box(out, unit, white, p.model,
           QVector3D(1.5F, wall_h * 0.5F + 0.2F, -0.5F),
           QVector3D(0.1F, wall_h * 0.5F, 0.6F), c.limestone);

  draw_box(out, unit, white, p.model, QVector3D(-0.6F, 0.65F, -1.15F),
           QVector3D(0.25F, 0.35F, 0.03F), c.cedar_dark);
  draw_box(out, unit, white, p.model, QVector3D(-0.6F, 0.98F, -1.15F),
           QVector3D(0.25F, 0.05F, 0.03F), c.blue_accent);

  draw_box(out, unit, white, p.model, QVector3D(0.6F, 0.65F, -1.15F),
           QVector3D(0.25F, 0.35F, 0.03F), c.cedar_dark);
  draw_box(out, unit, white, p.model, QVector3D(0.6F, 0.98F, -1.15F),
           QVector3D(0.25F, 0.05F, 0.03F), c.blue_accent);
}

void draw_terrace(const DrawContext &p, ISubmitter &out, Mesh *unit,
                  Texture *white, const CarthagePalette &c) {

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 2.05F, 0.0F),
           QVector3D(1.7F, 0.08F, 1.5F), c.marble);

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 2.12F, 1.45F),
           QVector3D(1.65F, 0.05F, 0.05F), c.gold);

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 2.18F, -0.2F),
           QVector3D(1.5F, 0.04F, 1.0F), c.terracotta);

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 2.28F, -0.65F),
           QVector3D(1.45F, 0.06F, 0.05F), c.limestone);

  for (float x : {-1.4F, 1.4F}) {
    draw_box(out, unit, white, p.model, QVector3D(x, 2.35F, -0.65F),
             QVector3D(0.08F, 0.08F, 0.08F), c.gold);
  }
}

void draw_trading_goods(const DrawContext &p, ISubmitter &out, Mesh *unit,
                        Texture *white, const CarthagePalette &c) {

  draw_cyl(out, p.model, QVector3D(-1.2F, 0.2F, 1.1F),
           QVector3D(-1.2F, 0.5F, 1.1F), 0.08F, c.terracotta_dark, white);
  draw_cyl(out, p.model, QVector3D(-0.9F, 0.2F, 1.15F),
           QVector3D(-0.9F, 0.45F, 1.15F), 0.07F, c.terracotta, white);

  draw_cyl(out, p.model, QVector3D(1.1F, 0.2F, -0.9F),
           QVector3D(1.1F, 0.42F, -0.9F), 0.06F, c.blue_accent, white);
}

void draw_phoenician_banner(const DrawContext &p, ISubmitter &out, Mesh *unit,
                            Texture *white, const CarthagePalette &c) {
  float const pole_x = 0.0F;
  float const pole_z = -2.0F;
  float const pole_height = 2.4F;
  float const pole_radius = 0.03F;
  float const banner_width = 0.5F;
  float const banner_height = 0.4F;
  float const panel_depth = 0.02F;

  QVector3D const pole_center(pole_x, pole_height / 2.0F, pole_z);
  QVector3D const pole_size(pole_radius * 1.6F, pole_height / 2.0F,
                            pole_radius * 1.6F);

  QMatrix4x4 poleTransform = p.model;
  poleTransform.translate(pole_center);
  poleTransform.scale(pole_size);
  out.mesh(unit, poleTransform, c.cedar, white, 1.0F);

  float const beam_length = banner_width * 0.45F;
  float const max_lowering = pole_height * 0.85F;

  auto captureColors = BarracksFlagRenderer::get_capture_colors(
      p, c.team, c.team_trim, max_lowering);

  float beam_y =
      pole_height - banner_height * 0.25F - captureColors.loweringOffset;
  float flag_y =
      pole_height - banner_height / 2.0F - captureColors.loweringOffset;

  QVector3D const beam_start(pole_x + 0.02F, beam_y, pole_z);
  QVector3D const beam_end(pole_x + beam_length + 0.02F, beam_y, pole_z);
  out.mesh(getUnitCylinder(),
           p.model * Render::Geom::cylinderBetween(beam_start, beam_end,
                                                   pole_radius * 0.35F),
           c.cedar, white, 1.0F);

  QVector3D const connector_top(
      beam_end.x(), beam_end.y() - banner_height * 0.35F, beam_end.z());
  out.mesh(getUnitCylinder(),
           p.model * Render::Geom::cylinderBetween(beam_end, connector_top,
                                                   pole_radius * 0.18F),
           c.limestone, white, 1.0F);

  float const panel_x = beam_end.x() + (banner_width * 0.5F - beam_length);

  QMatrix4x4 panelTransform = p.model;
  panelTransform.translate(QVector3D(panel_x, flag_y, pole_z + 0.01F));
  panelTransform.scale(
      QVector3D(banner_width / 2.0F, banner_height / 2.0F, panel_depth));
  out.mesh(unit, panelTransform, captureColors.teamColor, white, 1.0F);

  QMatrix4x4 trimBottom = p.model;
  trimBottom.translate(QVector3D(panel_x, flag_y - banner_height / 2.0F + 0.04F,
                                 pole_z + 0.01F));
  trimBottom.scale(QVector3D(banner_width / 2.0F + 0.02F, 0.04F, 0.015F));
  out.mesh(unit, trimBottom, captureColors.teamTrimColor, white, 1.0F);

  QMatrix4x4 trimTop = p.model;
  trimTop.translate(QVector3D(panel_x, flag_y + banner_height / 2.0F - 0.04F,
                              pole_z + 0.01F));
  trimTop.scale(QVector3D(banner_width / 2.0F + 0.02F, 0.04F, 0.015F));
  out.mesh(unit, trimTop, captureColors.teamTrimColor, white, 1.0F);

  draw_box(out, unit, white, p.model,
           QVector3D(pole_x + 0.2F, pole_height + 0.12F, pole_z + 0.03F),
           QVector3D(0.26F, 0.02F, 0.01F), c.gold);
}

void draw_rally_flag(const DrawContext &p, ISubmitter &out, Texture *white,
                     const CarthagePalette &c) {
  BarracksFlagRenderer::FlagColors colors{.team = c.team,
                                          .teamTrim = c.team_trim,
                                          .timber = c.cedar,
                                          .timberLight = c.limestone,
                                          .woodDark = c.cedar_dark};
  BarracksFlagRenderer::draw_rally_flag_if_any(p, out, white, colors);
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
  draw_box(out, unit, white, p.model, QVector3D(0.0F, 2.65F, 0.0F),
           QVector3D(0.9F, 0.04F, 0.06F), bg);

  QVector3D const fg = QVector3D(0.22F, 0.78F, 0.22F) * ratio +
                       QVector3D(0.85F, 0.15F, 0.15F) * (1.0F - ratio);
  draw_box(out, unit, white, p.model,
           QVector3D(-(0.9F * (1.0F - ratio)) * 0.5F, 2.66F, 0.0F),
           QVector3D(0.9F * ratio * 0.5F, 0.035F, 0.055F), fg);
}

void draw_selection(const DrawContext &p, ISubmitter &out) {
  QMatrix4x4 m;
  QVector3D const pos = p.model.column(3).toVector3D();
  m.translate(pos.x(), 0.0F, pos.z());
  m.scale(2.6F, 1.0F, 2.2F);
  if (p.selected) {
    out.selection_smoke(m, QVector3D(0.2F, 0.85F, 0.2F), 0.35F);
  } else if (p.hovered) {
    out.selection_smoke(m, QVector3D(0.95F, 0.92F, 0.25F), 0.22F);
  }
}

void draw_barracks(const DrawContext &p, ISubmitter &out) {
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
  CarthagePalette const c = make_palette(team);

  draw_platform(p, out, unit, white, c);
  draw_colonnade(p, out, unit, white, c);
  draw_central_courtyard(p, out, unit, white, c);
  draw_chamber(p, out, unit, white, c);
  draw_terrace(p, out, unit, white, c);
  draw_trading_goods(p, out, unit, white, c);
  draw_phoenician_banner(p, out, unit, white, c);
  draw_rally_flag(p, out, white, c);
  draw_health_bar(p, out, unit, white);
  draw_selection(p, out);
}

} // namespace

void register_barracks_renderer(Render::GL::EntityRendererRegistry &registry) {
  registry.register_renderer("barracks_carthage", draw_barracks);
}

} // namespace Render::GL::Carthage
