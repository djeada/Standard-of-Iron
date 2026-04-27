#pragma once

#include "../creature/pipeline/creature_render_graph.h"
#include "../creature/pipeline/creature_render_state.h"
#include "../creature/pipeline/prepared_submit.h"
#include "elephant_spec.h"

#include <QMatrix4x4>
#include <cstdint>
#include <functional>
#include <vector>

namespace Render::GL {
struct DrawContext;
struct AnimationInputs;
class ISubmitter;
struct ElephantVariant;
struct ElephantProfile;
struct HowdahAttachmentFrame;
struct ElephantMotionSample;
} // namespace Render::GL

namespace Render::GL {
class ElephantRendererBase;
}

namespace Render::Elephant {

using ElephantPreparation =
    Render::Creature::Pipeline::CreaturePreparationResult;

[[nodiscard]] auto make_elephant_prepared_row(
    const Render::GL::ElephantRendererBase &owner,
    const Render::Elephant::ElephantSpecPose &pose,
    const Render::GL::ElephantVariant &variant,
    const QMatrix4x4 &world_from_unit, std::uint32_t seed,
    Render::Creature::CreatureLOD lod,
    Render::Creature::Pipeline::RenderPassIntent pass =
        Render::Creature::Pipeline::RenderPassIntent::Main) noexcept
    -> Render::Creature::Pipeline::PreparedCreatureRenderRow;

void submit_prepared_elephant_body(
    const Render::GL::ElephantRendererBase &owner,
    const Render::Elephant::ElephantSpecPose &pose,
    const Render::GL::ElephantVariant &variant,
    const QMatrix4x4 &world_from_unit, std::uint32_t seed,
    Render::Creature::CreatureLOD lod, Render::GL::ISubmitter &out) noexcept;

void prepare_elephant_full(
    const Render::GL::ElephantRendererBase &owner,
    const Render::GL::DrawContext &ctx, const Render::GL::AnimationInputs &anim,
    Render::GL::ElephantProfile &profile,
    const Render::GL::HowdahAttachmentFrame *shared_howdah,
    const Render::GL::ElephantMotionSample *shared_motion,
    ElephantPreparation &out);

void prepare_elephant_simplified(
    const Render::GL::ElephantRendererBase &owner,
    const Render::GL::DrawContext &ctx, const Render::GL::AnimationInputs &anim,
    Render::GL::ElephantProfile &profile,
    const Render::GL::HowdahAttachmentFrame *shared_howdah,
    const Render::GL::ElephantMotionSample *shared_motion,
    ElephantPreparation &out);

void prepare_elephant_minimal(
    const Render::GL::ElephantRendererBase &owner,
    const Render::GL::DrawContext &ctx, Render::GL::ElephantProfile &profile,
    const Render::GL::ElephantMotionSample *shared_motion,
    ElephantPreparation &out);

void prepare_elephant_render(
    const Render::GL::ElephantRendererBase &owner,
    const Render::GL::DrawContext &ctx, const Render::GL::AnimationInputs &anim,
    Render::GL::ElephantProfile &profile,
    const Render::GL::HowdahAttachmentFrame *shared_howdah,
    const Render::GL::ElephantMotionSample *shared_motion,
    Render::Creature::CreatureLOD lod, ElephantPreparation &out);

} // namespace Render::Elephant
