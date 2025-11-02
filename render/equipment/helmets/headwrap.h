#pragma once

#include "../i_equipment_renderer.h"
#include "../../humanoid/rig.h"
#include "../../palette.h"

namespace Render::GL {

class HeadwrapRenderer : public IEquipmentRenderer {
public:
  HeadwrapRenderer() = default;

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;
};

} // namespace Render::GL
