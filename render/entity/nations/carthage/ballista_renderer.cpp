#include "ballista_renderer.h"
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
using Render::Geom::cylinderBetween;

struct CarthageBallistaPalette {
  QVector3D wood_frame{0.50F, 0.35F, 0.20F};
  QVector3D wood_dark{0.35F, 0.25F, 0.15F};
  QVector3D wood_light{0.60F, 0.45F, 0.28F};
  QVector3D metal_iron{0.35F, 0.33F, 0.32F};
  QVector3D metal_bronze{0.75F, 0.55F, 0.28F};
  QVector3D metal_gold{0.85F, 0.70F, 0.30F};
  QVector3D rope{0.58F, 0.52F, 0.40F};
  QVector3D leather{0.45F, 0.32F, 0.22F};
  QVector3D purple_accent{0.45F, 0.20F, 0.50F};
  QVector3D team{0.8F, 0.9F, 1.0F};
};

inline auto make_palette(const QVector3D &team) -> CarthageBallistaPalette {
  CarthageBallistaPalette p;
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
  out.mesh(getUnitCylinder(), model * cylinderBetween(a, b, r), color, white,
           1.0F);
}

void drawBaseFrame(const DrawContext &p, ISubmitter &out, Mesh *unit,
                   Texture *white, const CarthageBallistaPalette &c) {

  draw_box(out, unit, white, p.model, QVector3D(-0.38F, 0.18F, 0.0F),
           QVector3D(0.06F, 0.12F, 0.28F), c.wood_frame);
  draw_box(out, unit, white, p.model, QVector3D(0.38F, 0.18F, 0.0F),
           QVector3D(0.06F, 0.12F, 0.28F), c.wood_frame);

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.25F, -0.26F),
           QVector3D(0.43F, 0.08F, 0.06F), c.wood_dark);

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.15F, 0.23F),
           QVector3D(0.43F, 0.06F, 0.06F), c.wood_frame);

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.11F, -0.30F),
           QVector3D(0.48F, 0.01F, 0.02F), c.metal_bronze);
}

void drawWheels(const DrawContext &p, ISubmitter &out, Mesh *unit,
                Texture *white, const CarthageBallistaPalette &c) {

  float wheel_radius = 0.13F;
  float wheel_thickness = 0.032F;

  QVector3D left_pos(-0.40F, wheel_radius, 0.0F);
  QVector3D right_pos(0.40F, wheel_radius, 0.0F);

  auto drawWheel = [&](const QVector3D &pos, float side_offset) {
    QVector3D inner = pos + QVector3D(side_offset * wheel_thickness, 0, 0);
    QVector3D outer =
        pos + QVector3D(side_offset * (wheel_thickness + 0.045F), 0, 0);

    draw_cyl(out, p.model, inner, outer, wheel_radius, c.wood_dark, white);

    draw_cyl(out, p.model, inner - QVector3D(side_offset * 0.004F, 0, 0),
             outer + QVector3D(side_offset * 0.004F, 0, 0),
             wheel_radius + 0.010F, c.metal_bronze, white);

    draw_cyl(out, p.model, inner - QVector3D(side_offset * 0.012F, 0, 0),
             outer + QVector3D(side_offset * 0.012F, 0, 0), 0.032F,
             c.metal_gold, white);

    for (int s = 0; s < 8; ++s) {
      float angle = s * 3.14159F / 4.0F;
      float spoke_y = std::sin(angle) * wheel_radius * 0.7F;
      float spoke_z = std::cos(angle) * wheel_radius * 0.7F;
      QVector3D spoke_pos =
          pos +
          QVector3D(side_offset * (wheel_thickness + 0.022F), spoke_y, spoke_z);
      draw_cyl(out, p.model,
               pos + QVector3D(side_offset * (wheel_thickness + 0.022F), 0, 0),
               spoke_pos, 0.010F, c.wood_frame, white);
    }
  };

  drawWheel(left_pos, -1.0F);
  drawWheel(right_pos, 1.0F);

  draw_cyl(out, p.model, QVector3D(-0.36F, wheel_radius, 0.0F),
           QVector3D(0.36F, wheel_radius, 0.0F), 0.020F, c.metal_bronze, white);
}

