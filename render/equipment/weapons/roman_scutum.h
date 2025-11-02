#pragma once

#include "../../humanoid/rig.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"

namespace Render::GL {

// Roman Scutum - large curved rectangular shield
class RomanScutumRenderer : public IEquipmentRenderer {
public:
  RomanScutumRenderer() = default;

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;
};

} // namespace Render::GL
