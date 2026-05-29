#include "barracks_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>

#include "../../../../game/core/component.h"
#include "../../../../game/visuals/team_colors.h"
#include "../../../geom/flag.h"
#include "../../../geom/transforms.h"
#include "../../../gl/backend.h"
#include "../../../gl/primitives.h"
#include "../../../gl/resources.h"
#include "../../../render_archetype.h"
#include "../../../submitter.h"
#include "../../../template_cache.h"
#include "../../barracks_flag_renderer.h"
#include "../../barracks_renderer_common.h"
#include "../../building_archetype_desc.h"
#include "../../building_ornaments.h"
#include "../../building_render_common.h"
#include "../../building_state.h"
#include "../../registry.h"
#include "math/math_utils.h"

namespace Render::GL::Carthage {
namespace {

using Render::Geom::clamp_vec_01;

struct CarthagePalette {
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
  QVector3D royal_purple{0.46F, 0.22F, 0.44F};
  QVector3D team{0.8F, 0.9F, 1.0F};
  QVector3D team_trim{0.48F, 0.54F, 0.60F};
};

inline auto make_palette(const QVector3D& team) -> CarthagePalette {
  CarthagePalette p;
  p.team = clamp_vec_01(team);
  p.team_trim =
      clamp_vec_01(QVector3D(team.x() * 0.6F, team.y() * 0.6F, team.z() * 0.6F));
  return p;
}

inline void draw_box(ISubmitter& out,
                     Mesh* unit,
                     Texture* white,
                     const QMatrix4x4& model,
                     const QVector3D& pos,
                     const QVector3D& size,
                     const QVector3D& color) {
  submit_building_box(out, unit, white, model, pos, size, color);
}

inline void draw_cyl(ISubmitter& out,
                     const QMatrix4x4& model,
                     const QVector3D& a,
                     const QVector3D& b,
                     float r,
                     const QVector3D& color,
                     Texture* white) {
  submit_building_cylinder(out, model, a, b, r, color, white);
}

void draw_fortress_base(const DrawContext& p,
                        ISubmitter& out,
                        Mesh* unit,
                        Texture* white,
                        const CarthagePalette& c,
                        bool detailed);
void draw_fortress_walls(const DrawContext& p,
                         ISubmitter& out,
                         Mesh* unit,
                         Texture* white,
                         const CarthagePalette& c,
                         BuildingState state,
                         bool detailed);
void draw_corner_towers(const DrawContext& p,
                        ISubmitter& out,
                        Mesh* unit,
                        Texture* white,
                        const CarthagePalette& c,
                        BuildingState state,
                        bool detailed);
void draw_courtyard(const DrawContext& p,
                    ISubmitter& out,
                    Mesh* unit,
                    Texture* white,
                    const CarthagePalette& c,
                    BuildingState state,
                    bool detailed);
void draw_carthage_roof(const DrawContext& p,
                        ISubmitter& out,
                        Mesh* unit,
                        Texture* white,
                        const CarthagePalette& c,
                        BuildingState state,
                        bool detailed);
void draw_gate(const DrawContext& p,
               ISubmitter& out,
               Mesh* unit,
               Texture* white,
               const CarthagePalette& c,
               BuildingState state,
               bool detailed);

auto build_barracks_archetype(BuildingState state,
                              Mesh* unit,
                              Texture* white) -> RenderArchetype {
  CarthagePalette const palette = make_palette(QVector3D(1.0F, 1.0F, 1.0F));
  auto record_variant = [&](TemplateRecorder& recorder, bool detailed) {
    DrawContext local_ctx;
    local_ctx.model = QMatrix4x4{};
    draw_fortress_base(local_ctx, recorder, unit, white, palette, detailed);
    draw_fortress_walls(local_ctx, recorder, unit, white, palette, state, detailed);
    draw_corner_towers(local_ctx, recorder, unit, white, palette, state, detailed);
    draw_courtyard(local_ctx, recorder, unit, white, palette, state, detailed);
    draw_carthage_roof(local_ctx, recorder, unit, white, palette, state, detailed);
    draw_gate(local_ctx, recorder, unit, white, palette, state, detailed);
  };

  TemplateRecorder full_recorder;
  TemplateRecorder minimal_recorder;
  full_recorder.reset(144);
  minimal_recorder.reset(72);
  record_variant(full_recorder, true);
  record_variant(minimal_recorder, false);
  return build_building_archetype_from_recorded_lods("carthage_barracks",
                                                     full_recorder.take_commands(),
                                                     minimal_recorder.take_commands(),
                                                     70.0F);
}

auto barracks_archetype(BuildingState state,
                        Mesh* unit,
                        Texture* white) -> const RenderArchetype& {
  static const BuildingArchetypeSet k_set =
      build_stateful_building_archetype_set([unit, white](BuildingState current_state) {
        return build_barracks_archetype(current_state, unit, white);
      });
  return k_set.for_state(state);
}

void draw_fortress_base(const DrawContext& p,
                        ISubmitter& out,
                        Mesh* unit,
                        Texture* white,
                        const CarthagePalette& c,
                        bool detailed) {

  // == FOUNDATION: Heavy stepped fortress base (Punic masonry signature) ==
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 0.05F, 0.0F),
           QVector3D(1.92F, 0.05F, 1.62F),
           c.stone_dark);
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 0.12F, 0.0F),
           QVector3D(1.84F, 0.04F, 1.54F),
           c.stone_base);
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 0.18F, 0.0F),
           QVector3D(1.78F, 0.04F, 1.48F),
           c.stone_light);

  // == ENTRANCE STAIRS: Stepped approach (fortress access) ==
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 0.06F, 1.68F),
           QVector3D(0.78F, 0.04F, 0.20F),
           c.stone_dark);
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 0.13F, 1.52F),
           QVector3D(0.64F, 0.04F, 0.16F),
           c.stone_base);
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 0.20F, 1.38F),
           QVector3D(0.52F, 0.04F, 0.14F),
           c.stone_light);

  if (!detailed) {
    return;
  }

  // == BATTERED BASE STONES: Visible coursework along perimeter ==
  for (float x = -1.7F; x <= 1.7F; x += 0.38F) {
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(x, 0.28F, -1.42F),
             QVector3D(0.16F, 0.06F, 0.10F),
             c.brick_dark);
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(x, 0.28F, 1.42F),
             QVector3D(0.16F, 0.06F, 0.10F),
             c.brick_dark);
  }
  for (float z = -1.3F; z <= 1.3F; z += 0.38F) {
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(-1.72F, 0.28F, z),
             QVector3D(0.10F, 0.06F, 0.16F),
             c.brick_dark);
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(1.72F, 0.28F, z),
             QVector3D(0.10F, 0.06F, 0.16F),
             c.brick_dark);
  }

  // == CORNER PILASTERS: Battered foundation supports ==
  for (float const px : {-1.72F, 1.72F}) {
    for (float const pz : {-1.42F, 1.42F}) {
      draw_box(out,
               unit,
               white,
               p.model,
               QVector3D(px, 0.22F, pz),
               QVector3D(0.14F, 0.12F, 0.14F),
               c.stone_dark);
    }
  }
}

