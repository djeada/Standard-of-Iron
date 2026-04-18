#pragma once

#include "../i_horse_equipment_renderer.h"
// EquipmentBatch used in render signature

namespace Render::GL {

class RomanSaddleRenderer : public IHorseEquipmentRenderer {
public:
  RomanSaddleRenderer() = default;

  void render(const DrawContext &ctx, const HorseBodyFrames &frames,
              const HorseVariant &variant, const HorseAnimationContext &anim,
              EquipmentBatch &batch) const override;
};

} // namespace Render::GL