void drawTorsionBundles(const DrawContext &p, ISubmitter &out, Mesh *unit,
                        Texture *white, const CarthageBallistaPalette &c) {

  QMatrix4x4 tilted = p.model;
  tilted.rotate(30.0F, 1.0F, 0.0F, 0.0F);

  draw_cyl(out, tilted, QVector3D(-0.23F, 0.20F, -0.26F),
           QVector3D(-0.23F, 0.34F, -0.26F), 0.075F, c.rope, white);

  draw_cyl(out, tilted, QVector3D(0.23F, 0.20F, -0.26F),
           QVector3D(0.23F, 0.34F, -0.26F), 0.075F, c.rope, white);

  draw_cyl(out, tilted, QVector3D(-0.23F, 0.34F, -0.26F),
           QVector3D(-0.23F, 0.36F, -0.26F), 0.085F, c.metal_bronze, white);
  draw_cyl(out, tilted, QVector3D(0.23F, 0.34F, -0.26F),
           QVector3D(0.23F, 0.36F, -0.26F), 0.085F, c.metal_bronze, white);
  draw_cyl(out, tilted, QVector3D(-0.23F, 0.18F, -0.26F),
           QVector3D(-0.23F, 0.20F, -0.26F), 0.085F, c.metal_bronze, white);
  draw_cyl(out, tilted, QVector3D(0.23F, 0.18F, -0.26F),
           QVector3D(0.23F, 0.20F, -0.26F), 0.085F, c.metal_bronze, white);

  draw_cyl(out, tilted, QVector3D(-0.23F, 0.27F, -0.26F),
           QVector3D(-0.23F, 0.28F, -0.26F), 0.078F, c.metal_gold, white);
  draw_cyl(out, tilted, QVector3D(0.23F, 0.27F, -0.26F),
           QVector3D(0.23F, 0.28F, -0.26F), 0.078F, c.metal_gold, white);
}

void drawArms(const DrawContext &p, ISubmitter &out, Mesh *unit, Texture *white,
              const CarthageBallistaPalette &c) {

  QMatrix4x4 tilted = p.model;
  tilted.rotate(30.0F, 1.0F, 0.0F, 0.0F);

  draw_cyl(out, tilted, QVector3D(-0.23F, 0.27F, -0.26F),
           QVector3D(-0.43F, 0.31F, -0.08F), 0.023F, c.wood_frame, white);

  draw_cyl(out, tilted, QVector3D(0.23F, 0.27F, -0.26F),
           QVector3D(0.43F, 0.31F, -0.08F), 0.023F, c.wood_frame, white);

  QMatrix4x4 left_socket = tilted;
  left_socket.translate(QVector3D(-0.43F, 0.31F, -0.08F));
  left_socket.scale(0.022F);
  out.mesh(getUnitSphere(), left_socket, c.metal_bronze, white, 1.0F);

  QMatrix4x4 right_socket = tilted;
  right_socket.translate(QVector3D(0.43F, 0.31F, -0.08F));
  right_socket.scale(0.022F);
  out.mesh(getUnitSphere(), right_socket, c.metal_bronze, white, 1.0F);
}

void drawBowstring(const DrawContext &p, ISubmitter &out, Texture *white,
                   const CarthageBallistaPalette &c) {

  QMatrix4x4 tilted = p.model;
  tilted.rotate(30.0F, 1.0F, 0.0F, 0.0F);

  draw_cyl(out, tilted, QVector3D(-0.43F, 0.31F, -0.08F),
           QVector3D(0.0F, 0.29F, 0.14F), 0.007F, c.rope, white);
  draw_cyl(out, tilted, QVector3D(0.43F, 0.31F, -0.08F),
           QVector3D(0.0F, 0.29F, 0.14F), 0.007F, c.rope, white);
}

void drawSlide(const DrawContext &p, ISubmitter &out, Mesh *unit,
               Texture *white, const CarthageBallistaPalette &c) {

  QMatrix4x4 tilted = p.model;
  tilted.rotate(30.0F, 1.0F, 0.0F, 0.0F);

  draw_box(out, unit, white, tilted, QVector3D(0.0F, 0.21F, 0.0F),
           QVector3D(0.038F, 0.028F, 0.38F), c.wood_light);

  draw_box(out, unit, white, tilted, QVector3D(-0.032F, 0.23F, 0.0F),
           QVector3D(0.012F, 0.018F, 0.36F), c.metal_bronze);
  draw_box(out, unit, white, tilted, QVector3D(0.032F, 0.23F, 0.0F),
           QVector3D(0.012F, 0.018F, 0.36F), c.metal_bronze);

  draw_cyl(out, tilted, QVector3D(0.0F, 0.25F, -0.14F),
           QVector3D(0.0F, 0.25F, 0.18F), 0.014F, c.wood_dark, white);

  draw_cyl(out, tilted, QVector3D(0.0F, 0.25F, -0.23F),
           QVector3D(0.0F, 0.25F, -0.14F), 0.011F, c.metal_iron, white);
}

