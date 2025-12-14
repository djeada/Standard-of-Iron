#include "barracks_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/visuals/team_colors.h"
#include "../../../geom/flag.h"
#include "../../../geom/math_utils.h"
#include "../../../geom/transforms.h"
#include "../../../gl/backend.h"
#include "../../../gl/backend/banner_pipeline.h"
#include "../../../gl/primitives.h"
#include "../../../gl/resources.h"
#include "../../../submitter.h"
#include "../../barracks_flag_renderer.h"
#include "../../registry.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>

namespace Render::GL::Roman {
namespace {

using Render::Geom::clamp01;
using Render::Geom::clampVec01;
using Render::Geom::cylinder_between;

struct RomanPalette {
  QVector3D stone_light{0.62F, 0.60F, 0.58F};
  QVector3D stone_dark{0.50F, 0.48F, 0.46F};
  QVector3D stone_base{0.55F, 0.53F, 0.51F};
  QVector3D brick{0.75F, 0.52F, 0.42F};
  QVector3D brick_dark{0.62F, 0.42F, 0.32F};
  QVector3D tile_red{0.72F, 0.40F, 0.30F};
  QVector3D tile_dark{0.58F, 0.30F, 0.22F};
  QVector3D wood{0.42F, 0.28F, 0.16F};
  QVector3D wood_dark{0.32F, 0.20F, 0.10F};
  QVector3D iron{0.35F, 0.35F, 0.38F};
  QVector3D team{0.8F, 0.9F, 1.0F};
  QVector3D team_trim{0.48F, 0.54F, 0.60F};
};

inline auto make_palette(const QVector3D &team) -> RomanPalette {
  RomanPalette p;
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
  out.mesh(get_unit_cylinder(), model * cylinder_between(a, b, r), color, white,
           1.0F);
}

void drawFortressBase(const DrawContext &p, ISubmitter &out, Mesh *unit,
                      Texture *white, const RomanPalette &c) {

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.15F, 0.0F),
           QVector3D(1.8F, 0.15F, 1.5F), c.stone_base);

  for (float x = -1.6F; x <= 1.6F; x += 0.4F) {
    draw_box(out, unit, white, p.model, QVector3D(x, 0.35F, -1.4F),
             QVector3D(0.18F, 0.08F, 0.08F), c.stone_dark);
    draw_box(out, unit, white, p.model, QVector3D(x, 0.35F, 1.4F),
             QVector3D(0.18F, 0.08F, 0.08F), c.stone_dark);
  }
  for (float z = -1.3F; z <= 1.3F; z += 0.4F) {
    draw_box(out, unit, white, p.model, QVector3D(-1.7F, 0.35F, z),
             QVector3D(0.08F, 0.08F, 0.18F), c.stone_dark);
    draw_box(out, unit, white, p.model, QVector3D(1.7F, 0.35F, z),
             QVector3D(0.08F, 0.08F, 0.18F), c.stone_dark);
  }
}

void drawFortressWalls(const DrawContext &p, ISubmitter &out, Mesh *unit,
                       Texture *white, const RomanPalette &c) {
  float const wall_height = 1.2F;

  draw_box(out, unit, white, p.model,
           QVector3D(0.0F, wall_height * 0.5F + 0.3F, -1.3F),
           QVector3D(1.5F, wall_height * 0.5F, 0.12F), c.stone_light);
  draw_box(out, unit, white, p.model,
           QVector3D(0.0F, wall_height * 0.5F + 0.3F, 1.3F),
           QVector3D(1.5F, wall_height * 0.5F, 0.12F), c.stone_light);
  draw_box(out, unit, white, p.model,
           QVector3D(-1.6F, wall_height * 0.5F + 0.3F, 0.0F),
           QVector3D(0.12F, wall_height * 0.5F, 1.2F), c.stone_light);
  draw_box(out, unit, white, p.model,
           QVector3D(1.6F, wall_height * 0.5F + 0.3F, 0.0F),
           QVector3D(0.12F, wall_height * 0.5F, 1.2F), c.stone_light);

  for (int i = 0; i < 6; ++i) {
    float const x = -1.2F + float(i) * 0.5F;
    draw_box(out, unit, white, p.model,
             QVector3D(x, wall_height + 0.35F, -1.25F),
             QVector3D(0.2F, 0.05F, 0.05F), c.brick);
  }
}

