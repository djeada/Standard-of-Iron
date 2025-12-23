#include "elephant_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/visuals/team_colors.h"
#include "../../../elephant/rig.h"
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

struct CarthageElephantPalette {
  QVector3D fabric_purple{0.45F, 0.18F, 0.55F};
  QVector3D fabric_gold{0.85F, 0.70F, 0.35F};
  QVector3D metal_bronze{0.70F, 0.50F, 0.28F};
  QVector3D metal_gold{0.85F, 0.72F, 0.40F};
  QVector3D wood_cedar{0.52F, 0.35F, 0.22F};
  QVector3D wood_dark{0.38F, 0.25F, 0.15F};
  QVector3D leather{0.48F, 0.35F, 0.22F};
  QVector3D rope{0.58F, 0.50F, 0.38F};
  QVector3D team{0.8F, 0.9F, 1.0F};
};

inline auto make_palette(const QVector3D &team) -> CarthageElephantPalette {
  CarthageElephantPalette p;
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
  out.mesh(get_unit_cylinder(), model * cylinder_between(a, b, r), color, white,
           1.0F);
}

class CarthageElephantRenderer : public ElephantRendererBase {
public:
  CarthageElephantRenderer() = default;

protected:
  void draw_howdah(const DrawContext &ctx, const AnimationInputs &anim,
                   ElephantProfile &profile, const HowdahAttachmentFrame &howdah,
                   float phase, float bob, const ElephantBodyFrames &body_frames,
                   ISubmitter &out) const override {

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
    if (ctx.entity != nullptr) {
      if (auto *r =
              ctx.entity->get_component<Engine::Core::RenderableComponent>()) {
        team_color = QVector3D(r->color[0], r->color[1], r->color[2]);
      }
    }

    auto palette = make_palette(team_color);
    const ElephantDimensions &d = profile.dims;

    QVector3D const howdah_center = howdah.howdah_center;

    // Howdah base platform
    draw_box(out, unit_cube, white_tex, ctx.model, howdah_center,
             QVector3D(d.howdah_width, d.howdah_height * 0.15F, d.howdah_length),
             palette.wood_cedar);

    // Howdah corner posts
    float const post_height = d.howdah_height * 0.7F;
    float const post_radius = 0.04F;
    for (int x = -1; x <= 1; x += 2) {
      for (int z = -1; z <= 1; z += 2) {
        float const x_off = static_cast<float>(x) * d.howdah_width * 0.45F;
        float const z_off = static_cast<float>(z) * d.howdah_length * 0.45F;
        QVector3D const post_base =
            howdah_center +
            QVector3D(x_off, d.howdah_height * 0.1F, z_off);
        QVector3D const post_top = post_base + QVector3D(0.0F, post_height, 0.0F);
        draw_cyl(out, ctx.model, post_base, post_top, post_radius,
                 palette.wood_dark, white_tex);

        // Decorative finial
        QMatrix4x4 finial = ctx.model;
        finial.translate(post_top + QVector3D(0.0F, post_radius * 1.5F, 0.0F));
        finial.scale(post_radius * 1.5F);
        out.mesh(get_unit_sphere(), finial, palette.metal_bronze, white_tex,
                 1.0F);
      }
    }

    // Howdah side rails
    float const rail_y = howdah_center.y() + d.howdah_height * 0.55F;
    for (int x = -1; x <= 1; x += 2) {
      float const x_off = static_cast<float>(x) * d.howdah_width * 0.45F;
      QVector3D const rail_front =
          QVector3D(x_off, rail_y, d.howdah_length * 0.45F);
      QVector3D const rail_back =
          QVector3D(x_off, rail_y, -d.howdah_length * 0.45F);
      draw_cyl(out, ctx.model, rail_front + howdah_center,
               rail_back + howdah_center, post_radius * 0.7F,
               palette.wood_cedar, white_tex);
    }

    // Front and back rails
    for (int z = -1; z <= 1; z += 2) {
      float const z_off = static_cast<float>(z) * d.howdah_length * 0.45F;
      QVector3D const rail_left =
          QVector3D(-d.howdah_width * 0.45F, rail_y, z_off);
      QVector3D const rail_right =
          QVector3D(d.howdah_width * 0.45F, rail_y, z_off);
      draw_cyl(out, ctx.model, rail_left + howdah_center,
               rail_right + howdah_center, post_radius * 0.7F,
               palette.wood_cedar, white_tex);
    }

    // Purple fabric draping
    for (int x = -1; x <= 1; x += 2) {
      float const x_off = static_cast<float>(x) * d.howdah_width * 0.48F;
      draw_box(out, unit_cube, white_tex, ctx.model,
               howdah_center + QVector3D(x_off, d.howdah_height * 0.3F, 0.0F),
               QVector3D(0.02F, d.howdah_height * 0.4F, d.howdah_length * 0.85F),
               palette.fabric_purple);
    }

    // Gold trim on fabric
    for (int x = -1; x <= 1; x += 2) {
      float const x_off = static_cast<float>(x) * d.howdah_width * 0.49F;
      draw_box(out, unit_cube, white_tex, ctx.model,
               howdah_center + QVector3D(x_off, d.howdah_height * 0.1F, 0.0F),
               QVector3D(0.015F, 0.03F, d.howdah_length * 0.88F),
               palette.fabric_gold);
      draw_box(out, unit_cube, white_tex, ctx.model,
               howdah_center + QVector3D(x_off, d.howdah_height * 0.5F, 0.0F),
               QVector3D(0.015F, 0.03F, d.howdah_length * 0.88F),
               palette.fabric_gold);
    }

    // Cushion/seat
    draw_box(out, unit_cube, white_tex, ctx.model,
             howdah_center + QVector3D(0.0F, d.howdah_height * 0.2F, 0.0F),
             QVector3D(d.howdah_width * 0.8F, 0.08F, d.howdah_length * 0.8F),
             palette.fabric_purple);

    // Bronze decorative shields on sides
    for (int z = -1; z <= 1; z += 2) {
      float const z_off = static_cast<float>(z) * d.howdah_length * 0.35F;
      for (int x = -1; x <= 1; x += 2) {
        float const x_off = static_cast<float>(x) * d.howdah_width * 0.47F;
        QMatrix4x4 shield = ctx.model;
        shield.translate(howdah_center +
                         QVector3D(x_off, d.howdah_height * 0.35F, z_off));
        shield.scale(0.12F, 0.12F, 0.02F);
        out.mesh(get_unit_sphere(), shield, palette.metal_bronze, white_tex,
                 1.0F);
      }
    }

    // Rope/strap securing howdah
    float const strap_radius = 0.025F;
    QVector3D const strap_front_left =
        howdah_center + QVector3D(-d.howdah_width * 0.5F, -d.howdah_height * 0.1F,
                                  d.howdah_length * 0.4F);
    QVector3D const strap_front_right =
        howdah_center + QVector3D(d.howdah_width * 0.5F, -d.howdah_height * 0.1F,
                                  d.howdah_length * 0.4F);
    QVector3D const strap_back_left =
        howdah_center + QVector3D(-d.howdah_width * 0.5F, -d.howdah_height * 0.1F,
                                  -d.howdah_length * 0.4F);
    QVector3D const strap_back_right =
        howdah_center + QVector3D(d.howdah_width * 0.5F, -d.howdah_height * 0.1F,
                                  -d.howdah_length * 0.4F);

    // Connect straps underneath
    QVector3D const under_center =
        howdah_center + QVector3D(0.0F, -d.howdah_height * 0.5F, 0.0F);
    draw_cyl(out, ctx.model, strap_front_left, under_center, strap_radius,
             palette.leather, white_tex);
    draw_cyl(out, ctx.model, strap_front_right, under_center, strap_radius,
             palette.leather, white_tex);
    draw_cyl(out, ctx.model, strap_back_left, under_center, strap_radius,
             palette.leather, white_tex);
    draw_cyl(out, ctx.model, strap_back_right, under_center, strap_radius,
             palette.leather, white_tex);
  }
};

} // namespace

