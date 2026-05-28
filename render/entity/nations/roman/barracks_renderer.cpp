#include "barracks_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>

#include "../../../../game/core/component.h"
#include "../../../../game/visuals/team_colors.h"
#include "../../../geom/flag.h"
#include "../../../geom/math_utils.h"
#include "../../../geom/transforms.h"
#include "../../../gl/backend.h"
#include "../../../gl/backend/banner_pipeline.h"
#include "../../../gl/primitives.h"
#include "../../../gl/resources.h"
#include "../../../render_archetype.h"
#include "../../../rig_dsl/defs/barracks_rig.h"
#include "../../../rig_dsl/rig_interpreter.h"
#include "../../../rig_dsl/static_resolver.h"
#include "../../../submitter.h"
#include "../../../template_cache.h"
#include "../../barracks_flag_renderer.h"
#include "../../building_archetype_desc.h"
#include "../../building_ornaments.h"
#include "../../building_render_common.h"
#include "../../building_state.h"
#include "../../registry.h"

namespace Render::GL::Roman {
namespace {

using Render::Geom::clamp_vec_01;

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
  QVector3D gold{0.85F, 0.72F, 0.35F};
  QVector3D team{0.8F, 0.9F, 1.0F};
  QVector3D team_trim{0.48F, 0.54F, 0.60F};
};

inline auto make_palette(const QVector3D& team) -> RomanPalette {
  RomanPalette p;
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

void draw_static_structure(const DrawContext& p, ISubmitter& out);
void draw_platform(const DrawContext& p,
                   ISubmitter& out,
                   Mesh* unit,
                   Texture* white,
                   const RomanPalette& c,
                   bool detailed);
void draw_podium(const DrawContext& p,
                 ISubmitter& out,
                 Mesh* unit,
                 Texture* white,
                 const RomanPalette& c,
                 BuildingState state);
void draw_colonnade(const DrawContext& p,
                    ISubmitter& out,
                    Mesh* unit,
                    Texture* white,
                    const RomanPalette& c,
                    BuildingState state,
                    bool detailed);
void draw_terrace(const DrawContext& p,
                  ISubmitter& out,
                  Mesh* unit,
                  Texture* white,
                  const RomanPalette& c,
                  BuildingState state,
                  bool detailed);
void draw_roofline(const DrawContext& p,
                   ISubmitter& out,
                   Mesh* unit,
                   Texture* white,
                   const RomanPalette& c,
                   BuildingState state,
                   bool detailed);

auto build_barracks_archetype(BuildingState state,
                              Mesh* unit,
                              Texture* white) -> RenderArchetype {
  RomanPalette const palette = make_palette(QVector3D(1.0F, 1.0F, 1.0F));
  auto record_variant = [&](TemplateRecorder& recorder, bool detailed) {
    DrawContext local_ctx;
    local_ctx.model = QMatrix4x4{};
    draw_static_structure(local_ctx, recorder);
    draw_podium(local_ctx, recorder, unit, white, palette, state);
    draw_platform(local_ctx, recorder, unit, white, palette, detailed);
    draw_colonnade(local_ctx, recorder, unit, white, palette, state, detailed);
    draw_terrace(local_ctx, recorder, unit, white, palette, state, detailed);
    draw_roofline(local_ctx, recorder, unit, white, palette, state, detailed);
  };

  TemplateRecorder full_recorder;
  TemplateRecorder minimal_recorder;
  full_recorder.reset(144);
  minimal_recorder.reset(72);
  record_variant(full_recorder, true);
  record_variant(minimal_recorder, false);
  return build_building_archetype_from_recorded_lods("roman_barracks",
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

void draw_platform(const DrawContext& p,
                   ISubmitter& out,
                   Mesh* unit,
                   Texture* white,
                   const RomanPalette& c,
                   bool detailed) {
  // == FOUNDATION: Triple-stepped Roman podium ==
  if (!detailed) {
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(0.0F, 0.04F, 0.0F),
             QVector3D(1.72F, 0.04F, 1.52F),
             c.limestone_dark);
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(0.0F, 0.10F, 0.0F),
             QVector3D(1.62F, 0.02F, 1.42F),
             c.limestone_shade);
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(0.0F, 0.14F, 0.0F),
             QVector3D(1.55F, 0.02F, 1.35F),
             c.limestone);
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(0.0F, 0.21F, 0.0F),
             QVector3D(1.48F, 0.015F, 1.28F),
             c.terracotta);
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(0.0F, 0.235F, 0.0F),
             QVector3D(1.38F, 0.012F, 1.18F),
             c.terracotta_dark);
    return;
  }

  // Detailed stepped podium
  draw_box(out, unit, white, p.model,
           QVector3D(0.0F, 0.04F, 0.0F),
           QVector3D(1.72F, 0.04F, 1.52F), c.limestone_dark);
  draw_box(out, unit, white, p.model,
           QVector3D(0.0F, 0.10F, 0.0F),
           QVector3D(1.62F, 0.02F, 1.42F), c.limestone_shade);
  draw_box(out, unit, white, p.model,
           QVector3D(0.0F, 0.14F, 0.0F),
           QVector3D(1.55F, 0.02F, 1.42F), c.limestone);

  // == MOSAIC FLOOR: Terracotta tile pattern ==
  for (float x = -1.4F; x <= 1.4F; x += 0.32F) {
    for (float z = -1.2F; z <= 1.2F; z += 0.32F) {
      if (fabsf(x) > 0.6F || fabsf(z) > 0.5F) {
        draw_box(out,
                 unit,
                 white,
                 p.model,
                 QVector3D(x, 0.21F, z),
                 QVector3D(0.14F, 0.01F, 0.14F),
                 c.terracotta);
      }
    }
  }

  // == CENTRAL IMPLUVIUM: Training ground water basin ==
  draw_box(out, unit, white, p.model,
           QVector3D(0.0F, 0.20F, 0.0F),
           QVector3D(0.52F, 0.015F, 0.42F), c.blue_light);
  // Basin rim
  draw_box(out, unit, white, p.model,
           QVector3D(0.0F, 0.22F, -0.44F),
           QVector3D(0.56F, 0.02F, 0.04F), c.marble);
  draw_box(out, unit, white, p.model,
           QVector3D(0.0F, 0.22F, 0.44F),
           QVector3D(0.56F, 0.02F, 0.04F), c.marble);
}

