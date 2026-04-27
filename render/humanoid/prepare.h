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

using HumanoidPreparation =
    Render::Creature::Pipeline::CreaturePreparationResult;

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

[[nodiscard]] auto build_soldier_layout(
    const Render::GL::IFormationCalculator &formation_calculator,
    const SoldierLayoutInputs &inputs) -> SoldierLayout;

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

[[nodiscard]] auto build_humanoid_locomotion_state(
    const HumanoidLocomotionInputs &inputs) -> HumanoidLocomotionState;

void prepare_humanoid_instances(const Render::GL::HumanoidRendererBase &owner,
                                const Render::GL::DrawContext &ctx,
                                const Render::GL::AnimationInputs &anim,
                                std::uint32_t frame_index,
                                HumanoidPreparation &out);

} // namespace Render::Humanoid
