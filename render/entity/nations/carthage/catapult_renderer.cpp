#include "catapult_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>

#include <cmath>
#include <numbers>

#include "game/core/component.h"
#include "game/systems/projectile_kind.h"
#include "game/visuals/team_colors.h"
#include "math/math_utils.h"
#include "render/entity/nations/siege_anim_types.h"
#include "render/entity/registry.h"
#include "render/entity/siege_renderer_common.h"
#include "render/geom/stone.h"
#include "render/geom/transforms.h"
#include "render/gl/primitives.h"
#include "render/gl/resources.h"
#include "render/scene_renderer.h"
#include "render/submitter.h"

namespace Render::GL::Carthage {
namespace {

using Render::Geom::clamp01;
using Render::Geom::clamp_vec_01;
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

inline auto make_palette(const QVector3D& team) -> CarthageCatapultPalette {
  CarthageCatapultPalette p;
  p.team = clamp_vec_01(team);
  return p;
}

constexpr float k_rad_to_deg = 180.0F / std::numbers::pi_v<float>;
constexpr float k_pivot_y = 0.345F;
constexpr float k_pivot_z = 0.10F;
constexpr float k_arm_length = 0.72F;
constexpr float k_arm_rest_rad = 1.05F;
constexpr float k_arm_cocked_rad = 2.85F;

inline auto k_buffer_end(float side) -> QVector3D {
  return {side * 0.245F, 0.79F, -0.20F};
}

inline auto arm_swing_rad(const CatapultAnimContext& anim_ctx) -> float {
  switch (anim_ctx.state) {
  case CatapultAnimState::Loading:
    return k_arm_rest_rad +
           ((k_arm_cocked_rad - k_arm_rest_rad) * anim_ctx.loading_progress);
  case CatapultAnimState::Firing:
    return k_arm_cocked_rad - ((k_arm_cocked_rad - k_arm_rest_rad) *
                               std::min(1.0F, anim_ctx.firing_progress * 1.35F));
  case CatapultAnimState::Idle:
  case CatapultAnimState::Resetting:
    break;
  }
  return k_arm_rest_rad;
}

inline auto
get_anim_context(const Engine::Core::Entity* entity) -> CatapultAnimContext {
  CatapultAnimContext ctx;
  if (entity == nullptr) {
    return ctx;
  }

  const auto* loading = entity->get_component<Engine::Core::CatapultLoadingComponent>();
  if (loading == nullptr) {
    return ctx;
  }

  ctx.incendiary_round =
      Game::Systems::is_incendiary_projectile_kind(loading->loaded_projectile_kind);

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
                     const CarthageCatapultPalette& c) {

  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 0.24F, -0.38F),
           QVector3D(0.52F, 0.06F, 0.06F),
           c.wood_dark);
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 0.24F, 0.38F),
           QVector3D(0.52F, 0.06F, 0.06F),
           c.wood_dark);

  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(-0.42F, 0.24F, 0.0F),
           QVector3D(0.06F, 0.06F, 0.42F),
           c.wood_cedar);
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.42F, 0.24F, 0.0F),
           QVector3D(0.06F, 0.06F, 0.42F),
           c.wood_cedar);

  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(-0.42F, 0.28F, 0.0F),
           QVector3D(0.08F, 0.03F, 0.44F),
           c.metal_bronze);
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.42F, 0.28F, 0.0F),
           QVector3D(0.08F, 0.03F, 0.44F),
           c.metal_bronze);

  draw_cyl(out,
           p.model,
           QVector3D(-0.38F, 0.22F, -0.32F),
           QVector3D(-0.38F, 0.22F, 0.32F),
           0.028F,
           c.wood_dark,
           white);
  draw_cyl(out,
           p.model,
           QVector3D(0.38F, 0.22F, -0.32F),
           QVector3D(0.38F, 0.22F, 0.32F),
           0.028F,
           c.wood_dark,
           white);
}