void draw_podium(const DrawContext& p,
                 ISubmitter& out,
                 Mesh* unit,
                 Texture* white,
                 const RomanPalette& c,
                 BuildingState state) {
  // == FRONT STEPS: Grand entrance stairway ==
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 0.04F, 1.68F),
           QVector3D(1.02F, 0.04F, 0.20F),
           c.limestone_dark);
  if (state != BuildingState::Destroyed) {
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(0.0F, 0.10F, 1.52F),
             QVector3D(0.88F, 0.04F, 0.16F),
             c.limestone_shade);
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(0.0F, 0.16F, 1.38F),
             QVector3D(0.72F, 0.04F, 0.14F),
             c.limestone);
    // Top step marble capping
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(0.0F, 0.22F, 1.24F),
             QVector3D(0.60F, 0.02F, 0.12F),
             c.marble);
  }
}

void draw_static_structure(const DrawContext& p, ISubmitter& out) {
  using namespace Render::RigDSL::Barracks;

  constexpr std::size_t k_anchor_count = static_cast<std::size_t>(Goods_Amp3Top) + 1U;
  Render::RigDSL::StaticAnchorResolver<k_anchor_count> anchors;

  auto set = [&](Render::RigDSL::AnchorId id, float x, float y, float z) {
    anchors.set(id, QVector3D(x, y, z));
  };

  set(Platform_BaseLow, -2.0F, 0.00F, -1.8F);
  set(Platform_BaseHigh, 2.0F, 0.16F, 1.8F);
  set(Platform_TopLow, -1.8F, 0.16F, -1.6F);
  set(Platform_TopHigh, 1.8F, 0.20F, 1.6F);

  set(Court_StoneLow, -1.3F, 0.21F, -1.1F);
  set(Court_StoneHigh, 1.3F, 0.23F, 1.1F);
  set(Court_PoolLow, -0.7F, 0.22F, -0.5F);
  set(Court_PoolHigh, 0.7F, 0.26F, 0.5F);
  set(Court_PoolTrimSLow, -0.72F, 0.23F, -0.54F);
  set(Court_PoolTrimSHigh, 0.72F, 0.27F, -0.50F);
  set(Court_PoolTrimNLow, -0.72F, 0.23F, 0.50F);
  set(Court_PoolTrimNHigh, 0.72F, 0.27F, 0.54F);
  set(Court_PillarBot, 0.0F, 0.25F, 0.0F);
  set(Court_PillarTop, 0.0F, 0.55F, 0.0F);
  set(Court_PillarCapLow, -0.08F, 0.55F, -0.08F);
  set(Court_PillarCapHigh, 0.08F, 0.61F, 0.08F);

  set(Wall_BackLow, -1.4F, 0.20F, -1.3F);
  set(Wall_BackHigh, 1.4F, 1.60F, -1.1F);
  set(Wall_LeftLow, -1.6F, 0.20F, -1.1F);
  set(Wall_LeftHigh, -1.4F, 1.60F, 0.1F);
  set(Wall_RightLow, 1.4F, 0.20F, -1.1F);
  set(Wall_RightHigh, 1.6F, 1.60F, 0.1F);

  set(Door_LDoorLow, -0.85F, 0.30F, -1.18F);
  set(Door_LDoorHigh, -0.35F, 1.00F, -1.12F);
  set(Door_LLintelLow, -0.85F, 0.93F, -1.18F);
  set(Door_LLintelHigh, -0.35F, 1.03F, -1.12F);
  set(Door_RDoorLow, 0.35F, 0.30F, -1.18F);
  set(Door_RDoorHigh, 0.85F, 1.00F, -1.12F);
  set(Door_RLintelLow, 0.35F, 0.93F, -1.18F);
  set(Door_RLintelHigh, 0.85F, 1.03F, -1.12F);

  set(Goods_Amp1Bot, -1.2F, 0.20F, 1.10F);
  set(Goods_Amp1Top, -1.2F, 0.50F, 1.10F);
  set(Goods_Amp2Bot, -0.9F, 0.20F, 1.15F);
  set(Goods_Amp2Top, -0.9F, 0.45F, 1.15F);
  set(Goods_Amp3Bot, 1.1F, 0.20F, -0.90F);
  set(Goods_Amp3Top, 1.1F, 0.42F, -0.90F);

  Render::RigDSL::InterpretContext ictx;
  ictx.model = p.model;
  ictx.anchors = &anchors;
  ictx.palette = nullptr;
  ictx.scalars = nullptr;
  ictx.material = nullptr;
  ictx.lod = 0;
  ictx.global_alpha = 1.0F;
  Render::RigDSL::render_rig(k_rig, ictx, out);
}