void drawCornerTowers(const DrawContext &p, ISubmitter &out, Mesh *unit,
                      Texture *white, const RomanPalette &c) {
  QVector3D corners[4] = {
      QVector3D(-1.5F, 0.0F, -1.2F), QVector3D(1.5F, 0.0F, -1.2F),
      QVector3D(-1.5F, 0.0F, 1.2F), QVector3D(1.5F, 0.0F, 1.2F)};

  for (int i = 0; i < 4; ++i) {

    draw_box(out, unit, white, p.model,
             QVector3D(corners[i].x(), 0.65F, corners[i].z()),
             QVector3D(0.25F, 0.65F, 0.25F), c.stone_dark);

    draw_box(out, unit, white, p.model,
             QVector3D(corners[i].x(), 1.45F, corners[i].z()),
             QVector3D(0.28F, 0.15F, 0.28F), c.brick_dark);

    for (int j = 0; j < 4; ++j) {
      float angle = float(j) * 1.57F;
      float ox = sinf(angle) * 0.18F;
      float oz = cosf(angle) * 0.18F;
      draw_box(out, unit, white, p.model,
               QVector3D(corners[i].x() + ox, 1.68F, corners[i].z() + oz),
               QVector3D(0.06F, 0.08F, 0.06F), c.stone_light);
    }
  }
}

void drawCourtyard(const DrawContext &p, ISubmitter &out, Mesh *unit,
                   Texture *white, const RomanPalette &c) {

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.32F, 0.0F),
           QVector3D(1.2F, 0.02F, 0.9F), c.stone_base);

  draw_cyl(out, p.model, QVector3D(0.0F, 0.3F, 0.0F),
           QVector3D(0.0F, 0.95F, 0.0F), 0.08F, c.stone_light, white);

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.65F, -0.85F),
           QVector3D(0.35F, 0.35F, 0.08F), c.brick);
}

void drawRomanRoof(const DrawContext &p, ISubmitter &out, Mesh *unit,
                   Texture *white, const RomanPalette &c) {

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 1.58F, 0.0F),
           QVector3D(1.55F, 0.05F, 1.25F), c.tile_red);

  for (float z = -1.0F; z <= 1.0F; z += 0.3F) {
    draw_box(out, unit, white, p.model, QVector3D(0.0F, 1.62F, z),
             QVector3D(1.5F, 0.02F, 0.08F), c.tile_dark);
  }
}

void drawGate(const DrawContext &p, ISubmitter &out, Mesh *unit, Texture *white,
              const RomanPalette &c) {

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.6F, 1.35F),
           QVector3D(0.5F, 0.6F, 0.08F), c.wood_dark);

  for (int i = 0; i < 3; ++i) {
    float y = 0.3F + float(i) * 0.3F;
    draw_box(out, unit, white, p.model, QVector3D(0.0F, y, 1.37F),
             QVector3D(0.45F, 0.03F, 0.02F), c.iron);
  }
}

