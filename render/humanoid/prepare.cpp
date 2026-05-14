#include "prepare.h"

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

#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../../game/core/world.h"
#include "../../game/systems/nation_id.h"
#include "../../game/systems/troop_profile_service.h"
#include "../../game/units/spawn_type.h"
#include "../../game/units/troop_config.h"
#include "../../game/visuals/team_colors.h"
#include "../creature/archetype_registry.h"
#include "../creature/bpat/bpat_format.h"
#include "../creature/pipeline/creature_asset.h"
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

namespace Render::GL {

using namespace Render::GL::Geometry;

auto HumanoidRendererBase::resolve_team_tint(const DrawContext& ctx) -> QVector3D {
  QVector3D tunic(0.8F, 0.9F, 1.0F);
  Engine::Core::UnitComponent* unit = nullptr;
  Engine::Core::RenderableComponent* rc = nullptr;

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

auto resolved_individuals_per_unit(const Engine::Core::UnitComponent& unit) -> int {
  if (unit.render_individuals_per_unit_override > 0) {
    return unit.render_individuals_per_unit_override;
  }
  return Game::Units::TroopConfig::instance().get_individuals_per_unit(unit.spawn_type);
}

auto combat_phase_to_attack_phase(const AnimationInputs& anim,
                                  float visual_phase_offset) noexcept -> float {
  auto segment = [](float start, float end, float progress) noexcept {
    return start + (end - start) * std::clamp(progress, 0.0F, 1.0F);
  };
  auto progress_with_offset = [visual_phase_offset](float progress,
                                                    float weight) noexcept -> float {
    return std::clamp(progress + visual_phase_offset * weight, 0.0F, 1.0F);
  };

  switch (anim.combat_phase) {
  case CombatAnimPhase::Advance:
    return segment(
        0.00F, 0.14F, progress_with_offset(anim.combat_phase_progress, 0.90F));
  case CombatAnimPhase::WindUp:
    return segment(
        0.14F, 0.34F, progress_with_offset(anim.combat_phase_progress, 0.75F));
  case CombatAnimPhase::Strike:
    return segment(
        0.34F, 0.52F, progress_with_offset(anim.combat_phase_progress, 0.25F));
  case CombatAnimPhase::Impact:
    return segment(
        0.52F, 0.60F, progress_with_offset(anim.combat_phase_progress, 0.15F));
  case CombatAnimPhase::Recover:
    return segment(
        0.60F, 0.86F, progress_with_offset(anim.combat_phase_progress, 0.80F));
  case CombatAnimPhase::Reposition:
    return segment(
        0.86F, 0.995F, progress_with_offset(anim.combat_phase_progress, 0.70F));
  case CombatAnimPhase::Idle:
  default:
    break;
  }

  if (anim.has_attack_offset) {
    return 0.0F;
  }

  float const attack_offset = anim.has_attack_offset ? anim.attack_offset : 0.0F;
  float phase = std::fmod(anim.time + attack_offset, 1.0F);
  return (phase < 0.0F) ? phase + 1.0F : phase;
}

auto visual_attack_phase_offset(float layout_phase_offset) noexcept -> float {
  return std::clamp((layout_phase_offset - 0.125F) * 1.20F, -0.15F, 0.15F);
}

auto visual_attack_variant(std::uint8_t base_variant,
                           std::uint32_t inst_seed) noexcept -> std::uint8_t {
  constexpr std::uint8_t k_variant_slots =
      Engine::Core::CombatStateComponent::k_attack_variant_seed_slots;
  std::uint8_t const variant_jitter =
      static_cast<std::uint8_t>((inst_seed >> 11U) % 3U);
  return static_cast<std::uint8_t>((base_variant + variant_jitter) % k_variant_slots);
}

auto HumanoidRendererBase::resolve_formation(
    const HumanoidRendererBase& owner, const DrawContext& ctx) -> FormationParams {
  FormationParams params{};
  params.individuals_per_unit = 1;
  params.max_per_row = 1;
  params.spacing = 0.75F;

  if (ctx.entity != nullptr) {
    auto* unit = ctx.entity->get_component<Engine::Core::UnitComponent>();
    if (unit != nullptr) {
      params.individuals_per_unit = resolved_individuals_per_unit(*unit);
      params.max_per_row =
          Game::Units::TroopConfig::instance().get_max_units_per_row(unit->spawn_type);
      if (auto troop_type = Game::Units::spawn_typeToTroopType(unit->spawn_type)) {
        auto const profile = Game::Systems::TroopProfileService::instance().get_profile(
            unit->nation_id, *troop_type);
        params.spacing = resolve_formation_spacing(unit->spawn_type,
                                                   profile.visuals.formation_spacing,
                                                   owner.get_mount_scale());
      } else {
        params.spacing =
            resolve_formation_spacing(unit->spawn_type, 0.0F, owner.get_mount_scale());
      }
    }
  } else if (owner.uses_mounted_pipeline()) {
    params.spacing = resolve_formation_spacing(
        Game::Units::SpawnType::MountedKnight, 0.0F, owner.get_mount_scale());
  }

  return params;
}

namespace {

[[nodiscard]] auto make_runtime_prewarm_ctx(const DrawContext& ctx) -> DrawContext {
  DrawContext runtime_ctx = ctx;
  runtime_ctx.template_prewarm = false;
  runtime_ctx.allow_template_cache = false;
  return runtime_ctx;
}

} // namespace

void HumanoidRendererBase::render(const DrawContext& ctx, ISubmitter& out) const {
  AnimationInputs anim =
      Render::Creature::Pipeline::resolve_humanoid_animation_state(ctx).inputs;

  if (ctx.template_prewarm) {
    render_procedural(make_runtime_prewarm_ctx(ctx), anim, out);
    return;
  }

  render_procedural(ctx, anim, out);
}

void HumanoidRendererBase::render_procedural(const DrawContext& ctx,
                                             const AnimationInputs& anim,
                                             ISubmitter& out) const {
  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      *this, ctx, anim, humanoid_current_frame(), prep);

  Render::Creature::Pipeline::submit_preparation(prep, out);
}

namespace {

uint32_t s_current_frame = 0;
constexpr uint32_t k_pose_cache_max_age = 300;
constexpr uint32_t k_layout_cache_max_age = 600;

HumanoidRenderStats s_render_stats;

inline auto fast_random(uint32_t& state) -> float {
  state = state * 1664525U + 1013904223U;
  return float(state & 0x7FFFFFU) / float(0x7FFFFFU);
}

} // namespace

auto humanoid_current_frame() -> std::uint32_t {
  return s_current_frame;
}

void advance_pose_cache_frame() {
  ++s_current_frame;
}

void clear_humanoid_caches() {
  s_current_frame = 0;
}

auto get_humanoid_render_stats() -> const HumanoidRenderStats& {
  return s_render_stats;
}

void reset_humanoid_render_stats() {
  s_render_stats.reset();
}

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
using Render::GL::VariationParams;

namespace {
constexpr std::uint32_t k_humanoid_layout_cache_version = 3U;
}

auto build_soldier_layout(const IFormationCalculator& formation_calculator,
                          const SoldierLayoutInputs& inputs) -> SoldierLayout {
  SoldierLayout layout{};
  layout.inst_seed = inputs.seed ^ std::uint32_t(inputs.idx * 9176U);

  std::uint32_t rng_state = layout.inst_seed;
  if (!inputs.force_single_soldier) {
    FormationOffset const formation_offset =
        formation_calculator.calculate_offset(inputs.idx,
                                              inputs.row,
                                              inputs.col,
                                              inputs.rows,
                                              inputs.cols,
                                              inputs.formation_spacing,
                                              inputs.seed);
    layout.offset_x = formation_offset.offset_x;
    layout.offset_z = formation_offset.offset_z;
    layout.vertical_jitter = (::Render::GL::fast_random(rng_state) - 0.5F) * 0.03F;
    layout.yaw_offset = formation_offset.yaw_offset +
                        (::Render::GL::fast_random(rng_state) - 0.5F) * 5.0F;
    layout.phase_offset = ::Render::GL::fast_random(rng_state) * 0.25F;
  }

  if (inputs.melee_attack) {
    float const combat_jitter_x =
        (::Render::GL::fast_random(rng_state) - 0.5F) * inputs.formation_spacing * 0.4F;
    float const combat_jitter_z =
        (::Render::GL::fast_random(rng_state) - 0.5F) * inputs.formation_spacing * 0.3F;
    float const sway_time = inputs.animation_time + layout.phase_offset * 2.0F;
    float const sway_x = std::sin(sway_time * 1.5F) * 0.05F;
    float const sway_z = std::cos(sway_time * 1.2F) * 0.04F;
    float const combat_yaw = (::Render::GL::fast_random(rng_state) - 0.5F) * 15.0F;
    layout.offset_x += combat_jitter_x + sway_x;
    layout.offset_z += combat_jitter_z + sway_z;
    layout.yaw_offset += combat_yaw;
  }

  return layout;
}

auto build_humanoid_locomotion_state(const HumanoidLocomotionInputs& inputs)
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
    state.gait.cycle_phase = std::fmod((inputs.animation_time + inputs.phase_offset) /
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

void prepare_humanoid_instances(const HumanoidRendererBase& owner,
                                const DrawContext& ctx,
                                const AnimationInputs& anim,
                                std::uint32_t frame_index,
                                HumanoidPreparation& out) {
  using namespace Render::GL;

  FormationParams const formation = HumanoidRendererBase::resolve_formation(owner, ctx);

  Engine::Core::UnitComponent* unit_comp = nullptr;
  if (ctx.entity != nullptr) {
    unit_comp = ctx.entity->get_component<Engine::Core::UnitComponent>();
  }

  Engine::Core::MovementComponent* movement_comp = nullptr;
  Engine::Core::TransformComponent* transform_comp = nullptr;
  if (ctx.entity != nullptr) {
    movement_comp = ctx.entity->get_component<Engine::Core::MovementComponent>();
    transform_comp = ctx.entity->get_component<Engine::Core::TransformComponent>();
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

  int cols =
      std::max(1, std::min(formation.max_per_row, formation.individuals_per_unit));
  const int rows = std::max(1, (formation.individuals_per_unit + cols - 1) / cols);
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
                       st == SpawnType::HorseArcher || st == SpawnType::HorseSpearman;
  }

  int const total_layout_count =
      std::min(formation.individuals_per_unit, effective_rows * cols);
  int live_count = total_layout_count;
  if (!ctx.force_single_soldier && unit_comp != nullptr) {
    live_count = Engine::Core::resolve_surviving_individual_count(
        unit_comp->health, unit_comp->max_health, formation.individuals_per_unit);
  }
  bool const has_entity_death = anim.is_dying || anim.is_dead;
  int const visible_count = has_entity_death ? std::max(1, live_count) : live_count;
  auto* casualties_comp =
      (ctx.entity != nullptr)
          ? ctx.entity->get_component<Engine::Core::SoldierCasualtyAnimationComponent>()
          : nullptr;
  std::size_t active_casualty_count = 0U;
  if (!ctx.force_single_soldier && casualties_comp != nullptr) {
    active_casualty_count = static_cast<std::size_t>(
        std::count_if(casualties_comp->entries.begin(),
                      casualties_comp->entries.end(),
                      [total_layout_count, visible_count](const auto& entry) {
                        return entry.slot_index < total_layout_count &&
                               entry.slot_index >= visible_count;
                      }));
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

  const IFormationCalculator* formation_calculator =
      FormationCalculatorFactory::get_calculator(nation, category);

  s_render_stats.soldiers_total +=
      visible_count + static_cast<std::uint32_t>(active_casualty_count);

  out.bodies.reserve(out.bodies.size() + static_cast<std::size_t>(visible_count) +
                     active_casualty_count);

  namespace RCP = Render::Creature::Pipeline;
  const auto lod_config = RCP::humanoid_lod_config_from_settings();
  std::vector<SoldierLayout> soldier_layouts;
  HumanoidLayoutCacheComponent* layout_cache_comp =
      (ctx.entity != nullptr)
          ? Engine::Core::get_or_add_component<HumanoidLayoutCacheComponent>(ctx.entity)
          : nullptr;
  bool loaded_cached_layouts = false;
  if (layout_cache_comp != nullptr && layout_cache_comp->valid) {
    auto& entry = *layout_cache_comp;
    bool const matches =
        entry.seed == seed && entry.rows == rows && entry.cols == cols &&
        entry.layout_version == k_humanoid_layout_cache_version &&
        entry.formation.individuals_per_unit == formation.individuals_per_unit &&
        entry.formation.max_per_row == formation.max_per_row &&
        entry.formation.spacing == formation.spacing && entry.nation == nation &&
        entry.category == category &&
        entry.soldiers.size() == static_cast<std::size_t>(total_layout_count);
    bool const cache_valid = !matches ? false
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
    soldier_layouts.reserve(static_cast<std::size_t>(total_layout_count));
    for (int idx = 0; idx < total_layout_count; ++idx) {
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
      soldier_layouts.push_back(build_soldier_layout(*formation_calculator, inputs));
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

  auto append_prepared_soldier = [&](int idx, const AnimationInputs& soldier_anim) {
    SoldierLayout const& layout = soldier_layouts[static_cast<std::size_t>(idx)];
    float const offset_x = layout.offset_x;
    float const offset_z = layout.offset_z;
    uint32_t const inst_seed = layout.inst_seed;
    float const phase_offset = layout.phase_offset;
    float const applied_yaw_offset = layout.yaw_offset;
    bool commander_jump_active = false;
    float commander_jump_phase = 0.0F;
    float commander_jump_height_offset = 0.0F;
    if (auto const* commander_comp =
            ctx.entity != nullptr
                ? ctx.entity->get_component<Engine::Core::CommanderComponent>()
                : nullptr) {
      commander_jump_active = commander_comp->jump_active;
      commander_jump_phase = std::clamp(commander_comp->jump_phase, 0.0F, 1.0F);
      commander_jump_height_offset = std::max(0.0F, commander_comp->jump_height_offset);
    }

    QMatrix4x4 inst_model;
    float applied_yaw = applied_yaw_offset;

    if (transform_comp != nullptr) {
      applied_yaw = transform_comp->rotation.y + applied_yaw_offset;
      QMatrix4x4 m = k_identity_matrix;
      m.translate(transform_comp->position.x,
                  transform_comp->position.y,
                  transform_comp->position.z);
      m.rotate(applied_yaw, 0.0F, 1.0F, 0.0F);
      m.scale(
          transform_comp->scale.x, transform_comp->scale.y, transform_comp->scale.z);
      m.translate(offset_x, 0.0F, offset_z);
      inst_model = m;
    } else {
      inst_model = ctx.model;
      inst_model.rotate(applied_yaw, 0.0F, 1.0F, 0.0F);
      inst_model.translate(offset_x, 0.0F, offset_z);
    }

    DrawContext inst_ctx = ctx;
    inst_ctx.model = inst_model;

    VariationParams variation = VariationParams::from_seed(inst_seed);
    owner.adjust_variation(inst_ctx, inst_seed, variation);
    if (soldier_anim.is_running) {
      variation.walk_speed_mult *= 1.25F;
      variation.arm_swing_amp *= 1.12F;
      variation.stance_width *= 0.96F;
      variation.posture_slump = std::min(0.16F, variation.posture_slump + 0.020F);
    } else if (soldier_anim.is_moving) {
      variation.walk_speed_mult *= 1.05F;
    }

    float const combined_height_scale = height_scale * variation.height_scale;
    if (needs_height_scaling || std::abs(variation.height_scale - 1.0F) > 0.001F) {
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
    locomotion_inputs.anim = soldier_anim;
    locomotion_inputs.variation = variation;
    locomotion_inputs.move_speed = move_speed;
    locomotion_inputs.locomotion_direction = locomotion_direction;
    locomotion_inputs.movement_target = movement_target;
    locomotion_inputs.has_movement_target = has_movement_target;
    locomotion_inputs.animation_time = anim.time;
    locomotion_inputs.phase_offset = phase_offset;
    HumanoidLocomotionState locomotion_state =
        build_humanoid_locomotion_state(locomotion_inputs);
    if (commander_jump_active) {
      locomotion_state.motion_state = Render::GL::HumanoidMotionState::Idle;
      locomotion_state.move_speed = 0.0F;
      locomotion_state.has_movement_target = false;
      locomotion_state.locomotion_velocity = QVector3D(0.0F, 0.0F, 0.0F);
      locomotion_state.gait.state = Render::GL::HumanoidMotionState::Idle;
      locomotion_state.gait.speed = 0.0F;
      locomotion_state.gait.normalized_speed = 0.0F;
      locomotion_state.gait.velocity = QVector3D(0.0F, 0.0F, 0.0F);
      locomotion_state.gait.has_target = false;
      locomotion_state.gait.is_airborne = true;
    }
    if (unit_comp != nullptr &&
        unit_comp->spawn_type == Game::Units::SpawnType::Archer &&
        !soldier_anim.is_moving && !soldier_anim.is_attacking) {
      locomotion_state.gait.cycle_phase = 0.5F;
    }

    HumanoidAnimationContext anim_ctx{};
    anim_ctx.inputs = soldier_anim;
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
    if (soldier_anim.is_attacking) {
      float const attack_visual_offset =
          ctx.force_single_soldier ? 0.0F : visual_attack_phase_offset(phase_offset);
      anim_ctx.attack_phase =
          combat_phase_to_attack_phase(soldier_anim, attack_visual_offset);
      if (ctx.has_attack_variant_override) {
        anim_ctx.inputs.attack_variant = ctx.attack_variant_override;
      } else if (!ctx.force_single_soldier) {
        anim_ctx.inputs.attack_variant =
            visual_attack_variant(soldier_anim.attack_variant, inst_seed);
      }
    }

    if (commander_jump_active) {
      anim_ctx.ambient_idle_type = AmbientIdleType::Jump;
      anim_ctx.ambient_idle_phase = commander_jump_phase;
    } else if (!is_mounted_spawn) {
      bool const is_ambient_idle_eligible =
          !soldier_anim.is_moving && !soldier_anim.is_attacking &&
          !soldier_anim.is_in_hold_mode && !soldier_anim.is_constructing &&
          !soldier_anim.is_healing && !soldier_anim.is_hit_reacting &&
          !soldier_anim.is_dying && !soldier_anim.is_dead;
      if (is_ambient_idle_eligible) {
        AmbientIdleType const ambient_type =
            HumanoidPoseController::get_ambient_idle_type(
                anim.time, inst_seed, soldier_anim.idle_duration);
        if (ambient_type != AmbientIdleType::None) {
          anim_ctx.ambient_idle_type = ambient_type;
          anim_ctx.ambient_idle_phase =
              HumanoidPoseController::compute_ambient_idle_phase(
                  soldier_anim.idle_duration, inst_seed);
        }
      }
    }

    auto const visual_spec = owner.visual_spec();
    HumanoidPose pose{};
    bool const requires_runtime_pose =
        RCP::pass_intent_from_ctx(inst_ctx) == RCP::RenderPassIntent::Shadow;
    if (requires_runtime_pose) {
      HumanoidRendererBase::compute_locomotion_pose(
          inst_seed, anim.time, locomotion_state.gait, variation, pose);

      const float hold_kneel_depth = owner.get_hold_kneel_depth();
      float const effective_kneel =
          hold_transition_amount(soldier_anim) * hold_kneel_depth;
      if (effective_kneel > 1e-4F) {
        HumanoidPoseController kneel_ctrl(pose, anim_ctx);
        kneel_ctrl.kneel(effective_kneel);
      }
      if (soldier_anim.is_constructing) {
        HumanoidPoseController construct_ctrl(pose, anim_ctx);
        construct_ctrl.sword_slash_variant(
            RCP::humanoid_phase_for_anim(anim_ctx),
            RCP::humanoid_clip_variant_for_anim(visual_spec.archetype_id, anim_ctx));
      }
    }
    bool world_already_grounded = ctx.skip_ground_offset || requires_runtime_pose;
    if (!ctx.skip_ground_offset && requires_runtime_pose) {
      auto const* grounding_asset =
          Render::Creature::Pipeline::CreatureAssetRegistry::instance().resolve(
              visual_spec);
      std::uint32_t const grounding_species =
          grounding_asset != nullptr ? grounding_asset->bpat_species_id
                                     : Render::Creature::Bpat::k_species_humanoid;
      auto const grounding_archetype =
          (visual_spec.archetype_id != Render::Creature::k_invalid_archetype)
              ? visual_spec.archetype_id
              : Render::Creature::ArchetypeRegistry::k_humanoid_base;
      float const grounded_contact_y = RCP::grounded_humanoid_contact_y(
          grounding_archetype, grounding_species, pose, anim_ctx);
      RCP::ground_model_contact_to_surface(inst_ctx.model,
                                           grounded_contact_y,
                                           combined_height_scale,
                                           entity_ground_offset);
    } else if (!world_already_grounded) {
      RCP::ground_model_contact_to_surface(
          inst_ctx.model, 0.0F, combined_height_scale, entity_ground_offset);
    }
    if (commander_jump_height_offset > 0.0F) {
      RCP::set_model_world_y(inst_ctx.model,
                             RCP::model_world_origin(inst_ctx.model).y() +
                                 commander_jump_height_offset);
    }
    anim_ctx.instance_position = inst_ctx.model.map(QVector3D(0.0F, 0.0F, 0.0F));

    QVector3D const soldier_world_pos = anim_ctx.instance_position;

    constexpr float k_soldier_cull_radius = 0.6F;
    if (ctx.camera != nullptr &&
        !ctx.camera->is_in_frustum(soldier_world_pos, k_soldier_cull_radius)) {
      ++s_render_stats.soldiers_skipped_frustum;
      return;
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
      return;
    }
    HumanoidLOD soldier_lod = static_cast<HumanoidLOD>(lod_decision.lod);

    ++s_render_stats.soldiers_rendered;

    RCP::CreatureGraphInputs graph_inputs{};
    graph_inputs.ctx = &inst_ctx;
    graph_inputs.anim = &soldier_anim;
    graph_inputs.entity = ctx.entity;
    graph_inputs.unit = unit_comp;
    graph_inputs.transform = transform_comp;
    auto graph_output = RCP::build_base_graph_output(graph_inputs, lod_decision);
    graph_output.spec = visual_spec;
    graph_output.spec.owned_legacy_slots =
        graph_output.spec.owned_legacy_slots |
        Render::Creature::Pipeline::LegacySlotMask::FacialHair;
    if (!graph_output.spec.skip_default_facial_hair_archetype) {
      graph_output.spec.archetype_id = Render::Humanoid::resolve_facial_hair_archetype(
          graph_output.spec.archetype_id, variant);
    }
    graph_output.seed = inst_seed;
    graph_output.world_already_grounded = world_already_grounded;

    RCP::HumanoidShadowStateInputs shadow_inputs{};
    shadow_inputs.ctx = &inst_ctx;
    shadow_inputs.graph = &graph_output;
    shadow_inputs.unit = unit_comp;
    shadow_inputs.soldier_world_pos = soldier_world_pos;
    shadow_inputs.lod = soldier_lod;
    shadow_inputs.camera_distance = lod_state.camera_distance;
    shadow_inputs.mounted = is_mounted_spawn;
    const auto shadow_state = RCP::prepare_humanoid_shadow_state(shadow_inputs);
    if (shadow_state.enabled) {
      if (out.shadow_batch.empty()) {
        out.shadow_batch.init(
            shadow_state.shader, shadow_state.mesh, shadow_state.light_dir);
      }
      out.shadow_batch.add(shadow_state.model, shadow_state.alpha, shadow_state.pass);
    }

    switch (soldier_lod) {
    case HumanoidLOD::Full: {

      ++s_render_stats.lod_full;

      if (is_mounted_spawn) {
        owner.append_companion_preparation(
            inst_ctx, variant, pose, anim_ctx, inst_seed, graph_output.lod, out);
        break;
      }

      RCP::PreparedHumanoidBodyState body_state;
      body_state.graph = graph_output;
      body_state.pose = pose;
      body_state.variant = variant;
      body_state.animation = anim_ctx;
      out.bodies.add_humanoid(body_state);
      owner.append_companion_preparation(
          inst_ctx, variant, pose, anim_ctx, inst_seed, graph_output.lod, out);
      break;
    }

    case HumanoidLOD::Minimal: {

      ++s_render_stats.lod_minimal;
      RCP::PreparedHumanoidBodyState body_state;
      body_state.graph = graph_output;
      body_state.pose = pose;
      body_state.variant = variant;
      body_state.animation = anim_ctx;
      out.bodies.add_humanoid(body_state);
      break;
    }

    case HumanoidLOD::Billboard:

      break;
    }
  };

  for (int idx = 0; idx < visible_count; ++idx) {
    append_prepared_soldier(idx, anim);
  }

  if (!ctx.force_single_soldier && casualties_comp != nullptr) {
    for (const auto& entry : casualties_comp->entries) {
      if (entry.slot_index >= total_layout_count || entry.slot_index < visible_count) {
        continue;
      }

      AnimationInputs casualty_anim{};
      casualty_anim.time = anim.time;
      casualty_anim.death_variant = entry.sequence_variant;
      if (entry.state == Engine::Core::DeathSequenceState::Dying) {
        casualty_anim.is_dying = true;
        casualty_anim.death_progress =
            (entry.state_duration > 0.0F)
                ? std::clamp(entry.state_time / entry.state_duration, 0.0F, 1.0F)
                : 1.0F;
      } else {
        casualty_anim.is_dead = true;
        casualty_anim.death_progress = 1.0F;
      }
      append_prepared_soldier(static_cast<int>(entry.slot_index), casualty_anim);
    }
  }
}

} // namespace Render::Humanoid
