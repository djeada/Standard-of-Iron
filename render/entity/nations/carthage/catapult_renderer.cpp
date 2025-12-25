#include "catapult_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/visuals/team_colors.h"
#include "../../../geom/math_utils.h"
#include "../../../geom/transforms.h"
#include "../../../gl/primitives.h"
#include "../../../gl/resources.h"
#include "../../../scene_renderer.h"
#include "../../../submitter.h"
#include "../../registry.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>

namespace Render::GL::Carthage {
namespace {

using Render::Geom::clamp01;
using Render::Geom::clampVec01;
using Render::Geom::cylinder_between;

struct CarthageCatapultPalette {
  QVector3D wood_cedar{0.52F, 0.35F, 0.22F};
  QVector3D wood_dark{0.38F, 0.25F, 0.15F};
  QVector3D wood_light{0.60F, 0.45F, 0.30F};
  QVector3D metal_bronze{0.70F, 0.50F, 0.28F};
  QVector3D metal_iron{0.35F, 0.33F, 0.32F};
  QVector3D rope{0.58F, 0.50F, 0.38F};
  QVector3D leather{0.48F, 0.35F, 0.22F};
  QVector3D purple_trim{0.45F, 0.18F, 0.55F};
  QVector3D stone{0.55F, 0.52F, 0.48F};
  QVector3D team{0.8F, 0.9F, 1.0F};
};

enum class CatapultAnimState { Idle, Loading, Firing, Resetting };

struct CatapultAnimContext {
  CatapultAnimState state{CatapultAnimState::Idle};
  float loading_progress{0.0F};
  float firing_progress{0.0F};
  bool show_stone{false};
};

inline auto make_palette(const QVector3D &team) -> CarthageCatapultPalette {
  CarthageCatapultPalette p;
  p.team = clampVec01(team);
  return p;
}

inline auto
get_anim_context(const Engine::Core::Entity *entity) -> CatapultAnimContext {
  CatapultAnimContext ctx;
  if (entity == nullptr) {
    return ctx;
  }

  auto *loading =
      entity->get_component<Engine::Core::CatapultLoadingComponent>();
  if (loading == nullptr) {
    return ctx;
  }

  switch (loading->state) {
  case Engine::Core::CatapultLoadingComponent::LoadingState::Idle:
    ctx.state = CatapultAnimState::Idle;
    ctx.show_stone = false;
    break;
  case Engine::Core::CatapultLoadingComponent::LoadingState::Loading:
    ctx.state = CatapultAnimState::Loading;
    ctx.loading_progress = loading->get_loading_progress();
    ctx.show_stone = true;
    break;
  case Engine::Core::CatapultLoadingComponent::LoadingState::ReadyToFire:
    ctx.state = CatapultAnimState::Firing;
    ctx.loading_progress = 1.0F;
    ctx.firing_progress = 0.0F;
    ctx.show_stone = true;
    break;
  case Engine::Core::CatapultLoadingComponent::LoadingState::Firing:
    ctx.state = CatapultAnimState::Firing;
    ctx.firing_progress = loading->get_firing_progress();
    ctx.show_stone = ctx.firing_progress < 0.3F;
    break;
  }

  return ctx;
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

void draw_base_frame(const DrawContext &p, ISubmitter &out, Mesh *unit,
                     Texture *white, const CarthageCatapultPalette &c) {

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.24F, -0.38F),
           QVector3D(0.52F, 0.06F, 0.06F), c.wood_dark);
  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.24F, 0.38F),
           QVector3D(0.52F, 0.06F, 0.06F), c.wood_dark);

  draw_box(out, unit, white, p.model, QVector3D(-0.42F, 0.24F, 0.0F),
           QVector3D(0.06F, 0.06F, 0.42F), c.wood_cedar);
  draw_box(out, unit, white, p.model, QVector3D(0.42F, 0.24F, 0.0F),
           QVector3D(0.06F, 0.06F, 0.42F), c.wood_cedar);

  draw_box(out, unit, white, p.model, QVector3D(-0.42F, 0.28F, 0.0F),
           QVector3D(0.08F, 0.03F, 0.44F), c.metal_bronze);
  draw_box(out, unit, white, p.model, QVector3D(0.42F, 0.28F, 0.0F),
           QVector3D(0.08F, 0.03F, 0.44F), c.metal_bronze);

  draw_cyl(out, p.model, QVector3D(-0.38F, 0.22F, -0.32F),
           QVector3D(-0.38F, 0.22F, 0.32F), 0.028F, c.wood_dark, white);
  draw_cyl(out, p.model, QVector3D(0.38F, 0.22F, -0.32F),
           QVector3D(0.38F, 0.22F, 0.32F), 0.028F, c.wood_dark, white);
}