void draw_colonnade(const DrawContext& p,
                    ISubmitter& out,
                    Mesh* unit,
                    Texture* white,
                    const RomanPalette& c,
                    BuildingState state,
                    bool detailed) {
  float const col_height = 1.7F;
  float const col_radius = 0.10F;
  static constexpr float FRONT_COLUMN_SPACING_RANGE = 2.6F;
  static constexpr float SIDE_COLUMN_SPACING_RANGE = 2.1F;

  float height_multiplier = 1.0F;
  int num_columns = 7;
  if (state == BuildingState::Damaged) {
    height_multiplier = 0.7F;
    num_columns = 5;
  } else if (state == BuildingState::Destroyed) {
    height_multiplier = 0.4F;
    num_columns = 3;
  }
  if (!detailed) {
    num_columns = std::min(num_columns, 5);
  }

  // == FRONT COLONNADE: Classical columns with bases and Corinthian capitals ==
  for (int i = 0; i < num_columns; ++i) {
    float const x = -1.30F + float(i) * (num_columns > 1 ? FRONT_COLUMN_SPACING_RANGE /
                                                              (num_columns - 1)
                                                         : 0.0F);
    float const z = 1.42F;

    // Attic base
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(x, 0.22F, z),
             QVector3D(col_radius * 1.4F, 0.04F, col_radius * 1.4F),
             c.marble);
    // Plinth
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(x, 0.17F, z),
             QVector3D(col_radius * 1.6F, 0.03F, col_radius * 1.6F),
             c.limestone_shade);

    // Column shaft
    draw_cyl(out,
             p.model,
             QVector3D(x, 0.2F, z),
             QVector3D(x, 0.2F + col_height * height_multiplier, z),
             col_radius,
             c.limestone,
             white);

    // Corinthian capital
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(x, 0.2F + col_height * height_multiplier + 0.05F, z),
             QVector3D(col_radius * 1.6F, 0.08F, col_radius * 1.6F),
             c.marble);

    // Gold capital ornament (detailed only)
    if (state != BuildingState::Destroyed && detailed) {
      draw_box(out,
               unit,
               white,
               p.model,
               QVector3D(x, 0.2F + col_height * height_multiplier + 0.12F, z),
               QVector3D(col_radius * 1.3F, 0.03F, col_radius * 1.3F),
               c.gold);
    }
  }

  // == SIDE COLONNADES: Flanking wings ==
  int const side_columns = (state == BuildingState::Destroyed) ? 2 : (detailed ? 4 : 3);
  for (int i = 0; i < side_columns; ++i) {
    float const z = -1.0F + float(i) * (side_columns > 1 ? SIDE_COLUMN_SPACING_RANGE /
                                                              (side_columns - 1)
                                                         : 0.0F);

    for (float const x_side : {-1.62F, 1.62F}) {
      // Base
      draw_box(out,
               unit,
               white,
               p.model,
               QVector3D(x_side, 0.22F, z),
               QVector3D(col_radius * 1.4F, 0.04F, col_radius * 1.4F),
               c.marble);
      // Shaft
      draw_cyl(out,
               p.model,
               QVector3D(x_side, 0.2F, z),
               QVector3D(x_side, 0.2F + col_height * height_multiplier, z),
               col_radius,
               c.limestone,
               white);
      // Capital
      draw_box(out,
               unit,
               white,
               p.model,
               QVector3D(x_side, 0.2F + col_height * height_multiplier + 0.05F, z),
               QVector3D(col_radius * 1.6F, 0.08F, col_radius * 1.6F),
               c.marble);
    }
  }

  // == ENTABLATURE: Architrave beam connecting front columns ==
  if (state != BuildingState::Destroyed) {
    float const entab_y = 0.2F + col_height * height_multiplier + 0.16F;
    draw_box(out, unit, white, p.model,
             QVector3D(0.0F, entab_y, 1.42F),
             QVector3D(1.38F, 0.05F, 0.08F), c.limestone);
    // Frieze with blue accent
    if (detailed) {
      draw_box(out, unit, white, p.model,
               QVector3D(0.0F, entab_y + 0.06F, 1.42F),
               QVector3D(1.32F, 0.03F, 0.06F), c.blue_accent);
    }
    // Dentil course
    draw_box(out, unit, white, p.model,
             QVector3D(0.0F, entab_y - 0.04F, 1.42F),
             QVector3D(1.34F, 0.02F, 0.07F), c.marble);
  }
}

