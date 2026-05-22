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
#include <mutex>
#include <numbers>
#include <optional>
#include <vector>

#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../../game/core/world.h"
#include "../../game/systems/nation_id.h"
#include "../../game/systems/troop_profile_service.h"
#include "../../game/units/spawn_type.h"
#include "../../game/units/troop_config.h"
#include "../../game/visuals/team_colors.h"
#include "../creature/animation_state_components.h"
#include "../creature/archetype_registry.h"
#include "../creature/bpat/bpat_format.h"
#include "../creature/pipeline/creature_asset.h"
#include "../creature/pipeline/creature_prepared_state.h"
#include "../creature/pipeline/creature_render_graph.h"
#include "../creature/pipeline/humanoid_animation_selection.h"
#include "../creature/pipeline/lod_decision.h"
#include "../creature/pipeline/preparation_common.h"
#include "../creature/pipeline/prepared_submit.h"
#include "../creature/pipeline/unit_visual_spec.h"
#include "../creature/pose_intent.h"
#include "../creature/spec.h"
#include "../entity/registry.h"
#include "../geom/parts.h"
#include "../geom/transforms.h"
#include "../gl/backend.h"
#include "../gl/humanoid/animation/animation_inputs.h"
#include "../gl/humanoid/humanoid_constants.h"
#include "../gl/primitives.h"
#include "../gl/render_constants.h"
#include "../graphics_settings.h"
#include "../palette.h"
#include "../profiling/combat_animation_diagnostics.h"
#include "../profiling/frame_profile.h"
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
#include "skeleton.h"

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

auto infantry_guard_shield_pose(
    const Engine::Core::UnitComponent* unit,
    const Engine::Core::FormationModeComponent* formation_mode,
    int row,
    int col,
    int rows,
    int cols) noexcept -> ShieldFormationPose {
  if (unit == nullptr || formation_mode == nullptr || !formation_mode->active ||
      unit->spawn_type != Game::Units::SpawnType::Knight) {
    return ShieldFormationPose::None;
  }

  switch (unit->nation_id) {
  case Game::Systems::NationID::RomanRepublic: {
    bool const is_interior =
        rows > 2 && cols > 2 && row > 0 && row + 1 < rows && col > 0 && col + 1 < cols;
    return is_interior ? ShieldFormationPose::RomanTop
                       : ShieldFormationPose::RomanFront;
  }
  case Game::Systems::NationID::Carthage:
    return ShieldFormationPose::CarthageFront;
  default:
    return ShieldFormationPose::None;
  }
}

auto centered_grid_coordinate(int index, int count) noexcept -> float {
  if (count <= 1) {
    return 0.0F;
  }
  float const normalized = static_cast<float>(index) / static_cast<float>(count - 1);
  return normalized * 2.0F - 1.0F;
}

auto phase_cohort_offset(std::uint32_t inst_seed) noexcept -> float {
  int const cohort = static_cast<int>((inst_seed >> 9U) % 3U) - 1;
  return static_cast<float>(cohort);
}

auto structured_layout_phase_offset(
    int row, int col, int rows, int cols, std::uint32_t inst_seed) noexcept -> float {
  float const row_bias = centered_grid_coordinate(row, rows) * 0.040F;
  float const col_bias = centered_grid_coordinate(col, cols) * 0.018F;
  float const checker_bias = (((row + col) & 1) == 0 ? -1.0F : 1.0F) * 0.012F;
  float const cohort_bias = phase_cohort_offset(inst_seed) * 0.026F;

  std::uint32_t rng_state = inst_seed ^ 0xA511E9B3U;
  float const micro_bias =
      (::Render::Creature::Pipeline::fast_random(rng_state) - 0.5F) * 0.014F;

  return std::clamp(0.125F + row_bias + col_bias + checker_bias + cohort_bias +
                        micro_bias,
                    0.0F,
                    0.25F);
}

auto construction_role_for_variant_index(std::uint8_t variant_index) noexcept
    -> ConstructionRole {
  switch (variant_index) {
  case 0U:
    return ConstructionRole::Hammer;
  case 1U:
    return ConstructionRole::Saw;
  case 2U:
    return ConstructionRole::Chisel;
  case 3U:
    return ConstructionRole::KneelingChisel;
  default:
    return ConstructionRole::Hammer;
  }
}

auto resolve_construction_role(
    const Render::Creature::Pipeline::UnitVisualSpec& visual_spec,
    std::uint32_t inst_seed,
    bool force_single_soldier) noexcept -> ConstructionRole {
  if (force_single_soldier) {
    return ConstructionRole::Hammer;
  }

  auto const* variant_table = visual_spec.animation_manifest.variant_table;
  if (variant_table == nullptr ||
      variant_table->variant_trigger_pose != Render::Creature::PoseIntent::Construct ||
      variant_table->variant_stride == 0U || !variant_table->variant_is_seed_based) {
    return ConstructionRole::Hammer;
  }

  std::uint8_t const variant_index = Render::Creature::Pipeline::seeded_variant_index(
      inst_seed, variant_table->variant_stride);
  return construction_role_for_variant_index(variant_index);
}

