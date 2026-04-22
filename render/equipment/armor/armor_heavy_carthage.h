#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"

namespace Render::GL {

struct ArmorHeavyCarthageConfig {};

class ArmorHeavyCarthageRenderer : public IEquipmentRenderer {
public:
  ArmorHeavyCarthageRenderer() = default;

  static void submit(const ArmorHeavyCarthageConfig &config,
                     const DrawContext &ctx, const BodyFrames &frames,
                     const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;
};

} // namespace Render::GL