void draw_fortress_walls(const DrawContext& p,
                         ISubmitter& out,
                         Mesh* unit,
                         Texture* white,
                         const CarthagePalette& c,
                         BuildingState state,
                         bool detailed) {
  float const wall_height = 1.3F;

  float height_multiplier = 1.0F;
  if (state == BuildingState::Damaged) {
    height_multiplier = 0.7F;
  } else if (state == BuildingState::Destroyed) {
    height_multiplier = 0.4F;
  }

  float const wall_cy = wall_height * 0.5F * height_multiplier + 0.26F;
  float const wall_hy = wall_height * 0.5F * height_multiplier;

  // == MAIN WALLS: Thick Punic brick ==
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, wall_cy, -1.32F),
           QVector3D(1.54F, wall_hy, 0.14F),
           c.stone_light);
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, wall_cy, 1.32F),
           QVector3D(1.54F, wall_hy, 0.14F),
           c.stone_light);
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(-1.62F, wall_cy, 0.0F),
           QVector3D(0.14F, wall_hy, 1.22F),
           c.stone_light);
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(1.62F, wall_cy, 0.0F),
           QVector3D(0.14F, wall_hy, 1.22F),
           c.stone_light);

  // == HORIZONTAL STONE COURSES (Punic masonry bands) ==
  if (state != BuildingState::Destroyed) {
    float const lower_band_y = wall_height * 0.25F * height_multiplier + 0.26F;
    float const upper_band_y = wall_height * 0.70F * height_multiplier + 0.26F;
    for (float const band_y : {lower_band_y, upper_band_y}) {
      draw_box(out,
               unit,
               white,
               p.model,
               QVector3D(0.0F, band_y, -1.33F),
               QVector3D(1.56F, 0.03F, 0.16F),
               c.stone_dark);
      draw_box(out,
               unit,
               white,
               p.model,
               QVector3D(0.0F, band_y, 1.33F),
               QVector3D(1.56F, 0.03F, 0.16F),
               c.stone_dark);
      draw_box(out,
               unit,
               white,
               p.model,
               QVector3D(-1.63F, band_y, 0.0F),
               QVector3D(0.16F, 0.03F, 1.24F),
               c.stone_dark);
      draw_box(out,
               unit,
               white,
               p.model,
               QVector3D(1.63F, band_y, 0.0F),
               QVector3D(0.16F, 0.03F, 1.24F),
               c.stone_dark);
    }
  }

  // == ARROW SLITS: Narrow defensive windows ==
  if (detailed && state != BuildingState::Destroyed) {
    float const slit_y = wall_height * 0.45F * height_multiplier + 0.26F;
    for (float const x : {-0.8F, 0.0F, 0.8F}) {
      draw_box(out,
               unit,
               white,
               p.model,
               QVector3D(x, slit_y, -1.34F),
               QVector3D(0.04F, 0.18F, 0.02F),
               c.wood_dark);
    }
    for (float const z : {-0.6F, 0.0F, 0.6F}) {
      draw_box(out,
               unit,
               white,
               p.model,
               QVector3D(-1.64F, slit_y, z),
               QVector3D(0.02F, 0.18F, 0.04F),
               c.wood_dark);
      draw_box(out,
               unit,
               white,
               p.model,
               QVector3D(1.64F, slit_y, z),
               QVector3D(0.02F, 0.18F, 0.04F),
               c.wood_dark);
    }
  }

  // == STEPPED MERLONS: Battlement crowns on all four sides ==
  if (state != BuildingState::Destroyed) {
    float const merlon_top = wall_height * height_multiplier + 0.32F;
    const int front_merlons = (state == BuildingState::Damaged) ? 5 : 7;
    add_merlon_strip_x(
        [&](const QVector3D& center, const QVector3D& size, const QVector3D& color) {
          draw_box(out, unit, white, p.model, center, size, color);
        },
        merlon_top,
        -1.28F,
        (state == BuildingState::Damaged) ? -0.80F : -1.22F,
        0.42F,
        front_merlons,
        QVector3D(0.16F, 0.06F, 0.06F),
        c.brick);
    if (detailed && state == BuildingState::Normal) {
      add_merlon_strip_x(
          [&](const QVector3D& center, const QVector3D& size, const QVector3D& color) {
            draw_box(out, unit, white, p.model, center, size, color);
          },
          merlon_top,
          1.28F,
          -1.22F,
          0.42F,
          7,
          QVector3D(0.16F, 0.06F, 0.06F),
          c.brick);
      add_merlon_strip_z(
          [&](const QVector3D& center, const QVector3D& size, const QVector3D& color) {
            draw_box(out, unit, white, p.model, center, size, color);
          },
          -1.58F,
          merlon_top,
          -1.0F,
          0.40F,
          6,
          QVector3D(0.06F, 0.06F, 0.16F),
          c.brick_dark);
      add_merlon_strip_z(
          [&](const QVector3D& center, const QVector3D& size, const QVector3D& color) {
            draw_box(out, unit, white, p.model, center, size, color);
          },
          1.58F,
          merlon_top,
          -1.0F,
          0.40F,
          6,
          QVector3D(0.06F, 0.06F, 0.16F),
          c.brick_dark);
    }
  }

  // == GATE PILASTERS: Flanking the front entrance ==
  if (detailed && state != BuildingState::Destroyed) {
    for (float const x : {-1.36F, 1.36F}) {
      draw_box(out,
               unit,
               white,
               p.model,
               QVector3D(x, 0.60F, 1.14F),
               QVector3D(0.14F, 0.38F, 0.22F),
               c.stone_dark);
      // Pilaster cap
      draw_box(out,
               unit,
               white,
               p.model,
               QVector3D(x, 1.02F, 1.14F),
               QVector3D(0.18F, 0.04F, 0.26F),
               c.brick_dark);
    }
  }
}

