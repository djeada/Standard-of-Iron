#pragma once

#include "../i_horse_equipment_renderer.h"

namespace Render::GL {

class ReinsRenderer : public IHorseEquipmentRenderer {
public:
  ReinsRenderer() = default;

  void render(const DrawContext &ctx, const HorseBodyFrames &frames,
              const HorseVariant &variant, const HorseAnimationContext &anim,
              ISubmitter &out) const override;
};

} 
