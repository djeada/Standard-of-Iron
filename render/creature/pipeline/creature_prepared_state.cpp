#include "creature_prepared_state.h"

#include <QVector2D>
#include <QtMath>

#include <algorithm>
#include <cmath>

#include "../../../game/core/component.h"
#include "../../../game/core/entity.h"
#include "../../../game/map/terrain_service.h"
#include "../../../game/units/spawn_type.h"
#include "../../entity/registry.h"
#include "../../gl/backend.h"
#include "../../gl/camera.h"
#include "../../gl/humanoid/animation/animation_inputs.h"
#include "../../gl/resources.h"
#include "../../graphics_settings.h"
#include "../../profiling/combat_animation_diagnostics.h"
#include "../../visibility_budget.h"

namespace Render::Creature::Pipeline {

namespace {

constexpr float k_shadow_size_infantry = 0.16F;
constexpr float k_shadow_size_mounted = 0.35F;
constexpr float k_shadow_size_horse = 0.38F;
constexpr float k_shadow_size_elephant = 0.55F;
constexpr float k_shadow_ground_offset = 0.02F;
constexpr float k_shadow_base_alpha = 0.32F;
const QVector3D k_shadow_light_dir(0.65F, 0.50F, 0.40F);

const QVector3D k_shadow_light_dir_normalized = k_shadow_light_dir.normalized();
const QVector2D k_shadow_dir_xz_normalized = []() {
  QVector2D v(k_shadow_light_dir_normalized.x(), k_shadow_light_dir_normalized.z());
  if (v.lengthSquared() < 1e-6F) {
    return QVector2D(0.0F, 1.0F);
  }
  v.normalize();
  return v;
}();

auto admits_contact_shadow(const Render::GL::DrawContext& ctx,
                           Render::Creature::CreatureLOD lod,
                           float camera_distance,
                           std::uint32_t formation_id,
                           bool standing_idle) -> bool {
  const auto& graphics = Render::GraphicsSettings::instance();
  return ctx.allow_template_cache && graphics.shadows_enabled() &&
         lod == Render::Creature::CreatureLOD::Full &&
         camera_distance < graphics.shadow_max_distance() &&
         Render::VisibilityBudgetTracker::instance().request_contact_shadow(
             formation_id, standing_idle);
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
  lod_in.temporal = inputs.config.temporal;
  lod_in.frame_index = inputs.frame_index;
  lod_in.instance_seed = inputs.instance_seed;

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

  auto& terrain_service = Game::Map::TerrainService::instance();
  if (!terrain_service.is_initialized() ||
      !admits_contact_shadow(ctx,
                             inputs.lod,
                             inputs.camera_distance,
                             inputs.formation_id,
                             inputs.standing_idle)) {
    return state;
  }

  float const shadow_size =
      inputs.mounted ? k_shadow_size_mounted : k_shadow_size_infantry;
  float depth_boost = 1.0F;
  float width_boost = 1.0F;
  if (inputs.unit != nullptr) {
    using Game::Units::SpawnType;
    switch (inputs.unit->spawn_type) {
    case SpawnType::Spearman:
      depth_boost = 1.8F;
      width_boost = 0.95F;
      break;
    case SpawnType::HorseSpearman:
      depth_boost = 2.1F;
      width_boost = 1.05F;
      break;
    case SpawnType::Archer:
    case SpawnType::HorseArcher:
      depth_boost = 1.2F;
      width_boost = 0.95F;
      break;
    default:
      break;
    }
  }

  float const shadow_width =
      shadow_size * (inputs.mounted ? 1.05F : 1.0F) * width_boost;
  float const shadow_depth =
      shadow_size * (inputs.mounted ? 1.30F : 1.10F) * depth_boost;

  const float shadow_y = inputs.surface_height_valid
                             ? inputs.surface_world_y
                             : terrain_service.resolve_surface_world_y(
                                   inputs.soldier_world_pos.x(),
                                   inputs.soldier_world_pos.z(),
                                   0.0F,
                                   inputs.soldier_world_pos.y());

  QVector2D const shadow_dir = -k_shadow_dir_xz_normalized;
  QVector2D dir_for_use = shadow_dir;
  if (dir_for_use.lengthSquared() < 1e-6F) {
    dir_for_use = QVector2D(0.0F, 1.0F);
  }

  float const shadow_offset = shadow_depth * 1.25F;
  QVector2D const offset_2d = dir_for_use * shadow_offset;
  float const light_yaw_deg =
      qRadiansToDegrees(std::atan2(double(dir_for_use.x()), double(dir_for_use.y())));

  state.model.translate(inputs.soldier_world_pos.x() + offset_2d.x(),
                        shadow_y + k_shadow_ground_offset,
                        inputs.soldier_world_pos.z() + offset_2d.y());
  state.model.rotate(light_yaw_deg, 0.0F, 1.0F, 0.0F);
  state.model.rotate(-90.0F, 1.0F, 0.0F, 0.0F);
  state.model.scale(shadow_width, shadow_depth, 1.0F);
  state.light_dir = dir_for_use;
  state.alpha = k_shadow_base_alpha;
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

  auto& terrain_service = Game::Map::TerrainService::instance();
  if (!terrain_service.is_initialized() ||
      !admits_contact_shadow(ctx,
                             inputs.lod,
                             inputs.camera_distance,
                             inputs.formation_id,
                             inputs.standing_idle)) {
    return state;
  }

  float shadow_size = k_shadow_size_horse;
  float width_mult = 1.05F;
  float depth_mult = 1.70F;
  if (inputs.kind == CreatureKind::Elephant) {
    shadow_size = k_shadow_size_elephant;
    width_mult = 1.10F;
    depth_mult = 2.20F;
  }

  float const shadow_width = shadow_size * width_mult;
  float const shadow_depth = shadow_size * depth_mult;

  const float shadow_y = inputs.surface_height_valid
                             ? inputs.surface_world_y
                             : terrain_service.resolve_surface_world_y(
                                   inputs.world_pos.x(),
                                   inputs.world_pos.z(),
                                   0.0F,
                                   inputs.world_pos.y());

  QVector2D const shadow_dir = -k_shadow_dir_xz_normalized;
  QVector2D dir_for_use = shadow_dir;
  if (dir_for_use.lengthSquared() < 1e-6F) {
    dir_for_use = QVector2D(0.0F, 1.0F);
  }

  float const shadow_offset = shadow_depth * 1.25F;
  QVector2D const offset_2d = dir_for_use * shadow_offset;
  float const light_yaw_deg =
      qRadiansToDegrees(std::atan2(double(dir_for_use.x()), double(dir_for_use.y())));

  state.model.translate(inputs.world_pos.x() + offset_2d.x(),
                        shadow_y + k_shadow_ground_offset,
                        inputs.world_pos.z() + offset_2d.y());
  state.model.rotate(light_yaw_deg, 0.0F, 1.0F, 0.0F);
  state.model.rotate(-90.0F, 1.0F, 0.0F, 0.0F);
  state.model.scale(shadow_width, shadow_depth, 1.0F);
  state.light_dir = dir_for_use;
  state.alpha = k_shadow_base_alpha;
  state.pass = graph.pass_intent;
  state.enabled = true;
  return state;
}

} // namespace Render::Creature::Pipeline