void draw_corner_towers(const DrawContext& p,
                        ISubmitter& out,
                        Mesh* unit,
                        Texture* white,
                        const CarthagePalette& c,
                        BuildingState state,
                        bool detailed) {
  QVector3D const corners[4] = {QVector3D(-1.52F, 0.0F, -1.22F),
                                QVector3D(1.52F, 0.0F, -1.22F),
                                QVector3D(-1.52F, 0.0F, 1.22F),
                                QVector3D(1.52F, 0.0F, 1.22F)};

  float height_multiplier = 1.0F;
  if (state == BuildingState::Damaged) {
    height_multiplier = 0.7F;
  } else if (state == BuildingState::Destroyed) {
    height_multiplier = 0.3F;
  }

  for (auto corner : corners) {
    // == MAIN TOWER SHAFT ==
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(corner.x(), 0.70F * height_multiplier, corner.z()),
             QVector3D(0.28F, 0.70F * height_multiplier, 0.28F),
             c.stone_dark);

    // == BATTERED BASE: Wider foundation for each tower ==
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(corner.x(), 0.10F, corner.z()),
             QVector3D(0.34F, 0.10F, 0.34F),
             c.brick_dark);

    if (state != BuildingState::Destroyed) {
      // == TOWER CAP: Stepped crown ==
      draw_box(out,
               unit,
               white,
               p.model,
               QVector3D(corner.x(), 1.48F * height_multiplier, corner.z()),
               QVector3D(0.32F, 0.10F, 0.32F),
               c.brick_dark);

      // == STONE BAND on tower ==
      draw_box(out,
               unit,
               white,
               p.model,
               QVector3D(corner.x(), 0.85F * height_multiplier, corner.z()),
               QVector3D(0.30F, 0.03F, 0.30F),
               c.stone_light);

      if (detailed) {
        // == MINI MERLONS on each corner tower ==
        for (int j = 0; j < 4; ++j) {
          float const angle = float(j) * 1.57F;
          float const ox = sinf(angle) * 0.22F;
          float const oz = cosf(angle) * 0.22F;
          draw_box(
              out,
              unit,
              white,
              p.model,
              QVector3D(corner.x() + ox, 1.66F * height_multiplier, corner.z() + oz),
              QVector3D(0.07F, 0.09F, 0.07F),
              c.brick);
        }
        // == CENTER FINIAL ==
        draw_box(out,
                 unit,
                 white,
                 p.model,
                 QVector3D(corner.x(), 1.78F * height_multiplier, corner.z()),
                 QVector3D(0.05F, 0.08F, 0.05F),
                 c.stone_light);
      }
    }
  }
}