void draw_wheels(const DrawContext &p, ISubmitter &out, Mesh *unit,
                 Texture *white, const CarthageCatapultPalette &c) {

  float wheel_radius = 0.20F;
  float wheel_thickness = 0.045F;

  QVector3D left_front(-0.45F, wheel_radius, -0.28F);
  QVector3D left_back(-0.45F, wheel_radius, 0.28F);
  QVector3D right_front(0.45F, wheel_radius, -0.28F);
  QVector3D right_back(0.45F, wheel_radius, 0.28F);

  auto drawWheel = [&](const QVector3D &pos, float side_offset) {
    QVector3D inner = pos + QVector3D(side_offset * wheel_thickness, 0, 0);
    QVector3D outer =
        pos + QVector3D(side_offset * (wheel_thickness + 0.07F), 0, 0);

    draw_cyl(out, p.model, inner, outer, wheel_radius, c.wood_dark, white);

    draw_cyl(out, p.model, inner - QVector3D(side_offset * 0.005F, 0, 0),
             outer + QVector3D(side_offset * 0.005F, 0, 0),
             wheel_radius + 0.018F, c.metal_bronze, white);

    draw_cyl(out, p.model, inner - QVector3D(side_offset * 0.025F, 0, 0),
             outer + QVector3D(side_offset * 0.025F, 0, 0), 0.05F,
             c.metal_bronze, white);

    for (int s = 0; s < 6; ++s) {
      float angle = s * 3.14159F / 3.0F;
      float spoke_y = std::sin(angle) * wheel_radius * 0.75F;
      float spoke_z = std::cos(angle) * wheel_radius * 0.75F;
      QVector3D spoke_pos =
          pos +
          QVector3D(side_offset * (wheel_thickness + 0.035F), spoke_y, spoke_z);
      draw_cyl(out, p.model,
               pos + QVector3D(side_offset * (wheel_thickness + 0.035F), 0, 0),
               spoke_pos, 0.012F, c.wood_cedar, white);
    }
  };

  drawWheel(left_front, -1.0F);
  drawWheel(left_back, -1.0F);
  drawWheel(right_front, 1.0F);
  drawWheel(right_back, 1.0F);

  draw_cyl(out, p.model, QVector3D(-0.44F, wheel_radius, -0.28F),
           QVector3D(0.44F, wheel_radius, -0.28F), 0.028F, c.metal_iron, white);
  draw_cyl(out, p.model, QVector3D(-0.44F, wheel_radius, 0.28F),
           QVector3D(0.44F, wheel_radius, 0.28F), 0.028F, c.metal_iron, white);
}

void drawThrowingArm(const DrawContext &p, ISubmitter &out, Mesh *unit,
                     Texture *white, const CarthageCatapultPalette &c,
                     const CatapultAnimContext &anim_ctx) {

  draw_cyl(out, p.model, QVector3D(-0.30F, 0.22F, -0.10F),
           QVector3D(-0.20F, 0.70F, 0.05F), 0.055F, c.wood_cedar, white);
  draw_cyl(out, p.model, QVector3D(0.30F, 0.22F, -0.10F),
           QVector3D(0.20F, 0.70F, 0.05F), 0.055F, c.wood_cedar, white);

  draw_cyl(out, p.model, QVector3D(-0.22F, 0.68F, 0.03F),
           QVector3D(0.22F, 0.68F, 0.03F), 0.045F, c.wood_dark, white);

  draw_cyl(out, p.model, QVector3D(-0.08F, 0.65F, 0.03F),
           QVector3D(0.08F, 0.65F, 0.03F), 0.06F, c.metal_bronze, white);

  float arm_angle = 0.75F;

  switch (anim_ctx.state) {
  case CatapultAnimState::Idle:
    arm_angle = 0.75F;
    break;
  case CatapultAnimState::Loading:
    arm_angle = 0.75F + anim_ctx.loading_progress * 0.55F;
    break;
  case CatapultAnimState::Firing:
    arm_angle = 1.30F - anim_ctx.firing_progress * 1.9F;
    arm_angle = std::max(arm_angle, -0.35F);
    break;
  case CatapultAnimState::Resetting:
    arm_angle = 0.75F;
    break;
  }

  QMatrix4x4 armMatrix = p.model;
  armMatrix.translate(0.0F, 0.60F, 0.03F);
  armMatrix.rotate(arm_angle * 57.3F, 1.0F, 0.0F, 0.0F);

  draw_cyl(out, armMatrix, QVector3D(0.0F, 0.0F, -0.65F),
           QVector3D(0.0F, 0.0F, 0.35F), 0.05F, c.wood_cedar, white);

  draw_box(out, unit, white, armMatrix, QVector3D(0.0F, -0.06F, -0.60F),
           QVector3D(0.10F, 0.08F, 0.12F), c.leather);

  draw_box(out, unit, white, armMatrix, QVector3D(0.0F, 0.0F, 0.30F),
           QVector3D(0.08F, 0.08F, 0.08F), c.metal_bronze);

  if (anim_ctx.show_stone) {
    QMatrix4x4 stone_matrix = armMatrix;
    stone_matrix.translate(0.0F, 0.10F, -0.58F);
    float const stone_scale = 0.09F;
    stone_matrix.scale(stone_scale, stone_scale, stone_scale);

    out.mesh(get_unit_cube(), stone_matrix, c.stone, white, 1.0F);
  }
}

