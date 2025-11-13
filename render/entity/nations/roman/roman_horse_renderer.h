#pragma once

#include "../../horse_renderer.h"

namespace Render::GL::Roman {

class RomanHorseRenderer : public HorseRenderer {
protected:
  void drawAttachments(const DrawContext &ctx, const AnimationInputs &anim,
                       const HumanoidAnimationContext &rider_ctx,
                       HorseProfile &profile,
                       const MountedAttachmentFrame &mount, float phase,
                       float bob, float rein_slack,
                       const HorseBodyFrames &frames,
                       ISubmitter &out) const override;
};

} // namespace Render::GL::Roman