void register_elephant_renderer(EntityRendererRegistry &registry) {
  registry.register_renderer(
      "troops/carthage/elephant", [](const DrawContext &p, ISubmitter &out) {
        static CarthageElephantRenderer const static_renderer;

        if (p.entity == nullptr) {
          return;
        }

        QVector3D team_color{0.4F, 0.2F, 0.6F};
        if (auto *r =
                p.entity->get_component<Engine::Core::RenderableComponent>()) {
          team_color = QVector3D(r->color[0], r->color[1], r->color[2]);
        }

        uint32_t seed = 0U;
        seed = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p.entity) &
                                     0xFFFFFFFFU);

        QVector3D const fabric_base(0.45F, 0.18F, 0.55F);
        QVector3D const metal_base(0.70F, 0.50F, 0.28F);

        ElephantProfile profile =
            get_or_create_cached_elephant_profile(seed, fabric_base, metal_base);

        AnimationInputs anim;
        anim.time = 0.0F;
        anim.is_moving = false;

        if (p.entity != nullptr) {
          if (auto *movement =
                  p.entity->get_component<Engine::Core::MovementComponent>()) {
            anim.is_moving = movement->has_target;
          }
        }

        static_renderer.render(p, anim, profile, nullptr, nullptr, out);
      });
}

} // namespace Render::GL::Carthage
