#include "creature_prepared_state.h"

#include <QVector2D>
#include <QtMath>

#include <algorithm>
#include <cmath>

#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/map/terrain_service.h"
#include "game/units/spawn_type.h"
#include "render/contact_shadow.h"
#include "render/entity/registry.h"
#include "render/gl/backend.h"
#include "render/gl/humanoid/animation/animation_inputs.h"
#include "render/gl/resources.h"
#include "render/graphics_settings.h"
#include "render/profiling/combat_animation_diagnostics.h"
#include "render/visibility_budget.h"
#include "scene/camera.h"

namespace Render::Creature::Pipeline {

namespace {

constexpr QVector2D k_blob_half_extent_infantry{0.40F, 0.46F};
constexpr QVector2D k_blob_half_extent_mounted{0.58F, 1.00F};
constexpr QVector2D k_blob_half_extent_horse{0.58F, 1.00F};
constexpr QVector2D k_blob_half_extent_elephant{1.05F, 1.55F};
constexpr float k_shadow_ground_offset = 0.025F;
constexpr float k_shadow_base_alpha = 0.62F;
constexpr float k_shadow_min_visible_alpha = 0.004F;

constexpr float k_shadow_tilt_probe = 0.35F;

auto contact_shadow_opacity(const Render::GL::DrawContext& ctx,
                            float camera_distance) -> float {
  const auto& graphics = Render::GraphicsSettings::instance();
  const Render::ContactShadowInputs inputs{.camera_distance = camera_distance,
                                           .fade_distance =
                                               graphics.shadow_max_distance()};
  return Render::contact_shadow_placement(ctx.backend->environment_lighting(), inputs)
      .opacity;
}

auto admits_contact_shadow(const Render::GL::DrawContext& ctx,
                           Render::Creature::CreatureLOD lod,
                           float camera_distance) -> bool {
  const auto& graphics = Render::GraphicsSettings::instance();
  return ctx.allow_template_cache && lod != Render::Creature::CreatureLOD::Culled &&
         camera_distance < graphics.shadow_max_distance() &&
         Render::VisibilityBudgetTracker::instance().request_contact_shadow();
}

auto build_contact_shadow_model(const Game::Map::TerrainService& terrain,
                                const QVector3D& world_pos,
                                float ground_y,
                                float facing_yaw_degrees,
                                QVector2D half_extent) -> QMatrix4x4 {
  const float probe = k_shadow_tilt_probe;
  const float hx0 = terrain.resolve_surface_world_y(
      world_pos.x() - probe, world_pos.z(), 0.0F, ground_y);
  const float hx1 = terrain.resolve_surface_world_y(
      world_pos.x() + probe, world_pos.z(), 0.0F, ground_y);
  const float hz0 = terrain.resolve_surface_world_y(
      world_pos.x(), world_pos.z() - probe, 0.0F, ground_y);
  const float hz1 = terrain.resolve_surface_world_y(
      world_pos.x(), world_pos.z() + probe, 0.0F, ground_y);
  QVector3D normal(-(hx1 - hx0) / (2.0F * probe), 1.0F, -(hz1 - hz0) / (2.0F * probe));
  normal.normalize();

  const float yaw = qDegreesToRadians(facing_yaw_degrees);
  QVector3D forward(std::sin(yaw), 0.0F, std::cos(yaw));
  forward = (forward - normal * QVector3D::dotProduct(forward, normal)).normalized();
  if (forward.lengthSquared() < 1e-6F) {
    forward = QVector3D(0.0F, 0.0F, 1.0F);
  }
  const QVector3D right = QVector3D::crossProduct(forward, normal).normalized();

  QMatrix4x4 model;
  model.setColumn(0, QVector4D(right * half_extent.x(), 0.0F));
  model.setColumn(1, QVector4D(forward * half_extent.y(), 0.0F));
  model.setColumn(2, QVector4D(normal, 0.0F));
  model.setColumn(
      3,
      QVector4D(world_pos.x(), ground_y + k_shadow_ground_offset, world_pos.z(), 1.0F));
  return model;
}

} // namespace

auto resolve_humanoid_animation_state(const Render::GL::DrawContext& ctx)
    -> PreparedAnimationState {
  PreparedAnimationState state;
  state.used_override = (ctx.animation_override != nullptr);
  state.inputs = Render::GL::sample_anim_state(ctx);
  return state;
}

auto resolve_elephant_animation_state(const Render::GL::DrawContext& ctx)
    -> PreparedAnimationState {
  return resolve_humanoid_animation_state(ctx);
}

auto resolve_humanoid_lod_state(const HumanoidLodStateInputs& inputs)
    -> PreparedCreatureLodState {
  PreparedCreatureLodState state;
  if (inputs.ctx == nullptr) {
    return state;
  }

  const auto& ctx = *inputs.ctx;
  CreatureLodDecisionInputs lod_in{};
  if (ctx.force_humanoid_lod) {
    lod_in.forced_lod = ctx.forced_humanoid_lod;
  } else if (ctx.selected || ctx.hovered) {
    lod_in.forced_lod = Render::Creature::CreatureLOD::Full;
  }
  lod_in.has_camera = (ctx.camera != nullptr);
  if (ctx.camera != nullptr) {
    state.camera_distance =
        (inputs.soldier_world_pos - ctx.camera->get_position()).length();
  }
  lod_in.distance = state.camera_distance;
  lod_in.thresholds = inputs.config.thresholds;
  lod_in.apply_visibility_budget = inputs.config.apply_visibility_budget;
  lod_in.budget_grant_full = true;

  if (lod_in.apply_visibility_budget && !ctx.force_humanoid_lod &&
      ctx.camera != nullptr) {
    const auto distance_lod =
        select_distance_lod(state.camera_distance, lod_in.thresholds);
    if (distance_lod == Render::Creature::CreatureLOD::Full) {
      const auto granted =
          Render::VisibilityBudgetTracker::instance().request_humanoid_lod(
              Render::Creature::CreatureLOD::Full);
      state.budget_granted_full = (granted == Render::Creature::CreatureLOD::Full);
      lod_in.budget_grant_full = state.budget_granted_full;
    }
  }

  state.decision = decide_creature_lod(lod_in);
  return state;
}

auto prepare_humanoid_shadow_state(const HumanoidShadowStateInputs& inputs)
    -> PreparedHumanoidShadowState {
  PreparedHumanoidShadowState state;
  if (inputs.ctx == nullptr || inputs.graph == nullptr) {
    return state;
  }

  const auto& ctx = *inputs.ctx;
  const auto& graph = *inputs.graph;
  if (ctx.backend == nullptr || ctx.resources == nullptr) {
    return state;
  }

  state.shader = ctx.backend->troop_shadow_shader();
  state.mesh = ctx.resources->quad();
  if (state.shader == nullptr || state.mesh == nullptr) {
    state.shader = nullptr;
    state.mesh = nullptr;
    return state;
  }

  const float shadow_alpha = k_shadow_base_alpha * inputs.intensity_scale *
                             contact_shadow_opacity(ctx, inputs.camera_distance);
  const auto& terrain_service = ctx.world_view.terrain_or_empty();
  if (!terrain_service.is_initialized() || shadow_alpha < k_shadow_min_visible_alpha ||
      !admits_contact_shadow(ctx, inputs.lod, inputs.camera_distance)) {
    return state;
  }

  QVector2D half_extent =
      inputs.mounted ? k_blob_half_extent_mounted : k_blob_half_extent_infantry;
  if (inputs.unit != nullptr && !inputs.mounted) {
    using Game::Units::SpawnType;
    switch (inputs.unit->spawn_type) {
    case SpawnType::Spearman:
      half_extent.setY(half_extent.y() * 1.10F);
      break;
    default:
      break;
    }
  }

  const float shadow_y =
      inputs.surface_height_valid
          ? inputs.surface_world_y
          : terrain_service.resolve_surface_world_y(inputs.soldier_world_pos.x(),
                                                    inputs.soldier_world_pos.z(),
                                                    0.0F,
                                                    inputs.soldier_world_pos.y());

  state.model = build_contact_shadow_model(terrain_service,
                                           inputs.soldier_world_pos,
                                           shadow_y,
                                           inputs.facing_yaw_degrees,
                                           half_extent);
  state.light_dir = QVector2D(0.0F, 1.0F);
  state.alpha = shadow_alpha;
  state.pass = graph.pass_intent;
  state.enabled = true;
  return state;
}

auto prepare_quadruped_shadow_state(const QuadrupedShadowStateInputs& inputs)
    -> PreparedQuadrupedShadowState {
  PreparedQuadrupedShadowState state;
  if (inputs.ctx == nullptr || inputs.graph == nullptr) {
    return state;
  }

  const auto& ctx = *inputs.ctx;
  const auto& graph = *inputs.graph;
  if (ctx.backend == nullptr || ctx.resources == nullptr) {
    return state;
  }

  state.shader = ctx.backend->troop_shadow_shader();
  state.mesh = ctx.resources->quad();
  if (state.shader == nullptr || state.mesh == nullptr) {
    state.shader = nullptr;
    state.mesh = nullptr;
    return state;
  }

  const float shadow_alpha = k_shadow_base_alpha * inputs.intensity_scale *
                             contact_shadow_opacity(ctx, inputs.camera_distance);
  const auto& terrain_service = ctx.world_view.terrain_or_empty();
  if (!terrain_service.is_initialized() || shadow_alpha < k_shadow_min_visible_alpha ||
      !admits_contact_shadow(ctx, inputs.lod, inputs.camera_distance)) {
    return state;
  }

  const QVector2D half_extent = inputs.kind == CreatureKind::Elephant
                                    ? k_blob_half_extent_elephant
                                    : k_blob_half_extent_horse;

  const float shadow_y =
      inputs.surface_height_valid
          ? inputs.surface_world_y
          : terrain_service.resolve_surface_world_y(
                inputs.world_pos.x(), inputs.world_pos.z(), 0.0F, inputs.world_pos.y());

  state.model = build_contact_shadow_model(terrain_service,
                                           inputs.world_pos,
                                           shadow_y,
                                           inputs.facing_yaw_degrees,
                                           half_extent);
  state.light_dir = QVector2D(0.0F, 1.0F);
  state.alpha = shadow_alpha;
  state.pass = graph.pass_intent;
  state.enabled = true;
  return state;
}

} // namespace Render::Creature::Pipeline
