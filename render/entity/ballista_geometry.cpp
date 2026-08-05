#include "ballista_geometry.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <cmath>
#include <numbers>

#include "../../game/core/component.h"
#include "../../game/visuals/team_colors.h"
#include "../geom/arrow.h"
#include "../geom/transforms.h"
#include "../gl/primitives.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include "../submitter.h"
#include "math/math_utils.h"
#include "nations/siege_anim_types.h"
#include "registry.h"
#include "siege_renderer_common.h"

namespace Render::GL {
namespace {

using Render::Geom::clamp01;
using Render::Geom::clamp_vec_01;
using Render::Geom::cylinder_between;

using Render::Geom::clamp01;
using Render::Geom::clamp_vec_01;
using Render::Geom::cylinder_between;

constexpr float k_stock_tilt_deg = 22.0F;
constexpr float k_nock_rest_z = -0.07F;
constexpr float k_slide_travel = 0.38F;

inline auto k_arm_tip(float side) -> QVector3D {
  return {side * 0.605F, 0.348F, -0.395F};
}

inline auto slide_travel(const BallistaAnimContext& anim_ctx) -> float {
  switch (anim_ctx.state) {
  case BallistaAnimState::Loading:
    return anim_ctx.loading_progress * k_slide_travel;
  case BallistaAnimState::Firing:
    return k_slide_travel * (1.0F - anim_ctx.firing_progress);
  case BallistaAnimState::Idle:
  case BallistaAnimState::Resetting:
    break;
  }
  return 0.0F;
}

inline auto
get_anim_context(const Engine::Core::Entity* entity) -> BallistaAnimContext {
  BallistaAnimContext ctx;
  if (entity == nullptr) {
    return ctx;
  }

  const auto* loading = entity->get_component<Engine::Core::CatapultLoadingComponent>();
  if (loading == nullptr) {
    return ctx;
  }

  switch (loading->state) {
  case Engine::Core::CatapultLoadingComponent::LoadingState::Idle:
    ctx.state = BallistaAnimState::Idle;
    ctx.show_bolt = false;
    break;
  case Engine::Core::CatapultLoadingComponent::LoadingState::Loading:
    ctx.state = BallistaAnimState::Loading;
    ctx.loading_progress = loading->get_loading_progress();
    ctx.show_bolt = true;
    break;
  case Engine::Core::CatapultLoadingComponent::LoadingState::ReadyToFire:
    ctx.state = BallistaAnimState::Firing;
    ctx.loading_progress = 1.0F;
    ctx.firing_progress = 0.0F;
    ctx.show_bolt = true;
    break;
  case Engine::Core::CatapultLoadingComponent::LoadingState::Firing:
    ctx.state = BallistaAnimState::Firing;
    ctx.firing_progress = loading->get_firing_progress();
    ctx.show_bolt = ctx.firing_progress < 0.2F;
    break;
  }

  return ctx;
}

inline void draw_box(ISubmitter& out,
                     Mesh* unit,
                     Texture* white,
                     const QMatrix4x4& model,
                     const QVector3D& pos,
                     const QVector3D& size,
                     const QVector3D& color) {
  QMatrix4x4 m = model;
  m.translate(pos);
  m.scale(size);
  out.mesh(unit, m, color, white, 1.0F);
}

inline void draw_cyl(ISubmitter& out,
                     const QMatrix4x4& model,
                     const QVector3D& a,
                     const QVector3D& b,
                     float r,
                     const QVector3D& color,
                     Texture* white) {
  out.mesh(get_unit_cylinder(), model * cylinder_between(a, b, r), color, white, 1.0F);
}

void draw_base_frame(const DrawContext& p,
                     ISubmitter& out,
                     Mesh* unit,
                     Texture* white,
                     const BallistaPalette& c) {

  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(-0.29F, 0.15F, -0.03F),
           QVector3D(0.045F, 0.045F, 0.42F),
           c.wood_frame);
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.29F, 0.15F, -0.03F),
           QVector3D(0.045F, 0.045F, 0.42F),
           c.wood_frame);

  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 0.15F, -0.40F),
           QVector3D(0.31F, 0.042F, 0.05F),
           c.wood_dark);
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 0.15F, 0.33F),
           QVector3D(0.31F, 0.042F, 0.05F),
           c.wood_dark);

  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 0.09F, 0.44F),
           QVector3D(0.05F, 0.035F, 0.10F),
           c.wood_dark);

  draw_cyl(out,
           p.model,
           QVector3D(-0.30F, 0.19F, -0.30F),
           QVector3D(-0.30F, 0.19F, 0.24F),
           0.012F,
           c.metal_iron,
           white);
  draw_cyl(out,
           p.model,
           QVector3D(0.30F, 0.19F, -0.30F),
           QVector3D(0.30F, 0.19F, 0.24F),
           0.012F,
           c.metal_iron,
           white);
}