void draw_courtyard(const DrawContext& p,
                    ISubmitter& out,
                    Mesh* unit,
                    Texture* white,
                    const CarthagePalette& c,
                    BuildingState state,
                    bool detailed) {

  // == TRAINING GROUND: Large paved courtyard ==
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 0.24F, 0.0F),
           QVector3D(1.3F, 0.02F, 1.0F),
           c.stone_base);

  // == DRILL MARKER: Central stone circle ==
  draw_cyl(out,
           p.model,
           QVector3D(0.0F, 0.24F, 0.0F),
           QVector3D(0.0F, 0.26F, 0.0F),
           0.35F,
           c.stone_dark,
           white);

  // == CENTRAL STANDARD: Tall command post ==
  draw_cyl(out,
           p.model,
           QVector3D(0.0F, 0.24F, 0.0F),
           QVector3D(0.0F, 1.10F, 0.0F),
           0.08F,
           c.stone_light,
           white);

  // == WEAPONS RACK (back wall) ==
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 0.68F, -0.92F),
           QVector3D(0.42F, 0.40F, 0.08F),
           c.brick);

  if (state != BuildingState::Destroyed) {
    // == PURPLE CLOTH BANNER: Cultural marker (Punic identity) ==
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(0.0F, 1.06F, 0.12F),
             QVector3D(0.42F, 0.06F, 0.28F),
             c.royal_purple);
    // Banner supports
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(-0.36F, 0.76F, 0.12F),
             QVector3D(0.02F, 0.30F, 0.02F),
             c.wood_dark);
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(0.36F, 0.76F, 0.12F),
             QVector3D(0.02F, 0.30F, 0.02F),
             c.wood_dark);

    // == SECOND PURPLE BANNER (side wall) ==
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(-1.52F, 0.80F, 0.0F),
             QVector3D(0.02F, 0.22F, 0.32F),
             c.royal_purple);

    if (detailed) {
      // == ORNAMENTAL CAP on standard ==
      draw_box(out,
               unit,
               white,
               p.model,
               QVector3D(0.0F, 1.18F, 0.05F),
               QVector3D(0.20F, 0.12F, 0.20F),
               c.stone_light);
      draw_box(out,
               unit,
               white,
               p.model,
               QVector3D(0.0F, 1.36F, 0.05F),
               QVector3D(0.10F, 0.08F, 0.10F),
               c.royal_purple);

      // == CERAMIC POTTERY: Training ground decoration ==
      draw_cyl(out,
               p.model,
               QVector3D(0.85F, 0.24F, 0.72F),
               QVector3D(0.85F, 0.44F, 0.72F),
               0.08F,
               c.tile_red,
               white);
      draw_cyl(out,
               p.model,
               QVector3D(-0.92F, 0.24F, 0.68F),
               QVector3D(-0.92F, 0.40F, 0.68F),
               0.07F,
               c.tile_dark,
               white);

      // == WEAPON DETAILS on rack ==
      for (float const dx : {-0.15F, 0.0F, 0.15F}) {
        draw_box(out,
                 unit,
                 white,
                 p.model,
                 QVector3D(dx, 0.72F, -0.94F),
                 QVector3D(0.02F, 0.32F, 0.02F),
                 c.iron);
      }
    }
  }
}