void drawStandards(const DrawContext &p, ISubmitter &out, Mesh *unit,
                   Texture *white, const RomanPalette &c,
                   const BarracksFlagRenderer::ClothBannerResources *cloth) {
  float const pole_x = 2.0F;
  float const pole_z = -1.5F;
  float const pole_height = 2.6F;
  float const pole_radius = 0.045F;
  float const banner_width = 0.8F;
  float const banner_height = 0.5F;

  QVector3D const pole_center(pole_x, pole_height / 2.0F, pole_z);
  QVector3D const pole_size(pole_radius * 1.8F, pole_height / 2.0F,
                            pole_radius * 1.8F);

  QMatrix4x4 poleTransform = p.model;
  poleTransform.translate(pole_center);
  poleTransform.scale(pole_size);
  out.mesh(unit, poleTransform, c.wood, white, 1.0F);

  float const beam_length = banner_width * 0.5F;
  float const max_lowering = pole_height * 0.85F;

  auto captureColors = BarracksFlagRenderer::get_capture_colors(
      p, c.team, c.team_trim, max_lowering);

  float beam_y =
      pole_height - banner_height * 0.2F - captureColors.loweringOffset;
  float flag_y =
      pole_height - banner_height / 2.0F - captureColors.loweringOffset;

  QVector3D const beam_start(pole_x + 0.02F, beam_y, pole_z);
  QVector3D const beam_end(pole_x + beam_length + 0.02F, beam_y, pole_z);
  out.mesh(get_unit_cylinder(),
           p.model * Render::Geom::cylinder_between(beam_start, beam_end,
                                                    pole_radius * 0.35F),
           c.wood, white, 1.0F);

  QVector3D const connector_top(
      beam_end.x(), beam_end.y() - banner_height * 0.35F, beam_end.z());
  out.mesh(get_unit_cylinder(),
           p.model * Render::Geom::cylinder_between(beam_end, connector_top,
                                                    pole_radius * 0.18F),
           c.iron, white, 1.0F);

  float const panel_x = beam_end.x() + (banner_width * 0.5F - beam_length);

  QVector3D banner_center(panel_x, flag_y, pole_z + 0.02F);
  BarracksFlagRenderer::drawBannerWithTassels(
      p, out, unit, white, banner_center, banner_width * 0.5F,
      banner_height * 0.5F, 0.02F, captureColors.teamColor,
      captureColors.teamTrimColor, cloth);

  draw_box(out, unit, white, p.model,
           QVector3D(pole_x, pole_height + 0.2F, pole_z),
           QVector3D(0.12F, 0.10F, 0.12F), c.iron);

  for (int i = 0; i < 3; ++i) {
    float ring_y = 0.5F + static_cast<float>(i) * 0.6F;
    out.mesh(get_unit_cylinder(),
             p.model * Render::Geom::cylinder_between(
                           QVector3D(pole_x, ring_y, pole_z),
                           QVector3D(pole_x, ring_y + 0.03F, pole_z),
                           pole_radius * 2.2F),
             c.iron, white, 1.0F);
  }
}

void draw_rally_flag(const DrawContext &p, ISubmitter &out, Texture *white,
                     const RomanPalette &c) {
  BarracksFlagRenderer::FlagColors colors{.team = c.team,
                                          .teamTrim = c.team_trim,
                                          .timber = c.wood,
                                          .timberLight = c.stone_light,
                                          .woodDark = c.wood_dark};
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
  draw_box(out, unit, white, p.model, QVector3D(0.0F, 2.35F, 0.0F),
           QVector3D(0.9F, 0.04F, 0.06F), bg);

  QVector3D const fg = QVector3D(0.22F, 0.78F, 0.22F) * ratio +
                       QVector3D(0.85F, 0.15F, 0.15F) * (1.0F - ratio);
  draw_box(out, unit, white, p.model,
           QVector3D(-(0.9F * (1.0F - ratio)) * 0.5F, 2.36F, 0.0F),
           QVector3D(0.9F * ratio * 0.5F, 0.035F, 0.055F), fg);
}

void draw_selection(const DrawContext &p, ISubmitter &out) {
  QMatrix4x4 m;
  QVector3D const pos = p.model.column(3).toVector3D();
  m.translate(pos.x(), 0.0F, pos.z());
  m.scale(2.4F, 1.0F, 2.0F);
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
  RomanPalette const c = make_palette(team);

  BarracksFlagRenderer::ClothBannerResources cloth;
  if (p.backend != nullptr) {
    cloth.clothMesh = p.backend->banner_mesh();
    cloth.bannerShader = p.backend->banner_shader();
  }

  drawFortressBase(p, out, unit, white, c);
  drawFortressWalls(p, out, unit, white, c);
  drawCornerTowers(p, out, unit, white, c);
  drawCourtyard(p, out, unit, white, c);
  drawRomanRoof(p, out, unit, white, c);
  drawGate(p, out, unit, white, c);
  drawStandards(p, out, unit, white, c, &cloth);
  draw_rally_flag(p, out, white, c);
  draw_health_bar(p, out, unit, white);
  draw_selection(p, out);
}

} // namespace

void register_barracks_renderer(Render::GL::EntityRendererRegistry &registry) {
  registry.register_renderer("barracks_roman", draw_barracks);
}

} // namespace Render::GL::Roman
