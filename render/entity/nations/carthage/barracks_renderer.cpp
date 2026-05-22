#include "barracks_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>

#include "../../../../game/core/component.h"
#include "../../../../game/visuals/team_colors.h"
#include "../../../geom/flag.h"
#include "../../../geom/math_utils.h"
#include "../../../geom/transforms.h"
#include "../../../gl/backend.h"
#include "../../../gl/primitives.h"
#include "../../../gl/resources.h"
#include "../../../render_archetype.h"
#include "../../../submitter.h"
#include "../../../template_cache.h"
#include "../../barracks_flag_renderer.h"
#include "../../building_archetype_desc.h"
#include "../../building_ornaments.h"
#include "../../building_render_common.h"
#include "../../building_state.h"
#include "../../registry.h"

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

  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 0.15F, 0.0F),
           QVector3D(1.8F, 0.15F, 1.5F),
           c.stone_base);

  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 0.08F, 1.60F),
           QVector3D(0.72F, 0.04F, 0.18F),
           c.stone_dark);
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 0.16F, 1.40F),
           QVector3D(0.58F, 0.04F, 0.16F),
           c.stone_light);

  if (!detailed) {
    return;
  }

  for (float x = -1.6F; x <= 1.6F; x += 0.4F) {
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(x, 0.35F, -1.4F),
             QVector3D(0.18F, 0.08F, 0.08F),
             c.stone_dark);
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(x, 0.35F, 1.4F),
             QVector3D(0.18F, 0.08F, 0.08F),
             c.stone_dark);
  }
  for (float z = -1.3F; z <= 1.3F; z += 0.4F) {
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(-1.7F, 0.35F, z),
             QVector3D(0.08F, 0.08F, 0.18F),
             c.stone_dark);
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(1.7F, 0.35F, z),
             QVector3D(0.08F, 0.08F, 0.18F),
             c.stone_dark);
  }
}

void draw_fortress_walls(const DrawContext& p,
                         ISubmitter& out,
                         Mesh* unit,
                         Texture* white,
                         const CarthagePalette& c,
                         BuildingState state,
                         bool detailed) {
  float const wall_height = 1.2F;

  float height_multiplier = 1.0F;
  if (state == BuildingState::Damaged) {
    height_multiplier = 0.7F;
  } else if (state == BuildingState::Destroyed) {
    height_multiplier = 0.4F;
  }

  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, wall_height * 0.5F * height_multiplier + 0.3F, -1.3F),
           QVector3D(1.5F, wall_height * 0.5F * height_multiplier, 0.12F),
           c.stone_light);
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, wall_height * 0.5F * height_multiplier + 0.3F, 1.3F),
           QVector3D(1.5F, wall_height * 0.5F * height_multiplier, 0.12F),
           c.stone_light);
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(-1.6F, wall_height * 0.5F * height_multiplier + 0.3F, 0.0F),
           QVector3D(0.12F, wall_height * 0.5F * height_multiplier, 1.2F),
           c.stone_light);
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(1.6F, wall_height * 0.5F * height_multiplier + 0.3F, 0.0F),
           QVector3D(0.12F, wall_height * 0.5F * height_multiplier, 1.2F),
           c.stone_light);

  if (state != BuildingState::Destroyed) {
    const int front_merlons = (state == BuildingState::Damaged) ? 4 : 6;
    add_merlon_strip_x(
        [&](const QVector3D& center, const QVector3D& size, const QVector3D& color) {
          draw_box(out, unit, white, p.model, center, size, color);
        },
        wall_height * height_multiplier + 0.35F,
        -1.25F,
        (state == BuildingState::Damaged) ? -0.75F : -1.2F,
        0.5F,
        front_merlons,
        QVector3D(0.2F, 0.05F, 0.05F),
        c.brick);
    if (detailed && state == BuildingState::Normal) {
      add_merlon_strip_x(
          [&](const QVector3D& center, const QVector3D& size, const QVector3D& color) {
            draw_box(out, unit, white, p.model, center, size, color);
          },
          wall_height * height_multiplier + 0.35F,
          1.25F,
          -1.2F,
          0.5F,
          6,
          QVector3D(0.2F, 0.05F, 0.05F),
          c.brick);
      add_merlon_strip_z(
          [&](const QVector3D& center, const QVector3D& size, const QVector3D& color) {
            draw_box(out, unit, white, p.model, center, size, color);
          },
          -1.55F,
          wall_height * height_multiplier + 0.35F,
          -0.95F,
          0.48F,
          5,
          QVector3D(0.05F, 0.05F, 0.18F),
          c.brick_dark);
      add_merlon_strip_z(
          [&](const QVector3D& center, const QVector3D& size, const QVector3D& color) {
            draw_box(out, unit, white, p.model, center, size, color);
          },
          1.55F,
          wall_height * height_multiplier + 0.35F,
          -0.95F,
          0.48F,
          5,
          QVector3D(0.05F, 0.05F, 0.18F),
          c.brick_dark);
    }
  }

  if (detailed && state != BuildingState::Destroyed) {
    for (float const x : {-1.32F, 1.32F}) {
      draw_box(out,
               unit,
               white,
               p.model,
               QVector3D(x, 0.55F, 1.08F),
               QVector3D(0.12F, 0.32F, 0.20F),
               c.stone_dark);
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
  QVector3D const corners[4] = {QVector3D(-1.5F, 0.0F, -1.2F),
                                QVector3D(1.5F, 0.0F, -1.2F),
                                QVector3D(-1.5F, 0.0F, 1.2F),
                                QVector3D(1.5F, 0.0F, 1.2F)};

  float height_multiplier = 1.0F;
  if (state == BuildingState::Damaged) {
    height_multiplier = 0.7F;
  } else if (state == BuildingState::Destroyed) {
    height_multiplier = 0.3F;
  }

  for (auto corner : corners) {

    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(corner.x(), 0.65F * height_multiplier, corner.z()),
             QVector3D(0.25F, 0.65F * height_multiplier, 0.25F),
             c.stone_dark);

    if (state != BuildingState::Destroyed) {
      draw_box(out,
               unit,
               white,
               p.model,
               QVector3D(corner.x(), 1.45F * height_multiplier, corner.z()),
               QVector3D(0.28F, 0.15F, 0.28F),
               c.brick_dark);

      if (detailed) {
        for (int j = 0; j < 4; ++j) {
          float const angle = float(j) * 1.57F;
          float const ox = sinf(angle) * 0.18F;
          float const oz = cosf(angle) * 0.18F;
          draw_box(
              out,
              unit,
              white,
              p.model,
              QVector3D(corner.x() + ox, 1.68F * height_multiplier, corner.z() + oz),
              QVector3D(0.06F, 0.08F, 0.06F),
              c.stone_light);
        }
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

  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 0.32F, 0.0F),
           QVector3D(1.2F, 0.02F, 0.9F),
           c.stone_base);

  draw_cyl(out,
           p.model,
           QVector3D(0.0F, 0.3F, 0.0F),
           QVector3D(0.0F, 0.95F, 0.0F),
           0.08F,
           c.stone_light,
           white);

  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 0.65F, -0.85F),
           QVector3D(0.35F, 0.35F, 0.08F),
           c.brick);

  if (state != BuildingState::Destroyed) {
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(0.0F, 0.92F, 0.10F),
             QVector3D(0.36F, 0.05F, 0.24F),
             c.royal_purple);
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(-0.30F, 0.68F, 0.10F),
             QVector3D(0.02F, 0.24F, 0.02F),
             c.wood_dark);
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(0.30F, 0.68F, 0.10F),
             QVector3D(0.02F, 0.24F, 0.02F),
             c.wood_dark);
    if (detailed) {
      draw_box(out,
               unit,
               white,
               p.model,
               QVector3D(0.0F, 1.18F, 0.05F),
               QVector3D(0.18F, 0.10F, 0.18F),
               c.stone_light);
      draw_box(out,
               unit,
               white,
               p.model,
               QVector3D(0.0F, 1.34F, 0.05F),
               QVector3D(0.08F, 0.06F, 0.08F),
               c.royal_purple);
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

  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 1.58F, 0.0F),
           QVector3D(detailed ? 1.55F : 1.42F, 0.05F, detailed ? 1.25F : 1.14F),
           c.tile_red);
  add_tile_rows_z(
      [&](const QVector3D& center, const QVector3D& size, const QVector3D& color) {
        draw_box(out, unit, white, p.model, center, size, color);
      },
      1.62F,
      -1.0F,
      1.0F,
      0.3F,
      QVector3D(detailed ? 1.5F : 1.34F, 0.02F, 0.08F),
      c.tile_dark);
  if (detailed && state == BuildingState::Normal) {
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(0.0F, 1.76F, -0.10F),
             QVector3D(0.42F, 0.04F, 0.24F),
             c.stone_dark);
  }
}