void draw_wheels(const DrawContext& p,
                 ISubmitter& out,
                 Texture* white,
                 const CarthageCatapultPalette& c) {

  float const wheel_radius = 0.18F;

  auto draw_wheel = [&](const QVector3D& hub, float side) {
    QVector3D const wood_inner = hub - QVector3D(side * 0.030F, 0, 0);
    QVector3D const wood_outer = hub + QVector3D(side * 0.058F, 0, 0);
    QVector3D const tyre_inner = hub + QVector3D(side * 0.002F, 0, 0);
    QVector3D const tyre_outer = hub + QVector3D(side * 0.030F, 0, 0);

    draw_cyl(
        out, p.model, wood_inner, wood_outer, wheel_radius * 0.90F, c.wood_dark, white);
    draw_cyl(out, p.model, tyre_inner, tyre_outer, wheel_radius, c.metal_iron, white);
    draw_cyl(out,
             p.model,
             wood_inner - QVector3D(side * 0.022F, 0, 0),
             wood_outer + QVector3D(side * 0.022F, 0, 0),
             0.045F,
             c.metal_bronze,
             white);

    for (int spoke = 0; spoke < 6; ++spoke) {
      float const angle = static_cast<float>(spoke) * std::numbers::pi_v<float> / 3.0F;
      QVector3D const face(hub.x() + side * 0.070F, hub.y(), hub.z());
      QVector3D const rim(face.x(),
                          face.y() + std::sin(angle) * wheel_radius * 0.78F,
                          face.z() + std::cos(angle) * wheel_radius * 0.78F);
      draw_cyl(out, p.model, face, rim, 0.017F, c.wood_light, white);
    }
  };

  draw_wheel(QVector3D(-0.42F, wheel_radius, -0.25F), -1.0F);
  draw_wheel(QVector3D(-0.42F, wheel_radius, 0.25F), -1.0F);
  draw_wheel(QVector3D(0.42F, wheel_radius, -0.25F), 1.0F);
  draw_wheel(QVector3D(0.42F, wheel_radius, 0.25F), 1.0F);

  draw_cyl(out,
           p.model,
           QVector3D(-0.40F, wheel_radius, -0.25F),
           QVector3D(0.40F, wheel_radius, -0.25F),
           0.025F,
           c.metal_iron,
           white);
  draw_cyl(out,
           p.model,
           QVector3D(-0.40F, wheel_radius, 0.25F),
           QVector3D(0.40F, wheel_radius, 0.25F),
           0.025F,
           c.metal_iron,
           white);
}

void draw_torsion_mechanism(const DrawContext& p,
                            ISubmitter& out,
                            Mesh* unit,
                            Texture* white,
                            const CarthageCatapultPalette& c) {

  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(-0.235F, 0.33F, k_pivot_z),
           QVector3D(0.040F, 0.135F, 0.115F),
           c.wood_dark);
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.235F, 0.33F, k_pivot_z),
           QVector3D(0.040F, 0.135F, 0.115F),
           c.wood_dark);

  draw_cyl(out,
           p.model,
           QVector3D(-0.19F, k_pivot_y, k_pivot_z),
           QVector3D(0.19F, k_pivot_y, k_pivot_z),
           0.076F,
           c.rope,
           white);
  for (int wrap = 0; wrap < 4; ++wrap) {
    float const x = -0.135F + (static_cast<float>(wrap) * 0.09F);
    draw_cyl(out,
             p.model,
             QVector3D(x - 0.012F, k_pivot_y, k_pivot_z),
             QVector3D(x + 0.012F, k_pivot_y, k_pivot_z),
             0.082F,
             c.leather,
             white);
  }

  draw_cyl(out,
           p.model,
           QVector3D(-0.225F, k_pivot_y, k_pivot_z),
           QVector3D(-0.185F, k_pivot_y, k_pivot_z),
           0.094F,
           c.metal_bronze,
           white);
  draw_cyl(out,
           p.model,
           QVector3D(0.185F, k_pivot_y, k_pivot_z),
           QVector3D(0.225F, k_pivot_y, k_pivot_z),
           0.094F,
           c.metal_bronze,
           white);
}

