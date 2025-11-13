#include "carthage_horse_renderer.h"

#include "../../../equipment/horse/i_horse_equipment_renderer.h"
#include "../../../equipment/horse/saddles/carthage_saddle_renderer.h"
#include "../../../equipment/horse/tack/reins_renderer.h"

namespace Render::GL::Carthage {

void CarthageHorseRenderer::drawAttachments(
    const DrawContext &ctx, const AnimationInputs &anim,
    const HumanoidAnimationContext &, HorseProfile &profile,
    const MountedAttachmentFrame &, float phase, float bob, float,
    const HorseBodyFrames &frames, ISubmitter &out) const {

  HorseAnimationContext horse_anim;
  horse_anim.time = anim.time;
  horse_anim.phase = phase;
  horse_anim.bob = bob;
  horse_anim.is_moving = anim.isMoving;
  horse_anim.rider_intensity = 0.0F;

  CarthageSaddleRenderer saddle_renderer;
  saddle_renderer.render(ctx, frames, profile.variant, horse_anim, out);

  ReinsRenderer reins_renderer;
  reins_renderer.render(ctx, frames, profile.variant, horse_anim, out);
}

} // namespace Render::GL::Carthage
