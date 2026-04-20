#pragma once

#include "../creature/pipeline/creature_render_state.h"
#include "../creature/pipeline/prepared_submit.h"
#include "../gl/humanoid/humanoid_types.h"

#include <QMatrix4x4>
#include <cstdint>
#include <functional>
#include <vector>

namespace Render::GL {
struct DrawContext;
class ISubmitter;
struct AnimationInputs;
class HumanoidRendererBase;
} // namespace Render::GL

namespace Render::Humanoid {

struct HumanoidPreparation {
  std::vector<Render::Creature::Pipeline::PreparedCreatureRenderRow> rows;
  std::vector<Render::GL::DrawContext> per_instance_ctx;
  std::vector<std::function<void(Render::GL::ISubmitter &)>> post_body_draws;
};

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
