#pragma once

#include "../creature/pipeline/creature_render_graph.h"
#include "../creature/pipeline/creature_render_state.h"
#include "../creature/pipeline/prepared_submit.h"
#include "../gl/humanoid/humanoid_types.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cstdint>
#include <functional>
#include <vector>

namespace Render::GL {
struct DrawContext;
class ISubmitter;
struct AnimationInputs;
class HumanoidRendererBase;
class IFormationCalculator;
enum class AmbientIdleType : std::uint8_t;
} // namespace Render::GL

namespace Render::Humanoid {

using HumanoidPreparation = Render::Creature::Pipeline::CreaturePreparationResult;

struct SoldierLayout {
  float offset_x{0.0F};
  float offset_z{0.0F};
  float vertical_jitter{0.0F};
  float yaw_offset{0.0F};
  float phase_offset{0.0F};
  std::uint32_t inst_seed{0};
};

struct SoldierLayoutInputs {
  int idx{0};
  int row{0};
  int col{0};
  int rows{0};
  int cols{0};
  float formation_spacing{0.0F};
  std::uint32_t seed{0};
  bool force_single_soldier{false};
  bool melee_attack{false};
  float animation_time{0.0F};
};

[[nodiscard]] auto
build_soldier_layout(const Render::GL::IFormationCalculator &formation_calculator,
                     const SoldierLayoutInputs &inputs) -> SoldierLayout;

struct HumanoidAmbientIdleState {
  Render::GL::AmbientIdleType idle_type{};
  float phase{0.0F};
  int primary_index{-1};
  int secondary_index{-1};
};

[[nodiscard]] auto
build_humanoid_ambient_idle_state(const Render::GL::AnimationInputs &anim,
                                  std::uint32_t unit_seed, int visible_count,
                                  float animation_time)
    -> HumanoidAmbientIdleState;

[[nodiscard]] auto
is_humanoid_ambient_idle_active(const HumanoidAmbientIdleState &state,
                                int soldier_idx) -> bool;

struct HumanoidLocomotionInputs {
  Render::GL::AnimationInputs anim{};
  Render::GL::VariationParams variation{};
  float move_speed{0.0F};
  QVector3D locomotion_direction{0.0F, 0.0F, 1.0F};
  QVector3D movement_target{0.0F, 0.0F, 0.0F};
  bool has_movement_target{false};
  float animation_time{0.0F};
  float phase_offset{0.0F};
};

struct HumanoidLocomotionState {
  Render::GL::HumanoidMotionState motion_state{
      Render::GL::HumanoidMotionState::Idle};
  Render::GL::HumanoidGaitDescriptor gait{};
  QVector3D locomotion_direction{0.0F, 0.0F, 1.0F};
  QVector3D locomotion_velocity{0.0F, 0.0F, 0.0F};
  QVector3D movement_target{0.0F, 0.0F, 0.0F};
  float move_speed{0.0F};
  bool has_movement_target{false};
};

[[nodiscard]] auto
build_humanoid_locomotion_state(const HumanoidLocomotionInputs &inputs)
    -> HumanoidLocomotionState;

struct HumanoidRunPoseShaping {
  float lean{0.0F};
  float pelvis_setback{0.0F};
  float pelvis_drop{0.0F};
  float shoulder_drop{0.0F};
  float foot_extra_lift{0.0F};
  float stride_enhancement{0.0F};
  float arm_swing{0.0F};
  float max_arm_displacement{0.0F};
  float hand_raise{0.0F};
  float elbow_along_left{0.45F};
  float elbow_width_left{0.10F};
  float elbow_depth_left{-0.03F};
  float elbow_along_right{0.45F};
  float elbow_width_right{0.08F};
  float elbow_depth_right{0.0F};
};

[[nodiscard]] auto
build_humanoid_run_pose_shaping(const Render::GL::HumanoidAnimationContext &anim)
    -> HumanoidRunPoseShaping;

void apply_humanoid_run_pose_shaping(
    Render::GL::HumanoidPose &pose,
    const Render::GL::HumanoidAnimationContext &anim_ctx,
    const HumanoidRunPoseShaping &shaping) noexcept;

void prepare_humanoid_instances(const Render::GL::HumanoidRendererBase &owner,
                                const Render::GL::DrawContext &ctx,
                                const Render::GL::AnimationInputs &anim,
                                std::uint32_t frame_index,
                                HumanoidPreparation &out);

[[nodiscard]] auto make_humanoid_prepared_row(
    const Render::GL::HumanoidRendererBase &owner,
    const Render::GL::HumanoidPose &pose,
    const Render::GL::HumanoidVariant &variant,
    const Render::GL::HumanoidAnimationContext &anim_ctx,
    const QMatrix4x4 &inst_model, std::uint32_t inst_seed,
    Render::Creature::CreatureLOD lod,
    const Render::GL::DrawContext *legacy_ctx = nullptr,
    Render::Creature::Pipeline::RenderPassIntent pass =
        Render::Creature::Pipeline::RenderPassIntent::Main) noexcept
    -> Render::Creature::Pipeline::PreparedCreatureRenderRow;

} // namespace Render::Humanoid
