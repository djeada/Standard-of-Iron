#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
#include "../i_equipment_renderer.h"
#include <QVector3D>

namespace Render::GL {

struct RomanShoulderCoverConfig {
  float outward_scale = 1.0F;
};

class RomanShoulderCoverRenderer : public IEquipmentRenderer {
public:
  explicit RomanShoulderCoverRenderer(float outward_scale = 1.0F)
      : m_config{outward_scale} {}

  static void submit(const RomanShoulderCoverConfig &config,
                     const DrawContext &ctx, const BodyFrames &frames,
                     const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;

  [[nodiscard]] auto base_config() const -> const RomanShoulderCoverConfig & {
    return m_config;
  }

private:
  RomanShoulderCoverConfig m_config;
};

} // namespace Render::GL
