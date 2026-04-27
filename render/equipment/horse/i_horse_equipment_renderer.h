#pragma once

#include "../../horse/horse_renderer_base.h"

namespace Render::GL {

struct DrawContext;
struct EquipmentBatch;

struct HorseAnimationContext {
  float time{0.0F};
  float phase{0.0F};
  float bob{0.0F};
  bool is_moving{false};
  float rider_intensity{0.0F};
};

class IHorseEquipmentRenderer {
public:
  virtual ~IHorseEquipmentRenderer() = default;

  virtual void render(const DrawContext &ctx, const HorseBodyFrames &frames,
                      const HorseVariant &variant,
                      const HorseAnimationContext &anim,
                      EquipmentBatch &batch) const = 0;
};

} // namespace Render::GL
