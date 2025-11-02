#pragma once

#include "../../humanoid/rig.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"

namespace Render::GL {

class KingdomLightHelmetRenderer : public IEquipmentRenderer {
public:
  KingdomLightHelmetRenderer() = default;

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;
};

} // namespace Render::GL
