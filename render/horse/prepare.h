#pragma once

#include "../creature/pipeline/creature_render_graph.h"
#include "../creature/pipeline/unit_visual_spec.h"
#include "horse_spec.h"

#include <QMatrix4x4>
#include <cstdint>
#include <functional>
#include <vector>

namespace Render::GL {
struct DrawContext;
struct AnimationInputs;
struct HumanoidAnimationContext;
class ISubmitter;
struct HorseVariant;
struct HorseProfile;
struct MountedAttachmentFrame;
struct ReinState;
struct HorseMotionSample;
class HorseRendererBase;
} // namespace Render::GL

namespace Render::Horse {

using HorsePreparation = Render::Creature::Pipeline::CreaturePreparationResult;

void prepare_horse_full(const Render::GL::HorseRendererBase &owner,
                        const Render::GL::DrawContext &ctx,
                        const Render::GL::AnimationInputs &anim,
                        const Render::GL::HumanoidAnimationContext &rider_ctx,
                        Render::GL::HorseProfile &profile,
                        const Render::GL::MountedAttachmentFrame *shared_mount,
                        const Render::GL::HorseMotionSample *shared_motion,
                        HorsePreparation &out);

void prepare_horse_simplified(
    const Render::GL::HorseRendererBase &owner,
    const Render::GL::DrawContext &ctx, const Render::GL::AnimationInputs &anim,
    const Render::GL::HumanoidAnimationContext &rider_ctx,
    Render::GL::HorseProfile &profile,
    const Render::GL::MountedAttachmentFrame *shared_mount,
    const Render::GL::HorseMotionSample *shared_motion, HorsePreparation &out);

void prepare_horse_minimal(const Render::GL::HorseRendererBase &owner,
                           const Render::GL::DrawContext &ctx,
                           Render::GL::HorseProfile &profile,
                           const Render::GL::HorseMotionSample *shared_motion,
                           HorsePreparation &out);

void prepare_horse_render(
    const Render::GL::HorseRendererBase &owner,
    const Render::GL::DrawContext &ctx, const Render::GL::AnimationInputs &anim,
    const Render::GL::HumanoidAnimationContext &rider_ctx,
    Render::GL::HorseProfile &profile,
    const Render::GL::MountedAttachmentFrame *shared_mount,
    const Render::GL::HorseMotionSample *shared_motion,
    Render::Creature::CreatureLOD lod, HorsePreparation &out);

void prepare_horse_render(
    const Render::GL::HorseRendererBase &owner,
    const Render::GL::DrawContext &ctx, const Render::GL::AnimationInputs &anim,
    const Render::GL::HumanoidAnimationContext &rider_ctx,
    Render::GL::HorseProfile &profile,
    const Render::GL::MountedAttachmentFrame *shared_mount,
    const Render::GL::HorseMotionSample *shared_motion,
    Render::Creature::CreatureLOD lod, HorsePreparation &out,
    std::uint32_t request_seed);

} // namespace Render::Horse
