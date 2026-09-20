#include "siege_renderer_common.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <unordered_map>

#include "../entity_appearance.h"
#include "game/core/component_core.h"
#include "game/core/component_gameplay.h"
#include "render/geom/transforms.h"
#include "render/gl/primitives.h"
#include "render/gl/resources.h"
#include "render/scene_renderer.h"

namespace Render::GL {

auto siege_winding(float progress) -> float {
  const float t = std::clamp(progress, 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

auto siege_release(float progress) -> float {
  // Most of the stroke happens immediately; the rest is the buffer settling.
  const float t = std::clamp(progress / 0.24F, 0.0F, 1.0F);
  const float remaining = 1.0F - t;
  return 1.0F - remaining * remaining * remaining;
}

auto siege_motion(const DrawContext& ctx,
                  SiegeTravelState& state,
                  float wheel_radius,
                  float half_track) -> SiegeMotion {
  constexpr float tau = 2.0F * std::numbers::pi_v<float>;
  const auto position = ctx.model.column(3).toVector3D();
  const auto forward = ctx.model.column(2).toVector3D().normalized();
  const float yaw = std::atan2(forward.x(), forward.z());
  const float scale = std::max(ctx.model.column(0).toVector3D().length(), 0.001F);
  const float dt = ctx.animation_time - state.time;
  const auto delta = position - state.position;
  if (!state.initialized || dt < 0.0F || dt > 1.0F || delta.lengthSquared() > 36.0F) {
    state = {};
    state.initialized = true;
  } else if (dt > 0.0F) {
    const float turn = std::remainder(yaw - state.yaw, tau);
    const float mid_yaw = state.yaw + turn * 0.5F;
    const float travel =
        (delta.x() * std::sin(mid_yaw) + delta.z() * std::cos(mid_yaw)) / scale;
    const float radius = std::max(wheel_radius, 0.01F);
    state.left_roll =
        std::remainder(state.left_roll + (travel + turn * half_track) / radius, tau);
    state.right_roll =
        std::remainder(state.right_roll + (travel - turn * half_track) / radius, tau);
    const float speed = std::clamp(delta.length() / dt, 0.0F, 1.0F);
    state.movement += (speed - state.movement) * (1.0F - std::exp(-dt * 7.0F));
  }
  state.position = position;
  state.yaw = yaw;
  state.time = ctx.animation_time;
  SiegeMotion result{state.left_roll, state.right_roll, state.movement, 0.0F};
  if (ctx.entity != nullptr) {
    const auto* loading =
        ctx.entity->get_component<Engine::Core::CatapultLoadingComponent>();
    if (loading != nullptr &&
        loading->state ==
            Engine::Core::CatapultLoadingComponent::LoadingState::Firing) {
      const float t = loading->get_firing_progress();
      result.recoil =
          std::sin(std::min(t / 0.16F, 1.0F) * std::numbers::pi_v<float> * 0.5F) *
          std::exp(-5.0F * t) * (1.0F - t);
    }
  }
  return result;
}

auto siege_body_model(const DrawContext& ctx, const SiegeMotion& motion) -> QMatrix4x4 {
  auto model = ctx.model;
  const float rolling = (motion.left_roll + motion.right_roll) * 0.5F;
  model.translate(0.0F,
                  std::sin(rolling * 4.0F) * 0.006F * motion.movement,
                  -0.035F * motion.recoil);
  model.translate(0.0F, 0.18F, 0.0F);
  model.rotate(-2.2F * motion.recoil +
                   std::sin(rolling * 2.0F) * 0.65F * motion.movement,
               1.0F,
               0.0F,
               0.0F);
  model.rotate(
      (std::sin(motion.left_roll * 3.0F) + std::sin(motion.right_roll * 3.0F)) *
          0.375F * motion.movement,
      0.0F,
      0.0F,
      1.0F);
  model.translate(0.0F, -0.18F, 0.0F);
  return model;
}

void draw_siege_wheel(ISubmitter& out,
                      Texture* white,
                      const QMatrix4x4& model,
                      const QVector3D& hub,
                      float radius,
                      float roll,
                      const QVector3D& wood,
                      const QVector3D& iron,
                      const QVector3D& bronze) {
  auto wheel = model;
  wheel.translate(hub);
  wheel.rotate(roll * 180.0F / std::numbers::pi_v<float>, 1.0F, 0.0F, 0.0F);
  const auto cylinder =
      [&](const QVector3D& a, const QVector3D& b, float r, const QVector3D& color) {
        out.mesh(get_unit_cylinder(12),
                 wheel * Render::Geom::cylinder_between(a, b, r),
                 color,
                 white,
                 1.0F);
      };
  // Open spokes and a built-up felloe, rather than a solid metal disc.
  constexpr int segments = 16;
  for (int i = 0; i < segments; ++i) {
    const float a = static_cast<float>(i) * 2.0F * std::numbers::pi_v<float> / segments;
    const float b =
        static_cast<float>(i + 1) * 2.0F * std::numbers::pi_v<float> / segments;
    const QVector3D start(0.0F, std::sin(a), std::cos(a));
    const QVector3D end(0.0F, std::sin(b), std::cos(b));
    cylinder(start * radius * 0.84F, end * radius * 0.84F, radius * 0.12F, wood);
    cylinder(start * radius * 0.96F, end * radius * 0.96F, radius * 0.04F, iron);
    if (i % 2 == 0) {
      cylinder(start * radius * 0.17F, start * radius * 0.84F, radius * 0.075F, wood);
    }
  }
  cylinder(QVector3D(-0.045F, 0, 0), QVector3D(0.045F, 0, 0), radius * 0.25F, wood);
  cylinder(QVector3D(-0.055F, 0, 0), QVector3D(0.055F, 0, 0), radius * 0.16F, bronze);
  cylinder(QVector3D(-0.059F, 0, 0), QVector3D(0.059F, 0, 0), radius * 0.07F, iron);
}

void draw_siege_regalia(const DrawContext& ctx,
                        ISubmitter& out,
                        Mesh* cube,
                        Texture* white,
                        const QVector3D& team,
                        const QVector3D& bronze,
                        bool ballista) {
  const float width = ballista ? 0.29F : 0.42F;
  const float deck = ballista ? 0.15F : 0.23F;
  auto box =
      [&](const QVector3D& position, const QVector3D& size, const QVector3D& color) {
        auto m = ctx.model;
        m.translate(position);
        m.scale(size);
        out.mesh(cube, m, color, white, 1.0F);
      };
  for (float side : {-1.0F, 1.0F}) {
    // Painted side panels, bronze straps and proud rivet heads.
    box({side * (width + 0.048F), deck, 0.0F}, {0.009F, 0.040F, 0.27F}, team * 0.65F);
    for (float z : {-0.28F, 0.0F, 0.28F}) {
      box({side * width, deck, z}, {0.055F, 0.057F, 0.020F}, bronze);
      auto rivet = ctx.model;
      rivet.translate(side * (width + 0.06F), deck, z);
      rivet.scale(0.014F);
      out.mesh(get_unit_sphere(5, 8), rivet, bronze * 1.12F, white, 1.0F);
    }
  }
  // A short vexillum at the rear, clear of the bow and throwing arm.
  const float top = ballista ? 0.86F : 1.08F;
  const QVector3D foot(-width, deck, 0.32F);
  const QVector3D crown(-width, top, 0.32F);
  out.mesh(get_unit_cylinder(12),
           ctx.model * Render::Geom::cylinder_between(foot, crown, 0.012F),
           bronze,
           white,
           1.0F);
  box({-width, top - 0.04F, 0.32F}, {0.13F, 0.012F, 0.014F}, bronze);
  auto finial = ctx.model;
  finial.translate(crown);
  finial.scale(0.025F, 0.04F, 0.025F);
  out.mesh(get_unit_sphere(6, 10), finial, bronze, white, 1.0F);
  const float seed =
      ctx.entity != nullptr ? static_cast<float>(ctx.entity->get_id() % 31U) : 0.0F;
  for (int strip = 0; strip < 5; ++strip) {
    const float down = static_cast<float>(strip) * 0.041F;
    const float wave =
        std::sin(ctx.animation_time * 2.4F + seed - down * 12.0F) * down * 0.16F;
    box({-width, top - 0.07F - down, 0.32F + wave}, {0.105F, 0.023F, 0.005F}, team);
    for (float edge : {-1.0F, 1.0F}) {
      box({-width + edge * 0.10F, top - 0.07F - down, 0.32F + wave},
          {0.008F, 0.023F, 0.006F},
          bronze);
    }
    if (strip == 4) {
      box({-width, top - 0.09F - down, 0.32F + wave}, {0.105F, 0.008F, 0.006F}, bronze);
    }
  }
  auto badge = ctx.model;
  badge.translate(-width,
                  top - 0.15F,
                  0.32F + std::sin(ctx.animation_time * 2.4F + seed - 0.08F * 12.0F) *
                              0.08F * 0.16F);
  badge.scale(0.034F, 0.043F, 0.009F);
  out.mesh(get_unit_sphere(6, 10), badge, bronze, white, 1.0F);
}

void register_siege_renderer_variant(EntityRendererRegistry& registry,
                                     const SiegeRendererConfig& config) {
  struct History {
    std::unordered_map<std::uint64_t, SiegeTravelState> travel;
    Engine::Core::World* world{nullptr};
    float last_sweep{0.0F};
  };
  registry.register_renderer(
      std::string(config.renderer_key),
      [config, history = std::make_shared<History>()](const DrawContext& ctx,
                                                      ISubmitter& out) {
        auto& travel = history->travel;
        auto& world = history->world;
        auto& last_sweep = history->last_sweep;
        Mesh* unit = get_unit_cube();
        Texture* white = nullptr;

        if (ctx.resources != nullptr) {
          unit = ctx.resources->unit();
          white = ctx.resources->white();
        }
        if (auto* scene_renderer = dynamic_cast<Renderer*>(out.unwrap_submitter())) {
          unit = scene_renderer->get_mesh_cube();
          white = scene_renderer->get_white_texture();
        }

        if (unit == nullptr) {
          return;
        }

        QVector3D team_color = config.default_team;
        if (ctx.entity != nullptr) {
          if (ctx.entity->get_component<Engine::Core::RenderableComponent>() !=
              nullptr) {
            team_color = Render::entity_color(*ctx.entity);
          }
        }

        if (world != ctx.world || ctx.animation_time < last_sweep) {
          travel.clear();
          world = ctx.world;
          last_sweep = ctx.animation_time;
        }
        if (ctx.animation_time - last_sweep > 10.0F) {
          std::erase_if(travel, [&](const auto& item) {
            return ctx.animation_time - item.second.time > 30.0F;
          });
          last_sweep = ctx.animation_time;
        }
        SiegeTravelState transient;
        auto& state = ctx.entity != nullptr && should_persist_animation_state(ctx)
                          ? travel[ctx.entity->get_id()]
                          : transient;
        const bool ballista = config.renderer_key.ends_with("ballista");
        DrawContext presentation = ctx;
        presentation.model.scale(ballista ? 1.35F : 1.20F);
        const auto motion = siege_motion(
            presentation, state, ballista ? 0.155F : 0.21F, ballista ? 0.355F : 0.57F);
        config.draw_body(presentation, out, unit, white, team_color, motion);
      });
}

} // namespace Render::GL
