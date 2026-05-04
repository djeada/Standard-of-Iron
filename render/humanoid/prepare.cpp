#include "prepare.h"

#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../../game/core/world.h"
#include "../../game/systems/nation_id.h"
#include "../../game/units/spawn_type.h"
#include "../../game/units/troop_config.h"
#include "../../game/visuals/team_colors.h"
#include "../creature/archetype_registry.h"
#include "../creature/pipeline/creature_prepared_state.h"
#include "../creature/pipeline/creature_render_graph.h"
#include "../creature/pipeline/lod_decision.h"
#include "../creature/pipeline/preparation_common.h"
#include "../creature/pipeline/prepared_submit.h"
#include "../creature/pipeline/unit_visual_spec.h"
#include "../creature/spec.h"
#include "../entity/registry.h"
#include "../geom/parts.h"
#include "../geom/transforms.h"
#include "../gl/backend.h"
#include "../gl/humanoid/animation/animation_inputs.h"
#include "../gl/humanoid/animation/gait.h"
#include "../gl/humanoid/humanoid_constants.h"
#include "../gl/primitives.h"
#include "../gl/render_constants.h"
#include "../graphics_settings.h"
#include "../material.h"
#include "../palette.h"
#include "../pose_palette_cache.h"
#include "../scene_renderer.h"
#include "../submitter.h"
#include "cache_control.h"
#include "facial_hair_catalog.h"
#include "formation_calculator.h"
#include "humanoid_math.h"
#include "humanoid_renderer_base.h"
#include "humanoid_spec.h"
#include "pose_cache_components.h"
#include "pose_controller.h"
#include "render_stats.h"
#include "rig_stats_shim.h"

#include <QMatrix4x4>
#include <QVector4D>
#include <QtMath>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <numbers>
#include <unordered_map>
#include <vector>

namespace Render::GL {

using namespace Render::GL::Geometry;

auto HumanoidRendererBase::resolve_team_tint(const DrawContext &ctx)
    -> QVector3D {
  QVector3D tunic(0.8F, 0.9F, 1.0F);
  Engine::Core::UnitComponent *unit = nullptr;
  Engine::Core::RenderableComponent *rc = nullptr;

  if (ctx.entity != nullptr) {
    unit = ctx.entity->get_component<Engine::Core::UnitComponent>();
    rc = ctx.entity->get_component<Engine::Core::RenderableComponent>();
  }

  if ((unit != nullptr) && unit->owner_id > 0) {
    tunic = Game::Visuals::team_colorForOwner(unit->owner_id);
  } else if (rc != nullptr) {
    tunic = QVector3D(rc->color[0], rc->color[1], rc->color[2]);
  }

  return tunic;
}

auto resolved_individuals_per_unit(const Engine::Core::UnitComponent &unit)
    -> int {
  if (unit.render_individuals_per_unit_override > 0) {
    return unit.render_individuals_per_unit_override;
  }
  return Game::Units::TroopConfig::instance().get_individuals_per_unit(
      unit.spawn_type);
}

auto HumanoidRendererBase::resolve_formation(const HumanoidRendererBase &owner,
                                             const DrawContext &ctx)
    -> FormationParams {
  FormationParams params{};
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
        params.spacing = cavalry_formation_spacing(owner.get_mount_scale());
        break;
      default:
        break;
      }
    }
  } else if (owner.uses_mounted_pipeline()) {
    params.spacing = cavalry_formation_spacing(owner.get_mount_scale());
  }

  return params;
}

namespace {

[[nodiscard]] auto
make_runtime_prewarm_ctx(const DrawContext &ctx) -> DrawContext {
  DrawContext runtime_ctx = ctx;
  runtime_ctx.template_prewarm = false;
  runtime_ctx.allow_template_cache = false;
  return runtime_ctx;
}

} // namespace

void HumanoidRendererBase::render(const DrawContext &ctx,
                                  ISubmitter &out) const {
  AnimationInputs anim =
      Render::Creature::Pipeline::resolve_humanoid_animation_state(ctx).inputs;

  if (ctx.template_prewarm) {
    render_procedural(make_runtime_prewarm_ctx(ctx), anim, out);
    return;
  }

  render_procedural(ctx, anim, out);
}

