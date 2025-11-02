#pragma once

#include "../i_equipment_renderer.h"
#include "../../humanoid/rig.h"
#include "../../palette.h"

namespace Render::GL {

// Roman light helmet - galea (auxiliary/archer helmet)
// Features conical top, decorative rings, cheek guards, neck guard, and crest/plume
// Based on historical Roman auxiliary helmets worn by archers and light infantry
class RomanLightHelmetRenderer : public IEquipmentRenderer {
public:
  RomanLightHelmetRenderer() = default;

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;
};

} // namespace Render::GL
