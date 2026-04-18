#pragma once

#include "../../humanoid/rig.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"

namespace Render::GL {

class HeadwrapRenderer : public IEquipmentRenderer {
public:
  HeadwrapRenderer() = default;

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;
};

} // namespace Render::GL
