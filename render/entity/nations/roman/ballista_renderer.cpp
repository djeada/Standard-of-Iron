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

namespace Render::GL::Roman {
namespace {

using Render::Geom::clamp01;
using Render::Geom::clampVec01;
using Render::Geom::cylinderBetween;

struct RomanBallistaPalette {
  QVector3D wood_frame{0.45F, 0.32F, 0.18F};
  QVector3D wood_dark{0.32F, 0.22F, 0.12F};
  QVector3D wood_light{0.55F, 0.40F, 0.25F};
  QVector3D metal_iron{0.38F, 0.36F, 0.34F};
  QVector3D metal_bronze{0.72F, 0.52F, 0.30F};
  QVector3D rope{0.62F, 0.55F, 0.42F};
  QVector3D leather{0.42F, 0.30F, 0.20F};
  QVector3D team{0.8F, 0.9F, 1.0F};
};

inline auto make_palette(const QVector3D &team) -> RomanBallistaPalette {
  RomanBallistaPalette p;
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
                   Texture *white, const RomanBallistaPalette &c) {

  draw_box(out, unit, white, p.model, QVector3D(-0.40F, 0.18F, 0.0F),
           QVector3D(0.06F, 0.12F, 0.30F), c.wood_frame);
  draw_box(out, unit, white, p.model, QVector3D(0.40F, 0.18F, 0.0F),
           QVector3D(0.06F, 0.12F, 0.30F), c.wood_frame);

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.25F, -0.28F),
           QVector3D(0.45F, 0.08F, 0.06F), c.wood_dark);

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.15F, 0.25F),
           QVector3D(0.45F, 0.06F, 0.06F), c.wood_frame);
}

void drawWheels(const DrawContext &p, ISubmitter &out, Mesh *unit,
                Texture *white, const RomanBallistaPalette &c) {

  float wheel_radius = 0.14F;
  float wheel_thickness = 0.035F;

  QVector3D left_pos(-0.42F, wheel_radius, 0.0F);
  QVector3D right_pos(0.42F, wheel_radius, 0.0F);

  auto drawWheel = [&](const QVector3D &pos, float side_offset) {
    QVector3D inner = pos + QVector3D(side_offset * wheel_thickness, 0, 0);
    QVector3D outer =
        pos + QVector3D(side_offset * (wheel_thickness + 0.05F), 0, 0);

    draw_cyl(out, p.model, inner, outer, wheel_radius, c.wood_dark, white);

    draw_cyl(out, p.model, inner - QVector3D(side_offset * 0.004F, 0, 0),
             outer + QVector3D(side_offset * 0.004F, 0, 0),
             wheel_radius + 0.012F, c.metal_iron, white);

    draw_cyl(out, p.model, inner - QVector3D(side_offset * 0.015F, 0, 0),
             outer + QVector3D(side_offset * 0.015F, 0, 0), 0.035F,
             c.metal_iron, white);

    for (int s = 0; s < 6; ++s) {
      float angle = s * 3.14159F / 3.0F;
      float spoke_y = std::sin(angle) * wheel_radius * 0.7F;
      float spoke_z = std::cos(angle) * wheel_radius * 0.7F;
      QVector3D spoke_pos =
          pos +
          QVector3D(side_offset * (wheel_thickness + 0.025F), spoke_y, spoke_z);
      draw_cyl(out, p.model,
               pos + QVector3D(side_offset * (wheel_thickness + 0.025F), 0, 0),
               spoke_pos, 0.012F, c.wood_frame, white);
    }
  };

  drawWheel(left_pos, -1.0F);
  drawWheel(right_pos, 1.0F);

  draw_cyl(out, p.model, QVector3D(-0.38F, wheel_radius, 0.0F),
           QVector3D(0.38F, wheel_radius, 0.0F), 0.022F, c.metal_iron, white);
}

void drawTorsionBundles(const DrawContext &p, ISubmitter &out, Mesh *unit,
                        Texture *white, const RomanBallistaPalette &c) {

  QMatrix4x4 tilted = p.model;
  tilted.rotate(30.0F, 1.0F, 0.0F, 0.0F);

  draw_cyl(out, tilted, QVector3D(-0.25F, 0.20F, -0.28F),
           QVector3D(-0.25F, 0.35F, -0.28F), 0.08F, c.rope, white);

  draw_cyl(out, tilted, QVector3D(0.25F, 0.20F, -0.28F),
           QVector3D(0.25F, 0.35F, -0.28F), 0.08F, c.rope, white);

  draw_cyl(out, tilted, QVector3D(-0.25F, 0.35F, -0.28F),
           QVector3D(-0.25F, 0.37F, -0.28F), 0.09F, c.metal_bronze, white);
  draw_cyl(out, tilted, QVector3D(0.25F, 0.35F, -0.28F),
           QVector3D(0.25F, 0.37F, -0.28F), 0.09F, c.metal_bronze, white);
  draw_cyl(out, tilted, QVector3D(-0.25F, 0.18F, -0.28F),
           QVector3D(-0.25F, 0.20F, -0.28F), 0.09F, c.metal_bronze, white);
  draw_cyl(out, tilted, QVector3D(0.25F, 0.18F, -0.28F),
           QVector3D(0.25F, 0.20F, -0.28F), 0.09F, c.metal_bronze, white);
}

