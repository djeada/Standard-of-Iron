#include "creature_prepared_state.h"

#include "../../../game/core/component.h"
#include "../../../game/core/entity.h"
#include "../../../game/map/terrain_service.h"
#include "../../../game/units/spawn_type.h"
#include "../../../game/units/troop_config.h"
#include "../../elephant/elephant_motion.h"
#include "../../entity/registry.h"
#include "../../gl/backend.h"
#include "../../gl/camera.h"
#include "../../gl/humanoid/animation/animation_inputs.h"
#include "../../gl/humanoid/animation/gait.h"
#include "../../gl/humanoid/humanoid_constants.h"
#include "../../gl/resources.h"
#include "../../graphics_settings.h"
#include "../../horse/horse_motion.h"
#include "../../humanoid/formation_calculator.h"
#include "../../humanoid/humanoid_renderer_base.h"
#include "../../visibility_budget.h"
#include "../animation_state_components.h"
#include "../quadruped/clip_set.h"
#include "preparation_common.h"

#include <QVector2D>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace Render::Creature::Pipeline {

namespace {

constexpr float k_shadow_ground_offset = 0.02F;
constexpr float k_shadow_base_alpha = 0.24F;
const QVector3D k_shadow_light_dir(0.4F, 1.0F, 0.25F);

auto resolved_individuals_per_unit(const Engine::Core::UnitComponent &unit)
    -> int {
  if (unit.render_individuals_per_unit_override > 0) {
    return unit.render_individuals_per_unit_override;
  }
  return Game::Units::TroopConfig::instance().get_individuals_per_unit(
      unit.spawn_type);
}

auto resolve_humanoid_formation_params(
    const Render::GL::HumanoidRendererBase &owner,
    const Render::GL::DrawContext &ctx) -> Render::GL::FormationParams {
  Render::GL::FormationParams params{};
  params.individuals_per_unit = 1;
  params.max_per_row = 1;
  params.spacing = 0.75F;

  if (ctx.entity != nullptr) {
    auto *unit = ctx.entity->get_component<Engine::Core::UnitComponent>();
    if (unit != nullptr) {
      params.individuals_per_unit = resolved_individuals_per_unit(*unit);
      params.max_per_row =
          Game::Units::TroopConfig::instance().get_max_units_per_row(
              unit->spawn_type);
      switch (unit->spawn_type) {
      case Game::Units::SpawnType::MountedKnight:
      case Game::Units::SpawnType::HorseArcher:
      case Game::Units::SpawnType::HorseSpearman:
        params.spacing =
            Render::GL::cavalry_formation_spacing(owner.get_mount_scale());
        break;
      default:
        break;
      }
    }
  } else if (owner.uses_mounted_pipeline()) {
    params.spacing =
        Render::GL::cavalry_formation_spacing(owner.get_mount_scale());
  }

  return params;
}

} // namespace

auto make_prepared_visual_state(const CreatureGraphOutput &graph) noexcept
    -> PreparedCreatureVisualState {
  PreparedCreatureVisualState state;
  state.spec = graph.spec;
  state.kind = graph.spec.kind;
  state.lod = graph.lod;
  state.pass = graph.pass_intent;
  state.seed = graph.seed;
  state.world_from_unit = graph.world_matrix;
  state.world_already_grounded = graph.world_already_grounded;
  state.entity_id = graph.entity_id;
  return state;
}

auto resolve_humanoid_animation_state(const Render::GL::DrawContext &ctx)
    -> PreparedAnimationState {
  PreparedAnimationState state;
  state.used_override = (ctx.animation_override != nullptr);
  state.inputs = Render::GL::sample_anim_state(ctx);
  return state;
}

