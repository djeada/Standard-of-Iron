#pragma once

#include "../../humanoid/rig.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"

namespace Render::GL {

class CarthageShoulderCoverRenderer : public IEquipmentRenderer {
public:
  explicit CarthageShoulderCoverRenderer(float outward_scale = 1.0F)
      : m_outward_scale(outward_scale) {}

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              ISubmitter &submitter) override;

private:
  float m_outward_scale = 1.0F;
};

} 