void draw_stanchions(const DrawContext& p,
                     ISubmitter& out,
                     Texture* white,
                     const CarthageCatapultPalette& c) {

  draw_cyl(out,
           p.model,
           QVector3D(-0.245F, 0.24F, 0.12F),
           k_buffer_end(-1.0F),
           0.040F,
           c.wood_cedar,
           white);
  draw_cyl(out,
           p.model,
           QVector3D(0.245F, 0.24F, 0.12F),
           k_buffer_end(1.0F),
           0.040F,
           c.wood_cedar,
           white);

  draw_cyl(out,
           p.model,
           QVector3D(-0.245F, 0.26F, -0.30F),
           k_buffer_end(-1.0F),
           0.026F,
           c.wood_dark,
           white);
  draw_cyl(out,
           p.model,
           QVector3D(0.245F, 0.26F, -0.30F),
           k_buffer_end(1.0F),
           0.026F,
           c.wood_dark,
           white);

  QVector3D const buffer_left = k_buffer_end(-1.0F);
  QVector3D const buffer_right = k_buffer_end(1.0F);
  draw_cyl(out, p.model, buffer_left, buffer_right, 0.036F, c.wood_dark, white);
  draw_cyl(out,
           p.model,
           QVector3D(buffer_left.x() * 0.74F, buffer_left.y(), buffer_left.z()),
           QVector3D(buffer_right.x() * 0.74F, buffer_right.y(), buffer_right.z()),
           0.062F,
           c.leather,
           white);
}

void draw_throwing_arm(const DrawContext& p,
                       ISubmitter& out,
                       Mesh* unit,
                       Texture* white,
                       const CarthageCatapultPalette& c,
                       const CatapultAnimContext& anim_ctx) {

  float const arm_angle = arm_swing_rad(anim_ctx);

  QMatrix4x4 arm_matrix = p.model;
  arm_matrix.translate(0.0F, k_pivot_y, k_pivot_z);
  arm_matrix.rotate(arm_angle * k_rad_to_deg, 1.0F, 0.0F, 0.0F);

  draw_cyl(out,
           arm_matrix,
           QVector3D(0.0F, 0.0F, 0.14F),
           QVector3D(0.0F, 0.0F, -k_arm_length),
           0.050F,
           c.wood_cedar,
           white);
  draw_cyl(out,
           arm_matrix,
           QVector3D(0.0F, 0.0F, -k_arm_length * 0.55F),
           QVector3D(0.0F, 0.0F, -k_arm_length),
           0.038F,
           c.wood_light,
           white);

  for (int band = 0; band < 3; ++band) {
    float const z = -0.10F - (static_cast<float>(band) * 0.22F);
    draw_cyl(out,
             arm_matrix,
             QVector3D(0.0F, 0.0F, z - 0.014F),
             QVector3D(0.0F, 0.0F, z + 0.014F),
             0.054F,
             c.metal_iron,
             white);
  }

  QMatrix4x4 bowl = arm_matrix;
  bowl.translate(0.0F, 0.052F, -k_arm_length + 0.03F);
  bowl.scale(0.115F, 0.062F, 0.115F);
  out.mesh(get_unit_sphere(), bowl, c.leather, white, 1.0F);

  draw_cyl(out,
           arm_matrix,
           QVector3D(-0.11F, 0.052F, -k_arm_length + 0.03F),
           QVector3D(0.11F, 0.052F, -k_arm_length + 0.03F),
           0.016F,
           c.rope,
           white);

  if (anim_ctx.show_stone) {
    QVector3D const seat(0.0F, 0.115F, -k_arm_length + 0.03F);
    float const stone_scale = 0.085F;

    QMatrix4x4 stone_matrix = arm_matrix;
    stone_matrix.translate(seat);
    stone_matrix.rotate(37.0F, QVector3D(0.4F, 1.0F, 0.3F).normalized());
    stone_matrix.scale(stone_scale, stone_scale, stone_scale);

    QVector3D const round_color =
        anim_ctx.incendiary_round ? QVector3D(0.22F, 0.17F, 0.15F) : c.stone;
    out.mesh(Render::Geom::Stone::get(), stone_matrix, round_color, white, 1.0F);

    if (anim_ctx.incendiary_round) {
      for (int wrap = 0; wrap < 3; ++wrap) {
        float const offset = (static_cast<float>(wrap) - 1.0F) * 0.035F;
        draw_cyl(out,
                 arm_matrix,
                 seat + QVector3D(-0.085F, offset * 0.6F, offset),
                 seat + QVector3D(0.085F, offset * 0.6F, offset),
                 0.016F,
                 c.leather,
                 white);
      }

      auto* scene_renderer = dynamic_cast<Renderer*>(out.unwrap_submitter());
      if (scene_renderer != nullptr) {
        QVector3D const flame_origin =
            (arm_matrix * QVector4D(seat, 1.0F)).toVector3D();
        float const phase = scene_renderer->get_animation_time();
        scene_renderer->fireball(
            flame_origin, QVector3D(0.95F, 0.30F, 0.05F), 0.115F, 1.05F, phase);
        scene_renderer->fireball(flame_origin + QVector3D(0.0F, 0.055F, 0.0F),
                                 QVector3D(1.0F, 0.68F, 0.20F),
                                 0.062F,
                                 1.35F,
                                 phase * 1.31F + 1.7F);
      }
    }
  }
}

