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
#include <qmatrix4x4.h>
#include <qvectornd.h>

namespace Render::GL::Kingdom {
namespace {

using Render::Geom::clamp01;
using Render::Geom::clampVec01;
using Render::Geom::cylinderBetween;
using Render::Geom::lerp;

struct BuildingProportions {
  static constexpr float base_width = 2.4F;
  static constexpr float base_depth = 2.0F;
  static constexpr float base_height = 1.8F;
  static constexpr float foundation_height = 0.2F;
  static constexpr float wall_thickness = 0.08F;
  static constexpr float beam_thickness = 0.12F;
  static constexpr float corner_post_radius = 0.08F;
  static constexpr float roof_pitch = 0.8F;
  static constexpr float roof_overhang = 0.15F;
  static constexpr float thatch_layer_height = 0.12F;
  static constexpr float annex_width = 1.0F;
  static constexpr float annex_depth = 1.0F;
  static constexpr float annex_height = 1.2F;
  static constexpr float annex_roof_height = 0.5F;
  static constexpr float door_width = 0.5F;
  static constexpr float door_height = 0.8F;
  static constexpr float window_width = 0.4F;
  static constexpr float window_height = 0.5F;
  static constexpr float chimney_width = 0.25F;
  static constexpr float chimney_height = 1.0F;
  static constexpr float chimney_cap_size = 0.35F;
  static constexpr float banner_pole_height = 2.0F;
  static constexpr float banner_pole_radius = 0.05F;
  static constexpr float banner_width = 0.5F;
  static constexpr float banner_height = 0.6F;
};

struct BarracksPalette {
  QVector3D plaster{0.92F, 0.88F, 0.78F};
  QVector3D plasterShade{0.78F, 0.74F, 0.64F};
  QVector3D timber{0.35F, 0.25F, 0.15F};
  QVector3D timberLight{0.50F, 0.38F, 0.22F};
  QVector3D woodDark{0.30F, 0.20F, 0.12F};
  QVector3D thatch{0.82F, 0.70F, 0.28F};
  QVector3D thatchDark{0.68F, 0.58F, 0.22F};
  QVector3D stone{0.55F, 0.54F, 0.52F};
  QVector3D stoneDark{0.42F, 0.41F, 0.39F};
  QVector3D door{0.28F, 0.20F, 0.12F};
  QVector3D window{0.35F, 0.42F, 0.48F};
  QVector3D path{0.62F, 0.60F, 0.54F};
  QVector3D crate{0.48F, 0.34F, 0.18F};
  QVector3D team{0.8F, 0.9F, 1.0F};
  QVector3D teamTrim{0.48F, 0.54F, 0.60F};
};

inline auto make_palette(const QVector3D &team) -> BarracksPalette {
  BarracksPalette p;
  p.team = clampVec01(team);
  p.teamTrim =
      clampVec01(QVector3D(team.x() * 0.6F, team.y() * 0.6F, team.z() * 0.6F));
  return p;
}

inline void draw_cylinder(ISubmitter &out, const QMatrix4x4 &model,
                          const QVector3D &a, const QVector3D &b, float radius,
                          const QVector3D &color, Texture *white) {
  out.mesh(getUnitCylinder(), model * cylinderBetween(a, b, radius), color,
           white, 1.0F);
}

inline void unitBox(ISubmitter &out, Mesh *unitMesh, Texture *white,
                    const QMatrix4x4 &model, const QVector3D &t,
                    const QVector3D &s, const QVector3D &color) {
  QMatrix4x4 m = model;
  m.translate(t);
  m.scale(s);
  out.mesh(unitMesh, m, color, white, 1.0F);
}

inline void unitBox(ISubmitter &out, Mesh *unitMesh, Texture *white,
                    const QMatrix4x4 &model, const QVector3D &s,
                    const QVector3D &color) {
  QMatrix4x4 m = model;
  m.scale(s);
  out.mesh(unitMesh, m, color, white, 1.0F);
}

inline void drawFoundation(const DrawContext &p, ISubmitter &out, Mesh *unit,
                           Texture *white, const BarracksPalette &C) {
  constexpr float base_width = BuildingProportions::base_width;
  constexpr float base_depth = BuildingProportions::base_depth;
  constexpr float foundation_height = BuildingProportions::foundation_height;

  unitBox(out, unit, white, p.model,
          QVector3D(0.0F, -foundation_height / 2, 0.0F),
          QVector3D(base_width / 2 + 0.1F, foundation_height / 2,
                    base_depth / 2 + 0.1F),
          C.stoneDark);

  const float step_h = 0.015F;
  const float step_w = 0.16F;
  const float step_d = 0.10F;
  const float front_z = base_depth * 0.5F + 0.12F;
  for (int i = 0; i < 5; ++i) {
    float const t = i / 4.0F;
    float const x = (i % 2 == 0) ? -0.18F : 0.18F;
    QVector3D const c = lerp(C.path, C.stone, 0.25F * (i % 2));
    unitBox(out, unit, white, p.model,
            QVector3D(x, -foundation_height + step_h, front_z + t * 0.55F),
            QVector3D(step_w * (0.95F + 0.1F * (i % 2)), step_h, step_d), c);
  }

  QVector3D const skirt_color =
      lerp(C.stoneDark, QVector3D(0.0F, 0.0F, 0.0F), 0.25F);
  unitBox(out, unit, white, p.model, QVector3D(0.0F, 0.02F, 0.0F),
          QVector3D(base_width * 0.50F, 0.01F, base_depth * 0.50F),
          skirt_color);
}

inline void drawWalls(const DrawContext &p, ISubmitter &out, Mesh *,
                      Texture *white, const BarracksPalette &C) {
  constexpr float w = BuildingProportions::base_width;
  constexpr float d = BuildingProportions::base_depth;
  constexpr float h = BuildingProportions::base_height;

  const float r = 0.09F;
  const float notch = 0.07F;

  const float left_x = -w * 0.5F;
  const float right_x = w * 0.5F;
  const float back_z = -d * 0.5F;
  const float front_z = d * 0.5F;

  const int courses = std::max(4, int(h / (2.0F * r)));
  const float y0 = r;

  auto log_x = [&](float y, float z, float x0, float x1, const QVector3D &col) {
    draw_cylinder(out, p.model, QVector3D(x0 - notch, y, z),
                  QVector3D(x1 + notch, y, z), r, col, white);
  };
  auto log_z = [&](float y, float x, float z0, float z1, const QVector3D &col) {
    draw_cylinder(out, p.model, QVector3D(x, y, z0 - notch),
                  QVector3D(x, y, z1 + notch), r, col, white);
  };

  const float door_w = BuildingProportions::door_width;
  const float door_h = BuildingProportions::door_height;
  const float gap_half = door_w * 0.5F;

  for (int i = 0; i < courses; ++i) {
    float const y = y0 + i * (2.0F * r);
    QVector3D const log_col = lerp(C.timber, C.timberLight, (i % 2) * 0.25F);

    if (y <= (door_h - 0.5F * r)) {
      log_x(y, front_z, left_x, -gap_half, log_col);
      log_x(y, front_z, +gap_half, right_x, log_col);
    } else {
      log_x(y, front_z, left_x, right_x, log_col);
    }

    log_x(y, back_z, left_x, right_x, log_col);
    log_z(y, left_x, back_z, front_z, log_col);
    log_z(y, right_x, back_z, front_z, log_col);
  }

  QVector3D const post_col = C.woodDark;
  draw_cylinder(out, p.model, QVector3D(-gap_half, y0, front_z),
                QVector3D(-gap_half, y0 + door_h, front_z), r * 0.95F, post_col,
                white);
  draw_cylinder(out, p.model, QVector3D(+gap_half, y0, front_z),
                QVector3D(+gap_half, y0 + door_h, front_z), r * 0.95F, post_col,
                white);
  draw_cylinder(out, p.model, QVector3D(-gap_half, y0 + door_h, front_z),
                QVector3D(+gap_half, y0 + door_h, front_z), r, C.timberLight,
                white);

  float const brace_y0 = h * 0.35F;
  float const brace_y1 = h * 0.95F;
  draw_cylinder(out, p.model,
                QVector3D(left_x + 0.08F, brace_y0, back_z + 0.10F),
                QVector3D(left_x + 0.38F, brace_y1, back_z + 0.10F), r * 0.6F,
                C.woodDark, white);
  draw_cylinder(out, p.model,
                QVector3D(right_x - 0.08F, brace_y0, back_z + 0.10F),
                QVector3D(right_x - 0.38F, brace_y1, back_z + 0.10F), r * 0.6F,
                C.woodDark, white);
  draw_cylinder(out, p.model,
                QVector3D(left_x + 0.08F, brace_y0, front_z - 0.10F),
                QVector3D(left_x + 0.38F, brace_y1, front_z - 0.10F), r * 0.6F,
                C.woodDark, white);
  draw_cylinder(out, p.model,
                QVector3D(right_x - 0.08F, brace_y0, front_z - 0.10F),
                QVector3D(right_x - 0.38F, brace_y1, front_z - 0.10F), r * 0.6F,
                C.woodDark, white);
}

struct ChimneyInfo {
  float x;
  float z;
  float base_y;
  float topY;
  float gapRadius;
};

inline auto drawChimney(const DrawContext &p, ISubmitter &out, Mesh *unit,
                        Texture *white,
                        const BarracksPalette &C) -> ChimneyInfo {
  constexpr float w = BuildingProportions::base_width;
  constexpr float d = BuildingProportions::base_depth;
  constexpr float h = BuildingProportions::base_height;
  constexpr float rise = BuildingProportions::roof_pitch;

  float const x = -w * 0.32F;
  float const z = -d * 0.5F - 0.06F;

  float const base_y = 0.18F;
  float const ridge_y = h + rise;
  float const top_y = ridge_y + 0.35F;

  QVector3D const base_sz(BuildingProportions::chimney_width * 0.65F, 0.16F,
                          BuildingProportions::chimney_width * 0.55F);
  unitBox(out, unit, white, p.model, QVector3D(x, base_y + base_sz.y(), z),
          base_sz, C.stoneDark);

  int const segments = 4;
  float const seg_h = (top_y - (base_y + base_sz.y() * 2.0F)) / float(segments);
  float const w0 = BuildingProportions::chimney_width * 0.55F;
  float const w1 = BuildingProportions::chimney_width * 0.34F;

  for (int i = 0; i < segments; ++i) {
    float const t = float(i) / float(segments - 1);
    float const wy = w0 * (1.0F - t) + w1 * t;
    float const hz = wy * 0.85F;
    QVector3D const col =
        (i % 2 == 0) ? C.stone : lerp(C.stone, C.stoneDark, 0.35F);
    float const y_mid = base_y + base_sz.y() * 2.0F + seg_h * (i + 0.5F);
    unitBox(out, unit, white, p.model, QVector3D(x, y_mid, z),
            QVector3D(wy, seg_h * 0.5F, hz), col);
  }

  float const corbel_y = top_y - 0.14F;
  unitBox(out, unit, white, p.model, QVector3D(x, corbel_y, z),
          QVector3D(w1 * 1.22F, 0.025F, w1 * 1.22F), C.stoneDark);
  unitBox(out, unit, white, p.model, QVector3D(x, corbel_y + 0.05F, z),
          QVector3D(w1 * 1.05F, 0.02F, w1 * 1.05F),
          lerp(C.stone, C.stoneDark, 0.2F));

  float const pot_h = 0.10F;
  unitBox(out, unit, white, p.model, QVector3D(x, top_y + pot_h * 0.5F, z),
          QVector3D(w1 * 0.45F, pot_h * 0.5F, w1 * 0.45F),
          lerp(C.stoneDark, QVector3D(0.08F, 0.08F, 0.08F), 0.35F));

  unitBox(out, unit, white, p.model, QVector3D(x, h + rise * 0.55F, z + 0.06F),
          QVector3D(w1 * 1.35F, 0.01F, 0.04F),
          lerp(C.stoneDark, QVector3D(0.05F, 0.05F, 0.05F), 0.3F));

  return ChimneyInfo{x, z, base_y, top_y + pot_h, 0.28F};
}

inline void drawRoofs(const DrawContext &p, ISubmitter &out, Mesh *,
                      Texture *white, const BarracksPalette &C,
                      const ChimneyInfo &ch) {
  constexpr float w = BuildingProportions::base_width;
  constexpr float d = BuildingProportions::base_depth;
  constexpr float h = BuildingProportions::base_height;
  constexpr float rise = BuildingProportions::roof_pitch;
  constexpr float over = BuildingProportions::roof_overhang;

  const float r = 0.085F;

  const float left_x = -w * 0.5F;
  const float right_x = w * 0.5F;
  const float back_z = -d * 0.5F;
  const float front_z = d * 0.5F;

  const float plate_y = h;
  const float ridge_y = h + rise;

  draw_cylinder(out, p.model, QVector3D(left_x - over, plate_y, front_z + over),
                QVector3D(right_x + over, plate_y, front_z + over), r,
                C.woodDark, white);
  draw_cylinder(out, p.model, QVector3D(left_x - over, plate_y, back_z - over),
                QVector3D(right_x + over, plate_y, back_z - over), r,
                C.woodDark, white);

  draw_cylinder(out, p.model, QVector3D(left_x - over * 0.5F, ridge_y, 0.0F),
                QVector3D(right_x + over * 0.5F, ridge_y, 0.0F), r,
                C.timberLight, white);

  const int pairs = 7;
  for (int i = 0; i < pairs; ++i) {
    float const t = (pairs == 1) ? 0.0F : (float(i) / float(pairs - 1));
    float const x =
        (left_x - over * 0.5F) * (1.0F - t) + (right_x + over * 0.5F) * t;

    draw_cylinder(out, p.model, QVector3D(x, plate_y, back_z - over),
                  QVector3D(x, ridge_y, 0.0F), r * 0.85F, C.woodDark, white);

    draw_cylinder(out, p.model, QVector3D(x, plate_y, front_z + over),
                  QVector3D(x, ridge_y, 0.0F), r * 0.85F, C.woodDark, white);
  }

  auto purlin = [&](float tz, bool front) {
    float const z = front ? (front_z + over - tz * (front_z + over))
                          : (back_z - over - tz * (back_z - over));
    float const y = plate_y + tz * (ridge_y - plate_y);
    draw_cylinder(out, p.model, QVector3D(left_x - over * 0.4F, y, z),
                  QVector3D(right_x + over * 0.4F, y, z), r * 0.6F, C.timber,
                  white);
  };
  purlin(0.35F, true);
  purlin(0.70F, true);
  purlin(0.35F, false);
  purlin(0.70F, false);

  auto split_thatch = [&](float y, float z, float rad, const QVector3D &col) {
    float const gap_l = ch.x - ch.gapRadius;
    float const gap_r = ch.x + ch.gapRadius;
    draw_cylinder(out, p.model, QVector3D(left_x - over * 0.35F, y, z),
                  QVector3D(gap_l, y, z), rad, col, white);
    draw_cylinder(out, p.model, QVector3D(gap_r, y, z),
                  QVector3D(right_x + over * 0.35F, y, z), rad, col, white);
  };

  auto thatch_row = [&](float tz, bool front, float radScale, float tint) {
    float const z = front ? (front_z + over - tz * (front_z + over))
                          : (back_z - over - tz * (back_z - over));
    float const y = plate_y + tz * (ridge_y - plate_y);
    QVector3D const col = lerp(C.thatchDark, C.thatch, clamp01(tint));
    split_thatch(y, z, r * radScale, col);
  };

  const int rows = 9;
  for (int i = 0; i < rows; ++i) {
    float const tz = float(i) / float(rows - 1);
    float const s = 1.30F - 0.6F * tz;
    float const tint = 0.2F + 0.6F * (1.0F - tz);
    thatch_row(tz, true, s, tint);
    thatch_row(tz * 0.98F, false, s, tint * 0.95F);
  }

  float const eave_y = plate_y + 0.06F;
  split_thatch(eave_y, front_z + over * 1.02F, r * 0.55F, C.thatchDark);
  split_thatch(eave_y, back_z - over * 1.02F, r * 0.55F, C.thatchDark);

  float const flash_y = plate_y + (ridge_y - plate_y) * 0.55F;
  float const flash_zback = back_z - over * 0.20F;
  float const ring = ch.gapRadius + 0.04F;
  unitBox(out, nullptr, white, p.model, QVector3D(ch.x, flash_y, flash_zback),
          QVector3D(ring, 0.008F, 0.02F), C.stoneDark);
}

inline void drawDoor(const DrawContext &p, ISubmitter &out, Mesh *unit,
                     Texture *white, const BarracksPalette &C) {
  constexpr float d = BuildingProportions::base_depth;
  constexpr float d_w = BuildingProportions::door_width;
  constexpr float d_h = BuildingProportions::door_height;

  const float y0 = 0.09F;
  const float zf = d * 0.5F;

  QVector3D const frame_col = C.woodDark;
  unitBox(out, unit, white, p.model,
          QVector3D(0.0F, y0 + d_h * 0.5F, zf + 0.015F),
          QVector3D(d_w * 0.5F, d_h * 0.5F, 0.02F), C.door);

  float const plank_w = d_w / 6.0F;
  for (int i = 0; i < 6; ++i) {
    float const cx = -d_w * 0.5F + plank_w * (i + 0.5F);
    QVector3D const plank_col = lerp(C.door, C.woodDark, 0.15F * (i % 2));
    unitBox(out, unit, white, p.model,
            QVector3D(cx, y0 + d_h * 0.5F, zf + 0.022F),
            QVector3D(plank_w * 0.48F, d_h * 0.48F, 0.006F), plank_col);
  }

  draw_cylinder(out, p.model,
                QVector3D(-d_w * 0.45F, y0 + d_h * 0.35F, zf + 0.03F),
                QVector3D(+d_w * 0.45F, y0 + d_h * 0.35F, zf + 0.03F), 0.02F,
                frame_col, white);

  draw_cylinder(out, p.model,
                QVector3D(d_w * 0.32F, y0 + d_h * 0.45F, zf + 0.04F),
                QVector3D(d_w * 0.42F, y0 + d_h * 0.45F, zf + 0.04F), 0.012F,
                C.timberLight, white);

  unitBox(out, unit, white, p.model,
          QVector3D(0.0F, y0 + d_h + 0.10F, zf + 0.02F),
          QVector3D(0.22F, 0.06F, 0.01F), C.woodDark);
  unitBox(out, unit, white, p.model,
          QVector3D(0.0F, y0 + d_h + 0.10F, zf + 0.025F),
          QVector3D(0.18F, 0.05F, 0.008F), C.team);
  unitBox(out, unit, white, p.model,
          QVector3D(0.0F, y0 + d_h + 0.10F, zf + 0.03F),
          QVector3D(0.08F, 0.02F, 0.007F), C.teamTrim);
}

inline void drawWindows(const DrawContext &p, ISubmitter &out, Mesh *unit,
                        Texture *white, const BarracksPalette &C) {
  constexpr float base_w = BuildingProportions::base_width;
  constexpr float base_d = BuildingProportions::base_depth;
  constexpr float base_h = BuildingProportions::base_height;

  const float left_x = -base_w * 0.5F;
  const float right_x = base_w * 0.5F;
  const float back_z = -base_d * 0.5F;
  const float front_z = base_d * 0.5F;

  float const window_w = BuildingProportions::window_width * 0.55F;
  float const window_h = BuildingProportions::window_height * 0.55F;
  float const frame_t = 0.03F;

  auto framed_window = [&](QVector3D center, bool shutters) {
    unitBox(out, unit, white, p.model, center + QVector3D(0, 0, 0.012F),
            QVector3D(window_w * 0.5F, window_h * 0.5F, 0.008F), C.window);

    unitBox(out, unit, white, p.model, center + QVector3D(0, 0, 0.016F),
            QVector3D(window_w * 0.5F, frame_t, 0.006F), C.timber);
    unitBox(out, unit, white, p.model, center + QVector3D(0, 0, 0.016F),
            QVector3D(frame_t, window_h * 0.5F, 0.006F), C.timber);

    unitBox(out, unit, white, p.model, center + QVector3D(0, 0, 0.02F),
            QVector3D(window_w * 0.02F, window_h * 0.48F, 0.004F),
            C.timberLight);
    unitBox(out, unit, white, p.model, center + QVector3D(0, 0, 0.02F),
            QVector3D(window_w * 0.48F, window_h * 0.02F, 0.004F),
            C.timberLight);

    if (shutters) {
      unitBox(out, unit, white, p.model,
              center + QVector3D(-window_w * 0.65F, 0, 0.018F),
              QVector3D(window_w * 0.30F, window_h * 0.55F, 0.004F),
              C.woodDark);
      unitBox(out, unit, white, p.model,
              center + QVector3D(+window_w * 0.65F, 0, 0.018F),
              QVector3D(window_w * 0.30F, window_h * 0.55F, 0.004F),
              C.woodDark);
    }
  };

  framed_window(QVector3D(-0.65F, 0.95F, front_z + 0.01F), true);
  framed_window(QVector3D(+0.65F, 0.95F, front_z + 0.01F), true);
  framed_window(QVector3D(0.0F, 1.00F, back_z - 0.01F), true);

  framed_window(QVector3D(left_x + 0.06F, 0.85F, 0.0F), false);
  framed_window(QVector3D(right_x - 0.06F, 0.85F, 0.0F), false);
}

inline void drawAnnex(const DrawContext &p, ISubmitter &out, Mesh *unit,
                      Texture *white, const BarracksPalette &C) {
  constexpr float base_w = BuildingProportions::base_width;
  constexpr float base_d = BuildingProportions::base_depth;
  constexpr float annex_h = BuildingProportions::annex_height;
  constexpr float annex_w = BuildingProportions::annex_width;
  constexpr float annex_d = BuildingProportions::annex_depth;

  float const x = base_w * 0.5F + annex_w * 0.5F - 0.05F;
  float const z = 0.05F;

  unitBox(out, unit, white, p.model, QVector3D(x, annex_h * 0.5F, z),
          QVector3D(annex_w * 0.5F, annex_h * 0.5F, annex_d * 0.5F),
          C.plasterShade);

  unitBox(out, unit, white, p.model, QVector3D(x, annex_h + 0.02F, z),
          QVector3D(annex_w * 0.55F, 0.02F, annex_d * 0.55F), C.woodDark);

  float const plate_y = annex_h;
  float const front_z = z + annex_d * 0.5F;
  float const back_z = z - annex_d * 0.5F;
  draw_cylinder(out, p.model,
                QVector3D(x - annex_w * 0.52F, plate_y, back_z - 0.12F),
                QVector3D(x + annex_w * 0.52F, plate_y, back_z - 0.12F), 0.05F,
                C.woodDark, white);

  float const ridge_y = annex_h + BuildingProportions::annex_roof_height;
  draw_cylinder(out, p.model,
                QVector3D(x - annex_w * 0.50F, ridge_y, back_z - 0.02F),
                QVector3D(x + annex_w * 0.50F, ridge_y, back_z - 0.02F), 0.05F,
                C.timberLight, white);

  int const rows = 6;
  for (int i = 0; i < rows; ++i) {
    float const t = float(i) / float(rows - 1);
    float const y = plate_y + t * (ridge_y - plate_y);
    float const zrow = back_z - 0.02F - 0.10F * (1.0F - t);
    QVector3D const col =
        lerp(C.thatchDark, C.thatch, 0.5F + 0.4F * (1.0F - t));
    draw_cylinder(out, p.model, QVector3D(x - annex_w * 0.55F, y, zrow),
                  QVector3D(x + annex_w * 0.55F, y, zrow),
                  0.06F * (1.15F - 0.6F * t), col, white);
  }

  unitBox(out, unit, white, p.model,
          QVector3D(x + annex_w * 0.01F, 0.55F, front_z + 0.01F),
          QVector3D(0.20F, 0.18F, 0.01F), C.door);
}

inline void drawProps(const DrawContext &p, ISubmitter &out, Mesh *unit,
                      Texture *white, const BarracksPalette &C) {
  unitBox(out, unit, white, p.model, QVector3D(0.85F, 0.10F, 0.90F),
          QVector3D(0.16F, 0.10F, 0.16F), C.crate);
  unitBox(out, unit, white, p.model, QVector3D(0.85F, 0.22F, 0.90F),
          QVector3D(0.12F, 0.02F, 0.12F), C.woodDark);

  unitBox(out, unit, white, p.model, QVector3D(-0.9F, 0.12F, -0.80F),
          QVector3D(0.12F, 0.10F, 0.12F), C.crate);
  unitBox(out, unit, white, p.model, QVector3D(-0.9F, 0.20F, -0.80F),
          QVector3D(0.13F, 0.02F, 0.13F), C.woodDark);
}

inline void drawBannerAndPole(const DrawContext &p, ISubmitter &out, Mesh *unit,
                              Texture *white, const BarracksPalette &C) {
  constexpr float base_width = BuildingProportions::base_width;
  constexpr float base_depth = BuildingProportions::base_depth;
  constexpr float banner_pole_height = BuildingProportions::banner_pole_height;
  constexpr float banner_pole_radius = BuildingProportions::banner_pole_radius;
  constexpr float banner_width = BuildingProportions::banner_width;
  constexpr float banner_height = BuildingProportions::banner_height;

  float const pole_x = -base_width / 2 - 0.65F;
  float const pole_z = base_depth / 2 - 0.2F;

  float const pole_height = banner_pole_height * 1.9F;
  float const pole_radius = banner_pole_radius * 1.3F;
  float const bw = banner_width * 1.8F;
  float const bh = banner_height * 1.8F;

  QVector3D const pole_center(pole_x, pole_height / 2.0F, pole_z);
  QVector3D const pole_size(pole_radius * 1.6F, pole_height / 2.0F,
                            pole_radius * 1.6F);
  unitBox(out, unit, white, p.model, pole_center, pole_size, C.woodDark);

  float const target_width = bw * 1.25F;
  float const target_height = bh * 0.75F;
  float const panel_depth = 0.02F;

  float const beam_length = target_width * 0.45F;
  float const max_lowering = pole_height * 0.85F;

  auto captureColors = BarracksFlagRenderer::get_capture_colors(
      p, C.team, C.teamTrim, max_lowering);

  float beam_y =
      pole_height - target_height * 0.25F - captureColors.loweringOffset;
  float flag_y =
      pole_height - target_height / 2.0F - captureColors.loweringOffset;

  QVector3D const beam_start(pole_x + 0.02F, beam_y, pole_z);
  QVector3D const beam_end(pole_x + beam_length + 0.02F, beam_y, pole_z);
  draw_cylinder(out, p.model, beam_start, beam_end, pole_radius * 0.35F,
                C.timber, white);

  QVector3D const connector_top(
      beam_end.x(), beam_end.y() - target_height * 0.35F, beam_end.z());
  draw_cylinder(out, p.model, beam_end, connector_top, pole_radius * 0.18F,
                C.timberLight, white);

  float const panel_x = beam_end.x() + (target_width * 0.5F - beam_length);
  unitBox(out, unit, white, p.model, QVector3D(panel_x, flag_y, pole_z + 0.01F),
          QVector3D(target_width / 2.0F, target_height / 2.0F, panel_depth),
          captureColors.teamColor);

  unitBox(
      out, unit, white, p.model,
      QVector3D(panel_x, flag_y - target_height / 2.0F + 0.04F, pole_z + 0.01F),
      QVector3D(target_width / 2.0F + 0.02F, 0.04F, 0.015F),
      captureColors.teamTrimColor);
  unitBox(
      out, unit, white, p.model,
      QVector3D(panel_x, flag_y + target_height / 2.0F - 0.04F, pole_z + 0.01F),
      QVector3D(target_width / 2.0F + 0.02F, 0.04F, 0.015F),
      captureColors.teamTrimColor);
}

inline void draw_rally_flag_if_any(const DrawContext &p, ISubmitter &out,
                                   Texture *white, const BarracksPalette &C) {
  BarracksFlagRenderer::FlagColors colors{.team = C.team,
                                          .teamTrim = C.teamTrim,
                                          .timber = C.timber,
                                          .timberLight = C.timberLight,
                                          .woodDark = C.woodDark};
  BarracksFlagRenderer::draw_rally_flag_if_any(p, out, white, colors);
}

inline void draw_health_bar(const DrawContext &p, ISubmitter &out, Mesh *unit,
                            Texture *white) {
  if (p.entity == nullptr) {
    return;
  }
  auto *u = p.entity->getComponent<Engine::Core::UnitComponent>();
  if (u == nullptr) {
    return;
  }

  int const mh = std::max(1, u->max_health);
  float const ratio = std::clamp(u->health / float(mh), 0.0F, 1.0F);
  if (ratio <= 0.0F) {
    return;
  }

  constexpr float base_height = BuildingProportions::base_height;
  constexpr float roof_pitch = BuildingProportions::roof_pitch;
  float const roof_peak = base_height + roof_pitch;
  float const bar_y = roof_peak + 0.12F;

  constexpr float bar_width = BuildingProportions::base_width * 0.9F;
  constexpr float bar_height = 0.08F;
  constexpr float bar_depth = 0.12F;

  QVector3D const bg_color(0.06F, 0.06F, 0.06F);
  unitBox(out, unit, white, p.model, QVector3D(0.0F, bar_y, 0.0F),
          QVector3D(bar_width / 2.0F, bar_height / 2.0F, bar_depth / 2.0F),
          bg_color);

  float const fill_width = bar_width * ratio;
  float const fill_x = -(bar_width - fill_width) * 0.5F;

  QVector3D const red(0.85F, 0.15F, 0.15F);
  QVector3D const green(0.22F, 0.78F, 0.22F);
  QVector3D const fg_color = green * ratio + red * (1.0F - ratio);

  unitBox(out, unit, white, p.model, QVector3D(fill_x, bar_y + 0.005F, 0.0F),
          QVector3D(fill_width / 2.0F, (bar_height / 2.0F) * 0.9F,
                    (bar_depth / 2.0F) * 0.95F),
          fg_color);
}

inline void draw_selectionFX(const DrawContext &p, ISubmitter &out) {
  QMatrix4x4 m;
  QVector3D const pos = p.model.column(3).toVector3D();
  m.translate(pos.x(), 0.0F, pos.z());
  m.scale(2.2F, 1.0F, 2.0F);
  if (p.selected) {
    out.selectionSmoke(m, QVector3D(0.2F, 0.85F, 0.2F), 0.35F);
  } else if (p.hovered) {
    out.selectionSmoke(m, QVector3D(0.95F, 0.92F, 0.25F), 0.22F);
  }
}

void draw_barracks(const DrawContext &p, ISubmitter &out) {
  if ((p.resources == nullptr) || (p.entity == nullptr)) {
    return;
  }

  auto *t = p.entity->getComponent<Engine::Core::TransformComponent>();
  auto *r = p.entity->getComponent<Engine::Core::RenderableComponent>();
  if ((t == nullptr) || (r == nullptr)) {
    return;
  }

  Mesh *unit = p.resources->unit();
  Texture *white = p.resources->white();

  QVector3D const team(r->color[0], r->color[1], r->color[2]);
  BarracksPalette const c = make_palette(team);

  drawFoundation(p, out, unit, white, c);
  drawAnnex(p, out, unit, white, c);
  drawWalls(p, out, unit, white, c);
  ChimneyInfo const ch = drawChimney(p, out, unit, white, c);
  drawRoofs(p, out, unit, white, c, ch);
  drawDoor(p, out, unit, white, c);
  drawWindows(p, out, unit, white, c);
  drawBannerAndPole(p, out, unit, white, c);
  drawProps(p, out, unit, white, c);

  draw_rally_flag_if_any(p, out, white, c);
  draw_health_bar(p, out, unit, white);
  draw_selectionFX(p, out);
}

} // namespace

void register_barracks_renderer(Render::GL::EntityRendererRegistry &registry) {
  registry.register_renderer("barracks_kingdom", draw_barracks);
}

} // namespace Render::GL::Kingdom