void draw_gate(const DrawContext& p,
               ISubmitter& out,
               Mesh* unit,
               Texture* white,
               const CarthagePalette& c,
               BuildingState state,
               bool detailed) {
  const float gate_half_height = (state == BuildingState::Destroyed) ? 0.32F
                                 : (state == BuildingState::Damaged) ? 0.48F
                                                                     : 0.60F;

  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, gate_half_height, 1.35F),
           QVector3D(0.5F, gate_half_height, 0.08F),
           c.wood_dark);

  const int strap_count = detailed ? 3 : 2;
  for (int i = 0; i < strap_count; ++i) {
    float const y = 0.3F + float(i) * 0.3F;
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(0.0F, y, 1.37F),
             QVector3D(0.45F, 0.03F, 0.02F),
             c.iron);
  }
  if (state != BuildingState::Destroyed) {
    draw_box(out,
             unit,
             white,
             p.model,
             QVector3D(0.0F, gate_half_height * 2.0F + 0.10F, 1.28F),
             QVector3D(0.58F, 0.05F, 0.12F),
             c.brick_dark);
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
  CarthagePalette const c = make_palette(team);

  BarracksFlagRenderer::ClothBannerResources cloth;
  if (p.backend != nullptr) {
    cloth.cloth_mesh = p.backend->banner_mesh();
    cloth.banner_shader = p.backend->banner_shader();
  }

  submit_building_instance(
      out, p, barracks_archetype(resolve_building_state(p), unit, white));
  draw_standards(p, out, unit, white, c, &cloth);
  draw_rally_flag(p, out, white, c, &cloth);
  draw_building_health_bar(out, p, BuildingHealthBarStyle{1.4F, 0.10F, 2.45F, true});
  draw_building_selection_overlay(out, p, BuildingSelectionStyle{2.4F, 2.0F});
}

} // namespace

void register_barracks_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_building_renderer(registry, "carthage", "barracks", draw_barracks);
}

} // namespace Render::GL::Carthage
