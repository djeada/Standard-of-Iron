#pragma once

#include "../i_horse_equipment_renderer.h"

namespace Render::GL {

class PlumeRenderer : public IHorseEquipmentRenderer {
public:
  PlumeRenderer() = default;

  void render(const DrawContext &ctx, const HorseBodyFrames &frames,
              const HorseVariant &variant, const HorseAnimationContext &anim,
              ISubmitter &out) const override;
};

} // namespace Render::GL
