#pragma once

#include "../horse/rig.h"

namespace Render::GL {

class HorseRenderer : public HorseRendererBase {
public:
  using HorseRendererBase::render;

protected:
  void drawAttachments(const DrawContext &ctx, const AnimationInputs &anim,
                       const HumanoidAnimationContext &rider_ctx,
                       HorseProfile &profile,
                       const MountedAttachmentFrame &mount, float phase,
                       float bob, float rein_slack,
                       const HorseBodyFrames &frames,
                       ISubmitter &out) const override;
};

} // namespace Render::GL
