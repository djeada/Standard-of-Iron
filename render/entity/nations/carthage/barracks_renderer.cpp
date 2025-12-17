#include "barracks_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/visuals/team_colors.h"
#include "../../../geom/flag.h"
#include "../../../geom/math_utils.h"
#include "../../../geom/transforms.h"
#include "../../../gl/backend.h"
#include "../../../gl/primitives.h"
#include "../../../gl/resources.h"
#include "../../../submitter.h"
#include "../../barracks_flag_renderer.h"
#include "../../building_state.h"
#include "../../registry.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <cmath>

namespace Render::GL::Carthage {
namespace {

using Render::Geom::clamp01;
using Render::Geom::clampVec01;
using Render::Geom::cylinder_between;

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
  out.mesh(get_unit_cylinder(), model * cylinder_between(a, b, r), color, white,
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
                    Texture *white, const CarthagePalette &c,
                    BuildingState state) {
  float const col_height = 1.6F;
  float const col_radius = 0.10F;
  static constexpr float FRONT_COLUMN_SPACING_RANGE = 2.5F;
  static constexpr float SIDE_COLUMN_SPACING_RANGE = 2.0F;

  float height_multiplier = 1.0F;
  int num_columns = 6;
  if (state == BuildingState::Damaged) {
    height_multiplier = 0.7F;
    num_columns = 4;
  } else if (state == BuildingState::Destroyed) {
    height_multiplier = 0.4F;
    num_columns = 2;
  }

  for (int i = 0; i < num_columns; ++i) {
    float const x =
        -1.25F + float(i) * (num_columns > 1 ? FRONT_COLUMN_SPACING_RANGE /
                                                   (num_columns - 1)
                                             : 0.0F);
    float const z = 1.4F;

    draw_box(out, unit, white, p.model, QVector3D(x, 0.25F, z),
             QVector3D(col_radius * 1.2F, 0.05F, col_radius * 1.2F), c.marble);

    draw_cyl(out, p.model, QVector3D(x, 0.2F, z),
             QVector3D(x, 0.2F + col_height * height_multiplier, z), col_radius,
             c.limestone, white);

    draw_box(out, unit, white, p.model,
             QVector3D(x, 0.2F + col_height * height_multiplier + 0.05F, z),
             QVector3D(col_radius * 1.5F, 0.08F, col_radius * 1.5F), c.marble);

    if (state != BuildingState::Destroyed) {
      draw_box(out, unit, white, p.model,
               QVector3D(x, 0.2F + col_height * height_multiplier + 0.12F, z),
               QVector3D(col_radius * 1.3F, 0.04F, col_radius * 1.3F), c.gold);
    }
  }

  int side_columns = (state == BuildingState::Destroyed) ? 2 : 3;
  for (int i = 0; i < side_columns; ++i) {
    float const z =
        -1.0F + float(i) * (side_columns > 1
                                ? SIDE_COLUMN_SPACING_RANGE / (side_columns - 1)
                                : 0.0F);

    float const x_left = -1.6F;
    draw_box(out, unit, white, p.model, QVector3D(x_left, 0.25F, z),
             QVector3D(col_radius * 1.2F, 0.05F, col_radius * 1.2F), c.marble);
    draw_cyl(out, p.model, QVector3D(x_left, 0.2F, z),
             QVector3D(x_left, 0.2F + col_height * height_multiplier, z),
             col_radius, c.limestone, white);
    draw_box(
        out, unit, white, p.model,
        QVector3D(x_left, 0.2F + col_height * height_multiplier + 0.05F, z),
        QVector3D(col_radius * 1.5F, 0.08F, col_radius * 1.5F), c.marble);

    float const x_right = 1.6F;
    draw_box(out, unit, white, p.model, QVector3D(x_right, 0.25F, z),
             QVector3D(col_radius * 1.2F, 0.05F, col_radius * 1.2F), c.marble);
    draw_cyl(out, p.model, QVector3D(x_right, 0.2F, z),
             QVector3D(x_right, 0.2F + col_height * height_multiplier, z),
             col_radius, c.limestone, white);
    draw_box(
        out, unit, white, p.model,
        QVector3D(x_right, 0.2F + col_height * height_multiplier + 0.05F, z),
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
                  Texture *white, const CarthagePalette &c,
                  BuildingState state) {

  if (state == BuildingState::Destroyed) {
    return;
  }

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

void draw_phoenician_banner(
    const DrawContext &p, ISubmitter &out, Mesh *unit, Texture *white,
    const CarthagePalette &c,
    const BarracksFlagRenderer::ClothBannerResources *cloth) {
  float const pole_x = 0.0F;
  float const pole_z = -2.0F;
  float const pole_height = 3.0F;
  float const pole_radius = 0.045F;
  float const banner_width = 0.9F;
  float const banner_height = 0.6F;

  QVector3D const pole_center(pole_x, pole_height / 2.0F, pole_z);
  QVector3D const pole_size(pole_radius * 1.8F, pole_height / 2.0F,
                            pole_radius * 1.8F);

  QMatrix4x4 poleTransform = p.model;
  poleTransform.translate(pole_center);
  poleTransform.scale(pole_size);
  out.mesh(unit, poleTransform, c.cedar, white, 1.0F);

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
           c.cedar, white, 1.0F);

  QVector3D const connector_top(
      beam_end.x(), beam_end.y() - banner_height * 0.35F, beam_end.z());
  out.mesh(get_unit_cylinder(),
           p.model * Render::Geom::cylinder_between(beam_end, connector_top,
                                                    pole_radius * 0.18F),
           c.limestone, white, 1.0F);

  float const panel_x = beam_end.x() + (banner_width * 0.5F - beam_length);

  QVector3D banner_center(panel_x, flag_y, pole_z + 0.02F);
  BarracksFlagRenderer::drawBannerWithTassels(
      p, out, unit, white, banner_center, banner_width * 0.5F,
      banner_height * 0.5F, 0.02F, captureColors.teamColor,
      captureColors.teamTrimColor, cloth);

  draw_box(out, unit, white, p.model,
           QVector3D(pole_x + 0.25F, pole_height + 0.15F, pole_z + 0.03F),
           QVector3D(0.35F, 0.03F, 0.015F), c.gold);

  for (int i = 0; i < 4; ++i) {
    float ring_y = 0.4F + static_cast<float>(i) * 0.5F;
    out.mesh(get_unit_cylinder(),
             p.model * Render::Geom::cylinder_between(
                           QVector3D(pole_x, ring_y, pole_z),
                           QVector3D(pole_x, ring_y + 0.025F, pole_z),
                           pole_radius * 2.0F),
             c.gold, white, 1.0F);
  }
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

  // Check if building is under attack or being captured
  auto *capture = p.entity->get_component<Engine::Core::CaptureComponent>();
  bool under_attack = (capture != nullptr && capture->is_being_captured);
  
  // Also check if health is not at maximum (indicating recent damage)
  if (!under_attack && u->health >= u->max_health) {
    return; // Don't show health bar if at full health and not under attack
  }

  // Health bar dimensions - much larger and more prominent
  float const bar_width = 1.4F;
  float const bar_height = 0.10F;
  float const bar_y = 2.75F;
  float const border_thickness = 0.012F;
  
  // Outer glow effect - pulsing red when under attack
  if (under_attack) {
    float pulse = 0.7F + 0.3F * sinf(p.animation_time * 4.0F);
    QVector3D glow_color(0.9F, 0.2F, 0.2F);
    draw_box(out, unit, white, p.model, QVector3D(0.0F, bar_y, 0.0F),
             QVector3D(bar_width * 0.5F + border_thickness * 3.0F,
                       bar_height * 0.5F + border_thickness * 3.0F, 0.095F),
             glow_color * pulse * 0.6F);
  }
  
  // Metallic border frame - silver/steel color
  QVector3D const border_color(0.45F, 0.45F, 0.50F);
  draw_box(out, unit, white, p.model, QVector3D(0.0F, bar_y, 0.0F),
           QVector3D(bar_width * 0.5F + border_thickness,
                     bar_height * 0.5F + border_thickness, 0.09F),
           border_color);
  
  // Inner border (darker for depth)
  QVector3D const inner_border(0.25F, 0.25F, 0.28F);
  draw_box(out, unit, white, p.model, QVector3D(0.0F, bar_y, 0.0F),
           QVector3D(bar_width * 0.5F + border_thickness * 0.5F,
                     bar_height * 0.5F + border_thickness * 0.5F, 0.088F),
           inner_border);
  
  // Background - dark with slight gradient
  QVector3D const bg_dark(0.08F, 0.08F, 0.10F);
  draw_box(out, unit, white, p.model, QVector3D(0.0F, bar_y + 0.003F, 0.0F),
           QVector3D(bar_width * 0.5F, bar_height * 0.5F, 0.085F), bg_dark);

  // Determine state and color with vibrant, saturated colors
  QVector3D fg_color;
  QVector3D fg_dark; // Darker shade for gradient
  
  if (ratio >= 0.70F) {
    // Normal state: Bright emerald green
    fg_color = QVector3D(0.10F, 1.0F, 0.30F);
    fg_dark = QVector3D(0.05F, 0.60F, 0.15F);
  } else if (ratio >= 0.30F) {
    // Damaged state: Vibrant orange/amber
    float t = (ratio - 0.30F) / 0.40F;
    QVector3D damaged_color(1.0F, 0.75F, 0.10F);
    QVector3D damaged_dark(0.70F, 0.45F, 0.05F);
    QVector3D normal_color(0.10F, 1.0F, 0.30F);
    QVector3D normal_dark(0.05F, 0.60F, 0.15F);
    fg_color = normal_color * t + damaged_color * (1.0F - t);
    fg_dark = normal_dark * t + damaged_dark * (1.0F - t);
  } else {
    // Destroyed state: Intense red
    float t = ratio / 0.30F;
    QVector3D critical_color(1.0F, 0.15F, 0.15F);
    QVector3D critical_dark(0.70F, 0.08F, 0.08F);
    QVector3D damaged_color(1.0F, 0.75F, 0.10F);
    QVector3D damaged_dark(0.70F, 0.45F, 0.05F);
    fg_color = damaged_color * t + critical_color * (1.0F - t);
    fg_dark = damaged_dark * t + critical_dark * (1.0F - t);
  }
  
  // Bottom layer (darker) for gradient effect
  draw_box(
      out, unit, white, p.model,
      QVector3D(-(bar_width * (1.0F - ratio)) * 0.5F, bar_y + 0.005F, 0.0F),
      QVector3D(bar_width * ratio * 0.5F, bar_height * 0.48F, 0.08F), fg_dark);
  
  // Main health bar fill
  draw_box(
      out, unit, white, p.model,
      QVector3D(-(bar_width * (1.0F - ratio)) * 0.5F, bar_y + 0.008F, 0.0F),
      QVector3D(bar_width * ratio * 0.5F, bar_height * 0.40F, 0.078F), fg_color);
  
  // Top highlight for glossy/glass effect - very bright
  QVector3D const highlight = fg_color * 1.6F;
  draw_box(out, unit, white, p.model,
           QVector3D(-(bar_width * (1.0F - ratio)) * 0.5F,
                     bar_y + bar_height * 0.35F, 0.0F),
           QVector3D(bar_width * ratio * 0.5F, bar_height * 0.20F, 0.075F),
           clampVec01(highlight));
  
  // Add subtle shine line at very top
  QVector3D const shine = QVector3D(1.0F, 1.0F, 1.0F) * 0.8F;
  draw_box(out, unit, white, p.model,
           QVector3D(-(bar_width * (1.0F - ratio)) * 0.5F,
                     bar_y + bar_height * 0.48F, 0.0F),
           QVector3D(bar_width * ratio * 0.5F, bar_height * 0.08F, 0.073F),
           shine);
  
  // Enhanced segment markers - more visible
  QVector3D const segment_color(0.35F, 0.35F, 0.40F);
  QVector3D const segment_highlight(0.55F, 0.55F, 0.60F);
  
  // 70% marker with highlight
  float marker_70_x = bar_width * 0.5F * (0.70F - 0.5F);
  draw_box(out, unit, white, p.model,
           QVector3D(marker_70_x, bar_y, 0.0F),
           QVector3D(0.015F, bar_height * 0.55F, 0.09F), segment_color);
  draw_box(out, unit, white, p.model,
           QVector3D(marker_70_x - 0.003F, bar_y + bar_height * 0.40F, 0.0F),
           QVector3D(0.008F, bar_height * 0.15F, 0.091F), segment_highlight);
  
  // 30% marker with highlight
  float marker_30_x = bar_width * 0.5F * (0.30F - 0.5F);
  draw_box(out, unit, white, p.model,
           QVector3D(marker_30_x, bar_y, 0.0F),
           QVector3D(0.015F, bar_height * 0.55F, 0.09F), segment_color);
  draw_box(out, unit, white, p.model,
           QVector3D(marker_30_x - 0.003F, bar_y + bar_height * 0.40F, 0.0F),
           QVector3D(0.008F, bar_height * 0.15F, 0.091F), segment_highlight);
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
  CarthagePalette const c = make_palette(team);

  BarracksFlagRenderer::ClothBannerResources cloth;
  if (p.backend != nullptr) {
    cloth.clothMesh = p.backend->banner_mesh();
    cloth.bannerShader = p.backend->banner_shader();
  }

  draw_platform(p, out, unit, white, c);
  draw_colonnade(p, out, unit, white, c, state);
  draw_central_courtyard(p, out, unit, white, c);
  draw_chamber(p, out, unit, white, c);
  draw_terrace(p, out, unit, white, c, state);
  draw_trading_goods(p, out, unit, white, c);
  draw_phoenician_banner(p, out, unit, white, c, &cloth);
  draw_rally_flag(p, out, white, c);
  draw_health_bar(p, out, unit, white);
  draw_selection(p, out);
}

} // namespace

void register_barracks_renderer(Render::GL::EntityRendererRegistry &registry) {
  registry.register_renderer("barracks_carthage", draw_barracks);
}

} // namespace Render::GL::Carthage
