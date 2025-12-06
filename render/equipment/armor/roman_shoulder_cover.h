#pragma once

#include "../../humanoid/rig.h"
#include "../i_equipment_renderer.h"
#include <QVector3D>

namespace Render::GL {

class RomanShoulderCoverRenderer : public IEquipmentRenderer {
public:
  RomanShoulderCoverRenderer() = default;
  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;
};

} // namespace Render::GL