void draw_decorations(const DrawContext& p,
                      ISubmitter& out,
                      Mesh* unit,
                      Texture* white,
                      const CarthageCatapultPalette& c) {

  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 0.72F, -0.12F),
           QVector3D(0.04F, 0.08F, 0.02F),
           c.metal_bronze);
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.0F, 0.78F, -0.12F),
           QVector3D(0.06F, 0.02F, 0.02F),
           c.metal_bronze);

  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(-0.57F, 0.22F, -0.35F),
           QVector3D(0.05F, 0.05F, 0.05F),
           c.metal_bronze);
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.57F, 0.22F, -0.35F),
           QVector3D(0.05F, 0.05F, 0.05F),
           c.metal_bronze);
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(-0.57F, 0.22F, 0.35F),
           QVector3D(0.05F, 0.05F, 0.05F),
           c.metal_bronze);
  draw_box(out,
           unit,
           white,
           p.model,
           QVector3D(0.57F, 0.22F, 0.35F),
           QVector3D(0.05F, 0.05F, 0.05F),
           c.metal_bronze);
}

void draw_windlass(const DrawContext& p,
                   ISubmitter& out,
                   Texture* white,
                   const CarthageCatapultPalette& c) {

  draw_cyl(out,
           p.model,
           QVector3D(-0.22F, 0.25F, 0.35F),
           QVector3D(0.22F, 0.25F, 0.35F),
           0.06F,
           c.wood_cedar,
           white);
  draw_cyl(out,
           p.model,
           QVector3D(-0.15F, 0.25F, 0.35F),
           QVector3D(0.15F, 0.25F, 0.35F),
           0.07F,
           c.metal_bronze,
           white);

  draw_cyl(out,
           p.model,
           QVector3D(-0.28F, 0.25F, 0.35F),
           QVector3D(-0.28F, 0.38F, 0.35F),
           0.025F,
           c.wood_dark,
           white);
  draw_cyl(out,
           p.model,
           QVector3D(0.28F, 0.25F, 0.35F),
           QVector3D(0.28F, 0.38F, 0.35F),
           0.025F,
           c.wood_dark,
           white);

  draw_cyl(out,
           p.model,
           QVector3D(-0.12F, 0.25F, 0.35F),
           QVector3D(0.12F, 0.25F, 0.35F),
           0.065F,
           c.rope,
           white);
}

void draw_catapult_body(const DrawContext& p,
                        ISubmitter& out,
                        Mesh* unit_cube,
                        Texture* white_tex,
                        const QVector3D& team_color) {
  auto palette = make_palette(team_color);
  auto anim_ctx = get_anim_context(p.entity);

  draw_base_frame(p, out, unit_cube, white_tex, palette);
  draw_wheels(p, out, white_tex, palette);
  draw_torsion_mechanism(p, out, unit_cube, white_tex, palette);
  draw_stanchions(p, out, white_tex, palette);
  draw_throwing_arm(p, out, unit_cube, white_tex, palette, anim_ctx);
  draw_windlass(p, out, white_tex, palette);
  draw_decorations(p, out, unit_cube, white_tex, palette);
}

} // namespace

void register_catapult_renderer(EntityRendererRegistry& registry) {
  register_siege_renderer_variant(
      registry,
      SiegeRendererConfig{.renderer_key = "troops/carthage/catapult",
                          .default_team = QVector3D(0.4F, 0.2F, 0.6F),
                          .draw_body = &draw_catapult_body});
}

} // namespace Render::GL::Carthage