void drawArms(const DrawContext &p, ISubmitter &out, Mesh *unit, Texture *white,
              const RomanBallistaPalette &c) {

  QMatrix4x4 tilted = p.model;
  tilted.rotate(30.0F, 1.0F, 0.0F, 0.0F);

  draw_cyl(out, tilted, QVector3D(-0.25F, 0.28F, -0.28F),
           QVector3D(-0.45F, 0.32F, -0.10F), 0.025F, c.wood_frame, white);

  draw_cyl(out, tilted, QVector3D(0.25F, 0.28F, -0.28F),
           QVector3D(0.45F, 0.32F, -0.10F), 0.025F, c.wood_frame, white);

  QMatrix4x4 left_socket = tilted;
  left_socket.translate(QVector3D(-0.45F, 0.32F, -0.10F));
  left_socket.scale(0.025F);
  out.mesh(getUnitSphere(), left_socket, c.metal_bronze, white, 1.0F);

  QMatrix4x4 right_socket = tilted;
  right_socket.translate(QVector3D(0.45F, 0.32F, -0.10F));
  right_socket.scale(0.025F);
  out.mesh(getUnitSphere(), right_socket, c.metal_bronze, white, 1.0F);
}

void drawBowstring(const DrawContext &p, ISubmitter &out, Texture *white,
                   const RomanBallistaPalette &c) {

  QMatrix4x4 tilted = p.model;
  tilted.rotate(30.0F, 1.0F, 0.0F, 0.0F);

  draw_cyl(out, tilted, QVector3D(-0.45F, 0.32F, -0.10F),
           QVector3D(0.0F, 0.30F, 0.15F), 0.008F, c.rope, white);
  draw_cyl(out, tilted, QVector3D(0.45F, 0.32F, -0.10F),
           QVector3D(0.0F, 0.30F, 0.15F), 0.008F, c.rope, white);
}

void drawSlide(const DrawContext &p, ISubmitter &out, Mesh *unit,
               Texture *white, const RomanBallistaPalette &c) {

  QMatrix4x4 tilted = p.model;
  tilted.rotate(30.0F, 1.0F, 0.0F, 0.0F);

  draw_box(out, unit, white, tilted, QVector3D(0.0F, 0.22F, 0.0F),
           QVector3D(0.04F, 0.03F, 0.40F), c.wood_light);

  draw_box(out, unit, white, tilted, QVector3D(-0.035F, 0.24F, 0.0F),
           QVector3D(0.015F, 0.02F, 0.38F), c.metal_iron);
  draw_box(out, unit, white, tilted, QVector3D(0.035F, 0.24F, 0.0F),
           QVector3D(0.015F, 0.02F, 0.38F), c.metal_iron);

  draw_cyl(out, tilted, QVector3D(0.0F, 0.26F, -0.15F),
           QVector3D(0.0F, 0.26F, 0.20F), 0.015F, c.wood_dark, white);

  draw_cyl(out, tilted, QVector3D(0.0F, 0.26F, -0.25F),
           QVector3D(0.0F, 0.26F, -0.15F), 0.012F, c.metal_iron, white);
}

void drawTriggerMechanism(const DrawContext &p, ISubmitter &out, Mesh *unit,
                          Texture *white, const RomanBallistaPalette &c) {

  QMatrix4x4 tilted = p.model;
  tilted.rotate(30.0F, 1.0F, 0.0F, 0.0F);

  draw_box(out, unit, white, tilted, QVector3D(0.0F, 0.18F, 0.30F),
           QVector3D(0.08F, 0.08F, 0.06F), c.metal_iron);

  draw_cyl(out, tilted, QVector3D(0.0F, 0.15F, 0.32F),
           QVector3D(0.0F, 0.08F, 0.38F), 0.015F, c.wood_dark, white);

  draw_cyl(out, tilted, QVector3D(-0.12F, 0.12F, 0.25F),
           QVector3D(-0.20F, 0.12F, 0.25F), 0.012F, c.wood_frame, white);
  draw_cyl(out, tilted, QVector3D(0.12F, 0.12F, 0.25F),
           QVector3D(0.20F, 0.12F, 0.25F), 0.012F, c.wood_frame, white);
}

void drawRomanOrnaments(const DrawContext &p, ISubmitter &out, Mesh *unit,
                        Texture *white, const RomanBallistaPalette &c) {

  QMatrix4x4 base = p.model;
  base.rotate(30.0F, 1.0F, 0.0F, 0.0F);

  QMatrix4x4 left = base;
  left.translate(QVector3D(-0.40F, 0.28F, -0.28F));
  left.scale(0.02F);
  out.mesh(getUnitSphere(), left, c.metal_bronze, white, 1.0F);

  QMatrix4x4 right = base;
  right.translate(QVector3D(0.40F, 0.28F, -0.28F));
  right.scale(0.02F);
  out.mesh(getUnitSphere(), right, c.metal_bronze, white, 1.0F);

  draw_box(out, unit, white, p.model, QVector3D(0.0F, 0.30F, -0.32F),
           QVector3D(0.04F, 0.04F, 0.01F), c.metal_bronze);
}

} // namespace

void register_ballista_renderer(EntityRendererRegistry &registry) {
  registry.register_renderer(
      "troops/roman/ballista", [](const DrawContext &p, ISubmitter &out) {
        Mesh *unit = getUnitCube();
        Texture *white = nullptr;

        if (p.resources != nullptr) {
          unit = p.resources->unit();
          white = p.resources->white();
        }
        if (auto *scene_renderer = dynamic_cast<Renderer *>(&out)) {
          unit = scene_renderer->getMeshCube();
          white = scene_renderer->getWhiteTexture();
        }

        if (unit == nullptr || white == nullptr) {
          return;
        }

        QVector3D team_color{0.8F, 0.2F, 0.2F};
        if (p.entity != nullptr) {
          if (auto *r =
                  p.entity->get_component<Engine::Core::RenderableComponent>()) {
            team_color = QVector3D(r->color[0], r->color[1], r->color[2]);
          }
        }
        RomanBallistaPalette c = make_palette(team_color);

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
        drawRomanOrnaments(ctx, out, unit, white, c);
      });
}

} // namespace Render::GL::Roman