void apply_spec_pose_layer(
    const Render::Creature::Pipeline::UnitVisualSpec& visual_spec,
    const DrawContext& ctx,
    const HumanoidAnimationContext& anim_ctx,
    const HumanoidVariant& variant,
    std::uint32_t inst_seed,
    HumanoidPose& pose) {
  auto const pose_layer = visual_spec.animation_manifest.pose_layer;
  if (pose_layer == nullptr) {
    return;
  }

  Render::Creature::Pipeline::HumanoidPoseLayerContext layer_ctx;
  layer_ctx.draw_ctx = &ctx;
  layer_ctx.animation = &anim_ctx;
  layer_ctx.variant = &variant;
  layer_ctx.seed = inst_seed;
  pose_layer(layer_ctx, pose);
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

void HumanoidRendererBase::render(const DrawContext& ctx, ISubmitter& out) const {
  AnimationInputs const anim =
      Render::Creature::Pipeline::resolve_humanoid_animation_state(ctx).inputs;

  if (ctx.template_prewarm) {
    render_procedural(
        Render::Creature::Pipeline::make_runtime_prewarm_ctx(ctx), anim, out);
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
using Render::GL::VariationParams;

namespace {
constexpr std::uint32_t k_humanoid_layout_cache_version = 4U;
constexpr float k_humanoid_idle_cycle_time = 1.6F;
constexpr float k_locomotion_blend_tau = 0.12F;
constexpr float k_cadence_blend_tau = 0.14F;
constexpr float k_run_blend_tau = 0.16F;
constexpr float k_turn_blend_tau = 0.10F;
constexpr float k_acceleration_blend_tau = 0.14F;
constexpr float k_visual_locomotion_speed_epsilon = 1.0e-4F;

struct CommanderJumpPose {
  bool active = false;
  float phase = 0.0F;
  float height_offset = 0.0F;
};

struct CommanderAttackPose {
  bool amplified = false;
  bool has_style = false;
  std::uint8_t style = 0U;
};

class HumanoidPreparationModePolicy {
public:
  explicit HumanoidPreparationModePolicy(const Engine::Core::Entity* entity)
      : m_commander(entity != nullptr
                        ? entity->get_component<Engine::Core::CommanderComponent>()
                        : nullptr)
      , m_action(
            entity != nullptr
                ? entity->get_component<Engine::Core::RpgCommanderActionComponent>()
                : nullptr) {}

  [[nodiscard]] auto jump_pose() const -> CommanderJumpPose {
    if (m_commander == nullptr) {
      return {};
    }
    return {.active = m_commander->jump_active,
            .phase = std::clamp(m_commander->jump_phase, 0.0F, 1.0F),
            .height_offset = std::max(0.0F, m_commander->jump_height_offset)};
  }

  [[nodiscard]] auto
  attack_pose(const AnimationInputs& anim) const -> CommanderAttackPose {
    if (m_commander == nullptr || !m_commander->fpv_controlled || !anim.is_melee) {
      return {};
    }
    CommanderAttackPose pose{.amplified = true};
    if (m_action != nullptr) {
      pose.has_style = true;
      pose.style = m_action->melee_attack_style;
    }
    return pose;
  }

  [[nodiscard]] auto flag_rally_phase() const -> std::optional<float> {
    if (m_commander == nullptr || !m_commander->is_flag_rally_planting()) {
      return std::nullopt;
    }
    if (m_commander->flag_rally_cost <= 0.0F) {
      return 1.0F;
    }
    return std::clamp(
        1.0F - (m_commander->flag_rally_animation_timer / m_commander->flag_rally_cost),
        0.0F,
        1.0F);
  }

private:
  const Engine::Core::CommanderComponent* m_commander = nullptr;
  const Engine::Core::RpgCommanderActionComponent* m_action = nullptr;
};

auto wrap_phase(float phase) noexcept -> float {
  float wrapped = std::fmod(phase, 1.0F);
  if (wrapped < 0.0F) {
    wrapped += 1.0F;
  }
  return wrapped;
}

auto smoothing_alpha(float dt, float tau) noexcept -> float {
  if (dt <= 1.0e-4F) {
    return 1.0F;
  }
  return 1.0F - std::exp(-dt / std::max(tau, 1.0e-4F));
}

auto smooth_towards(float current,
                    float target,
                    float dt,
                    float tau) noexcept -> float {
  return current + (target - current) * smoothing_alpha(dt, tau);
}

auto lerp(float a, float b, float t) noexcept -> float {
  return a + (b - a) * t;
}

void reset_humanoid_locomotion_state(
    Render::Creature::HumanoidAnimationStateComponent& state) {
  state.locomotion_last_sample_time = 0.0F;
  state.locomotion_phase_bias = 0.0F;
  state.locomotion_cycle_time = 0.0F;
  state.locomotion_phase = 0.0F;
  state.filtered_speed = 0.0F;
  state.filtered_acceleration = 0.0F;
  state.filtered_turn = 0.0F;
  state.locomotion_blend = 0.0F;
  state.run_blend = 0.0F;
  state.locomotion_state = HumanoidMotionState::Idle;
  state.locomotion_initialized = false;
  state.combat_visual = {};
}

void sync_combat_visual_inputs(
    AnimationInputs& inputs,
    const Render::Creature::CombatVisualResolvedState& visual) {
  inputs.combat_visual = visual;
  if (!visual.authoritative) {
    return;
  }

  inputs.is_attacking = visual.active;
  inputs.is_melee = visual.is_melee;
  inputs.is_mounted = visual.is_mounted;
  inputs.is_casting = visual.is_casting;
  inputs.finisher_attack = visual.finisher_attack;
  inputs.attack_family = visual.attack_family;
  inputs.attack_variant = visual.attack_variant;

  if (!visual.active) {
    if (visual.lane == Render::Creature::SoldierCombatLane::ShieldBrace) {
      inputs.is_guarding = true;
    } else if (visual.lane == Render::Creature::SoldierCombatLane::StepOut) {
      inputs.movement_state = Render::Creature::MovementAnimationState::Walk;
    }
    inputs.combat_phase = Render::GL::CombatAnimPhase::Idle;
    inputs.combat_phase_progress = 0.0F;
    return;
  }

  auto const scrubbed = Render::Profiling::scrubbed_combat_phase_from_attack_phase(
      visual.attack_phase, visual.amplified_attack, visual.finisher_attack);
  inputs.combat_phase = scrubbed.phase;
  inputs.combat_phase_progress = scrubbed.progress;
}

void apply_combat_micro_variation(const HumanoidAnimationContext& anim_ctx,
                                  std::uint32_t inst_seed,
                                  bool multi_soldier_unit,
                                  HumanoidPose& pose) {
  if (!multi_soldier_unit || !anim_ctx.inputs.combat_visual.authoritative ||
      anim_ctx.inputs.is_dying || anim_ctx.inputs.is_dead) {
    return;
  }

  auto const lane = anim_ctx.inputs.combat_visual.lane;
  float const time = anim_ctx.inputs.time;
  float const attack_phase = std::clamp(anim_ctx.attack_phase, 0.0F, 1.0F);
  float const lane_sign = ((inst_seed >> 7U) & 1U) == 0U ? -1.0F : 1.0F;
  float const breath =
      std::sin(time * 5.3F + static_cast<float>(inst_seed & 31U)) * 0.004F;
  float const pressure = anim_ctx.inputs.is_attacking ? 1.0F : 0.45F;
  float torso_lean = 0.0F;
  float lateral_tilt = 0.0F;
  float shoulder_delay = 0.0F;
  float wrist_angle = 0.0F;
  float foot_adjust = 0.0F;
  float shield_reaction = 0.0F;
  float head_tracking = 0.0F;
  float impact_stagger = 0.0F;

  switch (lane) {
  case Render::Creature::SoldierCombatLane::LeadStrike:
    torso_lean = 0.030F * pressure;
    shoulder_delay = 0.018F;
    wrist_angle = 0.014F;
    foot_adjust = 0.022F;
    head_tracking = 0.010F;
    break;
  case Render::Creature::SoldierCombatLane::SupportStrike:
    torso_lean = 0.018F * pressure;
    shoulder_delay = -0.010F;
    wrist_angle = 0.010F;
    foot_adjust = 0.014F;
    head_tracking = 0.008F;
    break;
  case Render::Creature::SoldierCombatLane::ShieldBrace:
    lateral_tilt = -0.018F * lane_sign;
    shield_reaction = 0.020F;
    foot_adjust = -0.010F;
    head_tracking = -0.006F;
    break;
  case Render::Creature::SoldierCombatLane::StepIn:
    torso_lean = 0.016F;
    foot_adjust = 0.030F;
    head_tracking = 0.006F;
    break;
  case Render::Creature::SoldierCombatLane::StepOut:
    torso_lean = -0.012F;
    foot_adjust = -0.028F;
    lateral_tilt = 0.010F * lane_sign;
    break;
  case Render::Creature::SoldierCombatLane::IdleReady:
    lateral_tilt = 0.008F * lane_sign;
    head_tracking = 0.005F;
    break;
  case Render::Creature::SoldierCombatLane::RangedReload:
    torso_lean = -0.014F;
    shoulder_delay = -0.012F;
    wrist_angle = -0.010F;
    foot_adjust = -0.012F;
    break;
  case Render::Creature::SoldierCombatLane::None:
    break;
  }

  if (anim_ctx.inputs.combat_phase == Render::GL::CombatAnimPhase::Impact ||
      (attack_phase >= 0.50F && attack_phase <= 0.68F)) {
    impact_stagger = 0.016F * pressure;
  }

  pose.neck_base.setY(pose.neck_base.y() + breath);
  pose.head_pos.setY(pose.head_pos.y() + breath * 1.4F);
  pose.shoulder_l.setY(pose.shoulder_l.y() + breath);
  pose.shoulder_r.setY(pose.shoulder_r.y() + breath * 0.9F);

  pose.shoulder_l.setZ(pose.shoulder_l.z() - torso_lean + impact_stagger * 0.3F);
  pose.shoulder_r.setZ(pose.shoulder_r.z() - torso_lean + impact_stagger * 0.3F);
  pose.head_pos.setZ(pose.head_pos.z() - torso_lean * 0.55F - impact_stagger);
  pose.pelvis_pos.setZ(pose.pelvis_pos.z() + torso_lean * 0.30F +
                       impact_stagger * 0.7F);

  pose.shoulder_l.setX(pose.shoulder_l.x() + lateral_tilt + head_tracking);
  pose.shoulder_r.setX(pose.shoulder_r.x() + lateral_tilt + head_tracking);
  pose.head_pos.setX(pose.head_pos.x() + lateral_tilt * 0.55F +
                     head_tracking * lane_sign);
  pose.neck_base.setX(pose.neck_base.x() + lateral_tilt * 0.35F);

  pose.hand_r.setZ(pose.hand_r.z() + shoulder_delay + wrist_angle);
  pose.elbow_r.setZ(pose.elbow_r.z() + shoulder_delay * 0.6F);
  pose.hand_l.setZ(pose.hand_l.z() - shield_reaction);
  pose.elbow_l.setZ(pose.elbow_l.z() - shield_reaction * 0.6F);

  pose.foot_l.setZ(pose.foot_l.z() + foot_adjust);
  pose.knee_l.setZ(pose.knee_l.z() + foot_adjust * 0.6F);
  pose.foot_r.setZ(pose.foot_r.z() - foot_adjust * 0.5F);
  pose.knee_r.setZ(pose.knee_r.z() - foot_adjust * 0.3F);
}

auto reference_cycle_time_for_motion_state(
    HumanoidMotionState state, const VariationParams& variation) noexcept -> float {
  if (state == HumanoidMotionState::Run) {
    return 0.56F / std::max(0.1F, variation.walk_speed_mult);
  }
  if (state == HumanoidMotionState::Walk) {
    return 0.92F / std::max(0.1F, variation.walk_speed_mult);
  }
  return k_humanoid_idle_cycle_time;
}

auto walk_cycle_time_for_speed(float normalized_speed,
                               const VariationParams& variation) noexcept -> float {
  return lerp(1.08F, 0.90F, std::clamp(normalized_speed, 0.0F, 1.0F)) /
         std::max(0.1F, variation.walk_speed_mult);
}

auto run_cycle_time_for_speed(float normalized_speed,
                              const VariationParams& variation) noexcept -> float {
  return lerp(0.62F, 0.52F, std::clamp(normalized_speed, 0.0F, 1.0F)) /
         std::max(0.1F, variation.walk_speed_mult);
}

auto flattened_or(const QVector3D& v, const QVector3D& fallback) noexcept -> QVector3D {
  QVector3D flat(v.x(), 0.0F, v.z());
  if (flat.lengthSquared() <= 1.0e-6F) {
    flat = fallback;
  }
  if (flat.lengthSquared() <= 1.0e-6F) {
    flat = QVector3D(0.0F, 0.0F, 1.0F);
  }
  flat.normalize();
  return flat;
}

struct VisualLocomotionSample {
  QVector3D direction{0.0F, 0.0F, 1.0F};
  float speed{0.0F};
  bool has_movement_target{false};
  QVector3D movement_target{0.0F, 0.0F, 0.0F};
};

auto resolve_visual_locomotion_sample(
    const Render::GL::VisualMovementState& movement_state,
    const QVector3D& entity_forward) noexcept -> VisualLocomotionSample {
  VisualLocomotionSample sample{};
  sample.direction = entity_forward;
  if (Render::Creature::is_moving_animation(movement_state.movement_state)) {
    sample.speed = movement_state.speed_hint;
    sample.direction = movement_state.locomotion_direction;
    sample.has_movement_target = movement_state.has_movement_target;
    sample.movement_target = movement_state.movement_target;
  }

  return sample;
}

auto signed_turn_amount(const QVector3D& entity_forward,
                        const QVector3D& locomotion_direction) noexcept -> float {
  QVector3D const forward = flattened_or(entity_forward, QVector3D(0.0F, 0.0F, 1.0F));
  QVector3D const motion = flattened_or(locomotion_direction, forward);
  return std::clamp(QVector3D::crossProduct(forward, motion).y(), -1.0F, 1.0F);
}

struct LocomotionTargets {
  float normalized_speed{0.0F};
  float locomotion_blend{0.0F};
  float run_blend{0.0F};
  float turn_amount{0.0F};
  float cycle_time{k_humanoid_idle_cycle_time};
  float base_phase{0.0F};
};

auto build_locomotion_targets(const HumanoidLocomotionState& state,
                              const HumanoidLocomotionInputs& inputs) noexcept
    -> LocomotionTargets {
  LocomotionTargets targets{};
  bool const has_locomotion =
      Render::Creature::is_moving_animation(inputs.anim.movement_state);
  float const reference_speed = (state.gait.state == HumanoidMotionState::Run)
                                    ? k_reference_run_speed
                                    : k_reference_walk_speed;
  targets.normalized_speed =
      (has_locomotion && reference_speed > 0.0001F)
          ? std::clamp(state.gait.speed / reference_speed, 0.0F, 1.0F)
          : 0.0F;
  targets.locomotion_blend =
      has_locomotion ? std::clamp(targets.normalized_speed * 1.10F, 0.0F, 1.0F) : 0.0F;
  targets.run_blend = has_locomotion
                          ? std::clamp((state.gait.state == HumanoidMotionState::Run)
                                           ? 0.70F + targets.normalized_speed * 0.30F
                                           : 0.0F,
                                       0.0F,
                                       1.0F)
                          : 0.0F;
  targets.turn_amount = has_locomotion ? signed_turn_amount(inputs.entity_forward,
                                                            inputs.locomotion_direction)
                                       : 0.0F;
  if (has_locomotion) {
    float const walk_cycle_time =
        walk_cycle_time_for_speed(targets.normalized_speed, inputs.variation);
    float const run_cycle_time =
        run_cycle_time_for_speed(targets.normalized_speed, inputs.variation);
    targets.cycle_time = lerp(walk_cycle_time, run_cycle_time, targets.run_blend);
  } else {
    targets.cycle_time = k_humanoid_idle_cycle_time;
  }
  targets.base_phase = wrap_phase((inputs.animation_time + inputs.phase_offset) /
                                  std::max(0.001F, targets.cycle_time));
  return targets;
}

void apply_persistent_locomotion_state(HumanoidLocomotionState& state,
                                       const HumanoidLocomotionInputs& inputs,
                                       const LocomotionTargets& targets) {
  auto* persistent = inputs.persistent_state;
  if (persistent == nullptr) {
    return;
  }

  if (persistent->locomotion_initialized &&
      inputs.animation_time < persistent->locomotion_last_sample_time &&
      inputs.allow_persistent_update) {
    reset_humanoid_locomotion_state(*persistent);
  }

  float delta_time = 0.0F;
  if (persistent->locomotion_initialized) {
    delta_time =
        std::max(0.0F, inputs.animation_time - persistent->locomotion_last_sample_time);
  }

  if (persistent->locomotion_initialized &&
      Render::Creature::is_moving_animation(inputs.anim.movement_state)) {
    float const previous_cycle_time = (persistent->locomotion_cycle_time > 1.0e-4F)
                                          ? persistent->locomotion_cycle_time
                                          : targets.cycle_time;
    state.gait.cycle_time = smooth_towards(
        previous_cycle_time, targets.cycle_time, delta_time, k_cadence_blend_tau);
    state.gait.cycle_phase =
        wrap_phase(persistent->locomotion_phase +
                   delta_time / std::max(0.001F, state.gait.cycle_time));
  } else {
    state.gait.cycle_time = targets.cycle_time;
    state.gait.cycle_phase = targets.base_phase;
  }

  if (persistent->locomotion_initialized) {
    float const previous_filtered_speed = persistent->filtered_speed;
    state.gait.normalized_speed = smooth_towards(previous_filtered_speed,
                                                 targets.normalized_speed,
                                                 delta_time,
                                                 k_locomotion_blend_tau);
    state.gait.locomotion_blend = smooth_towards(persistent->locomotion_blend,
                                                 targets.locomotion_blend,
                                                 delta_time,
                                                 k_locomotion_blend_tau);
    state.gait.run_blend = smooth_towards(
        persistent->run_blend, targets.run_blend, delta_time, k_run_blend_tau);
    state.gait.turn_amount = smooth_towards(
        persistent->filtered_turn, targets.turn_amount, delta_time, k_turn_blend_tau);

    float instant_acceleration = 0.0F;
    if (delta_time > 1.0e-4F) {
      instant_acceleration =
          (state.gait.normalized_speed - previous_filtered_speed) / delta_time;
    }
    state.gait.acceleration =
        smooth_towards(persistent->filtered_acceleration,
                       std::clamp(instant_acceleration, -4.0F, 4.0F),
                       delta_time,
                       k_acceleration_blend_tau);
  }

  if (!inputs.allow_persistent_update) {
    return;
  }

  persistent->locomotion_initialized = true;
  persistent->locomotion_last_sample_time = inputs.animation_time;
  persistent->locomotion_phase = state.gait.cycle_phase;
  persistent->locomotion_phase_bias = state.gait.cycle_phase - targets.base_phase;
  persistent->locomotion_cycle_time = state.gait.cycle_time;
  persistent->filtered_speed = state.gait.normalized_speed;
  persistent->filtered_acceleration = state.gait.acceleration;
  persistent->filtered_turn = state.gait.turn_amount;
  persistent->locomotion_blend = state.gait.locomotion_blend;
  persistent->run_blend = state.gait.run_blend;
  persistent->locomotion_state = state.gait.state;
}
} // namespace

auto build_soldier_layout(const IFormationCalculator& formation_calculator,
                          const SoldierLayoutInputs& inputs) -> SoldierLayout {
  SoldierLayout layout{};
  layout.row_index = static_cast<std::uint8_t>(std::clamp(inputs.row, 0, 255));
  layout.col_index = static_cast<std::uint8_t>(std::clamp(inputs.col, 0, 255));
  layout.rank_band = static_cast<std::uint8_t>(
      (inputs.force_single_soldier || inputs.rows <= 1)
          ? 0
          : ((inputs.row <= 0) ? 0 : ((inputs.row + 1 >= inputs.rows) ? 2 : 1)));
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
    layout.vertical_jitter =
        (::Render::Creature::Pipeline::fast_random(rng_state) - 0.5F) * 0.03F;
    layout.yaw_offset =
        formation_offset.yaw_offset +
        (::Render::Creature::Pipeline::fast_random(rng_state) - 0.5F) * 5.0F;
    layout.phase_offset = Render::GL::structured_layout_phase_offset(
        inputs.row, inputs.col, inputs.rows, inputs.cols, layout.inst_seed);
  }

  if (inputs.melee_attack) {
    float const combat_jitter_x =
        (::Render::Creature::Pipeline::fast_random(rng_state) - 0.5F) *
        inputs.formation_spacing * 0.4F;
    float const combat_jitter_z =
        (::Render::Creature::Pipeline::fast_random(rng_state) - 0.5F) *
        inputs.formation_spacing * 0.3F;
    float const sway_time = inputs.animation_time + layout.phase_offset * 2.0F;
    float const sway_x = std::sin(sway_time * 1.5F) * 0.05F;
    float const sway_z = std::cos(sway_time * 1.2F) * 0.04F;
    float const combat_yaw =
        (::Render::Creature::Pipeline::fast_random(rng_state) - 0.5F) * 15.0F;
    layout.offset_x += combat_jitter_x + sway_x;
    layout.offset_z += combat_jitter_z + sway_z;
    layout.yaw_offset += combat_yaw;
  }

  return layout;
}

auto build_humanoid_locomotion_state(const HumanoidLocomotionInputs& inputs)
    -> HumanoidLocomotionState {
  HumanoidLocomotionState state{};
  auto const gait_state = Render::Creature::resolve_pose(inputs.anim).motion_state;
  state.move_speed = inputs.move_speed;
  state.has_movement_target = inputs.has_movement_target;
  state.locomotion_direction = inputs.locomotion_direction;
  state.locomotion_velocity = inputs.locomotion_direction * inputs.move_speed;
  state.movement_target = inputs.movement_target;

  state.gait.state = gait_state;
  state.gait.speed = state.move_speed;
  state.gait.velocity = state.locomotion_velocity;
  state.gait.has_target = state.has_movement_target;
  state.gait.is_airborne = false;

  LocomotionTargets const targets = build_locomotion_targets(state, inputs);
  state.gait.cycle_time = targets.cycle_time;
  state.gait.cycle_phase = targets.base_phase;
  state.gait.normalized_speed = targets.normalized_speed;
  state.gait.locomotion_blend = targets.locomotion_blend;
  state.gait.run_blend = targets.run_blend;
  state.gait.turn_amount = targets.turn_amount;
  state.gait.acceleration = 0.0F;

  apply_persistent_locomotion_state(state, inputs, targets);

  state.gait.stride_distance =
      Render::Creature::is_moving_animation(inputs.anim.movement_state)
          ? state.gait.speed * state.gait.cycle_time *
                std::max(0.0F, state.gait.locomotion_blend)
          : 0.0F;

  return state;
}

void prepare_humanoid_instances(const HumanoidRendererBase& owner,
                                const DrawContext& ctx,
                                const AnimationInputs& anim,
                                std::uint32_t frame_index,
                                HumanoidPreparation& out) {
  using namespace Render::GL;

  auto& profile = Render::Profiling::global_profile();
  Render::Profiling::AccumulatorScope const prepare_scope(
      ctx.template_prewarm ? nullptr : &profile.humanoid_preparation_us);

  FormationParams const formation = HumanoidRendererBase::resolve_formation(owner, ctx);

  Engine::Core::UnitComponent* unit_comp = nullptr;
  if (ctx.entity != nullptr) {
    unit_comp = ctx.entity->get_component<Engine::Core::UnitComponent>();
  }

  Engine::Core::TransformComponent* transform_comp = nullptr;
  if (ctx.entity != nullptr) {
    transform_comp = ctx.entity->get_component<Engine::Core::TransformComponent>();
  }
  Engine::Core::FormationModeComponent* formation_mode = nullptr;
  if (ctx.entity != nullptr) {
    formation_mode = ctx.entity->get_component<Engine::Core::FormationModeComponent>();
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
  seed_missing_humanoid_wear(variant, seed);

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
  if (unit_comp != nullptr) {
    switch (unit_comp->nation_id) {
    case Game::Systems::NationID::Carthage:
    case Game::Systems::NationID::IronSepulcher:
      nation = Nation::Carthage;
      break;
    case Game::Systems::NationID::RomanRepublic:
    default:
      break;
    }
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
  bool const allow_animation_persistence = should_persist_animation_state(ctx);
  bool preserve_soldier_state_prefix = false;
  bool loaded_cached_layouts = false;
  {
    Render::Profiling::AccumulatorScope const layout_scope(
        ctx.template_prewarm ? nullptr : &profile.soldier_layout_generation_us);

    if (layout_cache_comp != nullptr && layout_cache_comp->valid) {
      auto& entry = *layout_cache_comp;
      preserve_soldier_state_prefix =
          entry.seed == seed && entry.rows == rows && entry.cols == cols &&
          entry.layout_version == k_humanoid_layout_cache_version &&
          entry.formation.individuals_per_unit == formation.individuals_per_unit &&
          entry.formation.max_per_row == formation.max_per_row &&
          entry.formation.spacing == formation.spacing && entry.nation == nation &&
          entry.category == category;
      bool const matches =
          preserve_soldier_state_prefix &&
          entry.soldiers.size() == static_cast<std::size_t>(total_layout_count);
      bool const cache_valid = !matches
                                   ? false
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
  }

  bool const use_per_soldier_locomotion_state = total_layout_count > 1;
  if (layout_cache_comp != nullptr && use_per_soldier_locomotion_state) {
    auto& animation_states = layout_cache_comp->animation_states;
    auto& combat_lanes = layout_cache_comp->combat_lanes;
    std::size_t const previous_state_count = animation_states.size();
    bool const state_count_changed =
        animation_states.size() != static_cast<std::size_t>(total_layout_count);
    animation_states.resize(static_cast<std::size_t>(total_layout_count));
    combat_lanes.resize(static_cast<std::size_t>(total_layout_count));
    if (!preserve_soldier_state_prefix) {
      for (auto& state : animation_states) {
        reset_humanoid_locomotion_state(state);
      }
      for (auto& lane_state : combat_lanes) {
        lane_state = {};
      }
    } else if (state_count_changed) {
      for (std::size_t idx = previous_state_count;
           idx < static_cast<std::size_t>(total_layout_count);
           ++idx) {
        reset_humanoid_locomotion_state(animation_states[idx]);
        combat_lanes[idx] = {};
      }
    }
  }

  auto* humanoid_anim_state =
      ctx.entity != nullptr
          ? ctx.entity
                ->get_component<Render::Creature::HumanoidAnimationStateComponent>()
          : nullptr;
  Render::GL::VisualMovementState const visual_movement =
      Render::GL::visual_movement_for_animation_inputs(ctx, anim);
  float const local_enemy_pressure = anim.is_in_melee_lock
                                         ? 1.0F
                                         : (visual_movement.attack_target_in_range
                                                ? 0.7F
                                                : (anim.is_attacking ? 0.35F : 0.0F));
  const HumanoidPreparationModePolicy preparation_mode(ctx.entity);
  float const unit_health_ratio =
      (unit_comp != nullptr && unit_comp->max_health > 0)
          ? std::clamp(static_cast<float>(unit_comp->health) /
                           static_cast<float>(unit_comp->max_health),
                       0.0F,
                       1.0F)
          : 1.0F;
  std::uint32_t unit_attack_target_id = 0U;
  bool unit_attack_target_alive = false;
  if (ctx.entity != nullptr) {
    if (auto* attack_target =
            ctx.entity->get_component<Engine::Core::AttackTargetComponent>();
        attack_target != nullptr && attack_target->target_id > 0U) {
      unit_attack_target_id = attack_target->target_id;
      if (ctx.world != nullptr) {
        if (auto* target_entity = ctx.world->get_entity(attack_target->target_id);
            target_entity != nullptr &&
            !target_entity->has_component<Engine::Core::PendingRemovalComponent>()) {
          if (auto* death_anim =
                  target_entity->get_component<Engine::Core::DeathAnimationComponent>();
              death_anim == nullptr ||
              death_anim->state == Engine::Core::DeathSequenceState::Dying) {
            unit_attack_target_alive = (death_anim == nullptr);
          }
        }
      }
    }
  }
  auto record_soldier_debug = [&](int idx,
                                  const AnimationInputs& /*raw_anim*/,
                                  const AnimationInputs& resolved_anim,
                                  float attack_phase,
                                  Render::Creature::AnimationStateId animation_state,
                                  HumanoidLOD lod,
                                  Render::Profiling::SoldierCullReason cull_reason,
                                  bool transient_recovery_override) {
    if (ctx.template_prewarm || ctx.entity == nullptr) {
      return;
    }

    Render::Profiling::SoldierAnimationDebugSample sample{};
    sample.soldier_index = idx;
    sample.sample_time = resolved_anim.time;
    sample.combat_phase = resolved_anim.combat_phase;
    sample.combat_phase_progress = resolved_anim.combat_phase_progress;
    sample.attack_phase = attack_phase;
    sample.attack_variant = resolved_anim.attack_variant;
    sample.is_attacking = resolved_anim.is_attacking;
    sample.is_in_melee_lock = resolved_anim.is_in_melee_lock;
    sample.transient_recovery_override = transient_recovery_override;
    sample.locomotion_state = resolved_anim.movement_state;
    sample.animation_state = animation_state;
    sample.lod = static_cast<std::uint8_t>(lod);
    sample.cull_reason = cull_reason;
    Render::Profiling::CombatAnimationDiagnostics::instance().record_soldier_sample(
        ctx.entity->get_id(), sample);
  };

  auto append_prepared_soldier = [&](int idx, const AnimationInputs& soldier_anim) {
    SoldierLayout const& layout = soldier_layouts[static_cast<std::size_t>(idx)];
    AnimationInputs soldier_render_anim = soldier_anim;
    if (!is_mounted_spawn && guard_pose_amount(soldier_render_anim) > 0.0F) {
      int const row = idx / cols;
      int const col = idx % cols;
      soldier_render_anim.shield_formation_pose =
          infantry_guard_shield_pose(unit_comp, formation_mode, row, col, rows, cols);
    }
    float const offset_x = layout.offset_x;
    float const offset_z = layout.offset_z;
    uint32_t const inst_seed = layout.inst_seed;
    float const phase_offset = layout.phase_offset;
    float const applied_yaw_offset = layout.yaw_offset;
    const CommanderJumpPose commander_jump = preparation_mode.jump_pose();
    bool const soldier_has_locomotion =
        Render::Creature::is_moving_animation(soldier_render_anim.movement_state);
    bool const soldier_is_running =
        Render::Creature::is_running_animation(soldier_render_anim.movement_state);

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
    if (soldier_is_running) {
      variation.walk_speed_mult *= 1.25F;
      variation.arm_swing_amp *= 1.12F;
      variation.stance_width *= 0.96F;
      variation.posture_slump = std::min(0.16F, variation.posture_slump + 0.020F);
    } else if (soldier_has_locomotion) {
      variation.walk_speed_mult *= 1.05F;
    }

    float const combined_height_scale = height_scale * variation.height_scale;
    if (needs_height_scaling || std::abs(variation.height_scale - 1.0F) > 0.001F) {
      QMatrix4x4 scale_matrix;
      scale_matrix.scale(variation.bulk_scale, combined_height_scale, 1.0F);
      inst_ctx.model = inst_ctx.model * scale_matrix;
    }
    float const yaw_rad = qDegreesToRadians(applied_yaw);
    QVector3D forward(std::sin(yaw_rad), 0.0F, std::cos(yaw_rad));
    if (forward.lengthSquared() > 1e-8F) {
      forward.normalize();
    } else {
      forward = QVector3D(0.0F, 0.0F, 1.0F);
    }
    QVector3D const up(0.0F, 1.0F, 0.0F);
    QVector3D right = QVector3D::crossProduct(up, forward);
    if (right.lengthSquared() > 1e-8F) {
      right.normalize();
    } else {
      right = QVector3D(1.0F, 0.0F, 0.0F);
    }

    VisualLocomotionSample const visual_locomotion =
        resolve_visual_locomotion_sample(visual_movement, forward);

    auto* locomotion_persistent_state = humanoid_anim_state;
    Render::Creature::SoldierCombatLaneState* cached_lane_state = nullptr;
    if (layout_cache_comp != nullptr && use_per_soldier_locomotion_state) {
      auto& animation_states = layout_cache_comp->animation_states;
      auto const state_index = static_cast<std::size_t>(idx);
      if (state_index < animation_states.size()) {
        locomotion_persistent_state = &animation_states[state_index];
      }
      auto& combat_lanes = layout_cache_comp->combat_lanes;
      if (state_index < combat_lanes.size()) {
        cached_lane_state = &combat_lanes[state_index];
      }
    }

    const CommanderAttackPose commander_attack =
        preparation_mode.attack_pose(soldier_render_anim);
    std::uint8_t base_attack_variant = soldier_render_anim.attack_variant;
    if (ctx.has_attack_variant_override) {
      base_attack_variant = ctx.attack_variant_override;
    } else if (commander_attack.has_style) {
      base_attack_variant = commander_attack.style;
    }

    Render::Creature::CombatLaneInputs lane_inputs{};
    lane_inputs.unit_seed = seed;
    lane_inputs.soldier_seed = inst_seed;
    lane_inputs.row = static_cast<int>(layout.row_index);
    lane_inputs.col = static_cast<int>(layout.col_index);
    lane_inputs.rows = rows;
    lane_inputs.cols = cols;
    lane_inputs.health_ratio = unit_health_ratio;
    lane_inputs.local_enemy_pressure = local_enemy_pressure;
    lane_inputs.force_single_soldier = ctx.force_single_soldier;
    lane_inputs.is_melee = soldier_render_anim.is_melee;
    lane_inputs.is_mounted = is_mounted_spawn;
    lane_inputs.attack_family = soldier_render_anim.attack_family;

    Render::Creature::SoldierCombatLaneState const lane_snapshot =
        (cached_lane_state != nullptr) ? *cached_lane_state
                                       : Render::Creature::SoldierCombatLaneState{};
    auto const lane_resolution =
        Render::Creature::resolve_soldier_combat_lane(lane_snapshot, lane_inputs);
    if (allow_animation_persistence && cached_lane_state != nullptr) {
      *cached_lane_state = lane_resolution.state;
    }

    Render::Creature::CombatVisualPersistentState previous_combat_visual{};
    if (locomotion_persistent_state != nullptr) {
      previous_combat_visual = locomotion_persistent_state->combat_visual;
    }

    Render::Creature::CombatVisualRawInputs raw_combat{};
    raw_combat.sample_time = soldier_render_anim.time;
    raw_combat.attack_offset = soldier_render_anim.attack_offset;
    raw_combat.has_attack_offset = soldier_render_anim.has_attack_offset;
    raw_combat.attack_requested = soldier_render_anim.is_attacking;
    raw_combat.is_melee = soldier_render_anim.is_melee;
    raw_combat.is_mounted = is_mounted_spawn;
    raw_combat.is_casting = soldier_render_anim.is_casting;
    raw_combat.finisher_attack = soldier_render_anim.finisher_attack;
    raw_combat.amplified_attack = commander_attack.amplified;
    raw_combat.is_hit_reacting = soldier_render_anim.is_hit_reacting;
    raw_combat.is_healing = soldier_render_anim.is_healing;
    raw_combat.is_constructing = soldier_render_anim.is_constructing;
    raw_combat.is_dying = soldier_render_anim.is_dying;
    raw_combat.is_dead = soldier_render_anim.is_dead;
    raw_combat.locomotion = soldier_render_anim.movement_state;
    raw_combat.combat_phase = soldier_render_anim.combat_phase;
    raw_combat.combat_phase_progress = soldier_render_anim.combat_phase_progress;
    raw_combat.attack_variant = base_attack_variant;
    raw_combat.attack_target_id = unit_attack_target_id;
    raw_combat.attack_target_alive = unit_attack_target_alive;
    raw_combat.attack_family = soldier_render_anim.attack_family;

    auto const combat_resolution = Render::Creature::resolve_combat_visual_state(
        previous_combat_visual, raw_combat, lane_resolution.profile);
    if (allow_animation_persistence && locomotion_persistent_state != nullptr) {
      locomotion_persistent_state->combat_visual = combat_resolution.persistent;
    }
    sync_combat_visual_inputs(soldier_render_anim, combat_resolution.resolved);
    bool const render_has_locomotion =
        Render::Creature::is_moving_animation(soldier_render_anim.movement_state);

    HumanoidLocomotionInputs locomotion_inputs{};
    locomotion_inputs.anim = soldier_render_anim;
    locomotion_inputs.variation = variation;
    locomotion_inputs.move_speed = visual_locomotion.speed;
    locomotion_inputs.entity_forward = forward;
    locomotion_inputs.locomotion_direction = visual_locomotion.direction;
    locomotion_inputs.movement_target = visual_locomotion.movement_target;
    locomotion_inputs.has_movement_target = visual_locomotion.has_movement_target;
    locomotion_inputs.animation_time = anim.time;
    locomotion_inputs.phase_offset = phase_offset;
    locomotion_inputs.persistent_state = locomotion_persistent_state;
    locomotion_inputs.allow_persistent_update = allow_animation_persistence;
    HumanoidLocomotionState locomotion_state =
        build_humanoid_locomotion_state(locomotion_inputs);
    if (commander_jump.active) {
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
        !render_has_locomotion && !soldier_render_anim.is_attacking) {
      locomotion_state.gait.cycle_phase = 0.5F;
    }

    auto const visual_spec = owner.visual_spec();
    ConstructionRole construction_role = ConstructionRole::None;
    if (soldier_render_anim.is_constructing) {
      construction_role =
          resolve_construction_role(visual_spec, inst_seed, ctx.force_single_soldier);
    }

    HumanoidAnimationContext anim_ctx{};
    anim_ctx.inputs = soldier_render_anim;
    anim_ctx.inputs.is_mounted = is_mounted_spawn;
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
    anim_ctx.gait = locomotion_state.gait;
    anim_ctx.locomotion_cycle_time = locomotion_state.gait.cycle_time;
    anim_ctx.locomotion_phase = locomotion_state.gait.cycle_phase;
    anim_ctx.construction_role = construction_role;
    anim_ctx.attack_phase = anim_ctx.inputs.combat_visual.attack_phase;
    anim_ctx.attack_emphasis = anim_ctx.inputs.combat_visual.attack_emphasis;
    anim_ctx.amplified_attack = anim_ctx.inputs.combat_visual.amplified_attack;
    anim_ctx.finisher_attack = anim_ctx.inputs.combat_visual.finisher_attack;

    if (commander_jump.active) {
      anim_ctx.ambient_idle_type = AmbientIdleType::Jump;
      anim_ctx.ambient_idle_phase = commander_jump.phase;
    } else if (auto const rally_phase = preparation_mode.flag_rally_phase();
               rally_phase.has_value()) {
      anim_ctx.ambient_idle_type = AmbientIdleType::PlantFlag;
      anim_ctx.ambient_idle_phase = *rally_phase;
    } else if (!is_mounted_spawn) {
      bool const is_ambient_idle_eligible =
          !render_has_locomotion && !anim_ctx.inputs.is_attacking &&
          !anim_ctx.inputs.is_in_hold_mode && !anim_ctx.inputs.is_guarding &&
          !anim_ctx.inputs.is_exiting_guard && !anim_ctx.inputs.is_constructing &&
          !anim_ctx.inputs.is_healing && !anim_ctx.inputs.is_hit_reacting &&
          !anim_ctx.inputs.is_dying && !anim_ctx.inputs.is_dead;
      if (is_ambient_idle_eligible) {
        AmbientIdleType const ambient_type =
            HumanoidPoseController::get_ambient_idle_type(
                anim.time, inst_seed, anim_ctx.inputs.idle_duration);
        if (ambient_type != AmbientIdleType::None) {
          anim_ctx.ambient_idle_type = ambient_type;
          anim_ctx.ambient_idle_phase =
              HumanoidPoseController::compute_ambient_idle_phase(
                  anim_ctx.inputs.idle_duration, inst_seed);
        }
      }
    }

    HumanoidPose pose{};
    bool const requires_runtime_pose =
        RCP::pass_intent_from_ctx(inst_ctx) == RCP::RenderPassIntent::Shadow;
    if (requires_runtime_pose) {
      HumanoidRendererBase::compute_locomotion_pose(
          inst_seed, anim.time, locomotion_state.gait, variation, pose);

      const float hold_kneel_depth = owner.get_hold_kneel_depth();
      float const effective_kneel =
          hold_transition_amount(anim_ctx.inputs) * hold_kneel_depth;
      if (effective_kneel > 1e-4F) {
        HumanoidPoseController kneel_ctrl(pose, anim_ctx);
        kneel_ctrl.kneel(effective_kneel);
      }
      float const guard_amount = guard_pose_amount(anim_ctx.inputs);
      if (guard_amount > 0.0F && !render_has_locomotion &&
          !anim_ctx.inputs.is_attacking) {
        HumanoidPoseController guard_ctrl(pose, anim_ctx);
        guard_ctrl.guard_sword_and_shield_formation(
            anim_ctx.inputs.shield_formation_pose, guard_amount);
      }
      if (anim_ctx.inputs.is_constructing) {
        HumanoidPoseController construct_ctrl(pose, anim_ctx);
        float const construct_phase = RCP::humanoid_phase_for_anim(anim_ctx);
        switch (construction_role) {
        case ConstructionRole::Saw:
          construct_ctrl.construction_saw(construct_phase);
          break;
        case ConstructionRole::Chisel:
          construct_ctrl.construction_chisel(construct_phase, false);
          break;
        case ConstructionRole::KneelingChisel:
          construct_ctrl.kneel(0.84F);
          construct_ctrl.construction_chisel(construct_phase, true);
          break;
        case ConstructionRole::Hammer:
        case ConstructionRole::None:
          construct_ctrl.sword_slash_variant(
              construct_phase,
              RCP::humanoid_clip_variant_for_anim(visual_spec.archetype_id, anim_ctx));
          break;
        }
      }

      apply_spec_pose_layer(visual_spec, inst_ctx, anim_ctx, variant, inst_seed, pose);
      apply_combat_micro_variation(anim_ctx,
                                   inst_seed,
                                   !ctx.force_single_soldier && total_layout_count > 1,
                                   pose);
    }
    bool const world_already_grounded = ctx.skip_ground_offset || requires_runtime_pose;
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
    if (commander_jump.height_offset > 0.0F) {
      RCP::set_model_world_y(inst_ctx.model,
                             RCP::model_world_origin(inst_ctx.model).y() +
                                 commander_jump.height_offset);
    }
    anim_ctx.instance_position = inst_ctx.model.map(QVector3D(0.0F, 0.0F, 0.0F));

    QVector3D const soldier_world_pos = anim_ctx.instance_position;

    constexpr float k_soldier_cull_radius = 0.6F;
    if (ctx.camera != nullptr &&
        !ctx.camera->is_in_frustum(soldier_world_pos, k_soldier_cull_radius)) {
      ++s_render_stats.soldiers_skipped_frustum;
      record_soldier_debug(
          idx,
          soldier_render_anim,
          anim_ctx.inputs,
          anim_ctx.attack_phase,
          Render::Creature::resolve_pose(anim_ctx.inputs).animation_state,
          HumanoidLOD::Billboard,
          Render::Profiling::SoldierCullReason::Frustum,
          false);
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
      auto const cull_reason = lod_decision.reason == RCP::CullReason::Temporal
                                   ? Render::Profiling::SoldierCullReason::Temporal
                                   : Render::Profiling::SoldierCullReason::Billboard;
      if (lod_decision.reason == RCP::CullReason::Billboard) {
        ++s_render_stats.soldiers_skipped_lod;
      } else if (lod_decision.reason == RCP::CullReason::Temporal) {
        ++s_render_stats.soldiers_skipped_temporal;
      }
      record_soldier_debug(
          idx,
          soldier_render_anim,
          anim_ctx.inputs,
          anim_ctx.attack_phase,
          Render::Creature::resolve_pose(anim_ctx.inputs).animation_state,
          static_cast<HumanoidLOD>(lod_decision.lod),
          cull_reason,
          false);
      return;
    }
    auto const soldier_lod = static_cast<HumanoidLOD>(lod_decision.lod);

    ++s_render_stats.soldiers_rendered;

    RCP::CreatureGraphInputs graph_inputs{};
    graph_inputs.ctx = &inst_ctx;
    graph_inputs.anim = &soldier_render_anim;
    graph_inputs.entity = ctx.entity;
    graph_inputs.unit = unit_comp;
    graph_inputs.transform = transform_comp;
    auto graph_output = RCP::build_base_graph_output(graph_inputs, lod_decision);
    graph_output.spec = RCP::finalize_visible_humanoid_spec(
        visual_spec, variant, soldier_render_anim, render_has_locomotion);
    graph_output.seed = inst_seed;
    graph_output.world_already_grounded = world_already_grounded;
    graph_output.humanoid_selection = RCP::resolve_humanoid_animation_selection(
        graph_output.spec, anim_ctx, graph_output.seed, &variant);
    bool const transient_recovery_override =
        (soldier_render_anim.is_attacking != anim_ctx.inputs.is_attacking) ||
        (soldier_render_anim.combat_phase != anim_ctx.inputs.combat_phase) ||
        (std::abs(soldier_render_anim.combat_phase_progress -
                  anim_ctx.inputs.combat_phase_progress) > 1.0e-4F);
    record_soldier_debug(idx,
                         soldier_render_anim,
                         anim_ctx.inputs,
                         anim_ctx.attack_phase,
                         graph_output.humanoid_selection->state,
                         soldier_lod,
                         Render::Profiling::SoldierCullReason::None,
                         transient_recovery_override);

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
      if (is_mounted_spawn &&
          (ctx.template_prewarm ||
           (!ctx.allow_template_cache && ctx.suppress_animation_state_persistence))) {
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