void draw_wheels(const DrawContext& p,
                 ISubmitter& out,
                 Texture* white,
                 const BallistaPalette& c) {

  float const wheel_radius = 0.155F;
  float const wheel_half_thickness = 0.022F;

  auto draw_wheel = [&](float side) {
    QVector3D const hub(side * 0.355F, wheel_radius, 0.02F);
    QVector3D const inner = hub - QVector3D(side * wheel_half_thickness, 0, 0);
    QVector3D const outer = hub + QVector3D(side * wheel_half_thickness, 0, 0);

    draw_cyl(out, p.model, inner, outer, wheel_radius * 0.93F, c.wood_dark, white);
    draw_cyl(out,
             p.model,
             inner - QVector3D(side * 0.004F, 0, 0),
             outer + QVector3D(side * 0.004F, 0, 0),
             wheel_radius,
             c.metal_iron,
             white);
    draw_cyl(out,
             p.model,
             inner - QVector3D(side * 0.026F, 0, 0),
             outer + QVector3D(side * 0.026F, 0, 0),
             0.038F,
             c.accent,
             white);

    for (int spoke = 0; spoke < 6; ++spoke) {
      float const angle = static_cast<float>(spoke) * std::numbers::pi_v<float> / 3.0F;
      QVector3D const rim(hub.x() + side * (wheel_half_thickness + 0.012F),
                          hub.y() + std::sin(angle) * wheel_radius * 0.80F,
                          hub.z() + std::cos(angle) * wheel_radius * 0.80F);
      QVector3D const centre(
          hub.x() + side * (wheel_half_thickness + 0.012F), hub.y(), hub.z());
      draw_cyl(out, p.model, centre, rim, 0.013F, c.wood_light, white);
    }
  };

  draw_wheel(-1.0F);
  draw_wheel(1.0F);

  draw_cyl(out,
           p.model,
           QVector3D(-0.34F, 0.155F, 0.02F),
           QVector3D(0.34F, 0.155F, 0.02F),
           0.020F,
           c.metal_iron,
           white);
}

void draw_torsion_bundles(const DrawContext& p,
                          ISubmitter& out,
                          Mesh* unit,
                          Texture* white,
                          const BallistaPalette& c) {

  QMatrix4x4 tilted = p.model;
  tilted.rotate(k_stock_tilt_deg, 1.0F, 0.0F, 0.0F);

  auto draw_field_frame = [&](float side) {
    float const skein_x = side * 0.245F;

    draw_cyl(out,
             tilted,
             QVector3D(skein_x, 0.20F, -0.26F),
             QVector3D(skein_x, 0.50F, -0.26F),
             0.050F,
             c.rope,
             white);

    draw_cyl(out,
             tilted,
             QVector3D(skein_x, 0.49F, -0.26F),
             QVector3D(skein_x, 0.535F, -0.26F),
             0.062F,
             c.accent,
             white);
    draw_cyl(out,
             tilted,
             QVector3D(skein_x, 0.165F, -0.26F),
             QVector3D(skein_x, 0.21F, -0.26F),
             0.062F,
             c.accent,
             white);

    draw_box(out,
             unit,
             white,
             tilted,
             QVector3D(skein_x + side * 0.086F, 0.35F, -0.26F),
             QVector3D(0.020F, 0.195F, 0.036F),
             c.wood_frame);
    draw_box(out,
             unit,
             white,
             tilted,
             QVector3D(skein_x - side * 0.086F, 0.35F, -0.26F),
             QVector3D(0.020F, 0.195F, 0.036F),
             c.wood_frame);

    draw_box(out,
             unit,
             white,
             tilted,
             QVector3D(skein_x, 0.545F, -0.26F),
             QVector3D(0.105F, 0.020F, 0.036F),
             c.wood_dark);
    draw_box(out,
             unit,
             white,
             tilted,
             QVector3D(skein_x, 0.155F, -0.26F),
             QVector3D(0.105F, 0.020F, 0.036F),
             c.wood_dark);
  };

  draw_field_frame(-1.0F);
  draw_field_frame(1.0F);

  draw_box(out,
           unit,
           white,
           tilted,
           QVector3D(0.0F, 0.545F, -0.26F),
           QVector3D(0.155F, 0.018F, 0.030F),
           c.wood_dark);
  draw_box(out,
           unit,
           white,
           tilted,
           QVector3D(0.0F, 0.165F, -0.26F),
           QVector3D(0.155F, 0.018F, 0.030F),
           c.wood_dark);
}

