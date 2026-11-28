#include "supply_wagon_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <memory>
#include <unordered_map>

#include "game/core/component_core.h"
#include "game/core/entity.h"
#include "render/creature/horse/horse_components.h"
#include "render/creature/horse/horse_motion.h"
#include "render/creature/horse/horse_renderer_base.h"
#include "render/creature/humanoid/humanoid_types.h"
#include "render/entity/registry.h"
#include "render/frame/draw_resources.h"
#include "render/frame/entity_appearance.h"
#include "render/frame/submitter.h"
#include "render/frame/terrain_contact.h"
#include "render/geom/primitives.h"
#include "render/geom/transforms.h"
#include "siege_renderer_common.h"

namespace Render::GL {
namespace {

constexpr float k_wheel_radius = 0.52F;
constexpr float k_half_track = 0.74F;
constexpr float k_axle_z = -0.48F;
constexpr float k_draught_horse_z = 1.86F;

struct WagonAnimationState {
  SiegeTravelState travel;
  Render::Creature::HorseAnimationStateComponent horse;
  float horse_speed{0.0F};
};

struct WagonHistory {
  std::unordered_map<Engine::Core::EntityID, WagonAnimationState> wagons;
  float last_sweep = 0.0F;
};

auto grounded_wagon_model(const DrawContext& ctx) -> QMatrix4x4 {
  auto model = ctx.model;
  const auto& terrain = ctx.world_view.terrain_or_empty();
  if (!terrain.is_initialized()) {
    return model;
  }
  Render::tilt_model_to_ground(model, terrain, 20.0F);
  float axle_correction = 0.0F;
  for (const float side : {-1.0F, 1.0F}) {
    const auto tire = model.map(QVector3D(side * k_half_track, 0.0F, k_axle_z));
    axle_correction +=
        terrain.resolve_surface_world_y(tire.x(), tire.z(), 0.0F, tire.y()) - tire.y();
  }
  auto origin = model.column(3);
  origin.setY(origin.y() + std::clamp(axle_correction * 0.5F, -0.25F, 0.25F));
  model.setColumn(3, origin);
  return model;
}

void draw_wagon(const DrawContext& ctx, ISubmitter& out, WagonAnimationState& state) {
  auto* cube = get_unit_cube();
  auto* white = ctx.resources != nullptr ? ctx.resources->white() : nullptr;
  if (cube == nullptr) {
    return;
  }
  const auto displacement = ctx.model.column(3).toVector3D() - state.travel.position;
  const float dt = ctx.animation_time - state.travel.time;
  if (!state.travel.initialized || dt < 0.0F || dt > 1.0F ||
      displacement.lengthSquared() > 36.0F) {
    state.horse = {};
    state.horse_speed = 0.0F;
  } else if (dt > 0.0F) {
    const float speed = std::hypot(displacement.x(), displacement.z()) / dt;
    state.horse_speed += (speed - state.horse_speed) * (1.0F - std::exp(-8.0F * dt));
  }
  const auto motion = siege_motion(ctx, state.travel, k_wheel_radius, k_half_track);
  const auto wreck = resolve_siege_wreck(ctx, Animation::SiegeWreckKind::Ram);
  auto model = grounded_wagon_model(ctx);
  apply_siege_wreck(model, wreck, k_half_track, 0.5F);
  const auto tint = [&wreck](const QVector3D& color) {
    return siege_charred(color, wreck);
  };
  const auto box = [&](const QVector3D& position,
                       const QVector3D& size,
                       const QVector3D& color,
                       float roll = 0.0F) {
    auto part = model;
    part.translate(position);
    part.rotate(roll, 0.0F, 0.0F, 1.0F);
    part.scale(size);
    out.mesh(cube, part, tint(color), white, 1.0F);
  };
  const auto beam = [&](const QVector3D& a,
                        const QVector3D& b,
                        float radius,
                        const QVector3D& color) {
    out.mesh(get_unit_cylinder(8),
             model * Render::Geom::cylinder_between(a, b, radius),
             tint(color),
             white,
             1.0F);
  };
  const QVector3D oak{0.48F, 0.32F, 0.18F};
  const QVector3D dark_oak{0.29F, 0.19F, 0.10F};
  const QVector3D leather{0.27F, 0.13F, 0.07F};
  const QVector3D iron{0.24F, 0.23F, 0.20F};
  const QVector3D brass{0.72F, 0.52F, 0.29F};
  const QVector3D linen{0.76F, 0.65F, 0.47F};
  const QVector3D team = Render::entity_color(*ctx.entity);

  box({0.0F, 0.61F, -0.47F}, {1.15F, 0.16F, 1.84F}, dark_oak);
  box({0.0F, 0.82F, -1.32F}, {1.17F, 0.34F, 0.10F}, oak);
  box({0.0F, 0.82F, 0.41F}, {1.17F, 0.34F, 0.10F}, oak);
  box({0.0F, 0.98F, -1.381F}, {0.66F, 0.14F, 0.025F}, team * 0.77F);
  box({0.0F, 0.98F, -1.399F}, {0.35F, 0.022F, 0.012F}, brass);

  for (const float side : {-1.0F, 1.0F}) {
    box({side * 0.56F, 0.76F, -0.47F}, {0.10F, 0.16F, 1.82F}, oak);
    box({side * 0.56F, 0.92F, -0.47F}, {0.10F, 0.13F, 1.82F}, oak * 1.12F);
    for (const float post_z : {-1.28F, -0.62F, 0.28F}) {
      box({side * 0.606F, 0.82F, post_z}, {0.06F, 0.40F, 0.12F}, dark_oak);
    }
    draw_siege_wheel(out,
                     white,
                     model,
                     {side * k_half_track, k_wheel_radius, k_axle_z},
                     k_wheel_radius,
                     side > 0.0F ? -motion.left_roll : -motion.right_roll,
                     tint(oak * 1.13F),
                     tint(iron),
                     tint(brass));
    if (!wreck.destroyed) {
      beam({side * 0.53F, 0.66F, 0.18F},
           {side * 0.39F, 0.94F, k_draught_horse_z + 0.32F},
           0.035F,
           dark_oak);
      beam({side * 0.39F, 0.94F, k_draught_horse_z + 0.32F},
           {side * 0.23F, 1.12F, k_draught_horse_z + 0.52F},
           0.023F,
           leather);
    }
  }
  beam({-k_half_track, k_wheel_radius, k_axle_z},
       {k_half_track, k_wheel_radius, k_axle_z},
       0.055F,
       iron);

  if (wreck.destroyed) {
    box({0.36F, 0.59F, -0.25F}, {0.72F, 0.15F, 0.72F}, dark_oak, 35.0F);
    return;
  }

  // The canvas covers the middle load, leaving the crates and rear board visible.
  for (int strip = -3; strip <= 3; ++strip) {
    const float x = static_cast<float>(strip) * 0.156F;
    box({x, 1.32F - 0.71F * x * x, -0.43F},
        {0.183F, 0.034F, 0.99F},
        linen * (strip % 2 == 0 ? 1.0F : 0.94F),
        -67.0F * x);
  }
  for (const float rope_z : {-0.92F, 0.05F}) {
    for (const float side : {-1.0F, 1.0F}) {
      beam({side * 0.55F, 0.93F, rope_z},
           {side * 0.47F, 1.16F, rope_z},
           0.018F,
           leather);
      beam({side * 0.47F, 1.16F, rope_z}, {0.0F, 1.345F, rope_z}, 0.018F, leather);
    }
  }
  box({-0.22F, 0.87F, -1.06F}, {0.42F, 0.40F, 0.40F}, oak * 1.18F);
  box({0.24F, 0.84F, -1.08F}, {0.40F, 0.32F, 0.37F}, linen * 0.86F);
  box({-0.23F, 0.91F, -1.269F}, {0.05F, 0.30F, 0.018F}, dark_oak);

  using Render::Creature::Pipeline::CreatureKind;
  using Render::Creature::Pipeline::UnitVisualSpec;
  static const HorseRendererBase horse(
      UnitVisualSpec{.debug_name = "horse/convoy", .kind = CreatureKind::Horse});
  auto draft = ctx;
  draft.model.translate(0.0F, 0.0F, k_draught_horse_z);
  draft.model.scale(0.9F);
  auto profile = make_horse_profile(
      static_cast<std::uint32_t>(ctx.entity->get_id()), leather, team);
  HumanoidAnimationContext locomotion;
  locomotion.inputs.time = ctx.animation_time;
  locomotion.gait.speed = state.horse_speed;
  locomotion.gait.state =
      state.horse_speed > 0.08F ? HumanoidMotionState::Walk : HumanoidMotionState::Idle;
  const float scale = draft.model.column(0).toVector3D().length();
  const auto gait = evaluate_horse_motion(
      profile, locomotion.inputs, locomotion, &state.horse, scale);
  const auto lod = ctx.distance_sq < 36.0F * 36.0F
                       ? Render::Creature::CreatureLOD::Full
                       : Render::Creature::CreatureLOD::Minimal;
  horse.render(draft, locomotion.inputs, locomotion, profile, &gait, out, lod);
}

} // namespace

void register_supply_wagon_renderer(EntityRendererRegistry& registry) {
  registry.register_renderer(
      "supply_wagon",
      [history = std::make_shared<WagonHistory>()](const DrawContext& ctx,
                                                   ISubmitter& out) {
        if (ctx.entity == nullptr) {
          return;
        }
        if (ctx.animation_time < history->last_sweep) {
          history->wagons.clear();
          history->last_sweep = ctx.animation_time;
        }
        if (ctx.animation_time - history->last_sweep > 10.0F) {
          std::erase_if(history->wagons, [&ctx](const auto& item) {
            return ctx.animation_time - item.second.travel.time > 30.0F;
          });
          history->last_sweep = ctx.animation_time;
        }
        WagonAnimationState transient;
        auto& state = should_persist_animation_state(ctx)
                          ? history->wagons[ctx.entity->get_id()]
                          : transient;
        draw_wagon(ctx, out, state);
      });
}

} // namespace Render::GL
