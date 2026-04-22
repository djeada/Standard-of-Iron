#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"

namespace Render::GL {

struct TunicConfig {
  float torso_scale = 1.06F;
  float shoulder_width_scale = 1.2F;
  float chest_depth_scale = 0.85F;
  float waist_taper = 0.92F;
  bool include_pauldrons = true;
  bool include_gorget = true;
  bool include_belt = true;
};

class TunicRenderer : public IEquipmentRenderer {
public:
  explicit TunicRenderer(const TunicConfig &config = TunicConfig{});

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;

private:
  TunicConfig m_config;
};

} // namespace Render::GL