void drawTorsionMechanism(const DrawContext &p, ISubmitter &out, Mesh *unit,
                          Texture *white, const CarthageCatapultPalette &c) {

  draw_box(out, unit, white, p.model, QVector3D(-0.22F, 0.40F, 0.0F),
           QVector3D(0.05F, 0.20F, 0.18F), c.wood_dark);
  draw_box(out, unit, white, p.model, QVector3D(0.22F, 0.40F, 0.0F),
           QVector3D(0.05F, 0.20F, 0.18F), c.wood_dark);

  for (int i = 0; i < 4; ++i) {
    float offset = (float(i) - 1.5F) * 0.035F;
    draw_cyl(out, p.model, QVector3D(-0.15F, 0.28F + offset, -0.10F),
             QVector3D(-0.15F, 0.52F + offset, 0.10F), 0.028F, c.rope, white);
    draw_cyl(out, p.model, QVector3D(0.15F, 0.28F + offset, -0.10F),
             QVector3D(0.15F, 0.52F + offset, 0.10F), 0.028F, c.rope, white);
  }

  draw_cyl(out, p.model, QVector3D(-0.24F, 0.32F, 0.0F),
           QVector3D(-0.18F, 0.32F, 0.0F), 0.14F, c.metal_bronze, white);
  draw_cyl(out, p.model, QVector3D(0.18F, 0.32F, 0.0F),
           QVector3D(0.24F, 0.32F, 0.0F), 0.14F, c.metal_bronze, white);
}

void drawDecorations(const DrawContext &p, ISubmitter &out, Mesh *unit,
                     Texture *white, const CarthageCatapultPalette &c) {

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.72F, -0.12F),
           QVector3D(0.04F, 0.08F, 0.02F), c.metal_bronze);
  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.78F, -0.12F),
           QVector3D(0.06F, 0.02F, 0.02F), c.metal_bronze);

  draw_box(out, unit, white, p.model, QVector3D(-0.57F, 0.22F, -0.35F),
           QVector3D(0.05F, 0.05F, 0.05F), c.metal_bronze);
  draw_box(out, unit, white, p.model, QVector3D(0.57F, 0.22F, -0.35F),
           QVector3D(0.05F, 0.05F, 0.05F), c.metal_bronze);
  draw_box(out, unit, white, p.model, QVector3D(-0.57F, 0.22F, 0.35F),
           QVector3D(0.05F, 0.05F, 0.05F), c.metal_bronze);
  draw_box(out, unit, white, p.model, QVector3D(0.57F, 0.22F, 0.35F),
           QVector3D(0.05F, 0.05F, 0.05F), c.metal_bronze);
}

void drawWindlass(const DrawContext &p, ISubmitter &out, Mesh *unit,
                  Texture *white, const CarthageCatapultPalette &c) {

  draw_cyl(out, p.model, QVector3D(-0.22F, 0.25F, 0.35F),
           QVector3D(0.22F, 0.25F, 0.35F), 0.06F, c.wood_cedar, white);
  draw_cyl(out, p.model, QVector3D(-0.15F, 0.25F, 0.35F),
           QVector3D(0.15F, 0.25F, 0.35F), 0.07F, c.metal_bronze, white);

  draw_cyl(out, p.model, QVector3D(-0.28F, 0.25F, 0.35F),
           QVector3D(-0.28F, 0.38F, 0.35F), 0.025F, c.wood_dark, white);
  draw_cyl(out, p.model, QVector3D(0.28F, 0.25F, 0.35F),
           QVector3D(0.28F, 0.38F, 0.35F), 0.025F, c.wood_dark, white);

  draw_cyl(out, p.model, QVector3D(-0.12F, 0.25F, 0.35F),
           QVector3D(0.12F, 0.25F, 0.35F), 0.065F, c.rope, white);
}

} 

void register_catapult_renderer(EntityRendererRegistry &registry) {
  registry.register_renderer(
      "troops/carthage/catapult", [](const DrawContext &p, ISubmitter &out) {
        Mesh *unit_cube = get_unit_cube();
        Texture *white_tex = nullptr;

        if (auto *scene_renderer = dynamic_cast<Renderer *>(&out)) {
          unit_cube = scene_renderer->get_mesh_cube();
          white_tex = scene_renderer->get_white_texture();
        }

        if (unit_cube == nullptr || white_tex == nullptr) {
          return;
        }

        QVector3D team_color{0.4F, 0.2F, 0.6F};
        if (p.entity != nullptr) {
          if (auto *r =
                  p.entity
                      ->get_component<Engine::Core::RenderableComponent>()) {
            team_color = QVector3D(r->color[0], r->color[1], r->color[2]);
          }
        }

        auto palette = make_palette(team_color);
        auto anim_ctx = get_anim_context(p.entity);

        draw_base_frame(p, out, unit_cube, white_tex, palette);
        draw_wheels(p, out, unit_cube, white_tex, palette);
        drawTorsionMechanism(p, out, unit_cube, white_tex, palette);
        drawThrowingArm(p, out, unit_cube, white_tex, palette, anim_ctx);
        drawWindlass(p, out, unit_cube, white_tex, palette);
        drawDecorations(p, out, unit_cube, white_tex, palette);
      });
}

} 