void draw_carthage_roof(const DrawContext& p,
                        ISubmitter& out,
                        Mesh* unit,
                        Texture* white,
                        const CarthagePalette& c,
                        BuildingState state,
                        bool detailed) {

  if (state == BuildingState::Destroyed) {
    return;
  }

  // == MAIN ROOF SLAB: Flat terracotta ==
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 1.62F, 0.0F),
           QVector3D(detailed ? 1.58F : 1.44F, 0.06F, detailed ? 1.28F : 1.16F),
           c.tile_red);

  // == TILE ROWS: Visible roofing texture ==
  add_tile_rows_z(
      [&](const QVector3D& center, const QVector3D& size, const QVector3D& color) {
        draw_box(out, unit, white, p.model, center, size, color);
      },
      1.68F,
      -1.0F,
      1.0F,
      0.28F,
      QVector3D(detailed ? 1.52F : 1.36F, 0.02F, 0.06F),
      c.tile_dark);

  if (detailed && state == BuildingState::Normal) {
    // == CENTRAL ROOF RIDGE: Heavy stone cap ==
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(0.0F, 1.78F, -0.12F),
             QVector3D(0.48F, 0.05F, 0.28F),
             c.stone_dark);

    // == ROOF WATCHTOWER: Small elevated observation post ==
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(0.0F, 1.86F, -0.12F),
             QVector3D(0.32F, 0.08F, 0.22F),
             c.stone_light);
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(0.0F, 1.98F, -0.12F),
             QVector3D(0.28F, 0.04F, 0.18F),
             c.brick_dark);
    // Watchtower merlons
    for (float const dx : {-0.14F, 0.0F, 0.14F}) {
      draw_box(out,
               unit,
               white,
               p.model,
               QVector3D(dx, 2.06F, -0.12F),
               QVector3D(0.06F, 0.05F, 0.06F),
               c.brick);
    }
  }
}