void draw_arms(const DrawContext& p,
               ISubmitter& out,
               Texture* white,
               const BallistaPalette& c) {

  QMatrix4x4 tilted = p.model;
  tilted.rotate(k_stock_tilt_deg, 1.0F, 0.0F, 0.0F);

  auto draw_arm = [&](float side) {
    QVector3D const root(side * 0.245F, 0.355F, -0.255F);
    QVector3D const mid(side * 0.435F, 0.352F, -0.325F);
    QVector3D const tip = k_arm_tip(side);

    draw_cyl(out, tilted, root, mid, 0.032F, c.wood_frame, white);
    draw_cyl(out, tilted, mid, tip, 0.023F, c.wood_light, white);

    QMatrix4x4 collar = tilted;
    collar.translate(mid);
    collar.scale(0.028F);
    out.mesh(get_unit_sphere(), collar, c.metal_iron, white, 1.0F);

    QMatrix4x4 nock = tilted;
    nock.translate(tip);
    nock.scale(0.024F);
    out.mesh(get_unit_sphere(), nock, c.leather, white, 1.0F);
  };

  draw_arm(-1.0F);
  draw_arm(1.0F);
}

void draw_bowstring(const DrawContext& p,
                    ISubmitter& out,
                    Texture* white,
                    const BallistaPalette& c,
                    const BallistaAnimContext& anim_ctx) {

  QMatrix4x4 tilted = p.model;
  tilted.rotate(k_stock_tilt_deg, 1.0F, 0.0F, 0.0F);

  QVector3D const nock(0.0F, 0.315F, k_nock_rest_z + slide_travel(anim_ctx));

  draw_cyl(out, tilted, k_arm_tip(-1.0F), nock, 0.012F, c.rope, white);
  draw_cyl(out, tilted, k_arm_tip(1.0F), nock, 0.012F, c.rope, white);

  QMatrix4x4 grip = tilted;
  grip.translate(nock);
  grip.scale(0.026F, 0.026F, 0.020F);
  out.mesh(get_unit_sphere(), grip, c.leather, white, 1.0F);
}

inline void draw_loaded_bolt(ISubmitter& out,
                             Texture* white,
                             const QMatrix4x4& model,
                             const QVector3D& pos,
                             const BallistaPalette& c) {
  using Render::Geom::Arrow;
  constexpr float k_bolt_z_scale = 0.50F;
  constexpr float k_bolt_xy_scale = 0.90F;

  QMatrix4x4 bolt = model;
  bolt.translate(pos);
  bolt.rotate(180.0F, 0.0F, 1.0F, 0.0F);
  bolt.scale(k_bolt_xy_scale, k_bolt_xy_scale, k_bolt_z_scale);

  out.mesh(Arrow::get_shaft(), bolt, c.bolt, white, 1.0F);
  out.mesh(Arrow::get_tip(), bolt, Arrow::tip_color(0.88F), white, 1.0F);
  out.mesh(Arrow::get_fletching(), bolt, c.leather, white, 1.0F);
}

void draw_slide(const DrawContext& p,
                ISubmitter& out,
                Mesh* unit,
                Texture* white,
                const BallistaPalette& c,
                const BallistaAnimContext& anim_ctx) {

  QMatrix4x4 tilted = p.model;
  tilted.rotate(k_stock_tilt_deg, 1.0F, 0.0F, 0.0F);

  draw_box(out,
           unit,
           white,
           tilted,
           QVector3D(0.0F, 0.255F, -0.03F),
           QVector3D(0.055F, 0.032F, 0.44F),
           c.wood_light);

  draw_box(out,
           unit,
           white,
           tilted,
           QVector3D(-0.052F, 0.295F, -0.03F),
           QVector3D(0.013F, 0.022F, 0.43F),
           c.wood_frame);
  draw_box(out,
           unit,
           white,
           tilted,
           QVector3D(0.052F, 0.295F, -0.03F),
           QVector3D(0.013F, 0.022F, 0.43F),
           c.wood_frame);

  draw_cyl(out,
           tilted,
           QVector3D(-0.052F, 0.312F, -0.46F),
           QVector3D(-0.052F, 0.312F, 0.39F),
           0.008F,
           c.metal_iron,
           white);
  draw_cyl(out,
           tilted,
           QVector3D(0.052F, 0.312F, -0.46F),
           QVector3D(0.052F, 0.312F, 0.39F),
           0.008F,
           c.metal_iron,
           white);

  draw_box(out,
           unit,
           white,
           tilted,
           QVector3D(0.0F, 0.268F, -0.455F),
           QVector3D(0.068F, 0.044F, 0.030F),
           c.wood_dark);

  draw_box(out,
           unit,
           white,
           tilted,
           QVector3D(0.0F, 0.205F, 0.12F),
           QVector3D(0.048F, 0.055F, 0.09F),
           c.wood_dark);

  if (anim_ctx.show_bolt) {
    draw_loaded_bolt(out,
                     white,
                     tilted,
                     QVector3D(0.0F, 0.305F, k_nock_rest_z + slide_travel(anim_ctx)),
                     c);
  }
}

