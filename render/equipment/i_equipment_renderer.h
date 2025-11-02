#pragma once

#include "../humanoid/rig.h"
#include "../palette.h"
#include "../submitter.h"

namespace Render::GL {

struct DrawContext;

class IEquipmentRenderer {
public:
  virtual ~IEquipmentRenderer() = default;

  virtual void render(const DrawContext &ctx, const BodyFrames &frames,
                      const HumanoidPalette &palette,
                      const HumanoidAnimationContext &anim,
                      ISubmitter &submitter) = 0;
};

} // namespace Render::GL