auto resolve_elephant_animation_state(const Render::GL::DrawContext &ctx)
    -> PreparedAnimationState {
  PreparedAnimationState state = resolve_humanoid_animation_state(ctx);

  if (ctx.entity == nullptr || state.used_override) {
    return state;
  }

  auto *combat_state =
      ctx.entity->get_component<Engine::Core::CombatStateComponent>();
  if ((combat_state != nullptr) &&
      combat_state->animation_state !=
          Engine::Core::CombatAnimationState::Idle) {
    state.inputs.is_attacking = true;
    state.inputs.is_melee = true;
  }

  auto *attack = ctx.entity->get_component<Engine::Core::AttackComponent>();
  if ((attack != nullptr) && attack->in_melee_lock) {
    state.inputs.is_attacking = true;
    state.inputs.is_melee = true;
  }

  return state;
}

auto resolve_horse_motion_state(const HorseMotionStateInputs &inputs)
    -> PreparedHorseMotionState {
  PreparedHorseMotionState state;
  if (inputs.anim == nullptr || inputs.rider_ctx == nullptr ||
      inputs.profile == nullptr) {
    return state;
  }

  state.motion =
      inputs.shared_motion
          ? *inputs.shared_motion
          : Render::GL::evaluate_horse_motion(
                *inputs.profile, *inputs.anim, *inputs.rider_ctx,
                Engine::Core::get_or_add_component<
                    Render::Creature::HorseAnimationStateComponent>(
                    inputs.ctx != nullptr ? inputs.ctx->entity : nullptr));

  if (state.motion.is_fighting) {
    state.animation_state = Render::Creature::AnimationStateId::AttackMelee;
  } else {
    switch (state.motion.gait_type) {
    case Render::GL::GaitType::IDLE:
      state.animation_state = Render::Creature::AnimationStateId::Idle;
      break;
    case Render::GL::GaitType::WALK:
      state.animation_state = Render::Creature::AnimationStateId::Walk;
      break;
    case Render::GL::GaitType::TROT:
    case Render::GL::GaitType::CANTER:
    case Render::GL::GaitType::GALLOP:
      state.animation_state = Render::Creature::AnimationStateId::Run;
      break;
    }
  }

  state.clip_id = Render::Creature::Quadruped::clip_for_motion(
      Render::Creature::Quadruped::ClipSet{0U, 1U, 2U, 3U, 4U, 5U},
      state.motion.gait_type, state.motion.is_fighting);
  return state;
}

auto resolve_elephant_motion_state(const ElephantMotionStateInputs &inputs)
    -> PreparedElephantMotionState {
  PreparedElephantMotionState state;
  if (inputs.anim == nullptr || inputs.profile == nullptr) {
    return state;
  }

  state.motion =
      inputs.shared_motion
          ? *inputs.shared_motion
          : Render::GL::evaluate_elephant_motion(
                *inputs.profile, *inputs.anim,
                Engine::Core::get_or_add_component<
                    Render::Creature::ElephantAnimationStateComponent>(
                    inputs.ctx != nullptr ? inputs.ctx->entity : nullptr));
  state.howdah =
      inputs.shared_howdah ? *inputs.shared_howdah : state.motion.howdah;

  if (state.motion.is_fighting) {
    state.animation_state = Render::Creature::AnimationStateId::AttackMelee;
  } else if (!state.motion.is_moving) {
    state.animation_state = Render::Creature::AnimationStateId::Idle;
  } else {
    state.animation_state = inputs.anim->is_running
                                ? Render::Creature::AnimationStateId::Run
                                : Render::Creature::AnimationStateId::Walk;
  }

  return state;
}

