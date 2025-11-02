#pragma once

#include "../i_equipment_renderer.h"
#include "../../humanoid/rig.h"
#include "../../palette.h"

namespace Render::GL {

// Roman light helmet - simple cap-style helmet for archers and light infantry
// Based on historical Roman auxiliary helmets with minimal protection
class RomanLightHelmetRenderer : public IEquipmentRenderer {
public:
  RomanLightHelmetRenderer() = default;

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;
};

} // namespace Render::GL
