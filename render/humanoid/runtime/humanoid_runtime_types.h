#pragma once

#include <cstdint>

#include "../../../game/core/component_presentation.h"
#include "animation/action_manifest.h"
#include "animation/ambient_pose_manifest.h"
#include "render/creature/animation_state_components.h"
#include "render/creature/combat_visual_state.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/geom/transforms.h"
#include "render/gl/humanoid/humanoid_constants.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/humanoid/runtime/humanoid_math.h"
#include "render/humanoid/runtime/prepare.h"

namespace Engine::Core {
class Entity;
class UnitComponent;
class CreaturePresentationComponent;
} // namespace Engine::Core

namespace Render::GL {

using namespace Render::GL::Geometry;

inline constexpr uint32_t k_pose_cache_max_age = 300;
inline constexpr uint32_t k_layout_cache_max_age = 600;

auto shared_guard_shield_pose(
    const Engine::Core::UnitComponent* unit,
    const Render::Creature::Pipeline::UnitVisualSpec& visual_spec,
    bool formation_active,
    bool guard_mode_active,
    bool defensive_layout_locked,
    int row,
    int col,
    int rows,
    int cols) noexcept -> ShieldFormationPose;

auto resolve_construction_role(
    const Render::Creature::Pipeline::UnitVisualSpec& visual_spec,
    std::uint32_t inst_seed,
    bool force_single_soldier,
    std::uint8_t construction_job) noexcept -> ConstructionRole;

void apply_spec_pose_policy(
    const Render::Creature::Pipeline::UnitVisualSpec& visual_spec,
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
using Render::GL::FormationParams;
using Render::GL::HumanoidAnimationContext;
using Render::GL::HumanoidLOD;
using Render::GL::HumanoidMotionState;
using Render::GL::HumanoidPose;
using Render::GL::HumanoidRendererBase;
using Render::GL::HumanoidVariant;
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