void HumanoidRendererBase::render_procedural(const DrawContext &ctx,
                                             const AnimationInputs &anim,
                                             ISubmitter &out) const {
  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(*this, ctx, anim,
                                               humanoid_current_frame(), prep);

  Render::Creature::Pipeline::submit_preparation(prep, out);
}

namespace {

uint32_t s_current_frame = 0;
constexpr uint32_t k_pose_cache_max_age = 300;
constexpr uint32_t k_layout_cache_max_age = 600;

HumanoidRenderStats s_render_stats;

inline auto fast_random(uint32_t &state) -> float {
  state = state * 1664525U + 1013904223U;
  return float(state & 0x7FFFFFU) / float(0x7FFFFFU);
}

} // namespace

auto humanoid_current_frame() -> std::uint32_t { return s_current_frame; }

void advance_pose_cache_frame() { ++s_current_frame; }

void clear_humanoid_caches() { s_current_frame = 0; }

auto get_humanoid_render_stats() -> const HumanoidRenderStats & {
  return s_render_stats;
}

void reset_humanoid_render_stats() { s_render_stats.reset(); }

namespace detail {
void increment_facial_hair_skipped_distance() {
  ++s_render_stats.facial_hair_skipped_distance;
}
} // namespace detail

} // namespace Render::GL

