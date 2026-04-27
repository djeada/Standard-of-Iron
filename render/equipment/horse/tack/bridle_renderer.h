#pragma once

#include "../i_horse_equipment_renderer.h"

namespace Render::GL {

class BridleRenderer : public IHorseEquipmentRenderer {
public:
  BridleRenderer() = default;

  static void submit(const DrawContext &ctx, const HorseBodyFrames &frames,
                     const HorseVariant &variant,
                     const HorseAnimationContext &anim, EquipmentBatch &batch);

  void render(const DrawContext &ctx, const HorseBodyFrames &frames,
              const HorseVariant &variant, const HorseAnimationContext &anim,
              EquipmentBatch &batch) const override {
    submit(ctx, frames, variant, anim, batch);
  }
};

} // namespace Render::GL
