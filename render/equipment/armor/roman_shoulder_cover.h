#pragma once

#include "../../humanoid/rig.h"
#include "../i_equipment_renderer.h"
#include <QVector3D>

namespace Render::GL {

class RomanShoulderCoverRenderer : public IEquipmentRenderer {
public:
  explicit RomanShoulderCoverRenderer(float outward_scale = 1.0F)
      : m_outward_scale(outward_scale) {}
  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;

private:
  float m_outward_scale = 1.0F;
};

} // namespace Render::GL
