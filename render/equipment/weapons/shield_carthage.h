#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"
#include "shield_renderer.h"

namespace Render::GL {

struct CarthageShieldConfig {
  float scale_multiplier = 1.0F;
};

class CarthageShieldRenderer : public ShieldRenderer {
public:
  explicit CarthageShieldRenderer(float scale_multiplier = 1.0F);

  static void submit(const CarthageShieldConfig &config,
                     const DrawContext &ctx, const BodyFrames &frames,
                     const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;

  [[nodiscard]] auto base_config() const -> CarthageShieldConfig {
    return {m_scale_multiplier};
  }

private:
  float m_scale_multiplier = 1.0F;
};

} // namespace Render::GL
