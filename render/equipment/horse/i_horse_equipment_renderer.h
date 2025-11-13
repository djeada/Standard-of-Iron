#pragma once

#include "../../horse/rig.h"
#include "../../submitter.h"

namespace Render::GL {

struct DrawContext;

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
                      ISubmitter &out) const = 0;
};

} // namespace Render::GL
