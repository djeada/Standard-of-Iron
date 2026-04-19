#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
#include "../i_equipment_renderer.h"

namespace Render::GL {

class RomanGreavesRenderer : public IEquipmentRenderer {
public:
  RomanGreavesRenderer() = default;

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;
};

} // namespace Render::GL