auto resolve_humanoid_formation_state(
    const HumanoidFormationStateInputs &inputs)
    -> PreparedHumanoidFormationState {
  PreparedHumanoidFormationState state;
  if (inputs.owner == nullptr || inputs.ctx == nullptr) {
    return state;
  }

  state.formation =
      resolve_humanoid_formation_params(*inputs.owner, *inputs.ctx);

  state.cols = std::max(1, std::min(state.formation.max_per_row,
                                    state.formation.individuals_per_unit));
  state.rows = std::max(
      1, (state.formation.individuals_per_unit + state.cols - 1) / state.cols);
  if (inputs.ctx->force_single_soldier) {
    state.cols = 1;
    state.rows = 1;
  }

  state.mounted = inputs.owner->uses_mounted_pipeline();
  if (inputs.unit != nullptr) {
    using Game::Units::SpawnType;
    auto const spawn_type = inputs.unit->spawn_type;
    state.mounted = state.mounted || spawn_type == SpawnType::MountedKnight ||
                    spawn_type == SpawnType::HorseArcher ||
                    spawn_type == SpawnType::HorseSpearman;
  }

  state.visible_count =
      std::min(state.formation.individuals_per_unit, state.rows * state.cols);
  if (!inputs.ctx->force_single_soldier && inputs.unit != nullptr) {
    int const max_health = std::max(1, inputs.unit->max_health);
    float const ratio =
        std::clamp(inputs.unit->health / float(max_health), 0.0F, 1.0F);
    state.visible_count = std::max(
        1, std::min(state.formation.individuals_per_unit,
                    static_cast<int>(std::ceil(
                        ratio * float(state.formation.individuals_per_unit)))));
  }

  return state;
}

auto prepare_quadruped_frame_state(const QuadrupedFrameStateInputs &inputs)
    -> PreparedQuadrupedFrameState {
  PreparedQuadrupedFrameState state;
  if (inputs.ctx == nullptr || inputs.anim == nullptr) {
    return state;
  }

  state.ctx = *inputs.ctx;
  state.ctx.model = inputs.ctx->model;
  state.ctx.model.translate(inputs.pre_ground_translation);

  if (inputs.use_contact_grounding) {
    float const y_scale =
        state.ctx.model.mapVector(QVector3D(0.0F, 1.0F, 0.0F)).length();
    ground_model_contact_to_surface(state.ctx.model, inputs.contact_y, y_scale);
    if (std::abs(inputs.ground_clearance) > 0.0F) {
      auto const grounded_origin = model_world_origin(state.ctx.model);
      set_model_world_y(state.ctx.model,
                        grounded_origin.y() + inputs.ground_clearance);
    }
  } else {
    ground_model_to_terrain(state.ctx.model);
  }

  CreatureGraphInputs graph_inputs{};
  graph_inputs.ctx = &state.ctx;
  graph_inputs.anim = inputs.anim;
  graph_inputs.entity = inputs.ctx->entity;
  CreatureLodDecision lod_decision{};
  lod_decision.lod = inputs.lod;
  state.graph = build_base_graph_output(graph_inputs, lod_decision);
  state.graph.spec = inputs.spec;
  state.graph.seed = inputs.seed;
  return state;
}

auto resolve_humanoid_lod_state(const HumanoidLodStateInputs &inputs)
    -> PreparedCreatureLodState {
  PreparedCreatureLodState state;
  if (inputs.ctx == nullptr) {
    return state;
  }

  const auto &ctx = *inputs.ctx;
  CreatureLodDecisionInputs lod_in{};
  if (ctx.force_humanoid_lod) {
    lod_in.forced_lod = ctx.forced_humanoid_lod;
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
      state.budget_granted_full =
          (granted == Render::Creature::CreatureLOD::Full);
      lod_in.budget_grant_full = state.budget_granted_full;
    }
  }

  state.decision = decide_creature_lod(lod_in);
  return state;
}

