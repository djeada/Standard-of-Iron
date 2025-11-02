#pragma once

#include "../i_equipment_renderer.h"
#include "../../humanoid/rig.h"
#include "../../palette.h"

namespace Render::GL {

// Kingdom light helmet - kettle hat style with wide brim
// Used by archers and light infantry for protection from arrows
class KingdomLightHelmetRenderer : public IEquipmentRenderer {
public:
  KingdomLightHelmetRenderer() = default;

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;
};

} // namespace Render::GL
