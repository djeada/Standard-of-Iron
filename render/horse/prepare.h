#pragma once

#include "../creature/pipeline/creature_render_graph.h"
#include "../creature/pipeline/creature_render_state.h"
#include "../creature/pipeline/prepared_submit.h"
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

[[nodiscard]] auto make_horse_prepared_row(
    const Render::GL::HorseRendererBase &owner,
    const Render::Horse::HorseSpecPose &pose,
    const Render::GL::HorseVariant &variant, const QMatrix4x4 &world_from_unit,
    std::uint32_t seed, Render::Creature::CreatureLOD lod,
    Render::Creature::Pipeline::RenderPassIntent pass =
        Render::Creature::Pipeline::RenderPassIntent::Main) noexcept
    -> Render::Creature::Pipeline::PreparedCreatureRenderRow;

void submit_prepared_horse_body(const Render::GL::HorseRendererBase &owner,
                                const Render::Horse::HorseSpecPose &pose,
                                const Render::GL::HorseVariant &variant,
                                const QMatrix4x4 &world_from_unit,
                                std::uint32_t seed,
                                Render::Creature::CreatureLOD lod,
                                Render::GL::ISubmitter &out) noexcept;

void prepare_horse_full(
    const Render::GL::HorseRendererBase &owner,
    const Render::GL::DrawContext &ctx, const Render::GL::AnimationInputs &anim,
    const Render::GL::HumanoidAnimationContext &rider_ctx,
    Render::GL::HorseProfile &profile,
    const Render::GL::MountedAttachmentFrame *shared_mount,
    const Render::GL::ReinState *shared_reins,
    const Render::GL::HorseMotionSample *shared_motion,
    Render::Creature::Pipeline::EquipmentLoadout horse_loadout,
    const Render::Creature::Pipeline::EquipmentSubmitContext *sub_ctx_template,
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
    const Render::GL::ReinState *shared_reins,
    const Render::GL::HorseMotionSample *shared_motion,
    Render::Creature::CreatureLOD lod,
    Render::Creature::Pipeline::EquipmentLoadout horse_loadout,
    const Render::Creature::Pipeline::EquipmentSubmitContext *sub_ctx_template,
    HorsePreparation &out);

} // namespace Render::Horse
