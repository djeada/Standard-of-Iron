#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
#include "../i_equipment_renderer.h"

namespace Render::GL {

struct RomanGreavesConfig {};

class RomanGreavesRenderer : public IEquipmentRenderer {
public:
  RomanGreavesRenderer() = default;

  static void submit(const RomanGreavesConfig &config, const DrawContext &ctx,
                     const BodyFrames &frames, const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;
};

} // namespace Render::GL
