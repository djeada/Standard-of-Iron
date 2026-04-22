#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
#include "../../palette.h"
#include "../i_equipment_renderer.h"

namespace Render::GL {

struct CarthageShoulderCoverConfig {
  float outward_scale = 1.0F;
};

class CarthageShoulderCoverRenderer : public IEquipmentRenderer {
public:
  explicit CarthageShoulderCoverRenderer(float outward_scale = 1.0F)
      : m_config{outward_scale} {}

  static void submit(const CarthageShoulderCoverConfig &config,
                     const DrawContext &ctx, const BodyFrames &frames,
                     const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;

  [[nodiscard]] auto base_config() const
      -> const CarthageShoulderCoverConfig & {
    return m_config;
  }

private:
  CarthageShoulderCoverConfig m_config;
};

} // namespace Render::GL
