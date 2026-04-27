#include "prepare.h"

#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../../game/core/world.h"
#include "../../game/map/terrain_service.h"
#include "../../game/systems/nation_id.h"
#include "../../game/units/spawn_type.h"
#include "../../game/units/troop_config.h"
#include "../../game/visuals/team_colors.h"
#include "../creature/archetype_registry.h"
#include "../creature/pipeline/creature_render_graph.h"
#include "../creature/pipeline/lod_decision.h"
#include "../creature/pipeline/preparation_common.h"
#include "../creature/pipeline/prepared_submit.h"
#include "../creature/pipeline/unit_visual_spec.h"
#include "../creature/spec.h"
#include "../entity/registry.h"
#include "../geom/math_utils.h"
#include "../geom/parts.h"
#include "../geom/transforms.h"
#include "../gl/backend.h"
#include "../gl/camera.h"
#include "../gl/humanoid/animation/animation_inputs.h"
#include "../gl/humanoid/animation/gait.h"
#include "../gl/humanoid/humanoid_constants.h"
#include "../gl/primitives.h"
#include "../gl/render_constants.h"
#include "../gl/resources.h"
#include "../graphics_settings.h"
#include "../palette.h"
#include "../pose_palette_cache.h"
#include "../scene_renderer.h"
#include "../submitter.h"
#include "../visibility_budget.h"
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
#include <QVector2D>
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
      params.individuals_per_unit =
          Game::Units::TroopConfig::instance().getIndividualsPerUnit(
              unit->spawn_type);
      params.max_per_row =
          Game::Units::TroopConfig::instance().getMaxUnitsPerRow(
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
  AnimationInputs anim = sample_anim_state(ctx);

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

constexpr float k_shadow_size_infantry = 0.16F;
constexpr float k_shadow_size_mounted = 0.35F;

uint32_t s_current_frame = 0;
constexpr uint32_t k_pose_cache_max_age = 300;
constexpr uint32_t k_layout_cache_max_age = 600;

HumanoidRenderStats s_render_stats;

constexpr float k_shadow_ground_offset = 0.02F;
constexpr float k_shadow_base_alpha = 0.24F;
const QVector3D k_shadow_light_dir(0.4F, 1.0F, 0.25F);

constexpr float k_temporal_skip_distance_reduced = 35.0F;
constexpr float k_temporal_skip_distance_minimal = 45.0F;
constexpr uint32_t k_temporal_skip_period_reduced = 2;
constexpr uint32_t k_temporal_skip_period_minimal = 3;
constexpr float k_unit_ambient_anim_duration = 6.0F;
constexpr float k_unit_cycle_base = 15.0F;
constexpr float k_unit_cycle_range = 10.0F;

inline auto fast_random(uint32_t &state) -> float {
  state = state * 1664525U + 1013904223U;
  return float(state & 0x7FFFFFU) / float(0x7FFFFFU);
}

inline auto hash_u32(std::uint32_t x) -> std::uint32_t {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
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

auto build_humanoid_locomotion_state(const HumanoidLocomotionInputs &inputs)
    -> HumanoidLocomotionState {
  HumanoidLocomotionState state{};
  state.motion_state = classify_motion_state(inputs.anim, inputs.move_speed);
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

  float const reference_speed = (state.motion_state == HumanoidMotionState::Run)
                                    ? k_reference_run_speed
                                    : k_reference_walk_speed;
  if (inputs.anim.is_moving && reference_speed > 0.0001F) {
    state.gait.normalized_speed =
        std::clamp(state.gait.speed / reference_speed, 0.0F, 1.0F);
  } else {
    state.gait.normalized_speed = 0.0F;
  }

  if (inputs.anim.is_moving) {
    float const base_cycle =
        (state.motion_state == HumanoidMotionState::Run ? 0.56F : 0.92F) /
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

void prepare_humanoid_instances(const HumanoidRendererBase &owner,
                                const DrawContext &ctx,
                                const AnimationInputs &anim,
                                std::uint32_t frame_index,
                                HumanoidPreparation &out) {
  using namespace Render::GL;

  FormationParams const formation =
      HumanoidRendererBase::resolve_formation(owner, ctx);

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

  const int rows =
      (formation.individuals_per_unit + formation.max_per_row - 1) /
      formation.max_per_row;
  int cols = formation.max_per_row;
  int effective_rows = rows;
  if (ctx.force_single_soldier) {
    cols = 1;
    effective_rows = 1;
  }

  bool is_mounted_spawn = owner.uses_mounted_pipeline();
  if (unit_comp != nullptr) {
    using Game::Units::SpawnType;
    auto const st = unit_comp->spawn_type;
    is_mounted_spawn = is_mounted_spawn || st == SpawnType::MountedKnight ||
                       st == SpawnType::HorseArcher ||
                       st == SpawnType::HorseSpearman;
  }

  int visible_count = effective_rows * cols;
  if (!ctx.force_single_soldier && unit_comp != nullptr) {
    int const mh = std::max(1, unit_comp->max_health);
    float const ratio = std::clamp(unit_comp->health / float(mh), 0.0F, 1.0F);
    visible_count = std::max(1, (int)std::ceil(ratio * float(rows * cols)));
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

  const QMatrix4x4 k_identity_matrix;

  using Nation = FormationCalculatorFactory::Nation;
  using UnitCategory = FormationCalculatorFactory::UnitCategory;
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

  namespace RCP = Render::Creature::Pipeline;
  const auto lod_config = RCP::humanoid_lod_config_from_settings();
  std::vector<SoldierLayout> soldier_layouts;
  HumanoidLayoutCacheComponent *layout_cache_comp =
      (ctx.entity != nullptr)
          ? Engine::Core::get_or_add_component<HumanoidLayoutCacheComponent>(
                ctx.entity)
          : nullptr;
  bool loaded_cached_layouts = false;
  if (layout_cache_comp != nullptr && layout_cache_comp->valid) {
    auto &entry = *layout_cache_comp;
    bool const matches =
        entry.seed == seed && entry.rows == rows && entry.cols == cols &&
        entry.layout_version == k_humanoid_layout_cache_version &&
        entry.formation.individuals_per_unit ==
            formation.individuals_per_unit &&
        entry.formation.max_per_row == formation.max_per_row &&
        entry.formation.spacing == formation.spacing &&
        entry.nation == nation && entry.category == category &&
        entry.soldiers.size() == static_cast<std::size_t>(visible_count);
    bool const cache_valid =
        !matches ? false
                 : ((anim.is_attacking && anim.is_melee)
                        ? (entry.frame_number == frame_index)
                        : (frame_index - entry.frame_number <=
                           ::Render::GL::k_layout_cache_max_age));
    if (cache_valid) {
      soldier_layouts = entry.soldiers;
      entry.frame_number = frame_index;
      loaded_cached_layouts = true;
    }
  }

  if (!loaded_cached_layouts) {
    soldier_layouts.reserve(static_cast<std::size_t>(visible_count));
    for (int idx = 0; idx < visible_count; ++idx) {
      SoldierLayoutInputs inputs{};
      inputs.idx = idx;
      inputs.row = idx / cols;
      inputs.col = idx % cols;
      inputs.rows = rows;
      inputs.cols = cols;
      inputs.formation_spacing = formation.spacing;
      inputs.seed = seed;
      inputs.force_single_soldier = ctx.force_single_soldier;
      inputs.melee_attack = anim.is_attacking && anim.is_melee;
      inputs.animation_time = anim.time;
      soldier_layouts.push_back(
          build_soldier_layout(*formation_calculator, inputs));
    }

    if (layout_cache_comp != nullptr) {
      layout_cache_comp->soldiers = soldier_layouts;
      layout_cache_comp->formation = formation;
      layout_cache_comp->nation = nation;
      layout_cache_comp->category = category;
      layout_cache_comp->rows = rows;
      layout_cache_comp->cols = cols;
      layout_cache_comp->layout_version = k_humanoid_layout_cache_version;
      layout_cache_comp->seed = seed;
      layout_cache_comp->frame_number = frame_index;
      layout_cache_comp->valid = true;
    }
  }

  for (int idx = 0; idx < visible_count; ++idx) {
    SoldierLayout const &layout =
        soldier_layouts[static_cast<std::size_t>(idx)];
    float const offset_x = layout.offset_x;
    float const offset_z = layout.offset_z;
    uint32_t const inst_seed = layout.inst_seed;
    float const phase_offset = layout.phase_offset;
    float const applied_yaw_offset = layout.yaw_offset;

    QMatrix4x4 inst_model;
    float applied_yaw = applied_yaw_offset;

    if (transform_comp != nullptr) {
      applied_yaw = transform_comp->rotation.y + applied_yaw_offset;
      QMatrix4x4 m = k_identity_matrix;
      m.translate(transform_comp->position.x, transform_comp->position.y,
                  transform_comp->position.z);
      m.rotate(applied_yaw, 0.0F, 1.0F, 0.0F);
      m.scale(transform_comp->scale.x, transform_comp->scale.y,
              transform_comp->scale.z);
      m.translate(offset_x, 0.0F, offset_z);
      inst_model = m;
    } else {
      inst_model = ctx.model;
      inst_model.rotate(applied_yaw, 0.0F, 1.0F, 0.0F);
      inst_model.translate(offset_x, 0.0F, offset_z);
    }

    DrawContext inst_ctx = ctx;
    inst_ctx.model = inst_model;

    VariationParams variation = VariationParams::fromSeed(inst_seed);
    owner.adjust_variation(inst_ctx, inst_seed, variation);
    if (anim.is_running) {
      variation.walk_speed_mult *= 1.25F;
      variation.arm_swing_amp *= 1.12F;
      variation.stance_width *= 0.96F;
      variation.posture_slump =
          std::min(0.16F, variation.posture_slump + 0.020F);
    } else if (anim.is_moving) {
      variation.walk_speed_mult *= 1.05F;
    }

    float const combined_height_scale = height_scale * variation.height_scale;
    if (needs_height_scaling ||
        std::abs(variation.height_scale - 1.0F) > 0.001F) {
      QMatrix4x4 scale_matrix;
      scale_matrix.scale(variation.bulk_scale, combined_height_scale, 1.0F);
      inst_ctx.model = inst_ctx.model * scale_matrix;
    }
    float yaw_rad = qDegreesToRadians(applied_yaw);
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
    if (movement_comp != nullptr) {
      QVector3D velocity(movement_comp->vx, 0.0F, movement_comp->vz);
      move_speed = velocity.length();
      if (move_speed > 1e-4F) {
        locomotion_direction = velocity.normalized();
      }
      has_movement_target = movement_comp->has_target;
      movement_target =
          QVector3D(movement_comp->target_x, 0.0F, movement_comp->target_y);
    }

    HumanoidLocomotionInputs locomotion_inputs{};
    locomotion_inputs.anim = anim;
    locomotion_inputs.variation = variation;
    locomotion_inputs.move_speed = move_speed;
    locomotion_inputs.locomotion_direction = locomotion_direction;
    locomotion_inputs.movement_target = movement_target;
    locomotion_inputs.has_movement_target = has_movement_target;
    locomotion_inputs.animation_time = anim.time;
    locomotion_inputs.phase_offset = phase_offset;
    HumanoidLocomotionState locomotion_state =
        build_humanoid_locomotion_state(locomotion_inputs);
    if (unit_comp != nullptr &&
        unit_comp->spawn_type == Game::Units::SpawnType::Archer &&
        !anim.is_moving && !anim.is_attacking) {
      locomotion_state.gait.cycle_phase = 0.5F;
    }

    HumanoidAnimationContext anim_ctx{};
    anim_ctx.inputs = anim;
    anim_ctx.variation = variation;
    anim_ctx.formation = formation;
    anim_ctx.jitter_seed = phase_offset;

    anim_ctx.entity_forward = forward;
    anim_ctx.entity_right = right;
    anim_ctx.entity_up = up;
    anim_ctx.locomotion_direction = locomotion_state.locomotion_direction;
    anim_ctx.yaw_degrees = applied_yaw;
    anim_ctx.yaw_radians = yaw_rad;
    anim_ctx.has_movement_target = locomotion_state.has_movement_target;
    anim_ctx.move_speed = locomotion_state.move_speed;
    anim_ctx.movement_target = locomotion_state.movement_target;
    anim_ctx.locomotion_velocity = locomotion_state.locomotion_velocity;
    anim_ctx.motion_state = locomotion_state.motion_state;
    anim_ctx.gait = locomotion_state.gait;
    anim_ctx.locomotion_cycle_time = locomotion_state.gait.cycle_time;
    anim_ctx.locomotion_phase = locomotion_state.gait.cycle_phase;
    if (anim.is_attacking) {
      float const attack_offset = phase_offset * 1.5F;
      anim_ctx.attack_phase = std::fmod(anim.time + attack_offset, 1.0F);
      if (ctx.has_attack_variant_override) {
        anim_ctx.inputs.attack_variant = ctx.attack_variant_override;
      } else {
        anim_ctx.inputs.attack_variant =
            static_cast<std::uint8_t>(inst_seed % 3);
      }
    }

    HumanoidPose pose{};
    bool const requires_runtime_pose =
        RCP::pass_intent_from_ctx(inst_ctx) == RCP::RenderPassIntent::Shadow;
    if (requires_runtime_pose) {
      HumanoidRendererBase::compute_locomotion_pose(
          inst_seed, anim.time, locomotion_state.gait, variation, pose);
    }
    auto const visual_spec = owner.visual_spec();
    bool world_already_grounded =
        ctx.skip_ground_offset || requires_runtime_pose;
    if (!ctx.skip_ground_offset && requires_runtime_pose) {
      auto const grounding_archetype =
          (visual_spec.archetype_id != Render::Creature::kInvalidArchetype)
              ? visual_spec.archetype_id
              : Render::Creature::ArchetypeRegistry::kHumanoidBase;
      float const grounded_contact_y =
          RCP::grounded_humanoid_contact_y(grounding_archetype, pose, anim_ctx);
      RCP::ground_model_contact_to_surface(inst_ctx.model, grounded_contact_y,
                                           combined_height_scale,
                                           entity_ground_offset);
    } else if (!world_already_grounded) {
      RCP::ground_model_contact_to_surface(
          inst_ctx.model, 0.0F, combined_height_scale, entity_ground_offset);
    }
    anim_ctx.instance_position =
        inst_ctx.model.map(QVector3D(0.0F, 0.0F, 0.0F));

    QVector3D const soldier_world_pos = anim_ctx.instance_position;

    constexpr float k_soldier_cull_radius = 0.6F;
    if (ctx.camera != nullptr &&
        !ctx.camera->is_in_frustum(soldier_world_pos, k_soldier_cull_radius)) {
      ++s_render_stats.soldiers_skipped_frustum;
      continue;
    }

    RCP::CreatureLodDecisionInputs lod_in{};
    if (ctx.force_humanoid_lod) {
      lod_in.forced_lod =
          static_cast<Render::Creature::CreatureLOD>(ctx.forced_humanoid_lod);
    }
    lod_in.has_camera = (ctx.camera != nullptr);
    float soldier_distance = 0.0F;
    if (ctx.camera != nullptr) {
      soldier_distance =
          (soldier_world_pos - ctx.camera->get_position()).length();
    }
    lod_in.distance = soldier_distance;
    lod_in.thresholds = lod_config.thresholds;
    lod_in.apply_visibility_budget = lod_config.apply_visibility_budget;
    lod_in.budget_grant_full = true;
    lod_in.temporal = lod_config.temporal;
    lod_in.frame_index = frame_index;
    lod_in.instance_seed = inst_seed;

    if (lod_in.apply_visibility_budget && !ctx.force_humanoid_lod &&
        ctx.camera != nullptr) {
      const auto distance_lod =
          RCP::select_distance_lod(soldier_distance, lod_in.thresholds);
      if (distance_lod == Render::Creature::CreatureLOD::Full) {
        const auto granted =
            Render::VisibilityBudgetTracker::instance().request_humanoid_lod(
                HumanoidLOD::Full);
        lod_in.budget_grant_full = (granted == HumanoidLOD::Full);
      }
    }

    const auto lod_decision = RCP::decide_creature_lod(lod_in);
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

    const auto &gfx_settings = Render::GraphicsSettings::instance();
    const bool should_render_shadow =
        ctx.allow_template_cache && gfx_settings.shadows_enabled() &&
        (soldier_lod == HumanoidLOD::Full ||
         soldier_lod == HumanoidLOD::Reduced) &&
        soldier_distance < gfx_settings.shadow_max_distance();

    if (should_render_shadow && inst_ctx.backend != nullptr &&
        inst_ctx.resources != nullptr) {
      auto *shadow_shader =
          inst_ctx.backend->shader(QStringLiteral("troop_shadow"));
      auto *quad_mesh = inst_ctx.resources->quad();

      if (shadow_shader != nullptr && quad_mesh != nullptr) {

        float const shadow_size =
            is_mounted_spawn ? k_shadow_size_mounted : k_shadow_size_infantry;
        float depth_boost = 1.0F;
        float width_boost = 1.0F;
        if (unit_comp != nullptr) {
          using Game::Units::SpawnType;
          switch (unit_comp->spawn_type) {
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
            shadow_size * (is_mounted_spawn ? 1.05F : 1.0F) * width_boost;
        float const shadow_depth =
            shadow_size * (is_mounted_spawn ? 1.30F : 1.10F) * depth_boost;

        auto &terrain_service = Game::Map::TerrainService::instance();

        if (terrain_service.is_initialized()) {

          QVector3D const inst_pos =
              inst_ctx.model.map(QVector3D(0.0F, 0.0F, 0.0F));
          QVector3D const shadow_pos =
              terrain_service.resolve_surface_world_position(
                  inst_pos.x(), inst_pos.z(), 0.0F, inst_pos.y());
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

          QMatrix4x4 shadow_model;
          shadow_model.translate(inst_pos.x() + offset_2d.x(),
                                 shadow_y + k_shadow_ground_offset,
                                 inst_pos.z() + offset_2d.y());
          shadow_model.rotate(light_yaw_deg, 0.0F, 1.0F, 0.0F);
          shadow_model.rotate(-90.0F, 1.0F, 0.0F, 0.0F);
          shadow_model.scale(shadow_width, shadow_depth, 1.0F);

          out.add_post_body_draw(
              graph_output.pass_intent,
              [shadow_shader, quad_mesh, shadow_model,
               dir_for_use](Render::GL::ISubmitter &submitter) {
                if (auto *renderer = dynamic_cast<Renderer *>(&submitter)) {
                  Shader *previous_shader = renderer->get_current_shader();
                  renderer->set_current_shader(shadow_shader);
                  shadow_shader->set_uniform(QStringLiteral("u_lightDir"),
                                             dir_for_use);

                  submitter.mesh(quad_mesh, shadow_model,
                                 QVector3D(0.0F, 0.0F, 0.0F), nullptr,
                                 k_shadow_base_alpha, 0);

                  renderer->set_current_shader(previous_shader);
                }
              });
        }
      }
    }

    switch (soldier_lod) {
    case HumanoidLOD::Full: {

      ++s_render_stats.lod_full;

      if (is_mounted_spawn) {
        owner.append_companion_preparation(inst_ctx, variant, pose, anim_ctx,
                                           inst_seed, graph_output.lod, out);
        break;
      }

      out.bodies.add_humanoid(graph_output, pose, variant, anim_ctx);
      owner.append_companion_preparation(inst_ctx, variant, pose, anim_ctx,
                                         inst_seed, graph_output.lod, out);
      break;
    }

    case HumanoidLOD::Reduced: {

      ++s_render_stats.lod_reduced;

      if (is_mounted_spawn) {
        owner.append_companion_preparation(inst_ctx, variant, pose, anim_ctx,
                                           inst_seed, graph_output.lod, out);
        break;
      }

      out.bodies.add_humanoid(graph_output, pose, variant, anim_ctx);
      owner.append_companion_preparation(inst_ctx, variant, pose, anim_ctx,
                                         inst_seed, graph_output.lod, out);
      break;
    }

    case HumanoidLOD::Minimal:

      ++s_render_stats.lod_minimal;
      out.bodies.add_humanoid(graph_output, pose, variant, anim_ctx);
      break;

    case HumanoidLOD::Billboard:

      break;
    }
  }
}

} // namespace Render::Humanoid
