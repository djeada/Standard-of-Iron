#include "horse_renderer.h"

#include "../equipment/horse/saddles/roman_saddle_renderer.h"
#include "../equipment/horse/tack/reins_renderer.h"

namespace Render::GL {

void HorseRenderer::drawAttachments(
    const DrawContext &ctx, const AnimationInputs &anim,
    const HumanoidAnimationContext &rider_ctx, HorseProfile &profile,
    const MountedAttachmentFrame &mount, float phase, float bob,
    float rein_slack, const HorseBodyFrames &frames, ISubmitter &out) const {

  HorseAnimationContext horse_anim;
  horse_anim.time = anim.time;
  horse_anim.phase = phase;
  horse_anim.bob = bob;
  horse_anim.is_moving = anim.isMoving;
  horse_anim.rider_intensity = 0.0F;

  RomanSaddleRenderer saddle_renderer;
  saddle_renderer.render(ctx, frames, profile.variant, horse_anim, out);

  ReinsRenderer reins_renderer;
  reins_renderer.render(ctx, frames, profile.variant, horse_anim, out);
}

} // namespace Render::GL