void draw_trigger_mechanism(const DrawContext& p,
                            ISubmitter& out,
                            Mesh* unit,
                            Texture* white,
                            const BallistaPalette& c,
                            const BallistaAnimContext& anim_ctx) {

  QMatrix4x4 tilted = p.model;
  tilted.rotate(k_stock_tilt_deg, 1.0F, 0.0F, 0.0F);

  float const carriage_z = k_nock_rest_z + slide_travel(anim_ctx);

  draw_box(out,
           unit,
           white,
           tilted,
           QVector3D(0.0F, 0.30F, carriage_z + 0.06F),
           QVector3D(0.062F, 0.030F, 0.055F),
           c.metal_iron);

  draw_box(out,
           unit,
           white,
           tilted,
           QVector3D(0.0F, 0.27F, 0.36F),
           QVector3D(0.072F, 0.058F, 0.055F),
           c.metal_iron);

  draw_cyl(out,
           tilted,
           QVector3D(-0.10F, 0.27F, 0.36F),
           QVector3D(0.10F, 0.27F, 0.36F),
           0.030F,
           c.wood_dark,
           white);

  draw_cyl(out,
           tilted,
           QVector3D(-0.10F, 0.27F, 0.36F),
           QVector3D(-0.10F, 0.36F, 0.36F),
           0.014F,
           c.wood_frame,
           white);
  draw_cyl(out,
           tilted,
           QVector3D(0.10F, 0.27F, 0.36F),
           QVector3D(0.10F, 0.18F, 0.36F),
           0.014F,
           c.wood_frame,
           white);

  draw_cyl(out,
           tilted,
           QVector3D(0.0F, 0.27F, 0.36F),
           QVector3D(0.0F, 0.30F, carriage_z + 0.06F),
           0.007F,
           c.rope,
           white);
}

void draw_ornaments(const DrawContext& p,
                    ISubmitter& out,
                    Mesh* unit,
                    Texture* white,
                    const BallistaPalette& c) {

  QMatrix4x4 tilted = p.model;
  tilted.rotate(k_stock_tilt_deg, 1.0F, 0.0F, 0.0F);

  QMatrix4x4 left = tilted;
  left.translate(QVector3D(-0.245F, 0.575F, -0.26F));
  left.scale(0.024F);
  out.mesh(get_unit_sphere(), left, c.accent, white, 1.0F);

  QMatrix4x4 right = tilted;
  right.translate(QVector3D(0.245F, 0.575F, -0.26F));
  right.scale(0.024F);
  out.mesh(get_unit_sphere(), right, c.accent, white, 1.0F);

  draw_box(out,
           unit,
           white,
           tilted,
           QVector3D(0.0F, 0.585F, -0.26F),
           QVector3D(0.035F, 0.045F, 0.010F),
           c.accent);

  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 0.115F, 0.30F),
           QVector3D(0.10F, 0.030F, 0.06F),
           c.leather);
}

} // namespace

void draw_ballista_geometry(const DrawContext& p,
                            ISubmitter& out,
                            Mesh* unit,
                            Texture* white,
                            const QVector3D& team_color,
                            const BallistaPalette& palette) {
  BallistaPalette c = palette;
  c.team = clamp_vec_01(team_color);
  auto anim_ctx = get_anim_context(p.entity);

  DrawContext ctx = p;
  ctx.model = p.model;
  ctx.model.rotate(180.0F, 0.0F, 1.0F, 0.0F);

  draw_base_frame(ctx, out, unit, white, c);
  draw_wheels(ctx, out, white, c);
  draw_torsion_bundles(ctx, out, unit, white, c);
  draw_arms(ctx, out, white, c);
  draw_bowstring(ctx, out, white, c, anim_ctx);
  draw_slide(ctx, out, unit, white, c, anim_ctx);
  draw_trigger_mechanism(ctx, out, unit, white, c, anim_ctx);
  draw_ornaments(ctx, out, unit, white, c);
}

} // namespace Render::GL
