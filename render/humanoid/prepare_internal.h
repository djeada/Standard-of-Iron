#pragma once

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
#include "../../game/systems/formation_combat_geometry.h"
#include "../../game/systems/nation_id.h"
#include "../../game/systems/troop_profile_service.h"
#include "../../game/units/spawn_type.h"
#include "../../game/units/troop_config.h"
#include "../../game/visuals/team_colors.h"
#include "../creature/animation_core_bridge.h"
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
#include "animation/action_manifest.h"
#include "animation/ambient_pose_manifest.h"
#include "animation/layout_manifest.h"
#include "animation/locomotion_manifest.h"
#include "animation/micro_variation_manifest.h"
#include "cache_control.h"
#include "facial_hair_catalog.h"
#include "formation_calculator.h"
#include "humanoid_math.h"
#include "humanoid_renderer_base.h"
#include "humanoid_spec.h"
#include "pose_cache_components.h"
#include "pose_controller.h"
#include "prepare.h"
#include "render_stats.h"
#include "rig_stats_shim.h"
#include "skeleton.h"

namespace Render::GL {

using namespace Render::GL::Geometry;

inline constexpr uint32_t k_pose_cache_max_age = 300;
inline constexpr uint32_t k_layout_cache_max_age = 600;

extern HumanoidRenderStats s_render_stats;

auto shared_guard_shield_pose(
    const Engine::Core::UnitComponent* unit,
    const Render::Creature::Pipeline::UnitVisualSpec& visual_spec,
    bool formation_active,
    bool guard_mode_active,
    int row,
    int col,
    int rows,
    int cols) noexcept -> ShieldFormationPose;

auto resolve_construction_role(
    const Render::Creature::Pipeline::UnitVisualSpec& visual_spec,
    std::uint32_t inst_seed,
    bool force_single_soldier) noexcept -> ConstructionRole;

void apply_spec_pose_layer(
    const Render::Creature::Pipeline::UnitVisualSpec& visual_spec,
    const DrawContext& ctx,
    const HumanoidAnimationContext& anim_ctx,
    const HumanoidVariant& variant,
    std::uint32_t inst_seed,
    HumanoidPose& pose);

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

inline constexpr std::uint32_t k_humanoid_layout_cache_version = 4U;
inline constexpr float k_humanoid_idle_cycle_time = 1.6F;
inline constexpr float k_locomotion_blend_tau = 0.12F;
inline constexpr float k_cadence_blend_tau = 0.14F;
inline constexpr float k_run_blend_tau = 0.16F;
inline constexpr float k_turn_blend_tau = 0.10F;
inline constexpr float k_acceleration_blend_tau = 0.14F;
inline constexpr float k_visual_locomotion_speed_epsilon = 1.0e-4F;

class HumanoidPreparationModePolicy {
public:
  explicit HumanoidPreparationModePolicy(const Engine::Core::Entity* entity)
      : m_presentation(
            entity != nullptr
                ? entity->get_component<Engine::Core::CreaturePresentationComponent>()
                : nullptr) {}

  [[nodiscard]] auto jump_pose() const -> Animation::HumanoidCommanderJumpPose {
    return Animation::resolve_humanoid_commander_jump_pose({
        .has_commander = m_presentation != nullptr && m_presentation->has_commander,
        .active = m_presentation != nullptr && m_presentation->jump_active,
        .phase = m_presentation != nullptr ? m_presentation->jump_phase : 0.0F,
        .height_offset =
            m_presentation != nullptr ? m_presentation->jump_height_offset : 0.0F,
    });
  }

  [[nodiscard]] auto
  flag_rally_pose() const -> Animation::HumanoidCommanderFlagRallyPose {
    return Animation::resolve_humanoid_commander_flag_rally_pose({
        .has_commander = m_presentation != nullptr && m_presentation->has_commander,
        .planting = m_presentation != nullptr && m_presentation->flag_rally_planting,
        .animation_timer = m_presentation != nullptr
                               ? m_presentation->flag_rally_animation_timer
                               : 0.0F,
        .cost = m_presentation != nullptr ? m_presentation->flag_rally_cost : 0.0F,
    });
  }

private:
  const Engine::Core::CreaturePresentationComponent* m_presentation = nullptr;
};

void reset_humanoid_locomotion_state(
    Render::Creature::HumanoidAnimationStateComponent& state);
void sync_combat_visual_inputs(
    AnimationInputs& inputs, const Render::Creature::CombatVisualResolvedState& combat);
void apply_combat_micro_variation(const HumanoidAnimationContext& anim_ctx,
                                  std::uint32_t inst_seed,
                                  bool multi_soldier,
                                  HumanoidPose& pose);
auto flattened_or(const QVector3D& v, const QVector3D& fallback) noexcept -> QVector3D;

struct VisualLocomotionSample {
  float speed = 0.0F;
  QVector3D direction{0.0F, 0.0F, 1.0F};
  QVector3D movement_target{0.0F, 0.0F, 0.0F};
  bool has_movement_target = false;
};

auto resolve_visual_locomotion_sample(const Render::GL::VisualMovementState& movement,
                                      const QVector3D& forward) noexcept
    -> VisualLocomotionSample;
} // namespace Render::Humanoid