void draw_gate(const DrawContext& p,
               ISubmitter& out,
               Mesh* unit,
               Texture* white,
               const CarthagePalette& c,
               BuildingState state,
               bool detailed) {
  const float gate_half_height = (state == BuildingState::Destroyed) ? 0.34F
                                 : (state == BuildingState::Damaged) ? 0.52F
                                                                     : 0.66F;

  // == MAIN GATE: Heavy timber door ==
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, gate_half_height, 1.36F),
           QVector3D(0.54F, gate_half_height, 0.08F),
           c.wood_dark);

  // == IRON STRAPS: Reinforcement bands ==
  const int strap_count = detailed ? 4 : 2;
  for (int i = 0; i < strap_count; ++i) {
    float const y = 0.24F + float(i) * 0.26F;
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(0.0F, y, 1.38F),
             QVector3D(0.50F, 0.03F, 0.02F),
             c.iron);
  }

  if (state != BuildingState::Destroyed) {
    // == ARCH LINTEL: Stone archway over gate ==
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(0.0F, gate_half_height * 2.0F + 0.08F, 1.30F),
             QVector3D(0.64F, 0.06F, 0.14F),
             c.brick_dark);

    // == KEYSTONE: Central arch decoration ==
    if (detailed) {
      draw_box(out,
               unit,
               white,
               p.model,
               QVector3D(0.0F, gate_half_height * 2.0F + 0.16F, 1.32F),
               QVector3D(0.10F, 0.06F, 0.10F),
               c.stone_light);

      // == FRAME PILLARS: Flanking the gate ==
      for (float const gx : {-0.52F, 0.52F}) {
        draw_box(out,
                 unit,
                 white,
                 p.model,
                 QVector3D(gx, gate_half_height, 1.36F),
                 QVector3D(0.06F, gate_half_height, 0.06F),
                 c.stone_dark);
      }

      // == IRON STUDS: Door hardware ==
      for (float const sy : {0.35F, 0.65F, 0.95F}) {
        for (float const sx : {-0.24F, 0.24F}) {
          draw_box(out,
                   unit,
                   white,
                   p.model,
                   QVector3D(sx, sy, 1.40F),
                   QVector3D(0.03F, 0.03F, 0.02F),
                   c.iron);
        }
      }
    }
  }
}

void draw_standards(const DrawContext& p,
                    ISubmitter& out,
                    Mesh* unit,
                    Texture* white,
                    const CarthagePalette& c,
                    const BarracksFlagRenderer::ClothBannerResources* cloth) {
  BarracksFlagRenderer::draw_hanging_banner(
      p,
      out,
      unit,
      white,
      c.team,
      c.team_trim,
      {.pole_base = QVector3D(2.0F, 0.0F, -1.5F),
       .pole_height = 2.6F,
       .pole_radius = 0.045F,
       .banner_width = 0.8F,
       .banner_height = 0.5F,
       .pole_color = c.wood,
       .beam_color = c.wood,
       .connector_color = c.stone_light,
       .ornament_offset = QVector3D(0.0F, 2.8F, 0.0F),
       .ornament_size = QVector3D(0.12F, 0.10F, 0.12F),
       .ornament_color = c.iron,
       .ring_count = 3,
       .ring_y_start = 0.5F,
       .ring_spacing = 0.6F,
       .ring_height = 0.03F,
       .ring_radius_scale = 2.2F,
       .ring_color = c.iron},
      cloth);
}

void draw_rally_flag(const DrawContext& p,
                     ISubmitter& out,
                     Texture* white,
                     const CarthagePalette& c,
                     const BarracksFlagRenderer::ClothBannerResources* cloth) {
  BarracksFlagRenderer::FlagColors const colors{.team = c.team,
                                                .team_trim = c.team_trim,
                                                .timber = c.wood,
                                                .timber_light = c.stone_light,
                                                .wood_dark = c.wood_dark};
  BarracksFlagRenderer::draw_rally_flag_if_any(p, out, white, colors, cloth);
}

void draw_barracks_ornaments(const DrawContext& p,
                             ISubmitter& out,
                             Mesh* unit,
                             Texture* white,
                             const QVector3D& team,
                             const BarracksFlagRenderer::ClothBannerResources* cloth) {
  CarthagePalette const c = make_palette(team);
  draw_standards(p, out, unit, white, c, cloth);
  draw_rally_flag(p, out, white, c, cloth);
}

} // namespace

void register_barracks_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_barracks_renderer_variant(
      registry,
      BarracksRendererConfig{.nation_slug = "carthage",
                             .archetype = &barracks_archetype,
                             .draw_ornaments = &draw_barracks_ornaments,
                             .health_bar =
                                 BuildingHealthBarStyle{1.4F, 0.10F, 2.45F, true},
                             .selection = BuildingSelectionStyle{2.4F, 2.0F}});
}

} // namespace Render::GL::Carthage
