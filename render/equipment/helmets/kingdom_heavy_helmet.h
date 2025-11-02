#pragma once

#include "../i_equipment_renderer.h"
#include "../../humanoid/rig.h"
#include "../../palette.h"

namespace Render::GL {

// Kingdom heavy helmet - great helm style for knights and heavy infantry
// Features enclosed design with eye slits and breathing holes
class KingdomHeavyHelmetRenderer : public IEquipmentRenderer {
public:
  KingdomHeavyHelmetRenderer() = default;

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;
};

} // namespace Render::GL