auto prepare_ground_shadow_state(const GroundShadowStateInputs &inputs)
    -> PreparedGroundShadowState {
  PreparedGroundShadowState state;
  if (inputs.ctx == nullptr || inputs.graph == nullptr) {
    return state;
  }

  const auto &ctx = *inputs.ctx;
  const auto &graph = *inputs.graph;
  const auto &gfx_settings = Render::GraphicsSettings::instance();
  const bool should_render_shadow =
      ctx.allow_template_cache && gfx_settings.shadows_enabled() &&
      inputs.lod == Render::Creature::CreatureLOD::Full &&
      inputs.camera_distance < gfx_settings.shadow_max_distance();
  if (!should_render_shadow || ctx.backend == nullptr ||
      ctx.resources == nullptr) {
    return state;
  }

  state.mesh = ctx.resources->quad();
  if (state.mesh == nullptr) {
    state.mesh = nullptr;
    return state;
  }

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
      inputs.shadow_size * inputs.width_scale * width_boost;
  float const shadow_depth =
      inputs.shadow_size * inputs.depth_scale * depth_boost;

  auto &terrain_service = Game::Map::TerrainService::instance();
  if (!terrain_service.is_initialized()) {
    return state;
  }

  QVector3D const shadow_pos = terrain_service.resolve_surface_world_position(
      inputs.world_pos.x(), inputs.world_pos.z(), 0.0F, inputs.world_pos.y());
  float const shadow_y = shadow_pos.y();

  QVector3D light_dir = k_shadow_light_dir.normalized();
  QVector2D light_dir_xz(light_dir.x(), light_dir.z());
  if (light_dir_xz.lengthSquared() < 1e-6F) {
    light_dir_xz = QVector2D(0.0F, 1.0F);
  } else {
    light_dir_xz.normalize();
  }
  QVector2D const shadow_dir = -light_dir_xz;
  QVector2D dir_for_use = shadow_dir;
  if (dir_for_use.lengthSquared() < 1e-6F) {
    dir_for_use = QVector2D(0.0F, 1.0F);
  } else {
    dir_for_use.normalize();
  }

  float const shadow_offset = shadow_depth * 1.25F;
  QVector2D const offset_2d = dir_for_use * shadow_offset;
  float const light_yaw_deg = qRadiansToDegrees(
      std::atan2(double(dir_for_use.x()), double(dir_for_use.y())));

  state.model.translate(inputs.world_pos.x() + offset_2d.x(),
                        shadow_y + k_shadow_ground_offset,
                        inputs.world_pos.z() + offset_2d.y());
  state.model.rotate(light_yaw_deg, 0.0F, 1.0F, 0.0F);
  state.model.rotate(-90.0F, 1.0F, 0.0F, 0.0F);
  state.model.scale(shadow_width, shadow_depth, 1.0F);
  state.alpha = k_shadow_base_alpha;
  state.pass = graph.pass_intent;
  state.enabled = true;
  return state;
}

} // namespace Render::Creature::Pipeline

namespace Render::Humanoid {

auto build_humanoid_locomotion_state(const HumanoidLocomotionInputs &inputs)
    -> HumanoidLocomotionState {
  HumanoidLocomotionState state{};
  state.motion_state =
      Render::GL::classify_motion_state(inputs.anim, inputs.move_speed);
  state.move_speed = inputs.move_speed;
  state.has_movement_target = inputs.has_movement_target;
  state.locomotion_direction = inputs.locomotion_direction;
  state.locomotion_velocity = inputs.locomotion_direction * inputs.move_speed;
  state.movement_target = inputs.movement_target;

  state.gait.state = state.motion_state;
  state.gait.speed = state.move_speed;
  state.gait.velocity = state.locomotion_velocity;
  state.gait.has_target = state.has_movement_target;
  state.gait.is_airborne = false;

  float const reference_speed =
      (state.motion_state == Render::GL::HumanoidMotionState::Run)
          ? Render::GL::k_reference_run_speed
          : Render::GL::k_reference_walk_speed;
  if (inputs.anim.is_moving && reference_speed > 0.0001F) {
    state.gait.normalized_speed =
        std::clamp(state.gait.speed / reference_speed, 0.0F, 1.0F);
  } else {
    state.gait.normalized_speed = 0.0F;
  }

  if (inputs.anim.is_moving) {
    float const base_cycle =
        (state.motion_state == Render::GL::HumanoidMotionState::Run ? 0.56F
                                                                    : 0.92F) /
        std::max(0.1F, inputs.variation.walk_speed_mult);
    state.gait.cycle_time = base_cycle;
    state.gait.cycle_phase =
        std::fmod((inputs.animation_time + inputs.phase_offset) /
                      std::max(0.001F, base_cycle),
                  1.0F);
    state.gait.stride_distance = state.gait.speed * state.gait.cycle_time;
  } else {
    state.gait.cycle_time = 0.0F;
    state.gait.cycle_phase = 0.0F;
    state.gait.stride_distance = 0.0F;
  }

  return state;
}

} // namespace Render::Humanoid