void draw_terrace(const DrawContext& p,
                  ISubmitter& out,
                  Mesh* unit,
                  Texture* white,
                  const RomanPalette& c,
                  BuildingState state,
                  bool detailed) {

  if (state == BuildingState::Destroyed) {
    return;
  }

  // == MAIN TERRACE SLAB: Marble entablature ==
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 2.12F, 0.0F),
           QVector3D(1.74F, 0.08F, 1.54F),
           c.marble);

  // == FRONT MOLDING: Gold accent trim ==
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 2.19F, 1.48F),
           QVector3D(1.68F, 0.04F, 0.05F),
           c.gold);

  // == SIDE MOLDINGS: Blue accent bands ==
  draw_box(out, unit, white, p.model,
           QVector3D(-1.68F, 2.19F, 0.0F),
           QVector3D(0.04F, 0.04F, 1.48F), c.blue_accent);
  draw_box(out, unit, white, p.model,
           QVector3D(1.68F, 2.19F, 0.0F),
           QVector3D(0.04F, 0.04F, 1.48F), c.blue_accent);

  // == TERRACOTTA FLOOR: Upper training level ==
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 2.24F, -0.2F),
           QVector3D(detailed ? 1.54F : 1.42F, 0.04F, detailed ? 1.04F : 0.96F),
           c.terracotta);

  // == BACK PEDIMENT: Triangular classical element ==
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 2.34F, -0.68F),
           QVector3D(1.48F, 0.06F, 0.05F),
           c.limestone);

  // == EAGLE ORNAMENTS: Corner finials (Roman signature) ==
  for (float const x : {-1.44F, 1.44F}) {
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(x, 2.42F, -0.68F),
             QVector3D(0.09F, 0.09F, 0.09F),
             c.gold);
  }

  if (detailed) {
    // == CENTRAL EAGLE: Grand gold ornament ==
    draw_box(out, unit, white, p.model,
             QVector3D(0.0F, 2.46F, -0.68F),
             QVector3D(0.14F, 0.12F, 0.04F), c.gold);
  }
}

