#pragma once

#include "../i_horse_equipment_renderer.h"

namespace Render::GL {

class LeatherBardingRenderer : public IHorseEquipmentRenderer {
public:
  LeatherBardingRenderer() = default;

  void render(const DrawContext &ctx, const HorseBodyFrames &frames,
              const HorseVariant &variant, const HorseAnimationContext &anim,
              ISubmitter &out) const override;
};

} // namespace Render::GL
