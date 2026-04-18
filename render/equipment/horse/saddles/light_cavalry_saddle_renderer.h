#pragma once

#include "../i_horse_equipment_renderer.h"
// EquipmentBatch used in render signature

namespace Render::GL {

class LightCavalrySaddleRenderer : public IHorseEquipmentRenderer {
public:
  LightCavalrySaddleRenderer() = default;

  void render(const DrawContext &ctx, const HorseBodyFrames &frames,
              const HorseVariant &variant, const HorseAnimationContext &anim,
              EquipmentBatch &batch) const override;
};

} // namespace Render::GL
