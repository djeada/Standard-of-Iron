#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>
#include <functional>
#include <vector>

#include "animation/individuality_manifest.h"
#include "game/formation/unit_layout.h"
#include "render/creature/pipeline/creature_render_graph.h"
#include "render/creature/pipeline/creature_render_state.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/humanoid/runtime/runtime_context.h"

namespace Render::GL {
struct DrawContext;
class ISubmitter;
struct AnimationInputs;
class HumanoidRendererBase;
} // namespace Render::GL

namespace Render::Creature {
struct HumanoidAnimationStateComponent;
}

namespace Render::Humanoid {

using HumanoidPreparation = Render::Creature::Pipeline::CreaturePreparationResult;

struct HumanoidLocomotionInputs {
  Render::GL::AnimationInputs anim{};
  Render::GL::VariationParams variation{};
  float move_speed{0.0F};
  QVector3D entity_forward{0.0F, 0.0F, 1.0F};
  QVector3D locomotion_direction{0.0F, 0.0F, 1.0F};
  QVector3D movement_target{0.0F, 0.0F, 0.0F};
  bool has_movement_target{false};
  float animation_time{0.0F};
  Animation::SoldierIndividuality individuality{};
  Render::Creature::HumanoidAnimationStateComponent* persistent_state{nullptr};
  bool allow_persistent_update{false};
};

struct HumanoidLocomotionState {
  Render::GL::HumanoidGaitDescriptor gait{};
  QVector3D locomotion_direction{0.0F, 0.0F, 1.0F};
  QVector3D locomotion_velocity{0.0F, 0.0F, 0.0F};
  QVector3D movement_target{0.0F, 0.0F, 0.0F};
  float move_speed{0.0F};
  bool has_movement_target{false};
};

[[nodiscard]] auto build_humanoid_locomotion_state(
    const HumanoidLocomotionInputs& inputs) -> HumanoidLocomotionState;

void prepare_humanoid_instances(const Render::GL::HumanoidRendererBase& owner,
                                const Render::GL::DrawContext& ctx,
                                const Render::GL::AnimationInputs& anim,
                                HumanoidRuntimeContext& runtime,
                                HumanoidPreparation& out);

} // namespace Render::Humanoid