void draw_roofline(const DrawContext& p,
                   ISubmitter& out,
                   Mesh* unit,
                   Texture* white,
                   const RomanPalette& c,
                   BuildingState state,
                   bool detailed) {
  if (state == BuildingState::Destroyed) {
    return;
  }

  // == FRONT CORNICE: Classical entablature profile ==
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, detailed ? 2.32F : 2.28F, 1.28F),
           QVector3D(detailed ? 1.24F : 1.02F, 0.04F, 0.14F),
           c.limestone_shade);

  // == TERRACOTTA TILE ROWS: Visible roof texture ==
  if (detailed) {
    add_tile_rows_z(
        [&](const QVector3D& center, const QVector3D& size, const QVector3D& color) {
          draw_box(out,
                   unit,
                   white,
                   p.model,
                   center + QVector3D(0.0F, 0.0F, 0.18F),
                   size,
                   color);
        },
        2.40F,
        -0.62F,
        0.58F,
        0.26F,
        QVector3D(1.38F, 0.025F, 0.05F),
        c.terracotta_dark);
  }

  // == RIDGE LINE: Central spine ==
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 2.48F, 1.14F),
           QVector3D(detailed ? 0.90F : 0.72F, 0.05F, 0.06F),
           c.terracotta);

  // == UPPER RIDGE ==
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, state == BuildingState::Damaged ? 2.54F : 2.58F, 1.02F),
           QVector3D(state == BuildingState::Damaged ? 0.50F : 0.62F, 0.05F, 0.05F),
           c.terracotta_dark);

  if (state == BuildingState::Normal) {
    // == BLUE ACCENT PEAK: Decorative color band ==
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(0.0F, 2.66F, 0.92F),
             QVector3D(detailed ? 0.34F : 0.26F, 0.05F, 0.04F),
             c.blue_accent);

    if (detailed) {
      // == GOLD ACROTERIA: Roof-edge ornaments ==
      draw_box(out, unit, white, p.model,
               QVector3D(-0.88F, 2.50F, 1.16F),
               QVector3D(0.06F, 0.08F, 0.06F), c.gold);
      draw_box(out, unit, white, p.model,
               QVector3D(0.88F, 2.50F, 1.16F),
               QVector3D(0.06F, 0.08F, 0.06F), c.gold);
      // Central eagle finial
      draw_box(out, unit, white, p.model,
               QVector3D(0.0F, 2.74F, 0.92F),
               QVector3D(0.08F, 0.10F, 0.04F), c.gold);
    }
  }
}