void drawTriggerMechanism(const DrawContext &p, ISubmitter &out, Mesh *unit,
                          Texture *white, const CarthageBallistaPalette &c) {

  QMatrix4x4 tilted = p.model;
  tilted.rotate(30.0F, 1.0F, 0.0F, 0.0F);

  draw_box(out, unit, white, tilted, QVector3D(0.0F, 0.17F, 0.28F),
           QVector3D(0.075F, 0.075F, 0.055F), c.metal_bronze);

  draw_cyl(out, tilted, QVector3D(0.0F, 0.14F, 0.30F),
           QVector3D(0.0F, 0.07F, 0.36F), 0.014F, c.leather, white);

  draw_cyl(out, tilted, QVector3D(-0.11F, 0.11F, 0.23F),
           QVector3D(-0.18F, 0.11F, 0.23F), 0.011F, c.wood_frame, white);
  draw_cyl(out, tilted, QVector3D(0.11F, 0.11F, 0.23F),
           QVector3D(0.18F, 0.11F, 0.23F), 0.011F, c.wood_frame, white);
}

void drawCarthageOrnaments(const DrawContext &p, ISubmitter &out, Mesh *unit,
                           Texture *white, const CarthageBallistaPalette &c) {

  QMatrix4x4 base = p.model;
  base.rotate(30.0F, 1.0F, 0.0F, 0.0F);

  QMatrix4x4 front_orb = base;
  front_orb.translate(QVector3D(0.0F, 0.32F, -0.30F));
  front_orb.scale(0.025F);
  out.mesh(getUnitSphere(), front_orb, c.metal_gold, white, 1.0F);

  QMatrix4x4 left_orb = base;
  left_orb.translate(QVector3D(-0.38F, 0.27F, -0.26F));
  left_orb.scale(0.018F);
  out.mesh(getUnitSphere(), left_orb, c.metal_bronze, white, 1.0F);

  QMatrix4x4 right_orb = base;
  right_orb.translate(QVector3D(0.38F, 0.27F, -0.26F));
  right_orb.scale(0.018F);
  out.mesh(getUnitSphere(), right_orb, c.metal_bronze, white, 1.0F);

  draw_box(out, unit, white, p.model, QVector3D(-0.38F, 0.22F, 0.0F),
           QVector3D(0.02F, 0.01F, 0.25F), c.purple_accent);
  draw_box(out, unit, white, p.model, QVector3D(0.38F, 0.22F, 0.0F),
           QVector3D(0.02F, 0.01F, 0.25F), c.purple_accent);
}

} // namespace

void register_ballista_renderer(EntityRendererRegistry &registry) {
  registry.register_renderer(
      "troops/carthage/ballista", [](const DrawContext &p, ISubmitter &out) {
        Mesh *unit = getUnitCube();
        Texture *white = nullptr;

        if (p.resources != nullptr) {
          unit = p.resources->unit();
          white = p.resources->white();
        }
        if (auto *scene_renderer = dynamic_cast<Renderer *>(&out)) {
          unit = scene_renderer->get_mesh_cube();
          white = scene_renderer->get_white_texture();
        }

        if (unit == nullptr || white == nullptr) {
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

        CarthageBallistaPalette c = make_palette(team_color);

        DrawContext ctx = p;
        ctx.model = p.model;
        ctx.model.rotate(180.0F, 0.0F, 1.0F, 0.0F);

        drawBaseFrame(ctx, out, unit, white, c);
        drawWheels(ctx, out, unit, white, c);
        drawTorsionBundles(ctx, out, unit, white, c);
        drawArms(ctx, out, unit, white, c);
        drawBowstring(ctx, out, white, c);
        drawSlide(ctx, out, unit, white, c);
        drawTriggerMechanism(ctx, out, unit, white, c);
        drawCarthageOrnaments(ctx, out, unit, white, c);
      });
}

} // namespace Render::GL::Carthage
