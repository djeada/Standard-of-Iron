#pragma once

#include <QMatrix4x4>

#include <cstdint>
#include <functional>
#include <vector>

#include "elephant_spec.h"
#include "render/creature/pipeline/creature_render_graph.h"

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

using ElephantPreparation = Render::Creature::Pipeline::CreaturePreparationResult;

void prepare_elephant_render(const Render::GL::ElephantRendererBase& owner,
                             const Render::GL::DrawContext& ctx,
                             const Render::GL::AnimationInputs& anim,
                             Render::GL::ElephantProfile& profile,
                             const Render::GL::HowdahAttachmentFrame* shared_howdah,
                             const Render::GL::ElephantMotionSample* shared_motion,
                             Render::Creature::CreatureLOD lod,
                             ElephantPreparation& out,
                             QMatrix4x4* out_howdah_world = nullptr);

void submit_howdah(const Render::GL::DrawContext& ctx,
                   const Render::GL::ElephantProfile& profile,
                   const QMatrix4x4& world,
                   Render::GL::ISubmitter& out);

} // namespace Render::Elephant
