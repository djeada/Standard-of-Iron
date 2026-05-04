#pragma once

#include "../creature/pipeline/creature_prepared_state.h"
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

void prepare_humanoid_instances(const Render::GL::HumanoidRendererBase &owner,
                                const Render::GL::DrawContext &ctx,
                                const Render::GL::AnimationInputs &anim,
                                std::uint32_t frame_index,
                                HumanoidPreparation &out);

} // namespace Render::Humanoid