namespace Render::Humanoid {

using Render::GL::AmbientIdleType;
using Render::GL::AnimationInputs;
using Render::GL::AnimState;
using Render::GL::classify_motion_state;
using Render::GL::DrawContext;
using Render::GL::elbow_bend_torso;
using Render::GL::FormationCalculatorFactory;
using Render::GL::FormationOffset;
using Render::GL::FormationParams;
using Render::GL::HumanoidAnimationContext;
using Render::GL::HumanoidLOD;
using Render::GL::HumanoidMotionState;
using Render::GL::HumanoidPose;
using Render::GL::HumanoidRendererBase;
using Render::GL::HumanoidVariant;
using Render::GL::IFormationCalculator;
using Render::GL::ISubmitter;
using Render::GL::k_reference_run_speed;
using Render::GL::k_reference_walk_speed;
using Render::GL::PosePaletteCache;
using Render::GL::PosePaletteKey;
using Render::GL::Renderer;
using Render::GL::Shader;
using Render::GL::VariationParams;

namespace {
constexpr std::uint32_t k_humanoid_layout_cache_version = 2U;
}

auto build_soldier_layout(const IFormationCalculator &formation_calculator,
                          const SoldierLayoutInputs &inputs) -> SoldierLayout {
  SoldierLayout layout{};
  layout.inst_seed = inputs.seed ^ std::uint32_t(inputs.idx * 9176U);

  std::uint32_t rng_state = layout.inst_seed;
  if (!inputs.force_single_soldier) {
    FormationOffset const formation_offset =
        formation_calculator.calculate_offset(
            inputs.idx, inputs.row, inputs.col, inputs.rows, inputs.cols,
            inputs.formation_spacing, inputs.seed);
    layout.offset_x = formation_offset.offset_x;
    layout.offset_z = formation_offset.offset_z;
    layout.vertical_jitter =
        (::Render::GL::fast_random(rng_state) - 0.5F) * 0.03F;
    layout.yaw_offset = formation_offset.yaw_offset +
                        (::Render::GL::fast_random(rng_state) - 0.5F) * 5.0F;
    layout.phase_offset = ::Render::GL::fast_random(rng_state) * 0.25F;
  }

  if (inputs.melee_attack) {
    float const combat_jitter_x =
        (::Render::GL::fast_random(rng_state) - 0.5F) *
        inputs.formation_spacing * 0.4F;
    float const combat_jitter_z =
        (::Render::GL::fast_random(rng_state) - 0.5F) *
        inputs.formation_spacing * 0.3F;
    float const sway_time = inputs.animation_time + layout.phase_offset * 2.0F;
    float const sway_x = std::sin(sway_time * 1.5F) * 0.05F;
    float const sway_z = std::cos(sway_time * 1.2F) * 0.04F;
    float const combat_yaw =
        (::Render::GL::fast_random(rng_state) - 0.5F) * 15.0F;
    layout.offset_x += combat_jitter_x + sway_x;
    layout.offset_z += combat_jitter_z + sway_z;
    layout.yaw_offset += combat_yaw;
  }

  return layout;
}

namespace {

struct HumanoidLayoutResolutionInputs {
  const DrawContext *ctx{nullptr};
  const AnimationInputs *anim{nullptr};
  HumanoidLayoutCacheComponent *layout_cache{nullptr};
  const IFormationCalculator *formation_calculator{nullptr};
  FormationParams formation{};
  FormationCalculatorFactory::Nation nation{
      FormationCalculatorFactory::Nation::Roman};
  FormationCalculatorFactory::UnitCategory category{
      FormationCalculatorFactory::UnitCategory::Infantry};
  int rows{1};
  int cols{1};
  int visible_count{1};
  std::uint32_t seed{0U};
  std::uint32_t frame_index{0U};
};

auto resolve_soldier_layouts(const HumanoidLayoutResolutionInputs &inputs)
    -> std::vector<SoldierLayout> {
  std::vector<SoldierLayout> soldier_layouts;
  if (inputs.ctx == nullptr || inputs.anim == nullptr ||
      inputs.formation_calculator == nullptr) {
    return soldier_layouts;
  }

  bool loaded_cached_layouts = false;
  if (inputs.layout_cache != nullptr && inputs.layout_cache->valid) {
    auto &entry = *inputs.layout_cache;
    bool const matches =
        entry.seed == inputs.seed && entry.rows == inputs.rows &&
        entry.cols == inputs.cols &&
        entry.layout_version == k_humanoid_layout_cache_version &&
        entry.formation.individuals_per_unit ==
            inputs.formation.individuals_per_unit &&
        entry.formation.max_per_row == inputs.formation.max_per_row &&
        entry.formation.spacing == inputs.formation.spacing &&
        entry.nation == inputs.nation && entry.category == inputs.category &&
        entry.soldiers.size() == static_cast<std::size_t>(inputs.visible_count);
    bool const cache_valid =
        !matches ? false
                 : ((inputs.anim->is_attacking && inputs.anim->is_melee)
                        ? (entry.frame_number == inputs.frame_index)
                        : (inputs.frame_index - entry.frame_number <=
                           ::Render::GL::k_layout_cache_max_age));
    if (cache_valid) {
      soldier_layouts = entry.soldiers;
      entry.frame_number = inputs.frame_index;
      loaded_cached_layouts = true;
    }
  }

  if (loaded_cached_layouts) {
    return soldier_layouts;
  }

  soldier_layouts.reserve(static_cast<std::size_t>(inputs.visible_count));
  for (int idx = 0; idx < inputs.visible_count; ++idx) {
    SoldierLayoutInputs layout_inputs{};
    layout_inputs.idx = idx;
    layout_inputs.row = idx / inputs.cols;
    layout_inputs.col = idx % inputs.cols;
    layout_inputs.rows = inputs.rows;
    layout_inputs.cols = inputs.cols;
    layout_inputs.formation_spacing = inputs.formation.spacing;
    layout_inputs.seed = inputs.seed;
    layout_inputs.force_single_soldier = inputs.ctx->force_single_soldier;
    layout_inputs.melee_attack =
        inputs.anim->is_attacking && inputs.anim->is_melee;
    layout_inputs.animation_time = inputs.anim->time;
    soldier_layouts.push_back(
        build_soldier_layout(*inputs.formation_calculator, layout_inputs));
  }

  if (inputs.layout_cache != nullptr) {
    inputs.layout_cache->soldiers = soldier_layouts;
    inputs.layout_cache->formation = inputs.formation;
    inputs.layout_cache->nation = inputs.nation;
    inputs.layout_cache->category = inputs.category;
    inputs.layout_cache->rows = inputs.rows;
    inputs.layout_cache->cols = inputs.cols;
    inputs.layout_cache->layout_version = k_humanoid_layout_cache_version;
    inputs.layout_cache->seed = inputs.seed;
    inputs.layout_cache->frame_number = inputs.frame_index;
    inputs.layout_cache->valid = true;
  }

  return soldier_layouts;
}

struct HumanoidInstancePreparationInputs {
  const HumanoidRendererBase *owner{nullptr};
  const DrawContext *ctx{nullptr};
  const AnimationInputs *anim{nullptr};
  Engine::Core::UnitComponent *unit_comp{nullptr};
  Engine::Core::MovementComponent *movement_comp{nullptr};
  Engine::Core::TransformComponent *transform_comp{nullptr};
  FormationParams formation{};
  HumanoidVariant variant{};
  SoldierLayout layout{};
  float height_scale{1.0F};
  bool needs_height_scaling{false};
};

struct HumanoidInstancePreparationState {
  DrawContext inst_ctx{};
  VariationParams variation{};
  HumanoidLocomotionState locomotion_state{};
  HumanoidAnimationContext anim_ctx{};
  float combined_height_scale{1.0F};
  std::uint32_t inst_seed{0U};
};

auto prepare_humanoid_instance_state(
    const HumanoidInstancePreparationInputs &inputs)
    -> HumanoidInstancePreparationState {
  HumanoidInstancePreparationState state{};
  if (inputs.owner == nullptr || inputs.ctx == nullptr ||
      inputs.anim == nullptr) {
    return state;
  }

  float const offset_x = inputs.layout.offset_x;
  float const offset_z = inputs.layout.offset_z;
  float const applied_yaw_offset = inputs.layout.yaw_offset;
  state.inst_seed = inputs.layout.inst_seed;

  QMatrix4x4 inst_model;
  float applied_yaw = applied_yaw_offset;
  const QMatrix4x4 k_identity_matrix;

  if (inputs.transform_comp != nullptr) {
    applied_yaw = inputs.transform_comp->rotation.y + applied_yaw_offset;
    QMatrix4x4 m = k_identity_matrix;
    m.translate(inputs.transform_comp->position.x,
                inputs.transform_comp->position.y,
                inputs.transform_comp->position.z);
    m.rotate(applied_yaw, 0.0F, 1.0F, 0.0F);
    m.scale(inputs.transform_comp->scale.x, inputs.transform_comp->scale.y,
            inputs.transform_comp->scale.z);
    m.translate(offset_x, 0.0F, offset_z);
    inst_model = m;
  } else {
    inst_model = inputs.ctx->model;
    inst_model.rotate(applied_yaw, 0.0F, 1.0F, 0.0F);
    inst_model.translate(offset_x, 0.0F, offset_z);
  }

  state.inst_ctx = *inputs.ctx;
  state.inst_ctx.model = inst_model;

  state.variation = VariationParams::from_seed(state.inst_seed);
  inputs.owner->adjust_variation(state.inst_ctx, state.inst_seed,
                                 state.variation);
  if (inputs.anim->is_running) {
    state.variation.walk_speed_mult *= 1.25F;
    state.variation.arm_swing_amp *= 1.12F;
    state.variation.stance_width *= 0.96F;
    state.variation.posture_slump =
        std::min(0.16F, state.variation.posture_slump + 0.020F);
  } else if (inputs.anim->is_moving) {
    state.variation.walk_speed_mult *= 1.05F;
  }

  state.combined_height_scale =
      inputs.height_scale * state.variation.height_scale;
  if (inputs.needs_height_scaling ||
      std::abs(state.variation.height_scale - 1.0F) > 0.001F) {
    QMatrix4x4 scale_matrix;
    scale_matrix.scale(state.variation.bulk_scale, state.combined_height_scale,
                       1.0F);
    state.inst_ctx.model = state.inst_ctx.model * scale_matrix;
  }

  float const yaw_rad = qDegreesToRadians(applied_yaw);
  QVector3D forward(std::sin(yaw_rad), 0.0F, std::cos(yaw_rad));
  if (forward.lengthSquared() > 1e-8F) {
    forward.normalize();
  } else {
    forward = QVector3D(0.0F, 0.0F, 1.0F);
  }
  QVector3D up(0.0F, 1.0F, 0.0F);
  QVector3D right = QVector3D::crossProduct(up, forward);
  if (right.lengthSquared() > 1e-8F) {
    right.normalize();
  } else {
    right = QVector3D(1.0F, 0.0F, 0.0F);
  }

  QVector3D locomotion_direction = forward;
  bool has_movement_target = false;
  float move_speed = 0.0F;
  QVector3D movement_target(0.0F, 0.0F, 0.0F);
  if (inputs.movement_comp != nullptr) {
    QVector3D velocity(inputs.movement_comp->vx, 0.0F,
                       inputs.movement_comp->vz);
    move_speed = velocity.length();
    if (move_speed > 1e-4F) {
      locomotion_direction = velocity.normalized();
    }
    has_movement_target = inputs.movement_comp->has_target;
    movement_target = QVector3D(inputs.movement_comp->target_x, 0.0F,
                                inputs.movement_comp->target_y);
  }

  HumanoidLocomotionInputs locomotion_inputs{};
  locomotion_inputs.anim = *inputs.anim;
  locomotion_inputs.variation = state.variation;
  locomotion_inputs.move_speed = move_speed;
  locomotion_inputs.locomotion_direction = locomotion_direction;
  locomotion_inputs.movement_target = movement_target;
  locomotion_inputs.has_movement_target = has_movement_target;
  locomotion_inputs.animation_time = inputs.anim->time;
  locomotion_inputs.phase_offset = inputs.layout.phase_offset;
  state.locomotion_state = build_humanoid_locomotion_state(locomotion_inputs);
  if (inputs.unit_comp != nullptr &&
      inputs.unit_comp->spawn_type == Game::Units::SpawnType::Archer &&
      !inputs.anim->is_moving && !inputs.anim->is_attacking) {
    state.locomotion_state.gait.cycle_phase = 0.5F;
  }

  state.anim_ctx.inputs = *inputs.anim;
  state.anim_ctx.variation = state.variation;
  state.anim_ctx.formation = inputs.formation;
  state.anim_ctx.jitter_seed = inputs.layout.phase_offset;
  state.anim_ctx.entity_forward = forward;
  state.anim_ctx.entity_right = right;
  state.anim_ctx.entity_up = up;
  state.anim_ctx.locomotion_direction =
      state.locomotion_state.locomotion_direction;
  state.anim_ctx.yaw_degrees = applied_yaw;
  state.anim_ctx.yaw_radians = yaw_rad;
  state.anim_ctx.has_movement_target =
      state.locomotion_state.has_movement_target;
  state.anim_ctx.move_speed = state.locomotion_state.move_speed;
  state.anim_ctx.movement_target = state.locomotion_state.movement_target;
  state.anim_ctx.locomotion_velocity =
      state.locomotion_state.locomotion_velocity;
  state.anim_ctx.motion_state = state.locomotion_state.motion_state;
  state.anim_ctx.gait = state.locomotion_state.gait;
  state.anim_ctx.locomotion_cycle_time = state.locomotion_state.gait.cycle_time;
  state.anim_ctx.locomotion_phase = state.locomotion_state.gait.cycle_phase;
  if (inputs.anim->is_attacking) {
    float const attack_offset = inputs.layout.phase_offset * 1.5F;
    state.anim_ctx.attack_phase =
        std::fmod(inputs.anim->time + attack_offset, 1.0F);
    if (inputs.ctx->has_attack_variant_override) {
      state.anim_ctx.inputs.attack_variant =
          inputs.ctx->attack_variant_override;
    } else {
      state.anim_ctx.inputs.attack_variant =
          static_cast<std::uint8_t>(state.inst_seed % 3);
    }
  }

  return state;
}

struct HumanoidPosePreparationInputs {
  const HumanoidRendererBase *owner{nullptr};
  const DrawContext *ctx{nullptr};
  const AnimationInputs *anim{nullptr};
  const HumanoidInstancePreparationState *instance_state{nullptr};
  float entity_ground_offset{0.0F};
};

struct HumanoidPosePreparationState {
  DrawContext inst_ctx{};
  HumanoidAnimationContext anim_ctx{};
  HumanoidPose pose{};
  Render::Creature::Pipeline::UnitVisualSpec visual_spec{};
  bool world_already_grounded{true};
  QVector3D soldier_world_pos{};
};

auto prepare_humanoid_pose_state(const HumanoidPosePreparationInputs &inputs)
    -> HumanoidPosePreparationState {
  HumanoidPosePreparationState state{};
  if (inputs.owner == nullptr || inputs.ctx == nullptr ||
      inputs.anim == nullptr || inputs.instance_state == nullptr) {
    return state;
  }

  namespace RCP = Render::Creature::Pipeline;
  state.inst_ctx = inputs.instance_state->inst_ctx;
  state.anim_ctx = inputs.instance_state->anim_ctx;

  bool const requires_runtime_pose =
      RCP::pass_intent_from_ctx(state.inst_ctx) ==
      RCP::RenderPassIntent::Shadow;
  if (requires_runtime_pose) {
    HumanoidRendererBase::compute_locomotion_pose(
        inputs.instance_state->inst_seed, inputs.anim->time,
        inputs.instance_state->locomotion_state.gait,
        inputs.instance_state->variation, state.pose);
  }

  state.visual_spec = inputs.owner->visual_spec();
  state.world_already_grounded =
      inputs.ctx->skip_ground_offset || requires_runtime_pose;
  if (!inputs.ctx->skip_ground_offset && requires_runtime_pose) {
    auto const grounding_archetype =
        (state.visual_spec.archetype_id != Render::Creature::kInvalidArchetype)
            ? state.visual_spec.archetype_id
            : Render::Creature::ArchetypeRegistry::kHumanoidBase;
    float const grounded_contact_y = RCP::grounded_humanoid_contact_y(
        grounding_archetype, state.pose, state.anim_ctx);
    RCP::ground_model_contact_to_surface(
        state.inst_ctx.model, grounded_contact_y,
        inputs.instance_state->combined_height_scale,
        inputs.entity_ground_offset);
  } else if (!state.world_already_grounded) {
    RCP::ground_model_contact_to_surface(
        state.inst_ctx.model, 0.0F,
        inputs.instance_state->combined_height_scale,
        inputs.entity_ground_offset);
  }

  state.anim_ctx.instance_position =
      state.inst_ctx.model.map(QVector3D(0.0F, 0.0F, 0.0F));
  state.soldier_world_pos = state.anim_ctx.instance_position;
  return state;
}

void emit_humanoid_body_for_lod(
    const HumanoidVariant &variant, const HumanoidPose &pose,
    const HumanoidAnimationContext &anim_ctx, bool is_mounted_spawn,
    HumanoidLOD soldier_lod,
    const Render::Creature::Pipeline::CreatureGraphOutput &graph_output,
    const std::function<void()> &append_companion, HumanoidPreparation &out) {
  namespace RCP = Render::Creature::Pipeline;

  switch (soldier_lod) {
  case HumanoidLOD::Full: {
    if (is_mounted_spawn) {
      if (append_companion) {
        append_companion();
      }
      return;
    }

    RCP::PreparedHumanoidBodyState body_state;
    body_state.graph = graph_output;
    body_state.pose = pose;
    body_state.variant = variant;
    body_state.animation = anim_ctx;
    out.bodies.add_humanoid(body_state);
    if (append_companion) {
      append_companion();
    }
    return;
  }

  case HumanoidLOD::Minimal: {
    RCP::PreparedHumanoidBodyState body_state;
    body_state.graph = graph_output;
    body_state.pose = pose;
    body_state.variant = variant;
    body_state.animation = anim_ctx;
    out.bodies.add_humanoid(body_state);
    return;
  }

  case HumanoidLOD::Billboard:
    return;
  }
}

} // namespace

void prepare_humanoid_instances(const HumanoidRendererBase &owner,
                                const DrawContext &ctx,
                                const AnimationInputs &anim,
                                std::uint32_t frame_index,
                                HumanoidPreparation &out) {
  using namespace Render::GL;

  Engine::Core::UnitComponent *unit_comp = nullptr;
  if (ctx.entity != nullptr) {
    unit_comp = ctx.entity->get_component<Engine::Core::UnitComponent>();
  }

  Engine::Core::MovementComponent *movement_comp = nullptr;
  Engine::Core::TransformComponent *transform_comp = nullptr;
  if (ctx.entity != nullptr) {
    movement_comp =
        ctx.entity->get_component<Engine::Core::MovementComponent>();
    transform_comp =
        ctx.entity->get_component<Engine::Core::TransformComponent>();
  }

  float entity_ground_offset =
      owner.resolve_entity_ground_offset(ctx, unit_comp, transform_comp);

  uint32_t seed = 0U;
  if (ctx.has_seed_override) {
    seed = ctx.seed_override;
  } else {
    if (unit_comp != nullptr) {
      seed ^= uint32_t(unit_comp->owner_id * 2654435761U);
    }
    if (ctx.entity != nullptr) {
      seed ^= uint32_t(reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFU);
    }
  }

  HumanoidVariant variant;
  owner.get_variant(ctx, seed, variant);

  if (!owner.m_proportion_scale_cached) {
    owner.m_cached_proportion_scale = owner.get_proportion_scaling();
    owner.m_proportion_scale_cached = true;
  }
  const QVector3D prop_scale = owner.m_cached_proportion_scale;
  const float height_scale = prop_scale.y();
  const bool needs_height_scaling = std::abs(height_scale - 1.0F) > 0.001F;

  using Nation = FormationCalculatorFactory::Nation;
  using UnitCategory = FormationCalculatorFactory::UnitCategory;
  namespace RCP = Render::Creature::Pipeline;
  RCP::HumanoidFormationStateInputs formation_inputs{};
  formation_inputs.owner = &owner;
  formation_inputs.ctx = &ctx;
  formation_inputs.anim = &anim;
  formation_inputs.unit = unit_comp;
  const auto formation_state =
      RCP::resolve_humanoid_formation_state(formation_inputs);
  const FormationParams &formation = formation_state.formation;
  const int rows = formation_state.rows;
  const int cols = formation_state.cols;
  const bool is_mounted_spawn = formation_state.mounted;
  const int visible_count = formation_state.visible_count;

  Nation nation = Nation::Roman;
  if (unit_comp != nullptr &&
      unit_comp->nation_id == Game::Systems::NationID::Carthage) {
    nation = Nation::Carthage;
  }

  UnitCategory category =
      is_mounted_spawn ? UnitCategory::Cavalry : UnitCategory::Infantry;
  if (unit_comp != nullptr &&
      unit_comp->spawn_type == Game::Units::SpawnType::Builder &&
      anim.is_constructing) {
    category = UnitCategory::BuilderConstruction;
  }

  const IFormationCalculator *formation_calculator =
      FormationCalculatorFactory::get_calculator(nation, category);

  s_render_stats.soldiers_total += visible_count;

  out.bodies.reserve(out.bodies.size() +
                     static_cast<std::size_t>(visible_count));
  out.post_body_draws.reserve(out.post_body_draws.size() +
                              static_cast<std::size_t>(visible_count));

  const auto lod_config = RCP::humanoid_lod_config_from_settings();
  HumanoidLayoutCacheComponent *layout_cache_comp =
      (ctx.entity != nullptr)
          ? Engine::Core::get_or_add_component<HumanoidLayoutCacheComponent>(
                ctx.entity)
          : nullptr;
  HumanoidLayoutResolutionInputs layout_inputs{};
  layout_inputs.ctx = &ctx;
  layout_inputs.anim = &anim;
  layout_inputs.layout_cache = layout_cache_comp;
  layout_inputs.formation_calculator = formation_calculator;
  layout_inputs.formation = formation;
  layout_inputs.nation = nation;
  layout_inputs.category = category;
  layout_inputs.rows = rows;
  layout_inputs.cols = cols;
  layout_inputs.visible_count = visible_count;
  layout_inputs.seed = seed;
  layout_inputs.frame_index = frame_index;
  const auto soldier_layouts = resolve_soldier_layouts(layout_inputs);

  for (int idx = 0; idx < visible_count; ++idx) {
    HumanoidInstancePreparationInputs instance_inputs{};
    instance_inputs.owner = &owner;
    instance_inputs.ctx = &ctx;
    instance_inputs.anim = &anim;
    instance_inputs.unit_comp = unit_comp;
    instance_inputs.movement_comp = movement_comp;
    instance_inputs.transform_comp = transform_comp;
    instance_inputs.formation = formation;
    instance_inputs.variant = variant;
    instance_inputs.layout = soldier_layouts[static_cast<std::size_t>(idx)];
    instance_inputs.height_scale = height_scale;
    instance_inputs.needs_height_scaling = needs_height_scaling;
    const auto prepared_instance =
        prepare_humanoid_instance_state(instance_inputs);

    uint32_t const inst_seed = prepared_instance.inst_seed;
    HumanoidPosePreparationInputs pose_inputs{};
    pose_inputs.owner = &owner;
    pose_inputs.ctx = &ctx;
    pose_inputs.anim = &anim;
    pose_inputs.instance_state = &prepared_instance;
    pose_inputs.entity_ground_offset = entity_ground_offset;
    const auto prepared_pose = prepare_humanoid_pose_state(pose_inputs);

    DrawContext inst_ctx = prepared_pose.inst_ctx;
    HumanoidAnimationContext anim_ctx = prepared_pose.anim_ctx;
    HumanoidPose pose = prepared_pose.pose;
    const auto visual_spec = prepared_pose.visual_spec;
    const bool world_already_grounded = prepared_pose.world_already_grounded;
    QVector3D const soldier_world_pos = prepared_pose.soldier_world_pos;

    constexpr float k_soldier_cull_radius = 0.6F;
    if (ctx.camera != nullptr &&
        !ctx.camera->is_in_frustum(soldier_world_pos, k_soldier_cull_radius)) {
      ++s_render_stats.soldiers_skipped_frustum;
      continue;
    }

    RCP::HumanoidLodStateInputs lod_inputs{};
    lod_inputs.ctx = &ctx;
    lod_inputs.soldier_world_pos = soldier_world_pos;
    lod_inputs.config = lod_config;
    lod_inputs.frame_index = frame_index;
    lod_inputs.instance_seed = inst_seed;
    const auto lod_state = RCP::resolve_humanoid_lod_state(lod_inputs);
    const auto lod_decision = lod_state.decision;
    if (lod_decision.culled) {
      if (lod_decision.reason == RCP::CullReason::Billboard) {
        ++s_render_stats.soldiers_skipped_lod;
      } else if (lod_decision.reason == RCP::CullReason::Temporal) {
        ++s_render_stats.soldiers_skipped_temporal;
      }
      continue;
    }
    HumanoidLOD soldier_lod = static_cast<HumanoidLOD>(lod_decision.lod);

    ++s_render_stats.soldiers_rendered;
    switch (soldier_lod) {
    case HumanoidLOD::Full:
      ++s_render_stats.lod_full;
      break;
    case HumanoidLOD::Minimal:
      ++s_render_stats.lod_minimal;
      break;
    case HumanoidLOD::Billboard:
      break;
    }

    RCP::CreatureGraphInputs graph_inputs{};
    graph_inputs.ctx = &inst_ctx;
    graph_inputs.anim = &anim;
    graph_inputs.entity = ctx.entity;
    graph_inputs.unit = unit_comp;
    graph_inputs.transform = transform_comp;
    auto graph_output =
        RCP::build_base_graph_output(graph_inputs, lod_decision);
    graph_output.spec = visual_spec;
    graph_output.spec.owned_legacy_slots =
        graph_output.spec.owned_legacy_slots |
        Render::Creature::Pipeline::LegacySlotMask::FacialHair;
    graph_output.spec.archetype_id =
        Render::Humanoid::resolve_facial_hair_archetype(
            graph_output.spec.archetype_id, variant);
    graph_output.seed = inst_seed;
    graph_output.world_already_grounded = world_already_grounded;

    RCP::GroundShadowStateInputs shadow_inputs{};
    shadow_inputs.ctx = &inst_ctx;
    shadow_inputs.graph = &graph_output;
    shadow_inputs.unit = unit_comp;
    shadow_inputs.world_pos = soldier_world_pos;
    shadow_inputs.lod = soldier_lod;
    shadow_inputs.camera_distance = lod_state.camera_distance;
    const auto shadow_state = RCP::prepare_ground_shadow_state(shadow_inputs);
    if (shadow_state.enabled) {
      out.add_post_body_draw(
          shadow_state.pass, [shadow_state](Render::GL::ISubmitter &submitter) {
            submitter.part(shadow_state.mesh,
                           Render::GL::MaterialRegistry::instance().shadow(),
                           shadow_state.model, QVector3D(0.0F, 0.0F, 0.0F),
                           nullptr, shadow_state.alpha, 0);
          });
    }

    auto append_companion = [&owner, &inst_ctx, &variant, &pose, &anim_ctx,
                             inst_seed, &graph_output, &out]() {
      owner.append_companion_preparation(inst_ctx, variant, pose, anim_ctx,
                                         inst_seed, graph_output.lod, out);
    };

    emit_humanoid_body_for_lod(variant, pose, anim_ctx, is_mounted_spawn,
                               soldier_lod, graph_output, append_companion,
                               out);
  }
}

} // namespace Render::Humanoid
