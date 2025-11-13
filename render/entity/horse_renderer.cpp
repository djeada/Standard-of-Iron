#include "horse_renderer.h"

namespace Render::GL {

void HorseRenderer::drawAttachments(
    const DrawContext &, const AnimationInputs &,
    const HumanoidAnimationContext &, HorseProfile &,
    const MountedAttachmentFrame &, float, float,
    float, const HorseBodyFrames &, ISubmitter &) const {
  // Base implementation does not render any equipment
  // Subclasses should override this to render nation-specific equipment
}

} // namespace Render::GL
