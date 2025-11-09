#pragma once

#include "../../humanoid/rig.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"
#include "shield_renderer.h"

namespace Render::GL {

class CarthageShieldRenderer : public ShieldRenderer {
public:
  CarthageShieldRenderer();

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;
};

} // namespace Render::GL