void draw_phoenician_banner(const DrawContext& p,
                            ISubmitter& out,
                            Mesh* unit,
                            Texture* white,
                            const RomanPalette& c,
                            const BarracksFlagRenderer::ClothBannerResources* cloth) {
  BarracksFlagRenderer::draw_hanging_banner(
      p,
      out,
      unit,
      white,
      c.team,
      c.team_trim,
      {.pole_base = QVector3D(0.0F, 0.0F, -2.0F),
       .pole_height = 3.0F,
       .pole_radius = 0.045F,
       .banner_width = 0.9F,
       .banner_height = 0.6F,
       .pole_color = c.cedar,
       .beam_color = c.cedar,
       .connector_color = c.limestone,
       .ornament_offset = QVector3D(0.25F, 3.15F, 0.03F),
       .ornament_size = QVector3D(0.35F, 0.03F, 0.015F),
       .ornament_color = c.gold,
       .ring_count = 4,
       .ring_y_start = 0.4F,
       .ring_spacing = 0.5F,
       .ring_height = 0.025F,
       .ring_radius_scale = 2.0F,
       .ring_color = c.gold},
      cloth);
}

void draw_rally_flag(const DrawContext& p,
                     ISubmitter& out,
                     Texture* white,
                     const RomanPalette& c,
                     const BarracksFlagRenderer::ClothBannerResources* cloth) {
  BarracksFlagRenderer::FlagColors const colors{.team = c.team,
                                                .team_trim = c.team_trim,
                                                .timber = c.cedar,
                                                .timber_light = c.limestone,
                                                .wood_dark = c.cedar_dark};
  BarracksFlagRenderer::draw_rally_flag_if_any(p, out, white, colors, cloth);
}

void draw_barracks(const DrawContext& p, ISubmitter& out) {
  if ((p.resources == nullptr) || (p.entity == nullptr)) {
    return;
  }

  auto* t = p.entity->get_component<Engine::Core::TransformComponent>();
  auto* r = p.entity->get_component<Engine::Core::RenderableComponent>();
  if ((t == nullptr) || (r == nullptr)) {
    return;
  }

  Mesh* unit = p.resources->unit();
  if (unit == nullptr) {
    unit = get_unit_cube();
  }
  Texture* white = p.resources->white();
  QVector3D const team(r->color[0], r->color[1], r->color[2]);
  RomanPalette const c = make_palette(team);

  BarracksFlagRenderer::ClothBannerResources cloth;
  if (p.backend != nullptr) {
    cloth.cloth_mesh = p.backend->banner_mesh();
    cloth.banner_shader = p.backend->banner_shader();
  }

  submit_building_instance(
      out, p, barracks_archetype(resolve_building_state(p), unit, white));
  draw_phoenician_banner(p, out, unit, white, c, &cloth);
  draw_rally_flag(p, out, white, c, &cloth);
  draw_building_health_bar(out, p, BuildingHealthBarStyle{1.4F, 0.10F, 2.75F, true});
  draw_building_selection_overlay(out, p, BuildingSelectionStyle{2.6F, 2.2F});
}

} // namespace

void register_barracks_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_building_renderer(registry, "roman", "barracks", draw_barracks);
}

} // namespace Render::GL::Roman
